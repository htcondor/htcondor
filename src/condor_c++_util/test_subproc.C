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
