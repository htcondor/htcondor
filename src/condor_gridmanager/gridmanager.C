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
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "my_username.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "format_time.h"  // for format_time and friends
#include "condor_string.h"	// for strnewp and friends
#include "daemon.h"
#include "dc_schedd.h"
#include "condor_email.h"

#include "gridmanager.h"

#define QMGMT_TIMEOUT 15

#define UPDATE_SCHEDD_DELAY		5

#define HASH_TABLE_SIZE			500

// timer id values that indicates the timer is not registered
#define TIMER_UNSET		-1

extern char *myUserName;

struct ScheddUpdateAction {
	GlobusJob *job;
	int actions;
	int request_id;
	bool deleted;
};

struct OrphanCallback_t {
	char *job_contact;
	int state;
	int errorcode;
};

// Stole these out of the schedd code
int procIDHash( const PROC_ID &procID, int numBuckets )
{
	//dprintf(D_ALWAYS,"procIDHash: cluster=%d proc=%d numBuck=%d\n",procID.cluster,procID.proc,numBuckets);
	return ( (procID.cluster+(procID.proc*19)) % numBuckets );
}

bool operator==( const PROC_ID a, const PROC_ID b)
{
	return a.cluster == b.cluster && a.proc == b.proc;
}

template class HashTable<HashKey, GlobusJob *>;
template class HashBucket<HashKey, GlobusJob *>;
template class HashTable<PROC_ID, GlobusJob *>;
template class HashBucket<PROC_ID, GlobusJob *>;
template class HashTable<HashKey, GlobusResource *>;
template class HashBucket<HashKey, GlobusResource *>;
template class HashTable<HashKey, char *>;
template class HashBucket<HashKey, char *>;
template class HashTable<PROC_ID, ScheddUpdateAction *>;
template class HashBucket<PROC_ID, ScheddUpdateAction *>;
template class List<OrphanCallback_t>;
template class Item<OrphanCallback_t>;

HashTable <PROC_ID, ScheddUpdateAction *> pendingScheddUpdates( HASH_TABLE_SIZE,
																procIDHash );
HashTable <PROC_ID, ScheddUpdateAction *> completedScheddUpdates( HASH_TABLE_SIZE,
																  procIDHash );
bool addJobsSignaled = false;
bool removeJobsSignaled = false;
int contactScheddTid = TIMER_UNSET;
int contactScheddDelay;
time_t lastContactSchedd = 0;

List<OrphanCallback_t> OrphanCallbackList;

char *gramCallbackContact = NULL;
char *gassServerUrl = NULL;

char *ScheddAddr = NULL;
char *X509Proxy = NULL;
bool useDefaultProxy = true;
char *ScheddJobConstraint = NULL;
char *GridmanagerScratchDir = NULL;

HashTable <HashKey, GlobusJob *> JobsByContact( HASH_TABLE_SIZE,
												hashFunction );
HashTable <PROC_ID, GlobusJob *> JobsByProcID( HASH_TABLE_SIZE,
											   procIDHash );
HashTable <HashKey, GlobusResource *> ResourcesByName( HASH_TABLE_SIZE,
													   hashFunction );

static void EmailTerminateEvent(ClassAd * jobAd, bool exit_status_valid);

bool firstScheddContact = true;

char *Owner = NULL;

int checkResources_tid = TIMER_UNSET;

GahpClient GahpMain;

void RequestContactSchedd();
int doContactSchedd();
int checkResources();

// handlers
int ADD_JOBS_signalHandler( int );
int REMOVE_JOBS_signalHandler( int );


// return value of true means requested update has been committed to schedd.
// return value of false means requested update has been queued, but has not
//   been committed to the schedd yet
bool
addScheddUpdateAction( GlobusJob *job, int actions, int request_id )
{
	ScheddUpdateAction *curr_action;

	if ( request_id != 0 &&
		 completedScheddUpdates.lookup( job->procID, curr_action ) == 0 ) {
		ASSERT( curr_action->job == job );

		if ( request_id == curr_action->request_id && request_id != 0 ) {
			completedScheddUpdates.remove( job->procID );
			delete curr_action;
			return true;
		} else {
			completedScheddUpdates.remove( job->procID );
			delete curr_action;
		}
	}

	if ( pendingScheddUpdates.lookup( job->procID, curr_action ) == 0 ) {
		ASSERT( curr_action->job == job );

		curr_action->actions |= actions;
		curr_action->request_id = request_id;
	} else if ( actions ) {
		curr_action = new ScheddUpdateAction;
		curr_action->job = job;
		curr_action->actions = actions;
		curr_action->request_id = request_id;
		curr_action->deleted = false;

		pendingScheddUpdates.insert( job->procID, curr_action );
		RequestContactSchedd();
	} else {
		// If a new request comes in with no actions and there are no
		// pending actions, just return true (since there's nothing to be
		// committed to the schedd)
		return true;
	}

	return false;

}

void
removeScheddUpdateAction( GlobusJob *job ) {
	ScheddUpdateAction *curr_action;

	if ( completedScheddUpdates.lookup( job->procID, curr_action ) == 0 ) {
		completedScheddUpdates.remove( job->procID );
		delete curr_action;
	}

	if ( pendingScheddUpdates.lookup( job->procID, curr_action ) == 0 ) {
		pendingScheddUpdates.remove( job->procID );
		delete curr_action;
	}
}

void
RequestContactSchedd()
{
	if ( contactScheddTid == TIMER_UNSET ) {
		time_t now = time(NULL);
		time_t delay = 0;
		if ( lastContactSchedd + contactScheddDelay > now ) {
			delay = (lastContactSchedd + contactScheddDelay) - now;
		}
		contactScheddTid = daemonCore->Register_Timer( delay,
												(TimerHandler)&doContactSchedd,
												"doContactSchedd", NULL );
	}
}

char *
globusJobId( const char *contact )
{
	static char buff[1024];
	char *first_end;
	char *second_begin;

	ASSERT( strlen(contact) < sizeof(buff) );

	first_end = strrchr( contact, ':' );
	ASSERT( first_end );

	second_begin = strchr( first_end, '/' );
	ASSERT( second_begin );

	strncpy( buff, contact, first_end - contact );
	strcpy( buff + ( first_end - contact ), second_begin );

	return buff;
}

void
rehashJobContact( GlobusJob *job, const char *old_contact,
				  const char *new_contact )
{
	if ( old_contact ) {
		JobsByContact.remove(HashKey(globusJobId(old_contact)));
	}
	if ( new_contact ) {
		JobsByContact.insert(HashKey(globusJobId(new_contact)), job);
	}
}

