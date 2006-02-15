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

/*

   Instead of relying on hard-coded limits for how many times we
   should increment a variable to waste a second, we compute a
   reasonable value for whatever machine we're building the test suite
   on.  To avoid overflowing an int, we use a nested for() loop
   instead of just counting directly.  To try to have a fairly
   accurate number, we use kilo-instructions-per-second ("KIPS")
   instead of the more common mega-instructions-per-second ("MIPS").
   However, these aren't really instructions, they're bogus, hence
   "compute_bogokips".  This program outputs a header file that is
   included by waste_second.c.  This header just includes a single
   line with "BOGOKIPS" #define'ed to the right value.

   To compute the actual value, we rely on alarm() to send us a
   SIGALRM 1 second after we start counting.  In the signal handler
   for SIGALRM, we write out the file and exit.  This appears to be
   portable to all the current Condor platforms.  

   If this program is spawned with an argument, that's the filename to
   use to write the output.  If not, it defaults to "my_bogomips.h".  

   Author: Derek Wright <wright@cs.wisc.edu> 2003-11-23

*/

#define _POSIX_SOURCE
#ifdef DUX
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

volatile int i = 0;
FILE* OUT = NULL;

void
catch_sigalrm( int sig )
{
	fprintf( OUT, "#define BOGOKIPS %d\n", i );
	fclose( OUT );
	exit( 0 );
}

int
main( int argc, char* argv[] )
{
	int j;
	char* output;

	if( argc != 2 ) {
		output = "my_bogokips.h";
	} else {
		output = argv[1];
	}
	OUT = fopen( (const char*)output, "w" );
	if( ! OUT ) {
		fprintf( stderr, "Can't open %s", output );
		exit( 1 );
	}

	signal( SIGALRM, catch_sigalrm );

	alarm( 1 );

	while( 1 ) {
		i++;
		for(j=0; j<1000; j++ );
	}
}

