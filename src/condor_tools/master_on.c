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

/*
  Generic tool that just executes the condor_master program.  This
  program can be installed setuid root at sites where Condor should
  run as root but the Condor administrators don't have root access on
  all the machines.
*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <afs/stds.h>
#include <afs/afs.h>
#include <sys/syscall.h>
extern int errno;

static char* master_path = "/unsup/condor/sbin/condor_master";

int
main( int argc, char* argv[], char *env[] )
{
    int errcode;

	// Shed our "invoker's" AFS tokens
    if ( syscall(AFS_SYSCALL, AFSCALL_SETPAG) < 0 ) {
		fprintf( stderr, "error: Can't shed our AFS tokens"
				 " errno: %d (%s)\n",
				 errno, strerror(errno) );
		exit( 1 );
	}

	// Flush my environment
	*env = NULL;

	// Here we go!
	if( setuid(0) < 0 ) {
		fprintf( stderr, "error: can't set uid to root, errno: %d (%s)\n",
				 errno, strerror(errno) );
		exit( 1 );
	}

	openlog( "condor_master_on", LOG_PID, LOG_AUTH );
	syslog( LOG_INFO, "About to exec %s as root", master_path );
	closelog();

	execl( master_path, "condor_master", NULL );

	fprintf( stderr, "Can't exec %s, errno: %d (%s)\n",
			 master_path, errno, strerror(errno) );
	exit( 1 );
}