void
Init()
{
	pid_t schedd_pid;

	// schedd address may be overridden by a commandline option
	// only set it if it hasn't been set already
	if ( ScheddAddr == NULL ) {
		schedd_pid = daemonCore->getppid();
		ScheddAddr = daemonCore->InfoCommandSinfulString( schedd_pid );
		if ( ScheddAddr == NULL ) {
			EXCEPT( "Failed to determine schedd's address" );
		} else {
			ScheddAddr = strdup( ScheddAddr );
		}
	}

	// read config file
	// initialize variables

	Owner = my_username();
	if ( Owner == NULL ) {
		EXCEPT( "Can't determine username" );
	}

	if ( ScheddJobConstraint == NULL ) {
		// CRUFT: Backwards compatibility with pre-6.5.1 schedds that don't
		//   give us a constraint expression for querying our jobs. Build
		//   it ourselves like the old gridmanager did.
		MyString buf;
		MyString expr = "";

		if ( myUserName ) {
			if ( strchr(myUserName,'@') ) {
				// we were given a full name : owner@uid-domain
				buf.sprintf("%s == \"%s\"", ATTR_USER, myUserName);
			} else {
				// we were just give an owner name without a domain
				buf.sprintf("%s == \"%s\"", ATTR_OWNER, myUserName);
			}
		} else {
			buf.sprintf("%s == \"%s\"", ATTR_OWNER, Owner);
		}
		expr += buf;

		if(useDefaultProxy == false) {
			buf.sprintf(" && %s =?= \"%s\" ", ATTR_X509_USER_PROXY, X509Proxy);
		} else {
			buf.sprintf(" && %s =?= UNDEFINED ", ATTR_X509_USER_PROXY);
		}
		expr += buf;

		ScheddJobConstraint = strdup( expr.Value() );

		if ( X509Proxy == NULL ) {
			EXCEPT( "Old schedd didn't specify proxy with -x" );
		}
		if ( UseSingleProxy( X509Proxy, InitializeGahp ) == false ) {
			EXCEPT( "Failed to initialize ProxyManager" );
		}

	} else {

		if ( GridmanagerScratchDir == NULL ) {
			EXCEPT( "Schedd didn't specify scratch dir with -S" );
		}
		if ( UseMultipleProxies( GridmanagerScratchDir, InitializeGahp ) == false ) {
			EXCEPT( "Failed to initialize Proxymanager" );
		}
	}
}

void
Register()
{
	daemonCore->Register_Signal( GRIDMAN_ADD_JOBS, "AddJobs",
								 (SignalHandler)&ADD_JOBS_signalHandler,
								 "ADD_JOBS_signalHandler", NULL, WRITE );

	daemonCore->Register_Signal( GRIDMAN_REMOVE_JOBS, "RemoveJobs",
								 (SignalHandler)&REMOVE_JOBS_signalHandler,
								 "REMOVE_JOBS_signalHandler", NULL, WRITE );

	Reconfig();
}

void
Reconfig()
{
	int tmp_int;
	bool tmp_bool;

	// This method is called both at startup [from method Init()], and
	// when we are asked to reconfig.

	if ( checkResources_tid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer(checkResources_tid);
		checkResources_tid = TIMER_UNSET;
	}
	checkResources_tid = daemonCore->Register_Timer( 1, 60,
												(TimerHandler)&checkResources,
												"checkResources", NULL );

	contactScheddDelay = param_integer("GRIDMANAGER_CONTACT_SCHEDD_DELAY", 5);

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	GlobusJob::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_RESOURCE_PROBE_INTERVAL", 5 * 60 );
	GlobusResource::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	GlobusJob::setGahpCallTimeout( tmp_int );
	GlobusResource::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	GlobusJob::setConnectFailureRetry( tmp_int );

	tmp_bool = param_boolean("ENABLE_GRID_MONITOR",false);
	GlobusResource::setEnableGridMonitor( tmp_bool );

	CheckProxies_interval = param_integer( "GRIDMANAGER_CHECKPROXY_INTERVAL",
										   10 * 60 );

	minProxy_time = param_integer( "GRIDMANAGER_MINIMUM_PROXY_TIME", 3 * 60 );

	int max_requests = param_integer( "GRIDMANAGER_MAX_PENDING_REQUESTS", 50 );
//	if ( max_requests < max_pending_submits * 5 ) {
//		max_requests = max_pending_submits * 5;
//	}
	GahpMain.setMaxPendingRequests(max_requests);

	// Tell all the job objects to deal with their new config values
	GlobusJob *next_job;

	JobsByProcID.startIterations();

	while ( JobsByProcID.iterate( next_job ) != 0 ) {
		next_job->Reconfig();
	}

	// Tell all the resource objects to deal with their new config values
	GlobusResource *next_resource;

	ResourcesByName.startIterations();

	while ( ResourcesByName.iterate( next_resource ) != 0 ) {
		next_resource->Reconfig();
	}

	// Always check the proxy on a reconfig.
	doCheckProxies();
}

bool
InitializeGahp( const char *proxy_filename )
{
	int err;

	if ( GahpMain.Initialize( proxy_filename ) == false ) {
		dprintf( D_ALWAYS, "Error initializing GAHP\n" );
		return false;
	}

	GahpMain.setMode( GahpClient::blocking );

	err = GahpMain.globus_gram_client_callback_allow( gramCallbackHandler,
													  NULL,
													  &gramCallbackContact );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error enabling GRAM callback, err=%d - %s\n", 
				 err, GahpMain.globus_gram_client_error_string(err) );
		return false;
	}

	err = GahpMain.globus_gass_server_superez_init( &gassServerUrl, 0 );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error enabling GASS server, err=%d\n", err );
		return false;
	}
	dprintf( D_FULLDEBUG, "GASS server URL: %s\n", gassServerUrl );

	return true;
}

int
ADD_JOBS_signalHandler( int signal )
{
	dprintf(D_FULLDEBUG,"Received ADD_JOBS signal\n");

	if ( !addJobsSignaled ) {
		RequestContactSchedd();
		addJobsSignaled = true;
	}

	return TRUE;
}

int
REMOVE_JOBS_signalHandler( int signal )
{
	dprintf(D_FULLDEBUG,"Received REMOVE_JOBS signal\n");

	if ( !removeJobsSignaled ) {
		RequestContactSchedd();
		removeJobsSignaled = true;
	}

	return TRUE;
}

