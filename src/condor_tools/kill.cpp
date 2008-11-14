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
#include <dirent.h>
#include <errno.h>

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


#if defined( BSD_PS ) 
static char* PS_CMD = "/bin/ps auwwx";
#else 
static char* PS_CMD = "/bin/ps -ef";
#endif

List<CondorPid>* condor_pids = NULL;


// Prototypes
void usage(char*);			// print usage info and exit
void my_exit(int);			// close the syslog and exit
void find_condor_pids();	// find all processes that begin with "condor_"
CondorPid* find_cpid(pid_t);	// find the CondorPid object with the given pid
bool isaNum( char *s );     // checks if string contains solely numeric chars


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

// for Linux we use a special /proc-based version of find_condor_pids
#if defined( linux )

// Finds all Condor processes in the system by looking in /proc for
// directories representing processes (i.e., those that are numeric),
// opening "cmdline" in each one, and, if the command begins with
// "condor_", adding its pid to our list of condor pids.  We also save
// the corresponding command line to print out in the syslog.
void
find_condor_pids() {

    DIR *proc_root;
    struct dirent *proc_dir;
    char tmp_name[PATH_MAX];
    FILE *fp;
    char cmdline[64];
    pid_t pid;
    CondorPid *cpid;

    if( ! (condor_pids = new List<CondorPid>) ) {
        fprintf( stderr, "error: out of memory!\n" );
        my_exit( 1 );
    }

    if( ! (proc_root = opendir( "/proc" )) ) {
        fprintf( stderr, "error: can't open /proc (%s)\n", strerror( errno ) );
		my_exit( 1 );
    }

    while( (proc_dir = readdir( proc_root )) != NULL ) {

        // if this a numerically-named directory (i.e., a process)
        if( isaNum( proc_dir->d_name ) ) {
	    char	*cmdptr;

            // construct the filename to open (/proc/[pid]/cmdline)
            strncpy( tmp_name, "/proc/", PATH_MAX );
            strncat( tmp_name, proc_dir->d_name,
                     (PATH_MAX - strlen(tmp_name)) );
            strncat( tmp_name, "/cmdline",
                     (PATH_MAX - strlen(tmp_name)) );

            // open it, and slurp up the command line
            if( ! (fp = fopen( tmp_name, "r" )) ) {
                fprintf( stderr, "error: can't open %s (%s)\n", tmp_name,
                         strerror( errno ) );
		my_exit( 1 );
            }
            fgets( cmdline, 64, fp );
            fclose( fp );

	    // Grab the 'program name' portion of the command line
	    if (  ( cmdptr = strrchr( cmdline, '/' ) ) != NULL ) {
		cmdptr++;		// Point to the fist char after the /
	    } else {
		cmdptr = cmdline;	// No slash -- no path in cmdline
	    }

	    // if the command begins with "condor_", save the pid
	    if( strstr( cmdptr, "condor_" ) == cmdptr ) {
		pid = atoi( proc_dir->d_name );
		cpid = new CondorPid( pid, cmdline );
		condor_pids->Append( cpid );
	    }
	}
    }

    closedir( proc_root );
	return;
}


// returns 1 if s consists solely of digits 0-9, otherwise returns 0
bool isaNum( char *s )
{
    int i;

    // return 0 if empty
    if( s[0] == '\0' ) {
        return 0;
    }

    // check each char for non-digit values
    for( i = 0; s[i] != '\0'; i++ ) {
        if( s[i] < '0' || s[i] > '9' ) {
            return 0;
        }
    }
    return 1;
}

// for all other OSs we use a ps-based version of find_condor_pids
#else

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
				 PS_CMD[0] );
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
	pclose(ps);
}

#endif	// defined( linux )	

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


