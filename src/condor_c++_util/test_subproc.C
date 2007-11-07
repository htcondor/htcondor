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

#include <stdio.h>
#include "subproc.h"


main( int argc, char *argv[] )
{
	SubProc	p( "/usr/ucb/cat", "/etc/motd", "r" );
	SubProc	q( "/usr/ucb/dd", "of=/tmp/mike ibs=1 obs=1", "w" );
	FILE	*fp;
	char	buf[ 1024 ];
	int		i;

	p.display();
	fp = p.run();
	p.display();

	while( fgets(buf,sizeof(buf),fp) ) {
		printf( "Got: %s", buf );
	}

	p.terminate();
	fp = NULL;
	p.display();

	fp = q.run();
	q.display();
	for( i=0; i<5; i++ ) {
		fprintf( fp, "yes\n" );
	}
	fflush( fp );
	q.terminate();

	fp = execute_program( "/usr/ucb/cat", "/etc/motd", "r" );
	while( fgets(buf,sizeof(buf),fp) ) {
		printf( "Got: %s", buf );
	}
	terminate_program();

	fp = execute_program( "/usr/ucb/dd", "of=/tmp/mike.1 ibs=1 obs=1", "w" );
	for( i=0; i<5; i++ ) {
		fprintf( fp, "yes\n" );
	}
	terminate_program();
}
