#include "condor_common.h"
#include "condor_types.h"
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
