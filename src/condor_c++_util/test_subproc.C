/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
