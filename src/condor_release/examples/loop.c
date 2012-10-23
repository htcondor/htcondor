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


/*
** This program just computes for an approximate amount of time, then
** exits with status 0.  The approximate time in seconds is specified
** on the command line.  
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

extern int CkptInProgress;

main( argc, argv)
int		argc;
char	*argv[];
{
	int		count;
	int		i;

	switch( argc ) {

	  case 1:
		if( scanf("\n%d", &count) != 1 ) {
			fprintf( stderr, "Input syntax error\n" );
			exit( 1 );
		}
		break;

	  case 2:
		if( (count=atoi(argv[1])) <= 0 ) {
			usage();
		}
		break;
		

	  default:
		usage();

	}

	do_loop( count );
	printf( "Normal End of Job\n" );

	exit( 0 );
}

extern int errno;

do_loop( count )
int		count;
{
	int		i;

	for( i=0; i<count; i++ ) {
		waste_a_second();
		printf( "%d", i );
		fflush( stdout );
		if( write(1," ",1) != 1 ) {
			exit( errno );
		}

		if ( (i % 43) == 0 ) {
		  ckpt();
		}
	}
}


waste_a_second()
{
  int i;
  for(i=0; i < 8000000; i++);
}

usage()
{
	printf( "Usage: loop count\n" );
	exit( 1 );
}