int
checkResources()
{
	GlobusResource * next_resource;
	int num_resources = 0;
	int num_down_resources = 0;
	time_t most_recent_time = 0;
	time_t least_recent_time = 0;
	bool locked_up;

	ResourcesByName.startIterations();
	while ( ResourcesByName.iterate( next_resource ) != 0 ) {
		num_resources++;
		if ( next_resource->IsDown() ) {
			time_t downtime = next_resource->getLastStatusChangeTime();
			if ( downtime == 0 ) {
				// don't know when.... useless!
				continue;
			}
			most_recent_time = MAX(most_recent_time,downtime);
			if ( least_recent_time == 0 ) {
				least_recent_time = downtime;
			} else {
				least_recent_time = MIN(least_recent_time,downtime);
			}
			num_down_resources++;
		}
	}

	dprintf(D_FULLDEBUG,"checkResources(): %d resources, %d are down\n",
		num_resources, num_down_resources);


	locked_up = false;
	if ( num_resources > 0 && num_down_resources > 0 ) {
			// Decide if we want to take action if one resource is down,
			// or only if all resources are down.
		bool any_down = 
			param_boolean("GRIDMANAGER_RESTART_ON_ANY_DOWN_RESOURCES",false);
		if ( any_down ) {
				// Consider the gahp wedged if one or more resources are down.
				// We already know this to be true, so mark the flag..
			locked_up = true;
		} else {
				// Consider the gahp wedged only if all resources are down
			if ( num_resources == num_down_resources ) {
				locked_up = true;
			}
		}
	}

	if ( locked_up ) {
			// some resources are down!  see for how long...
		time_t downfor = time(NULL) - most_recent_time;
		int max_downtime = param_integer("GRIDMANAGER_MAX_TIME_DOWN_RESOURCES",
							15 * 60 );	// 15 minutes default
		if ( downfor > max_downtime ) {
			dprintf(D_ALWAYS,
				"Resources down for more than %d secs -- killing GAHP\n",
				max_downtime);
			if ( GahpMain.getPid() > 0 ) {
				daemonCore->Send_Signal(GahpMain.getPid(),SIGKILL);
			} else {
				dprintf(D_ALWAYS,"ERROR - no gahp found???\n");
			}
		} else {
			dprintf(D_ALWAYS,"Resources down for %d seconds!\n",
				downfor);
		}
	}

	return TRUE;
}


