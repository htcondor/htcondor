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



#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "my_hostname.h"
#include "get_daemon_name.h"

#include "exit.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "util_lib_proto.h"
#include "condor_qmgr.h"
#include "scheduler.h"
#include "dedicated_scheduler.h"
#include "condor_adtypes.h"
#include "condor_uid.h"
#include "grid_universe.h"

#if defined(BSD43) || defined(DYNIX)
#	define WEXITSTATUS(x) ((x).w_retcode)
#	define WTERMSIG(x) ((x).w_termsig)
#endif

extern "C"
{
	int		ReadLog(char*);
}
extern	void	mark_jobs_idle();
extern  int     clear_autocluster_id( ClassAd *job );


// global variables to control the daemon's running and logging
char*		Spool = NULL;							// spool directory
char* 		JobHistoryFileName = NULL;
char*		Name = NULL;
char*		mySubSystem = "SCHEDD";
char*		X509Directory = NULL;
bool        DoHistoryRotation = true;
filesize_t  MaxHistoryFileSize = 20 * 1024 * 1024; // 20MB
int         NumberBackupHistoryFiles = 2; // In addition to history file, how many to keep?

// global objects
Scheduler	scheduler;
DedicatedScheduler dedicated_scheduler;

void usage(char* name)
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t] [-n schedd_name]", name); 
	exit( 1 );
}


int
main_init(int argc, char* argv[])
{
	char**		ptr; 
	char		job_queue_name[_POSIX_PATH_MAX];
 
	int argc_count = 1;
	for(ptr = argv + 1, argc_count = 1; argc_count<argc && *ptr; ptr++,argc_count++)
	{
		if(ptr[0][0] != '-')
		{
			usage(argv[0]);
		}
		switch(ptr[0][1])
		{
		  case 'n':
			Name = build_valid_daemon_name( *(++ptr) );
			break;
		  default:
			usage(argv[0]);
		}
	}
	
		// Initialize all the modules
	scheduler.Init();
	scheduler.Register();

		// Initialize the job queue
	sprintf(job_queue_name, "%s/job_queue.log", Spool);

		// Make a backup of the job queue?
	char	*tmp;
	tmp = param( "SCHEDD_BACKUP_SPOOL" );
	if ( tmp ) {
		if ( (*tmp == 't') || (*tmp == 'T') ) {
			char	hostname[128];
			if ( gethostname( hostname, sizeof( hostname ) ) ) {
				strcpy( hostname, "" );
			}
			char		job_queue_backup[_POSIX_PATH_MAX];
			sprintf(job_queue_backup, "%s/job_queue.bak.%s",
					Spool, hostname );
			if ( copy_file( job_queue_name, job_queue_backup ) ) {
				dprintf( D_ALWAYS, "Failed to backup spool to '%s'\n",
						 job_queue_backup );
			} else {
				dprintf( D_FULLDEBUG, "Spool backed up to '%s'\n",
						 job_queue_backup );
			}
		}
		free( tmp );
	}
	InitJobQueue(job_queue_name);
	mark_jobs_idle();

		// The below must happen _after_ InitJobQueue is called.
	if ( scheduler.autocluster.config() ) {
		// clear out auto cluster id attributes
		WalkJobQueue( (int(*)(ClassAd *))clear_autocluster_id );
	}

		// Initialize the dedicated scheduler stuff
	dedicated_scheduler.initialize();

		// Do a timeout now at startup to get the ball rolling...
	scheduler.timeout();

	return 0;
} 

int
main_config( bool is_full )
{
	GridUniverseLogic::reconfig();
	scheduler.reconfig();
	dedicated_scheduler.reconfig();
	return 0;
}


int
main_shutdown_fast()
{
	dedicated_scheduler.shutdown_fast();
	GridUniverseLogic::shutdown_fast();
	scheduler.shutdown_fast();

	return 0;
}


int
main_shutdown_graceful()
{
	dedicated_scheduler.shutdown_graceful();
	GridUniverseLogic::shutdown_graceful();
	scheduler.shutdown_graceful();

	return 0;
}


void
main_pre_dc_init( int argc, char* argv[] )
{
}


void
main_pre_command_sock_init( )
{
}

