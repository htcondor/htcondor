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

usage()
{
	fprintf( stderr, "Usage: %s [-host hostname] variable ...\n", MyName );
	fprintf( stderr, "  (By specifying a hostname, %s tries to display the\n", 
			 MyName );
	fprintf( stderr, 
			 "  given parameter as it is configured on the requested host).\n" );
	exit( 1 );
}

main( int argc, char* argv[] )
{
	char	*value, *tmp, *host = NULL;
	int		i;
	StringList params;
	int		print_owner = FALSE;

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
		} else if( match_prefix( argv[i], "-owner" ) ) {
			print_owner = TRUE;
		} else if( match_prefix( argv[i], "-" ) ) {
			usage();
		} else {
			params.append( strdup( argv[i] ) );
		}
	}

	if( host ) {
		config_host( 0, 0, host );
	} else {
		config( 0 );
	}
		
	if( print_owner ) {
		printf( "%s\n", get_condor_username() );
		exit( 0 );
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