int
doContactSchedd()
{
	int rc;
	Qmgr_connection *schedd;
	ScheddUpdateAction *curr_action;
	GlobusJob *curr_job;
	ClassAd *next_ad;
	char expr_buf[12000];
	bool schedd_updates_complete = false;
	bool schedd_deletes_complete = false;
	bool add_remove_jobs_complete = false;
	bool commit_transaction = true;
	bool fake_job_in_queue = false;
	int failure_line_num = 0;

	dprintf(D_FULLDEBUG,"in doContactSchedd()\n");

	contactScheddTid = TIMER_UNSET;

	// Write user log events before connection to the schedd
	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		curr_job = curr_action->job;

		if ( curr_action->actions & UA_LOG_SUBMIT_EVENT &&
			 !curr_job->submitLogged ) {
			WriteGlobusSubmitEventToUserLog( curr_job->ad );
			curr_job->submitLogged = true;
		}
		if ( curr_action->actions & UA_LOG_EXECUTE_EVENT &&
			 !curr_job->executeLogged ) {
			WriteExecuteEventToUserLog( curr_job->ad );
			curr_job->executeLogged = true;
		}
		if ( curr_action->actions & UA_LOG_SUBMIT_FAILED_EVENT &&
			 !curr_job->submitFailedLogged ) {
			WriteGlobusSubmitFailedEventToUserLog( curr_job->ad,
												   curr_job->submitFailureCode );
			curr_job->submitFailedLogged = true;
		}
		if ( curr_action->actions & UA_LOG_TERMINATE_EVENT &&
			 !curr_job->terminateLogged ) {
			EmailTerminateEvent(curr_job->ad,
				curr_job->IsExitStatusValid());
			WriteTerminateEventToUserLog( curr_job );
			curr_job->terminateLogged = true;
		}
		if ( curr_action->actions & UA_LOG_ABORT_EVENT &&
			 !curr_job->abortLogged ) {
			WriteAbortEventToUserLog( curr_job->ad );
			curr_job->abortLogged = true;
		}
		if ( curr_action->actions & UA_LOG_EVICT_EVENT &&
			 !curr_job->evictLogged ) {
			WriteEvictEventToUserLog( curr_job->ad );
			curr_job->evictLogged = true;
		}
		if ( curr_action->actions & UA_HOLD_JOB &&
			 !curr_job->holdLogged ) {
			WriteHoldEventToUserLog( curr_job->ad );
			curr_job->holdLogged = true;
		}

	}

	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, false );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd!\n");
		// Should we be retrying infinitely?
		lastContactSchedd = time(NULL);
		RequestContactSchedd();
		return TRUE;
	}

	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		curr_job = curr_action->job;

		// Check the job status on the schedd to see if the job's been
		// held or removed. We don't want to blindly update the status.
		int job_status_schedd;
		rc = GetAttributeInt( curr_job->procID.cluster,
							  curr_job->procID.proc,
							  ATTR_JOB_STATUS, &job_status_schedd );
		if ( rc < 0 ) {
			if ( errno == ETIMEDOUT ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			} else {
					// The job is not in the schedd's job queue. This
					// probably means that the user did a condor_rm -f,
					// so report a job status of REMOVED and pretend that
					// all updates for the job succeed. Otherwise, we'll
					// never make forward progress on the job.
				job_status_schedd = REMOVED;
				fake_job_in_queue = true;
			}
		}

		// If the job is marked as REMOVED or HELD on the schedd, don't
		// change it. Instead, modify our state to match it.
		// exception: if job is marked as REMOVED, allow us to place it on hold.
		if ( (job_status_schedd == REMOVED && (!(curr_action->actions & UA_HOLD_JOB)))
			 || job_status_schedd == HELD ) {
			curr_job->UpdateCondorState( job_status_schedd );
			curr_job->ad->SetDirtyFlag( ATTR_JOB_STATUS, false );
			curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON, false );
			curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON_CODE, false );
			curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON_SUBCODE, false );
		} else if ( curr_action->actions & UA_HOLD_JOB ) {
			char *reason = NULL;
			rc = GetAttributeStringNew( curr_job->procID.cluster,
										curr_job->procID.proc,
										ATTR_RELEASE_REASON, &reason );
			if ( rc >= 0 ) {
				curr_job->UpdateJobAdString( ATTR_LAST_RELEASE_REASON,
											 reason );
			} else if ( errno == ETIMEDOUT ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			if ( reason ) {
				free( reason );
			}
			curr_job->UpdateJobAd( ATTR_RELEASE_REASON, "UNDEFINED" );
			curr_job->UpdateJobAdInt( ATTR_ENTERED_CURRENT_STATUS,
									  (int)time(0) );
				
				// if the job was in REMOVED state, make certain we return
				// to the removed state when it is released.
			if ( job_status_schedd == REMOVED ) {
				curr_job->UpdateJobAdInt(ATTR_JOB_STATUS_ON_RELEASE,
					job_status_schedd);
			}

			int sys_holds = 0;
			rc=GetAttributeInt(curr_job->procID.cluster, 
							   curr_job->procID.proc, ATTR_NUM_SYSTEM_HOLDS,
							   &sys_holds);
			if ( rc < 0 && fake_job_in_queue == false ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			sys_holds++;
			curr_job->UpdateJobAdInt( ATTR_NUM_SYSTEM_HOLDS, sys_holds );
		} else {	// !UA_HOLD_JOB
			// If we have a
			// job marked as HELD, it's because of an earlier hold
			// (either by us or the user). In this case, we don't want
			// to undo a subsequent unhold done on the schedd. Instead,
			// we keep our HELD state, kill the job, forget about it,
			// then relearn about it later (this makes it easier to
			// ensure that we pick up changed job attributes).
			if ( curr_job->condorState == HELD ) {
				curr_job->ad->SetDirtyFlag( ATTR_JOB_STATUS, false );
				curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON, false );
				curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON_CODE, false );
				curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON_SUBCODE, false );
			} else {
					// Finally, if we are just changing from one unintersting state
					// to another, update the ATTR_ENTERED_CURRENT_STATUS time.
				if ( curr_job->condorState != job_status_schedd ) {
					curr_job->UpdateJobAdInt( ATTR_ENTERED_CURRENT_STATUS,
											  (int)time(0) );
				}
			}
		}

		// Adjust run time for condor_q
		int shadowBirthdate = 0;
		curr_job->ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadowBirthdate );
		if ( curr_job->condorState == RUNNING &&
			 shadowBirthdate == 0 ) {

			// The job has started a new interval of running
			int current_time = (int)time(NULL);
			// ATTR_SHADOW_BIRTHDATE on the schedd will be updated below
			curr_job->UpdateJobAdInt( ATTR_SHADOW_BIRTHDATE, current_time );

		} else if ( curr_job->condorState != RUNNING &&
					shadowBirthdate != 0 ) {

			// The job has stopped an interval of running, add the current
			// interval to the accumulated total run time
			float accum_time = 0;
			rc = GetAttributeFloat(curr_job->procID.cluster,
								   curr_job->procID.proc,
								   ATTR_JOB_REMOTE_WALL_CLOCK,&accum_time);
			if ( fake_job_in_queue == true ) {
				accum_time = 0.0;
			} else if ( rc < 0 ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			accum_time += (float)( time(NULL) - shadowBirthdate );
			curr_job->UpdateJobAdFloat( ATTR_JOB_REMOTE_WALL_CLOCK,
										accum_time );
			curr_job->UpdateJobAd( ATTR_JOB_WALL_CLOCK_CKPT, "UNDEFINED" );
			// ATTR_SHADOW_BIRTHDATE on the schedd will be updated below
			curr_job->UpdateJobAdInt( ATTR_SHADOW_BIRTHDATE, 0 );

		}

		if ( curr_action->actions & UA_FORGET_JOB ) {
			curr_job->UpdateJobAdBool( ATTR_JOB_MANAGED, 0 );
		}

		dprintf( D_FULLDEBUG, "Updating classad values for %d.%d:\n",
				 curr_job->procID.cluster, curr_job->procID.proc );
		char attr_name[1024];
		char attr_value[1024];
		ExprTree *expr;
		curr_job->ad->ResetExpr();
		while ( (expr = curr_job->ad->NextDirtyExpr()) != NULL ) {
			attr_name[0] = '\0';
			attr_value[0] = '\0';
			expr->LArg()->PrintToStr(attr_name);
			expr->RArg()->PrintToStr(attr_value);

			dprintf( D_FULLDEBUG, "   %s = %s\n", attr_name, attr_value );
			rc = SetAttribute( curr_job->procID.cluster,
							   curr_job->procID.proc,
							   attr_name,
							   attr_value);
			if ( rc < 0 && fake_job_in_queue == false ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
		}

	}

	if ( CloseConnection() < 0 ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

	schedd_updates_complete = true;

	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		if ( curr_action->actions & UA_DELETE_FROM_SCHEDD ) {
			dprintf( D_FULLDEBUG, "Deleting job %d.%d from schedd\n",
					 curr_action->job->procID.cluster,
					 curr_action->job->procID.proc);
			BeginTransaction();
			if ( errno == ETIMEDOUT ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			rc = DestroyProc(curr_action->job->procID.cluster,
							 curr_action->job->procID.proc);
			if ( rc < 0 && fake_job_in_queue == false ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			if ( CloseConnection() < 0 ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			curr_action->deleted = true;
		}

	}

	schedd_deletes_complete = true;

//	if ( BeginTransaction() < 0 ) {
	errno = 0;
	BeginTransaction();
	if ( errno == ETIMEDOUT ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}


	// AddJobs
	/////////////////////////////////////////////////////
	if ( addJobsSignaled || firstScheddContact ) {
		int num_ads = 0;

		dprintf( D_FULLDEBUG, "querying for new jobs\n" );

		// Make sure we grab all Globus Universe jobs (except held ones
		// that we previously indicated we were done with)
		// when we first start up in case we're recovering from a
		// shutdown/meltdown.
		// Otherwise, grab all jobs that are unheld and aren't marked as
		// currently being managed and aren't marked as not matched.
		// If JobManaged is undefined, equate it with false.
		// If Matched is undefined, equate it with true.
		if ( firstScheddContact ) {
			sprintf( expr_buf, 
				"(%s) && %s == %d && (%s =!= FALSE || %s =?= TRUE) && ((%s == %d || %s == %d || (%s == %d && %s == \"%s\")) && %s =!= TRUE) == FALSE",
					 ScheddJobConstraint, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GLOBUS, 
					 ATTR_JOB_MATCHED, ATTR_JOB_MANAGED, ATTR_JOB_STATUS, HELD,
					 ATTR_JOB_STATUS, COMPLETED, ATTR_JOB_STATUS, REMOVED, ATTR_GLOBUS_CONTACT_STRING, NULL_JOB_CONTACT, ATTR_JOB_MANAGED );
		} else {
			sprintf( expr_buf, 
				"(%s) && %s == %d && %s =!= FALSE && %s != %d && %s != %d && (%s != %d || %s != \"%s\") && %s =!= TRUE",
					 ScheddJobConstraint, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GLOBUS,
					 ATTR_JOB_MATCHED, ATTR_JOB_STATUS, HELD, 
					 ATTR_JOB_STATUS, COMPLETED, ATTR_JOB_STATUS, REMOVED, ATTR_GLOBUS_CONTACT_STRING, NULL_JOB_CONTACT, ATTR_JOB_MANAGED );
		}
		dprintf( D_FULLDEBUG,"Using constraint %s\n",expr_buf);
		next_ad = GetNextJobByConstraint( expr_buf, 1 );
		while ( next_ad != NULL ) {
			PROC_ID procID;
			GlobusJob *old_job;
			int job_is_managed = 0;		// default to false if not in ClassAd
			int job_is_matched = 1;		// default to true if not in ClassAd

			next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );
			next_ad->LookupBool(ATTR_JOB_MANAGED,job_is_managed);
			next_ad->LookupBool(ATTR_JOB_MATCHED,job_is_matched);

			if ( JobsByProcID.lookup( procID, old_job ) != 0 ) {

				int rc;
				char resource_name[800];
				GlobusResource *resource = NULL;

				// job had better be either managed or matched! (or both)
				ASSERT( job_is_managed || job_is_matched );

				resource_name[0] = '\0';
				int must_expand = 0;

				next_ad->LookupBool(ATTR_JOB_MUST_EXPAND, must_expand);
				if ( !must_expand ) {
					next_ad->LookupString(ATTR_GLOBUS_RESOURCE, resource_name);
					if ( strstr(resource_name,"$$") ) {
						must_expand = 1;
					}
				}

				if (must_expand) {
					// Get the expanded ClassAd from the schedd, which
					// has the globus resource filled in with info from
					// the matched ad.
					delete next_ad;
					next_ad = NULL;
					next_ad = GetJobAd(procID.cluster,procID.proc);
					if ( next_ad == NULL && errno == ETIMEDOUT ) {
						failure_line_num = __LINE__;
						commit_transaction = false;
						goto contact_schedd_disconnect;
					}
					if ( next_ad == NULL ) {
						// We may get here if it was not possible to expand
						// one of the $$() expressions.  We don't want to
						// roll back the transaction and blow away the
						// hold that the schedd just put on the job, so
						// simply skip over this ad.

						dprintf(D_ALWAYS,"Failed to get expanded job ClassAd from Schedd for %d.%d.  errno=%d\n",procID.cluster,procID.proc,errno);
					}
					else {
						resource_name[0] = '\0';
						next_ad->LookupString(ATTR_GLOBUS_RESOURCE, resource_name);
					}
				}

				if ( !next_ad) ; //Failed to get ad.  Continue.
				else if ( resource_name[0] == '\0' ) {

					const char *hold_reason =
						"GlobusResource is not set in the job ad";	
					char buffer[128];
					char *reason = NULL;
					dprintf( D_ALWAYS, "Job %d.%d has no Globus resource name!\n",
							 procID.cluster, procID.proc );
					// No GlobusResource, so put the job on hold
					rc = SetAttributeInt( procID.cluster,
										  procID.proc,
										  ATTR_JOB_STATUS,
										  HELD );
					if ( rc < 0 ) {
						delete next_ad;
						failure_line_num = __LINE__;
						commit_transaction = false;
						goto contact_schedd_disconnect;
					}
					if ( next_ad->LookupString(ATTR_RELEASE_REASON, &reason)
						     != 0 ) {
						rc = SetAttributeString( procID.cluster, procID.proc,
												 ATTR_LAST_RELEASE_REASON,
												 reason );
						free( reason );
						if ( rc < 0 ) {
							delete next_ad;
							failure_line_num = __LINE__;
							commit_transaction = false;
							goto contact_schedd_disconnect;
						}
					}
					rc = SetAttribute( procID.cluster, procID.proc,
									   ATTR_RELEASE_REASON, "UNDEFINED" );
					if ( rc < 0 ) {
						delete next_ad;
						failure_line_num = __LINE__;
						commit_transaction = false;
						goto contact_schedd_disconnect;
					}
					rc = SetAttributeString( procID.cluster,
											 procID.proc,
											 ATTR_HOLD_REASON,
											 hold_reason );
					if ( rc < 0 ) {
						delete next_ad;
						failure_line_num = __LINE__;
						commit_transaction = false;
 						goto contact_schedd_disconnect;
					}
					sprintf( buffer, "%s = \"%s\"", ATTR_HOLD_REASON,
							 hold_reason );
					next_ad->InsertOrUpdate( buffer );
					WriteHoldEventToUserLog( next_ad );

					delete next_ad;

				} else {

					const char *canonical_name = GlobusResource::CanonicalName( resource_name );
					ASSERT(canonical_name);
					rc = ResourcesByName.lookup( HashKey( canonical_name ),
												  resource );

					if ( rc != 0 ) {
						resource = new GlobusResource( canonical_name );
						ASSERT(resource);
						ResourcesByName.insert( HashKey( canonical_name ),
												 resource );
					} else {
						ASSERT(resource);
					}

					GlobusJob *new_job = new GlobusJob( next_ad, resource );
					ASSERT(new_job);
					new_job->SetEvaluateState();
					dprintf(D_ALWAYS,"Found job %d.%d --- inserting\n",new_job->procID.cluster,new_job->procID.proc);
					JobsByProcID.insert( new_job->procID, new_job );
					num_ads++;

					if ( !job_is_managed ) {
						// Set Managed to true in the local ad and leave it
						// dirty so that if our update here to the schedd is
						// aborted, the change will make it the first time
						// the job tries to update anything on the schedd.
						new_job->UpdateJobAdBool( ATTR_JOB_MANAGED, 1 );
						rc = SetAttribute( new_job->procID.cluster,
										   new_job->procID.proc,
										   ATTR_JOB_MANAGED,
										   "TRUE" );
						if ( rc < 0 ) {
							failure_line_num = __LINE__;
							commit_transaction = false;
							goto contact_schedd_disconnect;
						}
					}

				}

			} else {

				// We already know about this job, skip
				delete next_ad;

			}

			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}	// end of while next_ad
		if ( errno == ETIMEDOUT ) {
			failure_line_num = __LINE__;
			commit_transaction = false;
			goto contact_schedd_disconnect;
		}

		dprintf(D_FULLDEBUG,"Fetched %d new job ads from schedd\n",num_ads);
	}	// end of handling add jobs

	/////////////////////////////////////////////////////

	// RemoveJobs
	/////////////////////////////////////////////////////
	if ( removeJobsSignaled ) {
		int num_ads = 0;

		dprintf( D_FULLDEBUG, "querying for removed/held jobs\n" );

		// Grab jobs marked as REMOVED or marked as HELD that we haven't
		// previously indicated that we're done with (by setting JobManaged
		// to FALSE. If JobManaged is undefined, equate it with false.
		sprintf( expr_buf, "(%s) && %s == %d && (%s == %d || (%s == %d && %s =?= TRUE))",
				 ScheddJobConstraint, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GLOBUS,
				 ATTR_JOB_STATUS, REMOVED, ATTR_JOB_STATUS, HELD,
				 ATTR_JOB_MANAGED );

		next_ad = GetNextJobByConstraint( expr_buf, 1 );
		while ( next_ad != NULL ) {
			PROC_ID procID;
			GlobusJob *next_job;
			int curr_status;

			next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );
			next_ad->LookupInteger( ATTR_JOB_STATUS, curr_status );

			if ( JobsByProcID.lookup( procID, next_job ) == 0 ) {
				// Should probably skip jobs we already have marked as
				// held or removed

				// Save the remove reason in our local copy of the job ad
				// so that we can write it in the abort log event.
				if ( curr_status == REMOVED ) {
					int rc;
					char *remove_reason = NULL;
					rc = GetAttributeStringNew( procID.cluster,
												procID.proc,
												ATTR_REMOVE_REASON,
												&remove_reason );
					if ( rc == 0 ) {
						next_job->UpdateJobAdString( ATTR_REMOVE_REASON,
													 remove_reason );
					} else if ( rc < 0 && errno == ETIMEDOUT ) {
						delete next_ad;
						failure_line_num = __LINE__;
						commit_transaction = false;
						goto contact_schedd_disconnect;
					}
					if ( remove_reason ) {
						free( remove_reason );
					}
				}

				// Save the hold reason in our local copy of the job ad
				// so that we can write it in the hold log event.
				if ( curr_status == HELD ) {
					int rc;
					char *hold_reason = NULL;
					int hcode = 0;
					int hsubcode = 0;
					rc = GetAttributeStringNew( procID.cluster,
												procID.proc,
												ATTR_HOLD_REASON,
												&hold_reason );
					rc += GetAttributeInt( procID.cluster,
												procID.proc,
												ATTR_HOLD_REASON_CODE,
												&hcode );
					rc += GetAttributeInt( procID.cluster,
												procID.proc,
												ATTR_HOLD_REASON_SUBCODE,
												&hsubcode );
					if ( rc == 0 ) {
						next_job->UpdateJobAdString( ATTR_HOLD_REASON,
													 hold_reason );
						next_job->UpdateJobAdInt( ATTR_HOLD_REASON_CODE,
													 hcode );
						next_job->UpdateJobAdInt( ATTR_HOLD_REASON_SUBCODE,
													 hsubcode );
					} else if ( rc < 0 && errno == ETIMEDOUT ) {
						delete next_ad;
						failure_line_num = __LINE__;
						commit_transaction = false;
						goto contact_schedd_disconnect;
					}
					if ( hold_reason ) {
						free( hold_reason );
					}
				}

				// Don't update the condor state if a communications error
				// kept us from getting a remove reason to go with it.
				next_job->UpdateCondorState( curr_status );
				num_ads++;

			} else if ( curr_status == REMOVED ) {

				// If we don't know about the job, remove it immediately
				// I don't think this can happen in the normal case,
				// but I'm not sure.
				dprintf( D_ALWAYS, 
						 "Don't know about removed job %d.%d. "
						 "Deleting it immediately\n", procID.cluster,
						 procID.proc );
				// Log the removal of the job from the queue
				WriteAbortEventToUserLog( next_ad );
				rc = DestroyProc( procID.cluster, procID.proc );
				if ( rc < 0 ) {
					delete next_ad;
					failure_line_num = __LINE__;
					commit_transaction = false;
					goto contact_schedd_disconnect;
				}

			} else {

				dprintf( D_ALWAYS, "Don't know about held job %d.%d. "
						 "Ignoring it\n",
						 procID.cluster, procID.proc );

			}

			delete next_ad;
			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}
		if ( errno == ETIMEDOUT ) {
			commit_transaction = false;
			failure_line_num = __LINE__;
			goto contact_schedd_disconnect;
		}

		dprintf(D_FULLDEBUG,"Fetched %d job ads from schedd\n",num_ads);
	}
	/////////////////////////////////////////////////////

	if ( CloseConnection() < 0 ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

	add_remove_jobs_complete = true;

 contact_schedd_disconnect:
	DisconnectQ( schedd, commit_transaction );

	if ( schedd_updates_complete == false ) {
		dprintf( D_ALWAYS, "Schedd connection error during updates at line %d! Will retry\n", failure_line_num );
		lastContactSchedd = time(NULL);
		RequestContactSchedd();
		return TRUE;
	}

	// Wake up jobs that had schedd updates pending and delete job
	// objects that wanted to be deleted
	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		curr_job = curr_action->job;

		if ( schedd_updates_complete ) {
			curr_job->ad->ClearAllDirtyFlags();
		}

		if ( curr_action->actions & UA_FORGET_JOB ) {

			if ( (curr_action->actions & UA_DELETE_FROM_SCHEDD) ?
				 curr_action->deleted == true :
				 true ) {

				if ( curr_job->jobContact != NULL ) {
					JobsByContact.remove( HashKey( globusJobId(curr_job->jobContact) ) );
				}
				JobsByProcID.remove( curr_job->procID );
					// If wantRematch is set, send a reschedule now
				if ( curr_job->wantRematch ) {
					static DCSchedd* schedd_obj = NULL;
					if ( !schedd_obj ) {
						schedd_obj = new DCSchedd(NULL,NULL);
						ASSERT(schedd_obj);
					}
					schedd_obj->reschedule();
				}
				GlobusResource *resource = curr_job->GetResource();
				delete curr_job;

				if ( resource->IsEmpty() ) {
					ResourcesByName.remove( HashKey( resource->ResourceName() ) );
					delete resource;
				}

				delete curr_action;
			}
		} else if ( curr_action->request_id != 0 ) {
			completedScheddUpdates.insert( curr_job->procID, curr_action );
			curr_job->SetEvaluateState();
		} else {
			delete curr_action;
		}

	}

	pendingScheddUpdates.clear();

	if ( add_remove_jobs_complete == true ) {
		firstScheddContact = false;
		addJobsSignaled = false;
		removeJobsSignaled = false;
	}

	// Check if we have any jobs left to manage. If not, exit.
	if ( JobsByProcID.getNumElements() == 0 ) {
		dprintf( D_ALWAYS, "No jobs left, shutting down\n" );
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}

	lastContactSchedd = time(NULL);

	if ( add_remove_jobs_complete == false ) {
		dprintf( D_ALWAYS, "Schedd connection error at line %d! Will retry\n", failure_line_num );
		RequestContactSchedd();
	}

dprintf(D_FULLDEBUG,"leaving doContactSchedd()\n");
	return TRUE;
}

int
orphanCallbackHandler()
{
	int rc;
	GlobusJob *this_job;
	OrphanCallback_t *orphan;

	// Remove the first element in the list
	OrphanCallbackList.Rewind();
	if ( OrphanCallbackList.Next( orphan ) == false ) {
		// Empty list
		return TRUE;
	}
	OrphanCallbackList.DeleteCurrent();

	// Find the right job object
	rc = JobsByContact.lookup( HashKey( globusJobId(orphan->job_contact) ), this_job );
	if ( rc != 0 || this_job == NULL ) {
		dprintf( D_ALWAYS, 
			"orphanCallbackHandler: Can't find record for globus job with "
			"contact %s on globus state %d, errorcode %d, ignoring\n",
			orphan->job_contact, orphan->state, orphan->errorcode );
		free( orphan->job_contact );
		delete orphan;
		return TRUE;
	}

	dprintf( D_ALWAYS, "(%d.%d) gram callback: state %d, errorcode %d\n",
			 this_job->procID.cluster, this_job->procID.proc, orphan->state,
			 orphan->errorcode );

	this_job->GramCallback( orphan->state, orphan->errorcode );

	free( orphan->job_contact );
	delete orphan;

	return TRUE;
}

void
gramCallbackHandler( void *user_arg, char *job_contact, int state,
					 int errorcode )
{
	int rc;
	GlobusJob *this_job;

	// Find the right job object
	rc = JobsByContact.lookup( HashKey( globusJobId(job_contact) ), this_job );
	if ( rc != 0 || this_job == NULL ) {
		dprintf( D_ALWAYS, 
			"gramCallbackHandler: Can't find record for globus job with "
			"contact %s on globus state %d, errorcode %d, delaying\n",
			job_contact, state, errorcode );
		OrphanCallback_t *new_orphan = new OrphanCallback_t;
		new_orphan->job_contact = strdup( job_contact );
		new_orphan->state = state;
		new_orphan->errorcode = errorcode;
		OrphanCallbackList.Append( new_orphan );
		daemonCore->Register_Timer( 1, (TimerHandler)&orphanCallbackHandler,
									"orphanCallbackHandler", NULL );
		return;
	}

	dprintf( D_ALWAYS, "(%d.%d) gram callback: state %d, errorcode %d\n",
			 this_job->procID.cluster, this_job->procID.proc, state,
			 errorcode );

	this_job->GramCallback( state, errorcode );
}

// TODO: This appears three times in the Condor source.  Unify?
//   (It only is made visible in condor_shadow.jim's prototypes.h.)
static char *
d_format_time( double dsecs )
{
	int days, hours, minutes, secs;
	static char answer[25];

	const int SECONDS = 1;
	const int MINUTES = (60 * SECONDS);
	const int HOURS   = (60 * MINUTES);
	const int DAYS    = (24 * HOURS);

	secs = (int)dsecs;

	days = secs / DAYS;
	secs %= DAYS;

	hours = secs / HOURS;
	secs %= HOURS;

	minutes = secs / MINUTES;
	secs %= MINUTES;

	(void)sprintf(answer, "%3d %02d:%02d:%02d", days, hours, minutes, secs);

	return( answer );
}

static
void
EmailTerminateEvent(ClassAd * jobAd, bool exit_status_valid)
{
	if ( !jobAd ) {
		dprintf(D_ALWAYS, 
			"email_terminate_event called with invalid ClassAd\n");
		return;
	}

	int cluster, proc;
	jobAd->LookupInteger( ATTR_CLUSTER_ID, cluster );
	jobAd->LookupInteger( ATTR_PROC_ID, proc );

	int notification = NOTIFY_COMPLETE; // default
	jobAd->LookupInteger(ATTR_JOB_NOTIFICATION,notification);

	switch( notification ) {
		case NOTIFY_NEVER:    return;
		case NOTIFY_ALWAYS:   break;
		case NOTIFY_COMPLETE: break;
		case NOTIFY_ERROR:    return;
		default:
			dprintf(D_ALWAYS, 
				"Condor Job %d.%d has unrecognized notification of %d\n",
				cluster, proc, notification );
				// When in doubt, better send it anyway...
			break;
	}

	char subjectline[50];
	sprintf( subjectline, "Condor Job %d.%d", cluster, proc );
	FILE * mailer =  email_user_open( jobAd, subjectline );

	if( ! mailer ) {
		// Is message redundant?  Check email_user_open and euo's children.
		dprintf(D_ALWAYS, 
			"email_terminate_event failed to open a pipe to a mail program.\n");
		return;
	}

		// gather all the info out of the job ad which we want to 
		// put into the email message.
	char JobName[_POSIX_PATH_MAX];
	JobName[0] = '\0';
	jobAd->LookupString( ATTR_JOB_CMD, JobName );

	char Args[_POSIX_ARG_MAX];
	Args[0] = '\0';
	jobAd->LookupString(ATTR_JOB_ARGUMENTS, Args);
	
	/*
	// Not present.  Probably doesn't make sense for Globus
	int had_core = FALSE;
	jobAd->LookupBool( ATTR_JOB_CORE_DUMPED, had_core );
	*/

	int q_date = 0;
	jobAd->LookupInteger(ATTR_Q_DATE,q_date);
	
	float remote_sys_cpu = 0.0;
	jobAd->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, remote_sys_cpu);
	
	float remote_user_cpu = 0.0;
	jobAd->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, remote_user_cpu);
	
	int image_size = 0;
	jobAd->LookupInteger(ATTR_IMAGE_SIZE, image_size);
	
	/*
	int shadow_bday = 0;
	jobAd->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	*/
	
	float previous_runs = 0;
	jobAd->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, previous_runs );
	
	time_t arch_time=0;	/* time_t is 8 bytes some archs and 4 bytes on other
						   archs, and this means that doing a (time_t*)
						   cast on & of a 4 byte int makes my life hell.
						   So we fix it by assigning the time we want to
						   a real time_t variable, then using ctime()
						   to convert it to a string */
	
	time_t now = time(NULL);

	fprintf( mailer, "Your Condor job %d.%d \n", cluster, proc);
	if ( JobName[0] ) {
		fprintf(mailer,"\t%s %s\n",JobName,Args);
	}
	if(exit_status_valid) {
		fprintf(mailer, "has ");

		int int_val;
		if( jobAd->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_val) ) {
			if( int_val ) {
				if( jobAd->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_val) ) {
					fprintf(mailer, "exited with the signal %d.\n", int_val);
				} else {
					fprintf(mailer, "exited with an unknown signal.\n");
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Signal code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_SIGNAL);
				}
			} else {
				if( jobAd->LookupInteger(ATTR_ON_EXIT_CODE, int_val) ) {
					fprintf(mailer, "exited normally with status %d.\n",
						int_val);
				} else {
					fprintf(mailer, "exited normally with unknown status.\n");
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Return code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_CODE);
				}
			}
		} else {
			fprintf(mailer,"has exited.\n");
			dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  ",
				 cluster, proc, ATTR_ON_EXIT_BY_SIGNAL);
		}
	} else {
		fprintf(mailer,"has exited.\n");
	}

	/*
	if( had_core ) {
		fprintf( mailer, "Core file is: %s\n", getCoreName() );
	}
	*/

	arch_time = q_date;
	fprintf(mailer, "\n\nSubmitted at:        %s", ctime(&arch_time));
	
	double real_time = now - q_date;
	arch_time = now;
	fprintf(mailer, "Completed at:        %s", ctime(&arch_time));
	
	fprintf(mailer, "Real Time:           %s\n", 
			d_format_time(real_time));


	fprintf( mailer, "\n" );
	
	if( exit_status_valid ) {
		fprintf(mailer, "Virtual Image Size:  %d Kilobytes\n\n", image_size);
	}
	
	double rutime = remote_user_cpu;
	double rstime = remote_sys_cpu;
	double trtime = rutime + rstime;
	/*
	double wall_time = now - shadow_bday;
	fprintf(mailer, "Statistics from last run:\n");
	fprintf(mailer, "Allocation/Run time:     %s\n",d_format_time(wall_time) );
	*/
	if( exit_status_valid ) {
		fprintf(mailer, "Remote User CPU Time:    %s\n", d_format_time(rutime) );
		fprintf(mailer, "Remote System CPU Time:  %s\n", d_format_time(rstime) );
		fprintf(mailer, "Total Remote CPU Time:   %s\n\n", d_format_time(trtime));
	}
	
	/*
	double total_wall_time = previous_runs + wall_time;
	fprintf(mailer, "Statistics totaled from all runs:\n");
	fprintf(mailer, "Allocation/Run time:     %s\n",
			d_format_time(total_wall_time) );

	// TODO: Can we/should we get this for Globus jobs.
		// TODO: deal w/ total bytes <- obsolete? in original code)
	float network_bytes;
	network_bytes = bytesSent();
	fprintf(mailer, "\nNetwork:\n" );
	fprintf(mailer, "%10s Run Bytes Received By Job\n", 
			metric_units(network_bytes) );
	network_bytes = bytesReceived();
	fprintf(mailer, "%10s Run Bytes Sent By Job\n",
			metric_units(network_bytes) );
	*/

	email_close(mailer);
}

