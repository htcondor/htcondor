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

char	*MyName;

usage()
{
	fprintf( stderr, "Usage: %s variable ...\n", MyName );
	exit( 1 );
}

main( int argc, char* argv[] )
{
	char *value;
	char	*tmp;

	MyName = argv[0];

	if( argc < 2 ) {
		usage();
	}

	config( 0 );
		
	while( *++argv ) {
		tmp = strdup( *argv );
		value = param( tmp );
		free( tmp );
		if( value == NULL ) {
			fprintf(stderr, "Not defined: %s\n", *argv);
			exit( 1 );
		} else {
			printf("%s\n", value);
			free( value );
		}
	}
	exit( 0 );
}

