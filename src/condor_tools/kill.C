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
  Generic tool that takes in a signal to send and a list of pids.  A
  linked list is constructed of the pids of all processes in the
  system with a name that begins with "condor_".  Each pid passed on
  the command line is checked against this list before the given
  signal is sent to it.  This program can be installed setuid root at
  sites where Condor should run as root but the Condor administrators
  don't have root access on all the machines.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include "simplelist.h"
template class SimpleList<pid_t>;

// Global variables
extern int errno;
static const char* PS_CMD = "/bin/ps -ef";
SimpleList<pid_t>* condor_pids = NULL;

// Prototypes
void find_condor_pids();	// find all pids from ps that begin with "condor_"
bool is_condor(pid_t pid);	// returns if the given pid is a Condor pid
void do_kill(pid_t pid, int sig);


void
usage( char* name ) {
	fprintf( stderr, "Usage: %s -<signal number> <pid> ...\n", name );
	exit( 1 );
}


int
main( int argc, char* argv[] ) 
{
	char** tmp;
	int sig = 0;
	pid_t pid;

	if( argc < 3 ) {
		usage( argv[0] );
	}

	if( argv[1][0] != '-' || !(argv[1][1]) ) {
		usage( argv[0] );
	}		
	sig = atoi( &argv[1][1] );
	if( !sig ) {
		fprintf( stderr, "%s: \"%s\" is not a valid signal number\n",
				 argv[0], &argv[1][1] );
		usage( argv[0] );
	}

	find_condor_pids();	
	
	for( tmp = &argv[2]; *tmp; tmp++ ) {
		pid = (pid_t)atoi(*tmp);
		if( !pid ) {
			fprintf( stderr, "Error: \"%s\" is not a valid pid\n", *tmp );
			continue;
		}
		if( is_condor(pid) ) {
			do_kill( pid, sig );
		} else {
			fprintf( stderr, "Error: %d is not a Condor pid\n", pid );
			continue;
		}
	}
}


void
find_condor_pids() {

	FILE *ps;
	char line[250], *tmp;
	pid_t pid;

	condor_pids = new SimpleList<pid_t>;
	if( !condor_pids ) {
		fprintf( stderr, "ERROR: out of memory!\n" );
		exit( 1 );
	}
	
	ps = popen( PS_CMD, "r" );
	if( !ps ) {
		fprintf( stderr, "Error: can't open %s for reading.\n", 
				 PS_CMD );
		exit( 1 );
	}

   fgets( line, 249, ps );  /* skip the column header line */
   while( fgets(line,249,ps) != NULL ) {
	   tmp = strstr( line, "condor_" );
	   if( tmp ) {
		   if ( sizeof(pid_t) == sizeof(long) ) {
			   sscanf( line, "%*s %ld", &pid );
		   } else { 
			   sscanf( line, "%*s %d", &pid );
		   }
		   condor_pids->Append( pid );
	   }    
   }
}


bool
is_condor( pid_t pid )
{
	
	pid_t tmp;
	if( !condor_pids ) {
		return false;
	}
	condor_pids->Rewind();
	while( condor_pids->Next(tmp) ) {
		if( pid == tmp ) return true;
	}
	return false;
}


void
do_kill( pid_t pid, int sig )
{
	if( kill(pid, sig) < 0 ) {
		fprintf( stderr, 
				 "Error: can't kill pid %d with signal %d, errno: %d (%s)\n",
				 pid, sig, errno, strerror(errno) );
	}
}