// Initialize a UserLog object for a given job and return a pointer to
// the UserLog object created.  This object can then be used to write
// events and must be deleted when you're done.  This returns NULL if
// the user didn't want a UserLog, so you must check for NULL before
// using the pointer you get back.
UserLog*
InitializeUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char userLogFile[_POSIX_PATH_MAX];
	char domain[_POSIX_PATH_MAX];


	userLogFile[0] = '\0';
	job_ad->LookupString( ATTR_ULOG_FILE, userLogFile );
	if ( userLogFile[0] == '\0' ) {
		// User doesn't want a log
		return NULL;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );
	job_ad->LookupString( ATTR_NT_DOMAIN, domain );

	UserLog *ULog = new UserLog();
	ULog->initialize(Owner, domain, userLogFile, cluster, proc, 0);
	return ULog;
}

bool
WriteExecuteEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char hostname[128];

	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing execute record to user logfile\n",
			 cluster, proc );

	hostname[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_RESOURCE, hostname,
						  sizeof(hostname) - 1 );
	int hostname_len = strcspn( hostname, ":/" );

	ExecuteEvent event;
	strncpy( event.executeHost, hostname, hostname_len );
	event.executeHost[hostname_len] = '\0';
	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_EXECUTE event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteAbortEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char removeReason[256];
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing abort record to user logfile\n",
			 cluster, proc );

	JobAbortedEvent event;

	removeReason[0] = '\0';
	job_ad->LookupString( ATTR_REMOVE_REASON, removeReason,
						   sizeof(removeReason) - 1 );

	event.setReason( removeReason );

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_ABORT event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

