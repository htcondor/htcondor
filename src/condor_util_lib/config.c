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
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


/*
** This file implements a configuration table which is read from the config
** file.  The file contains lines of the form "string = value" where either
** "string" or "value" may contain references to other (previously defined)
** parameters of the form "$(parameter)".  Blank lines and lines beginning
** with '#' are considered comments.  Forward references are not allowed,
** but multiple and or nested references are allowed.  The table is stored
** as a chain bucket hash table.
*/


#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <netdb.h>
#include <stdlib.h>
#include "trace.h"
#include "expr.h"
#include "files.h"
#include "debug.h"
#include "except.h"
#include "config.h"
#include "clib.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

char	*strdup(), *getline(), *expand_macro(), *ltrunc(),
		*index(), *rindex(), *lookup_macro();

int		ConfigLineNo;

#define ISIDCHAR(c)		(isalnum(c) || ((c) == '_'))
#define ISOP(c)		(((c) == '=') || ((c) == ':'))



/*
** Just compute a hash value for the given string such that
** 0 <= value < size 
*/
hash( string, size )
register char	*string;
register int	size;
{
	register unsigned int		answer;

	answer = 1;

	for( ; *string; string++ ) {
		answer <<= 1;
		answer += (int)*string;
	}
	answer >>= 1;	/* Make sure not negative */
	answer %= size;
	return answer;
}

/*
** Insert the parameter name and value into the given hash table.
*/
insert( name, value, table, table_size )
char	*name;
char	*value;
BUCKET	*table[];
int		table_size;
{
	register BUCKET	*ptr;
	int		loc;
	BUCKET	*bucket;
	char	tmp_name[ 1024 ];

		/* Make sure not already in hash table */
	strcpy( tmp_name, name );
	lower_case( tmp_name );
	loc = hash( tmp_name, table_size );
	for( ptr=table[loc]; ptr; ptr=ptr->next ) {
		if( strcmp(tmp_name,ptr->name) == 0 ) {
			FREE( ptr->value );
			ptr->value = strdup( value );
			return;
		}
	}

		/* Insert it */
	bucket = (BUCKET *)MALLOC( sizeof(BUCKET) );
	bucket->name = strdup( tmp_name );
	bucket->value = strdup( value );
	bucket->next = table[ loc ];
	table[loc] = bucket;
}

/*
** Read in the config file and produce the hash table (table).
*/
read_config( tilde, config_name, context, table, table_size, expand_flag )
char	*tilde, *config_name;
CONTEXT *context;
BUCKET	*table[];
int		table_size;
int		expand_flag;
{
	FILE	*conf_fp;
	EXPR	*expr, *scan();
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
		while( *ptr ) {
			if( isspace(*ptr) || ISOP(*ptr) ) {
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
			*ptr++ = '\0';
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

			/* Expand references to other parameters */
		name = expand_macro( name, table, table_size );
		if( name == NULL ) {
			(void)fclose( conf_fp );
			return( -1 );
		}

			/* Check that "name" is a legal identifier */
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
			if( context != NULL ) {
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

				line = MALLOC( (unsigned)(strlen(name) + strlen(evalue) + 4) );

				if( line == NULL ) {
					EXCEPT("Out of memory" );
				}

				(void)sprintf(line, "%s = %s", name, evalue);
				FREE( evalue );

				expr = scan( line );
				if( expr == NULL ) {
					EXCEPT("Expression syntax error in <%s> line %d",
							file_name, ConfigLineNo );
				}

				store_stmt( expr, context );

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

/*
** Read one line and any continuation lines that go with it.  Lines ending
** with <white space><backslash> are continued onto the next line.
*/
char *
getline( fp )
FILE	*fp;
{
	static char	buf[2048];
	int		len;
	char	*read_buf = buf;
	char	*line = NULL;
	char	*ptr;


	for(;;) {
		len = sizeof(buf) - (read_buf - buf);
		if( len <= 0 ) {
			EXCEPT( "Config file line too long" );
		}
		if( fgets(read_buf,len,fp) == NULL )
			return line;

		ConfigLineNo++;
		
			/* See if a continuation is indicated */
		line = ltrunc( read_buf );
		if( line != read_buf ) {
			(void)strcpy( read_buf, line );
		}
		if( (ptr = rindex(line,'\\')) == NULL )
			return buf;
		if( *(ptr+1) != '\0' )
			return buf;

			/* Ok read the continuation and concatenate it on */
		read_buf = ptr;
	}
}

#ifndef _tolower
#define _tolower(c) ((c) + 'a' - 'A')
#endif
/*
** Transform the given string into lower case in place.
*/
lower_case( str )
register char	*str;
{
#define ANSI_STYLE 1
	for( ; *str; str++ ) {
		if( *str >= 'A' && *str <= 'Z' )
#if ANSI_STYLE
			*str = _tolower( *str );
#else
			*str |= 040;
#endif
	}
}
	
/*
** Expand parameter references of the form "left$(middle)right".  This
** is deceptively simple, but does handle multiple and or nested references.
*/
char *
expand_macro( value, table, table_size )
char	*value;
BUCKET	*table[];
int		table_size;
{
	char *tmp = strdup( value );
	char *left, *name, *tvalue, *right;
	char *rval;

	while( get_var(tmp, &left, &name, &right) ) {
		tvalue = lookup_macro( name, table, table_size );
		if( tvalue == NULL ) {
			FREE( tmp );
			return( NULL );
		}

		rval = MALLOC( (unsigned)(strlen(left) + strlen(tvalue) +
														strlen(right) + 1));
		(void)sprintf( rval, "%s%s%s", left, tvalue, right );
		FREE( tmp );
		tmp = rval;
	}

	return( tmp );
}

get_var( value, leftp, namep, rightp )
register char *value, **leftp, **namep, **rightp;
{
	char *left, *left_end, *name, *right;
	char *tvalue;

	tvalue = value;
	left = value;

	for(;;) {
tryagain:
		value = index( tvalue, '$' );
		if( value == NULL ) {
			return( 0 );
		}

		if( *++value == '(' ) {
			left_end = value - 1;
			name = ++value;
			while( *value && *value != ')' ) {
				char c = *value++;
				if( !ISIDCHAR(c) ) {
					tvalue = name;
					goto tryagain;
				}
			}

			if( *value == ')' ) {
				right = value;
				break;
			} else {
				tvalue = name;
				goto tryagain;
			}
		} else {
			tvalue = value;
			goto tryagain;
		}
	}

	*left_end = '\0';
	*right++ = '\0';

	*leftp = left;
	*namep = name;
	*rightp = right;

	return( 1 );
}

/*
** Return the value associated with the named parameter.  Return NULL
** if the given parameter is not defined.
*/
char *
lookup_macro( name, table, table_size )
char	*name;
BUCKET	*table[];
int		table_size;
{
	int				loc;
	register BUCKET	*ptr;
	char			tmp_name[ 1024 ];

	strcpy( tmp_name, name );
	lower_case( tmp_name );
	loc = hash( tmp_name, table_size );
	for( ptr=table[loc]; ptr; ptr=ptr->next ) {
		if( !strcmp(tmp_name,ptr->name) ) {
			return ptr->value;
		}
	}
	return NULL;
}
