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
static char* config_val_cmd[] = {"/unsup/condor/bin/condor_config_val", "master", NULL};

int
main( int argc, char* argv[], char *env[] )
{
	int errcode, i;
	FILE *config_val;
	char *path_from_config_val;
	char *master_path;
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

	path_from_config_val = (char *)calloc(1, (_POSIX_PATH_MAX + 1));
	if(path_from_config_val == NULL ) {
		fprintf( stderr, "error: Couldn't allocate memory for path!\n");
		exit(1);
	}
	
	config_val = my_popenv(config_val_cmd, "r");
	if(config_val == NULL) {
		fprintf( stderr, "warning: Can't popen config_val"
				 " errno: %d (%s)\n",
				 errno, strerror(errno) );
		master_path = static_master_path;
	} else {
		count = fread( path_from_config_val, 1, _POSIX_PATH_MAX, config_val);
		if( count > 0 ) {
			path_from_config_val[_POSIX_PATH_MAX] = 0; // Null terminate it
			for(i = 0; i < count; i++) {
				if(path_from_config_val[i] == '\n') {
					path_from_config_val[i] = 0;
				}
			}
			master_path = path_from_config_val;
		} else {
			fprintf(stderr, "Unable to read config entry, using hard-coded ");
			fprintf(stderr, "default of /unsup/condor/sbin/condor_master\n ");
			master_path = static_master_path;
		}
		my_pclose(config_val);
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