// TODO: We could do this with the jobad (as it was called before)
// and just probe for the ATTR_USE_GRID_SHELL...?
bool
WriteTerminateEventToUserLog( GlobusJob *curr_job ) 
{
	if( ! curr_job) {
		dprintf( D_ALWAYS, 
			"Internal Error: WriteTerminateEventToUserLog passed invalid "
			"GlobusJob (null curr_job).\n");
		return false;
	}
	ClassAd *job_ad = curr_job->ad;
	if( ! job_ad) {
		dprintf( D_ALWAYS, 
			"Internal Error: WriteTerminateEventToUserLog passed invalid "
			"GlobusJob (null ad).\n");
		return false;
	}

	int cluster, proc;
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing terminate record to user logfile\n",
			 cluster, proc );

	JobTerminatedEvent event;
	struct rusage r;
	memset( &r, 0, sizeof( struct rusage ) );

#if !defined(WIN32)
	event.run_local_rusage = r;
	event.run_remote_rusage = r;
	event.total_local_rusage = r;
	event.total_remote_rusage = r;
#endif /* WIN32 */
	event.sent_bytes = 0;
	event.recvd_bytes = 0;
	event.total_sent_bytes = 0;
	event.total_recvd_bytes = 0;
	event.normal = true;

	// Globus doesn't tell us how the job exited, so we'll just assume it
	// exited normally.
	event.returnValue = 0;
	event.normal = true;

	if( curr_job->IsExitStatusValid() ) {
		int int_val;
		if( job_ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_val) ) {
			if( int_val ) {
				event.normal = false;
				if( job_ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_val) ) {
					event.signalNumber = int_val;
					event.normal = false;
				} else {
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Signal code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_SIGNAL);
					event.normal = false;
				}
			} else {
				if( job_ad->LookupInteger(ATTR_ON_EXIT_CODE, int_val) ) {
					event.normal = true;
					event.returnValue = int_val;
				} else {
					event.normal = false;
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Return code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_CODE);
					event.normal = false;
				}
			}
		} else {
			event.normal = false;
			dprintf( D_ALWAYS,
				 "(%d.%d) Job ad lacks %s.  Final state unknown.\n",
				 cluster, proc, ATTR_ON_EXIT_BY_SIGNAL);
			event.normal = false;
		}
	}

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_JOB_TERMINATED event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteEvictEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing evict record to user logfile\n",
			 cluster, proc );

	JobEvictedEvent event;
	struct rusage r;
	memset( &r, 0, sizeof( struct rusage ) );

