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
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "my_username.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "format_time.h"  // for format_time and friends
#include "condor_string.h"	// for strnewp and friends

#include "gridmanager.h"

#define QMGMT_TIMEOUT 5

#define UPDATE_SCHEDD_DELAY		5

#define HASH_TABLE_SIZE			500

// timer id values that indicates the timer is not registered
#define TIMER_UNSET		-1

struct ScheddUpdateAction {
	GlobusJob *job;
	int actions;
	int request_id;
};

struct OrphanCallback_t {
	char *job_contact;
	int state;
	int errorcode;
};

// Stole these out of the schedd code
int procIDHash( const PROC_ID &procID, int numBuckets )
{
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
template class List<GlobusJob>;
template class Item<GlobusJob>;
template class List<char *>;
template class Item<char *>;
template class List<Service>;
template class Item<Service>;
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

List<Service> ObjectDeleteList;

List<OrphanCallback_t> OrphanCallbackList;

char *gramCallbackContact = NULL;
char *gassServerUrl = NULL;

char *ScheddAddr = NULL;
char *X509Proxy = NULL;
bool useDefaultProxy = true;

HashTable <HashKey, GlobusJob *> JobsByContact( HASH_TABLE_SIZE,
												hashFunction );
HashTable <PROC_ID, GlobusJob *> JobsByProcID( HASH_TABLE_SIZE,
											   procIDHash );
HashTable <HashKey, GlobusResource *> ResourcesByName( HASH_TABLE_SIZE,
													   hashFunction );

bool firstScheddContact = true;

char *Owner = NULL;

int checkProxy_tid = TIMER_UNSET;
int checkProxy_interval;
int minProxy_time;

time_t Proxy_Expiration_Time = 0;

int syncJobIO_tid = TIMER_UNSET;
int syncJobIO_interval;

int checkResources_tid = TIMER_UNSET;

GahpClient GahpMain;

void RequestContactSchedd();
int doContactSchedd();
int checkResources();

// handlers
int ADD_JOBS_signalHandler( int );
int REMOVE_JOBS_signalHandler( int );
int checkProxy();
int syncJobIO();


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

void
rehashJobContact( GlobusJob *job, const char *old_contact,
				  const char *new_contact )
{
	if ( old_contact ) {
		JobsByContact.remove(HashKey(old_contact));
	}
	if ( new_contact ) {
		JobsByContact.insert(HashKey(new_contact), job);
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
	char *tmp = NULL;

	// This method is called both at startup [from method Init()], and
	// when we are asked to reconfig.


	if ( checkResources_tid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer(checkResources_tid);
		checkResources_tid = TIMER_UNSET;
	}
	checkResources_tid = daemonCore->Register_Timer( 1, 60,
												(TimerHandler)&checkResources,
												"checkResources", NULL );

	contactScheddDelay = -1;
	tmp = param("GRIDMANAGER_CONTACT_SCHEDD_DELAY");
	if ( tmp ) {
		contactScheddDelay = atoi(tmp);
		free(tmp);
	} 
	if ( contactScheddDelay < 0 ) {
		contactScheddDelay = 5; // default delay = 5 seconds
	}

	tmp_int = -1;
	tmp = param("GRIDMANAGER_JOB_PROBE_INTERVAL");
	if ( tmp ) {
		tmp_int = atoi(tmp);
		free(tmp);
	}
	if ( tmp_int < 0 ) {
		tmp_int = 5 * 60; // default interval is 5 minutes
	}
	GlobusJob::setProbeInterval( tmp_int );

	tmp_int = -1;
	tmp = param("GRIDMANAGER_RESOURCE_PROBE_INTERVAL");
	if ( tmp ) {
		tmp_int = atoi(tmp);
		free(tmp);
	}
	if ( tmp_int < 0 ) {
		tmp_int = 5 * 60; // default interval is 5 minutes
	}
	GlobusResource::setProbeInterval( tmp_int );

	int max_pending_submits = -1;
	tmp = param("GRIDMANAGER_MAX_PENDING_SUBMITS");
	if ( tmp ) {
		max_pending_submits = atoi(tmp);
		free(tmp);
	}
	if ( max_pending_submits < 0 ) {
		max_pending_submits = 5; // default limit is 5
	}
	GlobusResource::setSubmitLimit( max_pending_submits );

	tmp_int = -1;
	tmp = param("GRIDMANAGER_GAHP_CALL_TIMEOUT");
	if ( tmp ) {
		tmp_int = atoi(tmp);
		free(tmp);
	}
	if ( tmp_int < 0 ) {
		tmp_int = 5 * 60; // default interval is 5 minutes
	}
	GlobusJob::setGahpCallTimeout( tmp_int );
	GlobusResource::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	GlobusJob::setConnectFailureRetry( tmp_int );

	checkProxy_interval = -1;
	tmp = param("GRIDMANAGER_CHECKPROXY_INTERVAL");
	if ( tmp ) {
		checkProxy_interval = atoi(tmp);
		free(tmp);
	} 
	if ( checkProxy_interval < 0 ) {
		checkProxy_interval = 10 * 60 ; // default interval = 10 minutes
	}

	minProxy_time = -1;
	tmp = param("GRIDMANAGER_MINIMUM_PROXY_TIME");
	if ( tmp ) {
		minProxy_time = atoi(tmp);
		free(tmp);
	} 
	if ( minProxy_time < 0 ) {
		minProxy_time = 3 * 60 ; // default = 3 minutes
	}

	syncJobIO_interval = -1;
	tmp = param("GRIDMANAGER_SYNC_JOB_IO_INTERVAL");
	if ( tmp ) {
		syncJobIO_interval = atoi(tmp);
		free(tmp);
	}
	if ( syncJobIO_interval < 0 ) {
		syncJobIO_interval = 5 * 60; // default interval = 5 minutes
	}

	int max_requests = 50;
	tmp = param("GRIDMANAGER_MAX_PENDING_REQUESTS");
	if ( tmp ) {
		max_requests = atoi(tmp);
		free(tmp);
		if ( max_requests < 1 ) {
			max_requests = 50;
		}
		if ( max_requests < max_pending_submits * 5 ) {
		        max_requests = max_pending_submits * 5;
		}
	}
	GahpMain.setMaxPendingRequests(max_requests);

	// Always check the proxy on a reconfig.
	checkProxy();

	// Always sync IO on a reconfig
	syncJobIO();
}

int
checkProxy()
{
	time_t now = 	time(NULL);
	ASSERT(X509Proxy);
	int seconds_left = x509_proxy_seconds_until_expire(X509Proxy);
	time_t current_expiration_time = now + seconds_left;
	static time_t last_expiration_time = 0;

	if ( seconds_left < 0 ) {
		// Proxy file is gone. Since the GASS needs the proxy file for
		// new connections, we should treat this as an expired proxy.
		seconds_left = 0;
		current_expiration_time = now;
		Proxy_Expiration_Time = current_expiration_time;
	}

	if ( Proxy_Expiration_Time == 0 ) {
		// First time through....
		Proxy_Expiration_Time = current_expiration_time;
		dprintf(D_ALWAYS,
			"Condor-G proxy cert valid for %s\n",format_time(seconds_left));
	}

	if ( last_expiration_time == 0 ) {
		// First time through....
		last_expiration_time = Proxy_Expiration_Time;
	}

	// If our proxy expired in the past, make it seem like it expired
	// right now so we don't have to deal with time_t negative overflows
	if ( now > Proxy_Expiration_Time ) {
		Proxy_Expiration_Time = now;
	}
	if ( now > last_expiration_time ) {
		last_expiration_time = now;
	}

	// Check if we have a refreshed proxy
	if ( (current_expiration_time > Proxy_Expiration_Time) &&
		 (current_expiration_time > last_expiration_time) ) 
	{
		// We have a refreshed proxy!
		dprintf(D_FULLDEBUG,"New proxy found, valid for %s\n",
				format_time(seconds_left));
		last_expiration_time = current_expiration_time;

		// Try to refresh the proxy cached in the gahp server
		int res = GahpMain.globus_gram_client_set_credentials( X509Proxy );
		if ( res == 0  ) {
			// Success!  Gahp server has refreshed the proxy
			Proxy_Expiration_Time = current_expiration_time;
			// signal every job, in case some were waiting for a new proxy
			GlobusJob *next_job;
			JobsByProcID.startIterations();
			while ( JobsByProcID.iterate( next_job ) != 0 ) {
				next_job->SetEvaluateState();
			}
		} else {
			// Failed to refresh proxy in the gahp server
			dprintf(D_FULLDEBUG,"Failed to reset credentials to new proxy\n");
		}
	}

	// Verify our proxy is longer than the minimum allowed
	if ((Proxy_Expiration_Time - now) <= minProxy_time) 
	{
		// Proxy is either already expired, or will expire in
		// a very short period of time.
		// Write something out to the log and send a signal to shutdown.
		// The schedd will try to restart us every few minutes.
		// TODO: should email the user somewhere around here....

		char *formated_minproxy = strdup(format_time(minProxy_time));
		dprintf(D_ALWAYS,
			"ERROR: Condor-G proxy expiring; "
			"valid for %s - minimum allowed is %s\n",
			format_time((int)(Proxy_Expiration_Time - now) ), 
			formated_minproxy);
		free(formated_minproxy);

			// Shutdown with haste!
		daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );  
		return FALSE;
	}

	// Setup timer to automatically check it next time.  We want the
	// timer to go off either at the next user-specified interval,
	// or right before our credentials expire, whichever is less.
	int interval = MIN(checkProxy_interval, 
						Proxy_Expiration_Time - now - minProxy_time);

	if ( interval ) {
		if ( checkProxy_tid != TIMER_UNSET ) {
			daemonCore->Reset_Timer(checkProxy_tid,interval);
		} else {
			checkProxy_tid = daemonCore->Register_Timer( interval,
												(TimerHandler)&checkProxy,
												"checkProxy", NULL );
		}
	} else {
		// interval is 0, cancel any timer if we have one
		if (checkProxy_tid != TIMER_UNSET ) {
			daemonCore->Cancel_Timer(checkProxy_tid);
			checkProxy_tid = TIMER_UNSET;
		}
	}

	return TRUE;
}

int
syncJobIO()
{
	GlobusJob *next_job;

	JobsByProcID.startIterations();

	while ( JobsByProcID.iterate( next_job ) != 0 ) {
		daemonCore->Register_Timer( 0, (TimerHandlercpp)&GlobusJob::syncIO,
									"syncIO", (Service *)next_job );
	}

	if ( syncJobIO_interval ) {
		if ( syncJobIO_tid != TIMER_UNSET ) {
			daemonCore->Reset_Timer( syncJobIO_tid, syncJobIO_interval);
		} else {
			syncJobIO_tid = daemonCore->Register_Timer( syncJobIO_interval,
												(TimerHandler)&syncJobIO,
												"syncJobIO", NULL );
		}
	} else {
		// syncJobIO_interval is 0, cancel any timer if we have one
		if (syncJobIO_tid != TIMER_UNSET ) {
			daemonCore->Cancel_Timer(syncJobIO_tid);
			syncJobIO_tid = TIMER_UNSET;
		}
	}

	return TRUE;
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
			num_down_resources++;
		}
	}

	dprintf(D_FULLDEBUG,"checkResources(): %d resources, %d are down\n",
		num_resources, num_down_resources);

	if ( num_resources > 0 && num_resources == num_down_resources ) {
			// all resources are down!  see for how long...
		time_t downfor = time(NULL) - most_recent_time;
		int max_downtime = param_integer(
							"GRIDMANAGER_MAX_TIME_DOWN_RESOURCES",
							15 * 60 );	// 15 minutes default
		if ( downfor > max_downtime ) {
			dprintf(D_ALWAYS,
				"All resources down for more than %d secs -- killing GAHP\n",
				max_downtime);
			if ( GahpMain.getPid() > 0 ) {
				daemonCore->Send_Signal(GahpMain.getPid(),SIGKILL);
			} else {
				dprintf(D_ALWAYS,"ERROR - no gahp found???\n");
			}
		} else {
			dprintf(D_ALWAYS,"All resources down for %d seconds!\n",
				downfor);
		}
	}

	return TRUE;
}


