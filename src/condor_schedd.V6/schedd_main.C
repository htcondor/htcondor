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



#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "my_hostname.h"
#include "get_daemon_addr.h"

#include "sched.h"
#include "exit.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"
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

