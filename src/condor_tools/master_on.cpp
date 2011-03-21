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
  Generic tool that just executes the condor_master program.  This
  program can be installed setuid root at sites where Condor should
  run as root but the Condor administrators don't have root access on
  all the machines.
*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <limits.h>
#include <errno.h>
#include <afs/stds.h>
#include <afs/afs.h>
#include <sys/syscall.h>
extern int errno;

static char* static_master_path = "/unsup/condor/sbin/condor_master";
static char* config_val_cmd = "/unsup/condor/bin/condor_config_val master";

int
main( int argc, char* argv[], char *env[] )
{
	int errcode, i;
	FILE *config_val;
	MyString path_from_config_val;
	char const *master_path;
	size_t count;


	// Shed our "invoker's" AFS tokens
    if ( syscall(AFS_SYSCALL, AFSCALL_SETPAG) < 0 ) {
		fprintf( stderr, "error: Can't shed our AFS tokens"
				 " errno: %d (%s)\n",
				 errno, strerror(errno) );
		exit( 1 );
	}

	// Flush my environment
	*env = NULL;

	// Now, we've ditched our environment, so we'll pick up the default
	// config file for the machine.

	// Ask this machine what condor_master it should run, ala the startup
	// script...

	config_val = popen(config_val_cmd, "r");
	if(config_val == NULL) {
		fprintf( stderr, "warning: Can't popen config_val"
				 " errno: %d (%s)\n",
				 errno, strerror(errno) );
		master_path = static_master_path;
	} else {
		if( path_from_config_val.readLine(config_val) ) {
			path_from_config_val.chomp();
			master_path = path_from_config_val.Value();
		} else {
			fprintf(stderr, "Unable to read config entry, using hard-coded ");
			fprintf(stderr, "default of /unsup/condor/sbin/condor_master\n ");
			master_path = static_master_path;
		}
		pclose(config_val);
	}
				

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
