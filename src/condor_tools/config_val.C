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

 

/***********************************************************************
*
* Print declarations from the condor config files, condor_config and
* condor_config.local.  Declarations in condor_config.local override
* those in condor_config, so this prints the same declarations found
* by condor prgrams using the config() routine.
*
***********************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "match_prefix.h"
#include "string_list.h"


char	*MyName;
char	*mySubSystem = NULL;

usage()
{
	fprintf( stderr, "Usage: %s [-host hostname] variable ...\n", MyName );
	fprintf( stderr, "  (By specifying a hostname, %s tries to display the\n", 
			 MyName );
	fprintf( stderr, 
			 "  given parameter as it is configured on the requested host).\n" );
	exit( 1 );
}

enum PrintType {CONDOR_OWNER, CONDOR_TILDE, CONDOR_NONE};

main( int argc, char* argv[] )
{
	char	*value, *tmp, *host = NULL;
	int		i;
	StringList params;
	
	PrintType pt = CONDOR_NONE;

	MyName = argv[0];

	if( argc < 2 ) {
		usage();
	}

	for( i=1; i<argc; i++ ) {
		if( match_prefix( argv[i], "-host" ) ) {
			if( argv[i + 1] ) {
				host = strdup( argv[++i] );
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-subsystem" ) ) {
			if( argv[i + 1] ) {
				mySubSystem = strdup( argv[++i] );
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-owner" ) ) {
			pt = CONDOR_OWNER;
		} else if( match_prefix( argv[i], "-tilde" ) ) {
			pt = CONDOR_TILDE;
		} else if( match_prefix( argv[i], "-" ) ) {
			usage();
		} else {
			params.append( strdup( argv[i] ) );
		}
	}

		// If we didn't get told what subsystem we should use, set it
		// to "TOOL".
	if( !mySubSystem ) {
		mySubSystem = strdup( "TOOL" );
	}

	if( pt == CONDOR_TILDE ) {
		if( (tmp = get_tilde()) ) {
			printf( "%s\n", tmp );
			exit( 0 );
		} else {
			fprintf( stderr,
					 "Error: Specified -tilde but can't find %s\n",
					 "condor's home directory." );
			exit( 1 );
		}
	}		

	if( pt == CONDOR_OWNER ) {
		printf( "%s\n", get_condor_username() );
		exit( 0 );
	}

	if( host ) {
		config_host( 0, host );
	} else {
		config( 0 );
	}

	params.rewind();
	while( (tmp = params.next()) ) {
		value = param( tmp );
		if( value == NULL ) {
			fprintf(stderr, "Not defined: %s\n", tmp);
			exit( 1 );
		} else {
			printf("%s\n", value);
			free( value );
		}
	}
	exit( 0 );
}

