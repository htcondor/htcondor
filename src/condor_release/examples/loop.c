/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

/*
** This program just computes for an approximate amount of time, then
** exits with status 0.  The approximate time in seconds is specified
** on the command line.  
*/

#include <stdio.h>
#include <unistd.h>

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

