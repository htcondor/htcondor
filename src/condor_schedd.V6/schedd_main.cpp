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




#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "get_daemon_name.h"

#include "exit.h"

#include "condor_daemon_core.h"
#include "util_lib_proto.h"
#include "condor_qmgr.h"
#include "scheduler.h"
#include "dedicated_scheduler.h"
#include "condor_adtypes.h"
#include "condor_uid.h"
#include "grid_universe.h"
#include "condor_netdb.h"
#include "subsystem_info.h"
#include "ipv6_hostname.h"

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
#include "ScheddPlugin.h"
#include "ClassAdLogPlugin.h"
#endif
#endif

extern "C"
{
	int		ReadLog(char*);
}
extern	void	mark_jobs_idle();
extern  int     clear_autocluster_id( ClassAd *job );

char*          Spool = NULL;
char*          Name = NULL;
char*          X509Directory = NULL;

// global objects
Scheduler	scheduler;
DedicatedScheduler dedicated_scheduler;

void usage(char* name)
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t] [-n schedd_name]", name); 
	exit( 1 );
}


void
main_init(int argc, char* argv[])
{
	char**		ptr; 
	MyString		job_queue_name;
 
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

		// Tell Attrlist to publish the server time
	AttrList_setPublishServerTime( true );

		// Initialize DaemonCore's use of ProcFamily. We do this so that we
		// launch a ProcD if necessary so that any Starters that we launch
		// for Local Universe jobs can share a single ProcD, instead of
		// each creating their own
	daemonCore->Proc_Family_Init();

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
		// Intialization of the plugin manager, i.e. loading all
		// plugins, should be performed before the job queue log is
		// read so plugins have a chance to learn about all jobs
		// already in the queue
	ClassAdLogPluginManager::Load();

		// Load all ScheddPlugins. In reality this doesn't do much
		// since initializing any plugin manager loads plugins for all
		// plugin manager.
	ScheddPluginManager::Load();

		// Tell all ScheddPlugins to initialze themselves
	ScheddPluginManager::EarlyInitialize();

		// Tell all plugins to initialize themselves
	ClassAdLogPluginManager::EarlyInitialize();
#endif
#endif
	
		// Initialize all the modules
	scheduler.Init();
	scheduler.Register();

		// Initialize the job queue
	char *job_queue_param_name = param("JOB_QUEUE_LOG");

	if (job_queue_param_name == NULL) {
		// the default place for the job_queue.log is in spool
		job_queue_name.formatstr( "%s/job_queue.log", Spool);
	} else {
		job_queue_name = job_queue_param_name; // convert char * to MyString
		free(job_queue_param_name);
	}

		// Make a backup of the job queue?
	if ( param_boolean_crufty("SCHEDD_BACKUP_SPOOL", false) ) {
			MyString hostname;
			UtcTime now(true);
			hostname = get_local_hostname();
			MyString		job_queue_backup;
			job_queue_backup.formatstr( "%s/job_queue.bak.%s.%ld",
									  Spool, hostname.Value(), now.seconds() );
			if ( copy_file( job_queue_name.Value(), job_queue_backup.Value() ) ) {
				dprintf( D_ALWAYS, "Failed to backup spool to '%s'\n",
						 job_queue_backup.Value() );
			} else {
				dprintf( D_FULLDEBUG, "Spool backed up to '%s'\n",
						 job_queue_backup.Value() );
			}
	}

	int max_historical_logs = param_integer( "MAX_JOB_QUEUE_LOG_ROTATIONS", DEFAULT_MAX_JOB_QUEUE_LOG_ROTATIONS );

	InitJobQueue(job_queue_name.Value(),max_historical_logs);
	mark_jobs_idle();

		// The below must happen _after_ InitJobQueue is called.
	if ( scheduler.autocluster.config() ) {
		// clear out auto cluster id attributes
		WalkJobQueue( (int(*)(ClassAd *))clear_autocluster_id );
	}
	
		//
		// Update the SchedDInterval attributes in jobs if they
		// have it defined. This will be for JobDeferral and
		// CronTab jobs
		//
	WalkJobQueue( (int(*)(ClassAd *))::updateSchedDInterval );

		// Initialize the dedicated scheduler stuff
	dedicated_scheduler.initialize();

		// Do a timeout now at startup to get the ball rolling...
	scheduler.timeout();

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
		// Tell all ScheddPlugins to initialze themselves
	ScheddPluginManager::Initialize();

		// Tell all plugins to initialize themselves
	ClassAdLogPluginManager::Initialize();
#endif
#endif
} 

void
main_config()
{
	GridUniverseLogic::reconfig();
	scheduler.reconfig();
	dedicated_scheduler.reconfig();
}


void
main_shutdown_fast()
{
	dedicated_scheduler.shutdown_fast();
	GridUniverseLogic::shutdown_fast();
	scheduler.shutdown_fast();
}


void
main_shutdown_graceful()
{
	dedicated_scheduler.shutdown_graceful();
	GridUniverseLogic::shutdown_graceful();
	scheduler.shutdown_graceful();
}


int
main( int argc, char **argv )
{
	set_mySubSystem( "SCHEDD", SUBSYSTEM_TYPE_SCHEDD );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
