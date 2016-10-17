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
#include "qmgmt.h"
#include "condor_qmgr.h"
#include "scheduler.h"
#include "dedicated_scheduler.h"
#include "condor_adtypes.h"
#include "condor_uid.h"
#include "grid_universe.h"
#include "condor_netdb.h"
#include "subsystem_info.h"
#include "ipv6_hostname.h"
#include "credmon_interface.h"

#if defined(HAVE_DLOPEN)
#include "ScheddPlugin.h"
#include "ClassAdLogPlugin.h"
#endif

extern "C"
{
	int		ReadLog(char*);
}


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
			if (Name) {
				free(Name);
			}
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

#if defined(HAVE_DLOPEN)
	ClassAdLogPluginManager::Load();
	ScheddPluginManager::Load();

	ScheddPluginManager::EarlyInitialize();
	ClassAdLogPluginManager::EarlyInitialize();
#endif

/* schedd doesn't care about other daemons.  only that it has the ability
 * to run jobs.  so the following code is for now not needed.

	// ZKM HACK TO MAKE SURE SCHEDD HAS USER CREDENTIALS
	//
	// if we are using the credd and credmon, we need to init them before
	// doing anything!
	char* p = param("SEC_CREDENTIAL_DIRECTORY");
	if(p) {
		free(p);
		dprintf(D_ALWAYS, "SCHEDD: INITIALIZING USER CREDS\n");
		Daemon *my_credd;

		// we will abort if we can't locate the credd, so let's try a
		// few times. locate() caches the result so we have to destroy
		// the object and make a new one each time.
		int retries = 20;
		bool success = false;
		do {
			// allocate a credd
			my_credd = new Daemon(DT_CREDD);
			if(my_credd) {
				// call locate
				bool loc_rc = my_credd->locate();
				if(loc_rc) {
					// get a connected relisock
					CondorError errstack;
					ReliSock* r = (ReliSock*)my_credd->startCommand(
						CREDD_REFRESH_ALL, Stream::reli_sock, 20, &errstack);
					if ( r ) {
						// ask the credd to get us some fresh user creds
						ClassAd ad;
						putClassAd(r, ad);
						r->end_of_message();
						r->decode();
						getClassAd(r, ad);
						r->end_of_message();
						dprintf(D_SECURITY | D_FULLDEBUG, "SCHEDD: received ad from CREDD:\n");
						dPrintAd(D_SECURITY | D_FULLDEBUG, ad);
						MyString result;
						ad.LookupString("Result", result);
						if(result == "success") {
							success = true;
						} else {
							dprintf(D_FULLDEBUG, "SCHEDD: warning, creddmon returned failure.\n");
						}

						// clean up.
						delete r;
					} else {
						dprintf(D_FULLDEBUG, "SCHEDD: warning, startCommand failed, %s\n", errstack.getFullText(true).c_str());
					}
				} else {
					dprintf(D_FULLDEBUG, "SCHEDD: warning, locate failed.\n");
				}

				// clean up.
				delete my_credd;
			} else {
				dprintf(D_FULLDEBUG, "SCHEDD: warning, new Daemon(DT_CREDD) failed.\n");
			}

			// if something went wrong, sleep and retry (finit number of times)
			if(!success) {
				dprintf(D_FULLDEBUG, "SCHEDD: sleeping and trying again %i times.\n", retries);
				sleep(1);
				retries--;
			}
		} while ((retries > 0) && (success == false));

		// except if fail
		if (!success) {
			EXCEPT("FAILED TO INITIALIZE USER CREDS (locate failed)");
		}
	}
	// END ZKM HACK
*/

#ifndef WIN32
	// if using the SEC_CREDENTIAL_DIRECTORY, confirm we are "up-to-date".
	// at the moment, we take an "all-or-nothing" approach.  ultimately, this
	// should be per-user, and the SchedD should start normally and run jobs
	// for users who DO have valid credentials, and simply holding on to jobs
	// in idle state for users who do NOT have valid credentials.
	//
	char* p = param("SEC_CREDENTIAL_DIRECTORY");
	if(p) {
		free(p);
		bool success = false;
		int retries = 60;
		do {
			// look for existence of file that says everything is up-to-date.
			success = credmon_poll(NULL, false, false);
			if(!success) {
				dprintf(D_ALWAYS, "SCHEDD: User credentials not up-to-date.  Start-up delayed.  Waiting 10 seconds and trying %i more times.\n", retries);
				sleep(10);
				retries--;
			}
		} while ((!success) && (retries > 0));

		// we tried, we give up.
		if(!success) {
			EXCEPT("User credentials unavailable after 10 minutes");
		}
	}
	// User creds good to go, let's start this thing up!
#endif  // WIN32

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
	PostInitJobQueue();

		// Initialize the dedicated scheduler stuff
	dedicated_scheduler.initialize();

		// Do a timeout now at startup to get the ball rolling...
	scheduler.timeout();

#if defined(HAVE_DLOPEN)
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
	set_mySubSystem( "SCHEDD", SUBSYSTEM_TYPE_SCHEDD );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
