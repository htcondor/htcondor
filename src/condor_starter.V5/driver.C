/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#define _POSIX_SOURCE

#include <stdio.h>
#include "environ.h"

main( int argc, char *argv[] )
{
	char	buf[1024];
	char	**vec;
	char	*answer;
	Environ	foo;
	Environ bar;
	FILE	*fp;

	if( argc != 2 ) {
		printf( "Usage: %s filename\n" );
		return 1;
	}

	if( (fp=fopen(argv[1],"r")) == NULL ) {
		perror( argv[1] );
		return 1;
	}

	foo.add_string( "FOO=bar" );
	foo.add_string( "BAR=glarch" );
	foo.display();

	while( fgets(buf,sizeof(buf),fp) ) {
		bar.add_string( buf );
	}
	vec = bar.get_vector();
	for( ; *vec; vec++ ) {
		printf( "%s\n", *vec );
	}

	while( gets(buf) ) {
		answer = bar.getenv(buf);
		printf( "Variable \"%s\" = \"%s\"\n", buf, answer ? answer : "(NULL)" );
	}
	return 0;
}