int
doContactSchedd()
{
	Qmgr_connection *schedd;
	ScheddUpdateAction *curr_action;
	GlobusJob *curr_job;
	ClassAd *next_ad;
	char expr_buf[_POSIX_PATH_MAX];
	char owner_buf[_POSIX_PATH_MAX];

	dprintf(D_FULLDEBUG,"in doContactSchedd()\n");

	contactScheddTid = TIMER_UNSET;

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
			WriteTerminateEventToUserLog( curr_job->ad );
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
		GetAttributeInt( curr_job->procID.cluster,
						 curr_job->procID.proc,
						 ATTR_JOB_STATUS, &job_status_schedd );

		// If the job is marked as REMOVED or HELD on the schedd, don't
		// change it. Instead, modify our state to match it.
		if ( job_status_schedd == REMOVED || job_status_schedd == HELD ) {
			curr_job->UpdateCondorState( job_status_schedd );
			curr_job->ad->SetDirtyFlag( ATTR_JOB_STATUS, false );
			curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON, false );
		} else if ( curr_action->actions & UA_HOLD_JOB ) {
			char *reason;
			if ( GetAttributeStringNew( curr_job->procID.cluster,
										curr_job->procID.proc,
										ATTR_RELEASE_REASON, &reason )
				 >= 0 ) {
				SetAttributeString( curr_job->procID.cluster,
									curr_job->procID.proc,
									ATTR_LAST_RELEASE_REASON, reason );
			}
			free( reason );
			DeleteAttribute(curr_job->procID.cluster,
							curr_job->procID.proc,
							ATTR_RELEASE_REASON );
			SetAttributeInt( curr_job->procID.cluster, 
							 curr_job->procID.proc,
				ATTR_ENTERED_CURRENT_STATUS, (int)time(0) );
			int sys_holds = 0;
			GetAttributeInt(curr_job->procID.cluster, 
						curr_job->procID.proc, ATTR_NUM_SYSTEM_HOLDS,
						&sys_holds);
			sys_holds++;
			SetAttributeInt(curr_job->procID.cluster, 
						curr_job->procID.proc, ATTR_NUM_SYSTEM_HOLDS,
						sys_holds);
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
			} else {
					// Finally, if we are just changing from one unintersting state
					// to another, update the ATTR_ENTERED_CURRENT_STATUS time.
				if ( curr_job->condorState != job_status_schedd ) {
					SetAttributeInt( curr_job->procID.cluster,
									 curr_job->procID.proc, 
						ATTR_ENTERED_CURRENT_STATUS, (int)time(0) );
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
			GetAttributeFloat(curr_job->procID.cluster, curr_job->procID.proc,
							  ATTR_JOB_REMOTE_WALL_CLOCK,&accum_time);
			accum_time += (float)( time(NULL) - shadowBirthdate );
			SetAttributeFloat(curr_job->procID.cluster, curr_job->procID.proc,
							  ATTR_JOB_REMOTE_WALL_CLOCK,accum_time);
			DeleteAttribute(curr_job->procID.cluster, curr_job->procID.proc,
							ATTR_JOB_WALL_CLOCK_CKPT);
			// ATTR_SHADOW_BIRTHDATE on the schedd will be updated below
			curr_job->UpdateJobAdInt( ATTR_SHADOW_BIRTHDATE, 0 );

		}

dprintf(D_FULLDEBUG,"Updating classad values:\n");
		char attr_name[1024];
		char attr_value[1024];
		ExprTree *expr;
		curr_job->ad->ResetExpr();
		while ( (expr = curr_job->ad->NextDirtyExpr()) != NULL ) {
			attr_name[0] = '\0';
			attr_value[0] = '\0';
			expr->LArg()->PrintToStr(attr_name);
			expr->RArg()->PrintToStr(attr_value);

dprintf(D_FULLDEBUG,"   %s = %s\n",attr_name,attr_value);
			SetAttribute( curr_job->procID.cluster,
						  curr_job->procID.proc,
						  attr_name,
						  attr_value);
		}

		curr_job->ad->ClearAllDirtyFlags();

		if ( curr_action->actions & UA_FORGET_JOB ) {
			SetAttribute( curr_job->procID.cluster,
						  curr_job->procID.proc,
						  ATTR_JOB_MANAGED,
						  "FALSE" );
		}

		if ( curr_action->actions & UA_DELETE_FROM_SCHEDD ) {
			CloseConnection();
			BeginTransaction();
			DestroyProc(curr_job->procID.cluster,
						curr_job->procID.proc);
		}
	}


	// setup for AddJobs and RemoveJobs
	if(useDefaultProxy == false) {
		sprintf(owner_buf, "%s == \"%s\" && %s =?= \"%s\" ", ATTR_OWNER, Owner,
				ATTR_X509_USER_PROXY, X509Proxy);
	} else {
		sprintf(owner_buf, "%s == \"%s\" && %s =?= UNDEFINED ", ATTR_OWNER, 
						Owner, ATTR_X509_USER_PROXY);
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
		// currently being managed.
		// If JobManaged is undefined, equate it with false.
		if ( firstScheddContact ) {
//			sprintf( expr_buf, "%s && %s == %d && !(%s == %d && %s =!= TRUE)",
			sprintf( expr_buf, "%s && %s == %d && (%s == %d && %s =!= TRUE) == FALSE",
					 owner_buf, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GLOBUS,
					 ATTR_JOB_STATUS, HELD, ATTR_JOB_MANAGED );
		} else {
			sprintf( expr_buf, "%s && %s == %d && %s != %d && %s =!= TRUE",
					 owner_buf, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GLOBUS,
					 ATTR_JOB_STATUS, HELD, ATTR_JOB_MANAGED );
		}

		next_ad = GetNextJobByConstraint( expr_buf, 1 );
		while ( next_ad != NULL ) {
			PROC_ID procID;
			GlobusJob *old_job;

			next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );

			if ( JobsByProcID.lookup( procID, old_job ) != 0 ) {

				int rc;
				char resource_name[800];
				GlobusResource *resource = NULL;

				resource_name[0] = '\0';
				next_ad->LookupString( ATTR_GLOBUS_RESOURCE, resource_name );

				if ( resource_name[0] == '\0' ) {

					const char *hold_reason =
						"GlobusResource is not set in the job ad";	
					char buffer[128];
					char *reason;
					dprintf( D_ALWAYS, "Job %d.%d has no Globus resource name!\n",
							 procID.cluster, procID.proc );
					// No GlobusResource, so put the job on hold
					SetAttributeInt( procID.cluster,
									 procID.proc,
									 ATTR_JOB_STATUS,
									 HELD );
					if ( GetAttributeStringNew( procID.cluster, procID.proc,
												ATTR_RELEASE_REASON, &reason )
						 >= 0 ) {
						SetAttributeString( procID.cluster, procID.proc,
											ATTR_LAST_RELEASE_REASON, reason );
					}
					free( reason );
					DeleteAttribute( procID.cluster,
									 procID.proc,
									 ATTR_RELEASE_REASON );
					SetAttributeString( procID.cluster,
										procID.proc,
										ATTR_HOLD_REASON,
										hold_reason );
					sprintf( buffer, "%s = \"%s\"", ATTR_HOLD_REASON,
							 hold_reason );
					next_ad->InsertOrUpdate( buffer );
					WriteHoldEventToUserLog( next_ad );

					delete next_ad;

				} else {

					rc = ResourcesByName.lookup( HashKey( resource_name ),
												  resource );

					if ( rc != 0 ) {
						resource = new GlobusResource( resource_name );
						ASSERT(resource);
						ResourcesByName.insert( HashKey( resource_name ),
												 resource );
					} else {
						ASSERT(resource);
					}

					GlobusJob *new_job = new GlobusJob( next_ad, resource );
					ASSERT(new_job);
					new_job->SetEvaluateState();
					JobsByProcID.insert( new_job->procID, new_job );
					num_ads++;

					SetAttribute( new_job->procID.cluster,
								  new_job->procID.proc,
								  ATTR_JOB_MANAGED,
								  "TRUE" );

				}

			} else {

				// We already know about this job, skip
				delete next_ad;

			}

			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}

		dprintf(D_FULLDEBUG,"Fetched %d new job ads from schedd\n",num_ads);

		firstScheddContact = false;
		addJobsSignaled = false;
	}
	/////////////////////////////////////////////////////

	// RemoveJobs
	/////////////////////////////////////////////////////
	if ( removeJobsSignaled ) {
		int num_ads = 0;

		dprintf( D_FULLDEBUG, "querying for removed/held jobs\n" );

		// Grab jobs marked as REMOVED or marked as HELD that we haven't
		// previously indicated that we're done with (by setting JobManaged
		// to FALSE. If JobManaged is undefined, equate it with false.
		sprintf( expr_buf, "%s && %s == %d && (%s == %d || (%s == %d && %s =?= TRUE))",
				 owner_buf, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GLOBUS,
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

				next_job->UpdateCondorState( curr_status );
				num_ads++;

				// Save the remove reason in our local copy of the job ad
				// so that we can write it in the abort log event.
				if ( curr_status == REMOVED ) {
					int rc;
					char *remove_reason;
					rc = GetAttributeStringNew( procID.cluster,
												procID.proc,
												ATTR_REMOVE_REASON,
												&remove_reason );
					if ( rc == 0 ) {
						next_job->UpdateJobAdString( ATTR_REMOVE_REASON,
													 remove_reason );
					}
					free( remove_reason );
				}

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
				DestroyProc( procID.cluster, procID.proc );

			} else {

				dprintf( D_ALWAYS, "Don't know about held job %d.%d. "
						 "Ignoring it\n",
						 procID.cluster, procID.proc );

			}

			delete next_ad;
			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}

		dprintf(D_FULLDEBUG,"Fetched %d job ads from schedd\n",num_ads);

		removeJobsSignaled = false;
	}
	/////////////////////////////////////////////////////

	DisconnectQ( schedd );

	// Wake up jobs that had schedd updates pending and delete job
	// objects that wanted to be deleted
	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		curr_job = curr_action->job;

		if ( curr_action->actions & UA_FORGET_JOB ) {
			if ( curr_job->jobContact != NULL ) {
				JobsByContact.remove( HashKey( curr_job->jobContact ) );
			}
			JobsByProcID.remove( curr_job->procID );
			GlobusResource *resource = curr_job->GetResource();
			delete curr_job;

			if ( resource->IsEmpty() ) {
				ResourcesByName.remove( HashKey( resource->ResourceName() ) );
				delete resource;
			}

			delete curr_action;
		} else if ( curr_action->request_id != 0 ) {
			completedScheddUpdates.insert( curr_job->procID, curr_action );
			curr_job->SetEvaluateState();
		} else {
			delete curr_action;
		}

	}

	pendingScheddUpdates.clear();

	// Check if we have any jobs left to manage. If not, exit.
	if ( JobsByProcID.getNumElements() == 0 ) {
		dprintf( D_ALWAYS, "No jobs left, shutting down\n" );
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}

	lastContactSchedd = time(NULL);

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
	rc = JobsByContact.lookup( HashKey( orphan->job_contact ), this_job );
	if ( rc != 0 || this_job == NULL ) {
		dprintf( D_ALWAYS, 
			"orphanCallbackHandler: Can't find record for globus job with "
			"contact %s on globus event %d, ignoring\n", orphan->job_contact,
			 orphan->state );
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
	rc = JobsByContact.lookup( HashKey( job_contact ), this_job );
	if ( rc != 0 || this_job == NULL ) {
		dprintf( D_ALWAYS, 
			"gramCallbackHandler: Can't find record for globus job with "
			"contact %s on globus event %d, delaying\n", job_contact, state );
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

	userLogFile[0] = '\0';
	job_ad->LookupString( ATTR_ULOG_FILE, userLogFile );
	if ( userLogFile[0] == '\0' ) {
		// User doesn't want a log
		return NULL;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	UserLog *ULog = new UserLog();
	ULog->initialize(Owner, userLogFile, cluster, proc, 0);
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

bool
WriteTerminateEventToUserLog( ClassAd *job_ad )
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

	// Globus doesn't tell us how the job exited, so we'll just assume it
	// exited normally.
	event.normal = true;
	event.returnValue = 0;

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
	char holdReason[256];
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

	holdReason[0] = '\0';
	job_ad->LookupString( ATTR_HOLD_REASON, holdReason,
						   sizeof(holdReason) - 1 );

	event.setReason( holdReason );

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
