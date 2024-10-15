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

#include "authentication.h"
#include "condor_daemon_core.h"
#include "util_lib_proto.h"
#include "qmgmt.h"
#include "condor_qmgr.h"
#include "scheduler.h"
#include "dedicated_scheduler.h"
#include "condor_adtypes.h"
#include "condor_uid.h"
#include "grid_universe.h"
#include "subsystem_info.h"
#include "ipv6_hostname.h"
#include "credmon_interface.h"
#include "directory_util.h"

#ifdef UNIX
#include "ScheddPlugin.h"
#include "ClassAdLogPlugin.h"
#endif

extern "C"
{
	int		ReadLog(char*);
}


char*          Spool = nullptr;
char*          Name = nullptr;
char*          X509Directory = nullptr;

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
	char**		ptr = nullptr; 
	std::string job_queue_name;
 
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
			if (Name) {
				free(Name);
			}
			Name = build_valid_daemon_name( *(++ptr) );
			break;
		  default:
			usage(argv[0]);
		}
	}

#ifdef UNIX
	ClassAdLogPluginManager::Load();
	ScheddPluginManager::Load();

	ScheddPluginManager::EarlyInitialize();
	ClassAdLogPluginManager::EarlyInitialize();
#endif


	// if using the SEC_CREDENTIAL_DIRECTORY_KRB, confirm we are "up-to-date".
	// at the moment, we take an "all-or-nothing" approach.  ultimately, this
	// should be per-user, and the SchedD should start normally and run jobs
	// for users who DO have valid credentials, and simply holding on to jobs
	// in idle state for users who do NOT have valid credentials.
	//
	auto_free_ptr cred_dir_krb(param("SEC_CREDENTIAL_DIRECTORY_KRB"));
	if (cred_dir_krb) {
		dprintf(D_ALWAYS, "SEC_CREDENTIAL_DIRECTORY_KRB is %s, will wait up to 10 minutes for user credentials to be updated before continuing.\n", cred_dir_krb.ptr());

		// we tried, we give up.
		if ( ! credmon_poll_for_completion(credmon_type_KRB, cred_dir_krb, 600)) {
			EXCEPT("User credentials unavailable after 10 minutes");
		}
	}
	// User creds good to go, let's start this thing up!

		// Initialize all the modules
	scheduler.Init();
	scheduler.Register();

		// Initialize the job queue
	param(job_queue_name, "JOB_QUEUE_LOG");
	if (job_queue_name.empty()) {
		// the default place for the job_queue.log is in spool
		formatstr(job_queue_name, "%s/job_queue.log", Spool);
	}

		// Make a backup of the job queue?
	if ( param_boolean_crufty("SCHEDD_BACKUP_SPOOL", false) ) {
			std::string hostname;
			hostname = get_local_hostname();
			std::string		job_queue_backup;
			formatstr( job_queue_backup, "%s/job_queue.bak.%s.%ld",
			                            Spool, hostname.c_str(), (long)time(nullptr) );
			if ( copy_file( job_queue_name.c_str(), job_queue_backup.c_str() ) ) {
				dprintf( D_ALWAYS, "Failed to backup spool to '%s'\n",
						 job_queue_backup.c_str() );
			} else {
				dprintf( D_FULLDEBUG, "Spool backed up to '%s'\n",
						 job_queue_backup.c_str() );
			}
	}

	int max_historical_logs = param_integer( "MAX_JOB_QUEUE_LOG_ROTATIONS", DEFAULT_MAX_JOB_QUEUE_LOG_ROTATIONS );

	InitJobQueue(job_queue_name.c_str(),max_historical_logs);
	PostInitJobQueue();

		// Initialize the dedicated scheduler stuff
	dedicated_scheduler.initialize();

		// Do a timeout now at startup to get the ball rolling...
	scheduler.timeout();

#ifdef UNIX
	ScheddPluginManager::Initialize();
	ClassAdLogPluginManager::Initialize();
#endif

	daemonCore->InstallAuditingCallback( AuditLogNewConnection );
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
	set_mySubSystem( "SCHEDD", true, SUBSYSTEM_TYPE_SCHEDD );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
