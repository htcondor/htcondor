/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Anand Narayanan
** 
*/ 

#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include "trace.h"
#include "expr.h"
#include "files.h"
#include "debug.h"
#include "except.h"
#include "config.h"
#include "clib.h"
#include "condor_classad.h"

#if defined(__cplusplus)
extern "C" {
#endif

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

char	*getline(), *expand_macro(), *ltrunc(), *lookup_macro();

extern int		ConfigLineNo;

#define ISIDCHAR(c)		(isalnum(c) || ((c) == '_'))
#define ISOP(c)		(((c) == '=') || ((c) == ':'))

int Read_config(char* tilde, char* config_name, ClassAd* classAd,
		BUCKET** table, int table_size, int expand_flag)
{
  	FILE	*conf_fp;
	char	*name, *value, *rhs;
	char	*ptr;
	char	op;
	char	file_name[1024];

	ConfigLineNo = 0;

	(void)sprintf(file_name, "%s/%s", tilde, config_name);
	conf_fp = fopen(file_name, "r");
	if( conf_fp == NULL ) {
		return( -1 );
	}

	for(;;) {
		name = getline(conf_fp);
		if( name == NULL ) {
			break;
		}
		
		/* Skip over comments */
		if( *name == '#' || blankline(name) )
			continue;

		ptr = name;

		// Assumption is that the line starts with a non whitespace
		// character
		// Example :
		// OP_SYS=SunOS
		while( *ptr ) {
			if( isspace(*ptr) || ISOP(*ptr) ) {
			  /* *ptr is now whitespace or '=' or ':' */
			  break;
			} else {
			  ptr++;
			}
		}

		if( !*ptr ) {
			(void)fclose( conf_fp );
			return( -1 );
		}

		if( ISOP(*ptr) ) {
			op = *ptr;
			//op is now '=' in the above eg
			*ptr++ = '\0';
			// name is now 'OpSys' in the above eg
		} else {
			*ptr++ = '\0';
			while( *ptr && !ISOP(*ptr) ) {
				ptr++;
			}

			if( !*ptr ) {
				(void)fclose( conf_fp );
				return( -1 );
			}

			op = *ptr++;
		}

		/* Skip to next non-space character */
		while( *ptr && isspace(*ptr) ) {
			ptr++;
		}

		rhs = ptr;
		// rhs is now 'SunOS' in the above eg
		
		/* Expand references to other parameters */
		name = expand_macro( name, table, table_size );
		if( name == NULL ) {
			(void)fclose( conf_fp );
			return( -1 );
		}

		/* Check that "name" is a legal identifier : only
		   alphanumeric characters and _ allowed*/
		ptr = name;
		while( *ptr ) {
			char c = *ptr++;
			if( !ISIDCHAR(c) ) {
				(void) fclose( conf_fp );
				fprintf( stderr,
		"Configuration Error File <%s>, Line %d: Illegal Identifier: <%s>\n",
					file_name, ConfigLineNo, name );
				return( -1 );
			}
		}

		if( expand_flag == EXPAND_IMMEDIATE ) {
			value = expand_macro( rhs, table, table_size );
			if( value == NULL ) {
				(void)fclose( conf_fp );
				return( -1 );
			}
		} else  {
			value = strdup( rhs );
		}  

		if( op == ':' ) {
			if( classAd != NULL ) {
				char *evalue;
				char *line;

				if( expand_flag == EXPAND_IMMEDIATE ) {
					evalue = strdup( value );
				} else {
					evalue = expand_macro( value, table, table_size );
					if( evalue == NULL ) {
						(void)fclose( conf_fp );
						return( -1 );
					}
				}

				line = (char*)MALLOC( (unsigned)(strlen(name) + strlen(evalue) + 4) );

				if( line == NULL ) {
					EXCEPT("Out of memory" );
				}

				(void)sprintf(line, "%s = %s", name, evalue);
				FREE( evalue );

				if(!(classAd->Insert(line))) 
				{
				  EXCEPT("Expression syntax error in <%s> line %d",
					 file_name, ConfigLineNo );
				}

				FREE( line );
			}
		} else if( op == '=' ) {
			lower_case( name );

			/* Put the value in the Configuration Table */
			insert( name, value, table, table_size );
		} else {
			fprintf( stderr,
				"Configuration Error File <%s>, Line %d: Syntax Error\n",
				file_name, ConfigLineNo );
			(void)fclose( conf_fp );
			return -1;
		}

		FREE( name );
		FREE( value );
	}

	(void)fclose( conf_fp );
	return 0;
}

#if defined(__cplusplus)
}
#endif
