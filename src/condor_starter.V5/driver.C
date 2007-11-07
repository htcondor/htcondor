/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


 

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
