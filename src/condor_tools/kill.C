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
#include <syslog.h>
#include <assert.h>

#include "list.h"

class CondorPid {
public:
	CondorPid( pid_t, char* );
	~CondorPid();
	bool Kill( int );	// Kill this pid with the given signal, and log it 
	pid_t Pid() {return m_pid;};
private:
	pid_t m_pid;
	char* m_line;
};


// Global variables
extern int errno;

// We know Linux's ps is BSD-like
#if defined( linux ) && !defined( BSD_PS )
#define BSD_PS
#endif

#if defined( BSD_PS ) 
static const char* PS_CMD = "/bin/ps auwwx";
#else 
static const char* PS_CMD = "/bin/ps -ef";
#endif

List<CondorPid>* condor_pids = NULL;


// Prototypes
void usage(char*);			// print usage info and exit
void my_exit(int);			// close the syslog and exit
void find_condor_pids();	// find all pids from ps that begin with "condor_"
CondorPid* find_cpid(pid_t);	// find the CondorPid object with the given pid


int
main( int argc, char* argv[] ) 
{
	char** tmp;
	int sig = 0;
	pid_t pid;
	CondorPid *cpid;
	int had_err = 0;

		// Make sure we've got reasonable input
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

		// Initialize system logging
	openlog( "condor_kill", LOG_PID, LOG_AUTH );

		// Find all Condor processes and store them in a linked list
	find_condor_pids();			
	
		// Process the rest of our args
	for( tmp = &argv[2]; *tmp; tmp++ ) {

			// Make sure our arg is a reasonably valid pid
		pid = (pid_t)atoi(*tmp);
		if( !pid ) {
			fprintf( stderr, "Error: \"%s\" is not a valid pid\n", *tmp );
			had_err = 1;
			continue;
		}

			// See if that pid is a Condor process 
		if( (cpid = find_cpid(pid)) ) {
				// If so, try to kill it.
			if( !cpid->Kill(sig) ) {
				had_err = 1;
			}
		} else {
			fprintf( stderr, "Error: %ld is not a Condor pid\n", (long)pid );
			had_err = 1;
			continue;
		}
	}
	my_exit( had_err );
}


//////////////////////////////////////////////////////////////////////
// Utility functions
//////////////////////////////////////////////////////////////////////

void
usage( char* name ) {
	fprintf( stderr, "Usage: %s -<signal number> <pid> ...\n", name );
	my_exit( 1 );
}


// Find all Condor processes in the system.  We open /bin/ps, and find
// all entries that have "condor_" in argv[0].  For each pid, we save
// the whole line from ps to print out in the syslog.
void
find_condor_pids() {

	FILE *ps;
	char line[250];
	pid_t pid;
	CondorPid *cpid;

	condor_pids = new List<CondorPid>;
	if( !condor_pids ) {
		fprintf( stderr, "ERROR: out of memory!\n" );
		my_exit( 1 );
	}
	
	ps = popen( PS_CMD, "r" );
	if( !ps ) {
		fprintf( stderr, "Error: can't open %s for reading.\n", 
				 PS_CMD );
		my_exit( 1 );
	}

	fgets( line, 249, ps );  /* skip the column header line */
	while( fgets(line,249,ps) != NULL ) {
		if( strstr( line, "condor_" ) == line ) {
			// found a line that begins with "condor_", so grab the pid
			sscanf( line, "%*s %ld", &pid );
			cpid = new CondorPid( pid, line );
			condor_pids->Append( cpid );
		}
	}    
}


// Return a pointer to the CondorPid object with the given pid
CondorPid*
find_cpid( pid_t pid ) 
{
	CondorPid *cpid; 
	if( !condor_pids ) {
		return NULL;
	}
	condor_pids->Rewind();
	while( (cpid = condor_pids->Next()) ) {
		if( cpid->Pid() == pid ) return cpid;
	}
	return NULL;
}


void
my_exit( int status ) 
{
	closelog();
	exit( status );
}


//////////////////////////////////////////////////////////////////////
// class CondorPid member functions
//////////////////////////////////////////////////////////////////////

bool
CondorPid::Kill( int sig )
{
	if( kill(m_pid, sig) < 0 ) {
		fprintf( stderr, 
				 "Error: can't kill pid %ld with signal %d, errno: %d (%s)\n",
				 (long)m_pid, sig, errno, strerror(errno) );
		return false;
	} else {
		printf( "killed pid: %ld, %s\n", 
				(long)m_pid, m_line );
		syslog( LOG_INFO, "killed pid %ld, ps info: %s", 
				(long)m_pid, m_line );
		return true;
	}
}


CondorPid::CondorPid( pid_t pid, char* line )
{
	assert( line );
	m_pid = pid;
	m_line = strdup( line );
}


CondorPid::~CondorPid()
{
	free( m_line );
}


