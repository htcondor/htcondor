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
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

int
main() 
{
	FILE *in, *out;
	int lines = 0;

	char line[250];

	in = fopen( "x_data.in", "r" );
	if( !in ) {
		fprintf( stderr, "Can't open x_data.in, errno: %d (%s)\n",
				 errno, strerror(errno) );
		exit( 1 );
	}

	out = fopen( "job_rsc_fgets_std.output", "w" );
	if( !out ) {
		fprintf( stderr, "Can't open fgets.output, errno: %d (%s)\n", 
				 errno, strerror(errno) );
		exit( 1 );
	}

	while( fgets(line,249,in) != NULL ) {
		lines++;
		fprintf( stdout, "Read a line (%d)\n", lines );
		fprintf( out, "%s", line );
		fflush( out );
	}

	fclose( in );
	fclose( out );

	fprintf( stdout, "All done.\n" );
	exit( 0 );
}