#if !defined(WIN32)
	event.run_local_rusage = r;
	event.run_remote_rusage = r;
#endif /* WIN32 */
	event.sent_bytes = 0;
	event.recvd_bytes = 0;

	event.checkpointed = false;

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_JOB_EVICTED event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteHoldEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing hold record to user logfile\n",
			 cluster, proc );

	JobHeldEvent event;


	event.initFromClassAd(job_ad);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_JOB_HELD event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteGlobusSubmitEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	int version;
	char contact[256];
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing globus submit record to user logfile\n",
			 cluster, proc );

	GlobusSubmitEvent event;

	contact[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_RESOURCE, contact,
						   sizeof(contact) - 1 );
	event.rmContact = strnewp(contact);

	contact[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_CONTACT_STRING, contact,
						   sizeof(contact) - 1 );
	event.jmContact = strnewp(contact);

	version = 0;
	job_ad->LookupInteger( ATTR_GLOBUS_GRAM_VERSION, version );
	event.restartableJM = version >= GRAM_V_1_5;

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_SUBMIT event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteGlobusSubmitFailedEventToUserLog( ClassAd *job_ad, int failure_code )
{
	int cluster, proc;
	char buf[1024];

	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing submit-failed record to user logfile\n",
			 cluster, proc );

	GlobusSubmitFailedEvent event;

	snprintf( buf, 1024, "%d %s", failure_code,
			GahpMain.globus_gram_client_error_string(failure_code) );
	event.reason =  strnewp(buf);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_SUBMIT_FAILED event\n",
				 cluster, proc);
		return false;
	}

	return true;
}

bool
WriteGlobusResourceUpEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char contact[256];
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing globus up record to user logfile\n",
			 cluster, proc );

	GlobusResourceUpEvent event;

	contact[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_RESOURCE, contact,
						   sizeof(contact) - 1 );
	event.rmContact =  strnewp(contact);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_RESOURCE_UP event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteGlobusResourceDownEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char contact[256];
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing globus down record to user logfile\n",
			 cluster, proc );

	GlobusResourceDownEvent event;

	contact[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_RESOURCE, contact,
						   sizeof(contact) - 1 );
	event.rmContact =  strnewp(contact);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_RESOURCE_DOWN event\n",
				 cluster, proc );
		return false;
	}

	return true;
}
