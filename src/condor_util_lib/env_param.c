/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "debug.h"

#define EQUAL '='		/* chars looked for during parsing */
#define SPACE ' '
#define TAB '\t'
#define SEMI ';'

/*
  Given an environment string as from a PROC structure, fetch the
  value of the desired parameter.
*/
char *
GetEnvParam( parameter, env_string )
char *parameter;
char *env_string;
{
	char *ptr = env_string;
	char name[BUFSIZ];
	int i;
	static char tmp[BUFSIZ];

	while( TRUE ) {
		/* skip until alphanumeric char */
		while( *ptr ) {
			if( isalnum( *ptr ) ) {
				break;
			}
			ptr++;
		}

		if( !*ptr ) {
			dprintf( D_FULLDEBUG, "No matching param found\n");
			return( NULL );
		}
		
		/* parse name */
		memset( name,0, sizeof(name) ); /* ..dhaval 9/11/95 */
		i = 0;
		while( *ptr ) {
			if( *ptr == SPACE || *ptr == TAB || *ptr == EQUAL 
							|| *ptr == SEMI ) {
				break;
			}
			name[i++] = *ptr;
			ptr++;
		}

		if( i > 0 ) {
			if( strcmp(name,parameter) != MATCH ) {
				/* No match - move to parameter delimiter ';' */
				while( *ptr ) {
					if( *ptr == ';' )
						break;
					ptr++;
				}
				if( !*ptr ) {
					dprintf( D_ALWAYS, "No matching param found\n");
					return( NULL );
				}
				continue;
			}
		} else {
			dprintf( D_FULLDEBUG, "No matching param found\n");
			return( NULL );
		}

		/* parse over white space and the equal character */
		while( *ptr ) {
			if( isalnum( *ptr ) ) {
				break;
			}
			ptr++;
		}

		/* if at end of env or at semi then return a value of "" */
		if( *ptr == '\0' || *ptr == SEMI ) {
			return( "" );
		}
	
		/* parse value */
		i = 0;
		while( *ptr ) {
			if( *ptr == SEMI ) {
				break;
			}
			tmp[i++] = *ptr;
			ptr++;
		}
		tmp[i] = '\0';

		if( *tmp == '\0' )
			return( "" );	/* No value set, return a value of "" */
		else 
			return( tmp );
	}
}
