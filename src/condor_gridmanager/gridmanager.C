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

#include "globus_gram_client.h"
#include "globus_gss_assist.h"

#include "gridmanager.h"

#include "sslutils.h"	// for proxy_get_filenames

extern gss_cred_id_t globus_i_gram_http_credential;

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

HashTable <PROC_ID, ScheddUpdateAction *> pendingScheddUpdates( HASH_TABLE_SIZE,
																procIDHash );
HashTable <PROC_ID, ScheddUpdateAction *> completedScheddUpdates( HASH_TABLE_SIZE,
																  procIDHash );
bool addJobsSignaled = false;
bool removeJobsSignaled = false;
int contactScheddTid = TIMER_UNSET;

List<Service *> ObjectDeleteList;

char *gramCallbackContact = NULL;
char *ScheddAddr = NULL;
char *X509Proxy = NULL;
bool useDefaultProxy = true;

HashTable <HashKey, GlobusJob *> JobsByContact( HASH_TABLE_SIZE,
												hashFunction );
HashTable <PROC_ID, GlobusJob *> JobsByProcID( HASH_TABLE_SIZE,
											   procIDHash );
HashTable <HashKey, GlobusResource *> ResourcesByName( HASH_TABLE_SIZE,
													   hashFunction );

bool grabAllJobs = true;

char *Owner = NULL;

int checkProxy_tid = TIMER_UNSET;
int checkProxy_interval;
int minProxy_time;

time_t Proxy_Expiration_Time = 0;
time_t Initial_Proxy_Expiration_Time = 0;

int RequestContactSchedd();
int doContactSchedd();

// handlers
int ADD_JOBS_signalHandler( int );
int REMOVE_JOBS_signalHandler( int );
int checkProxy();

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
template class List<Service *>;
template class Item<Service *>;


int 
addScheddUpdateAction( GlobusJob *job, int actions, int request_id )
{
	ScheddUpdateAction *curr_action;

	if ( completedScheddUpdates.lookup( job->procID, curr_action ) == 0 ) {
		ASSERT( curr_action->job == job );

		if ( request_id == curr_action->request_id && request_id != 0 ) {
			completedScheddUpdates.remove( job->procID );
			delete curr_action;
			// return done;
		} else {
			completedScheddUpdates.remove( job->procID );
			delete curr_action;
		}
	}

	if ( pendingScheddUpdates.lookup( job->procID, curr_action ) == 0 ) {
		ASSERT( curr_action->job == job );

		curr_action->actions |= actions;
	} else {
		curr_action = new ScheddUpdateAction;
		curr_action->job = job;
		curr_action->actions = actions;
		curr_action->request_id = request_id;

		pendingScheddUpdates.insert( job->procID, curr_action );
		RequestContactSchedd();
	}

	// return pending;

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
		contactScheddTid = daemonCore->Register_Timer( CONTACT_SCHEDD_DELAY,
												(TimerHandler)&doContactSchedd,
												"doContactSchedd", NULL );
	}
}

void
rehashJobContact( GlobusJob *job, const char *old_contact,
				  const char *new_contact )
{
	if ( old_contact ) {
		gridmanager.JobsByContact->remove(HashKey(old_contact));
	}
	if ( new_contact ) {
		gridmanager.JobsByContact->insert(HashKey(new_contact), job);
	}
}

void
DeleteJob( GlobusJob *job ) {
	removeScheddUpdateAction( job );
	if ( job->jobContact != NULL ) {
		gridmanager.JobsByContact->remove( HashKey( job->jobContact ) );
	}
	gridmanager.JobsByProcID->remove( job->procID );

	ObjectDeleteList.Delete( (service *)job );
	ObjectDeleteList.Append( (Service *)job );
	daemonCore->Send_Signal( daemonCore->getpid(), GRIDMAN_DELETE_OBJS );
}

void
DeleteResource( GlobusResource *resource ) {
	ASSERT( resource->IsEmpty() );
	
	gridmanager.ResourcesByName->remove( HashKey( resource->ResourceName() ) );

	ObjectDeleteList.Delete( (Service *)resource );
	ObjectDeleteList.Append( (Service *)resource );
	daemonCore->Send_Signal( daemonCore->getpid(), GRIDMAN_DELETE_OBJS );
}

int
DELETE_OBJS_signalHandler( Service *srvc, int signal )
{
	Service *curr_obj;

	ObjectDeleteList.Rewind();

	while ( ObjectDeleteList.Next( curr_obj ) ) {
		delete curr_obj;
		ObjectDeleteList.DeleteCurrent();
	}

	return TRUE;
}


void
Init()
{
	int rc;
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

	// Find the location of our proxy file, if we don't already
	// know (from the command line)
	if (X509Proxy == NULL) {
		proxy_get_filenames(1, NULL, NULL, &X509Proxy, NULL, NULL);
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

	daemonCore->Register_Signal( GRIDMAN_DELETE_OBJS, "DeleteObjs",
								 (SignalHandler)&DELETE_OBJS_signalHandler,
								 "DELETE_OBJS_signalHandler" );

	Reconfig();
}

void
Reconfig()
{
	char *tmp = NULL;

	// This method is called both at startup [from method Init()], and
	// when we are asked to reconfig.

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

	// Always check the proxy on a reconfig.
	checkProxy();

}

int
checkProxy()
{
	time_t now = 	time(NULL);
	ASSERT(X509Proxy);
	int seconds_left = x509_proxy_seconds_until_expire(X509Proxy);
	time_t current_expiration_time = now + seconds_left;

	if ( seconds_left < 0 ) {
		// Proxy file is gone.  Set things so the below logic
		// will still do the right thing when our current proxy
		// goes away, but will never try to refresh with the
		// non-existant proxy.
		seconds_left = 0;
		current_expiration_time = now;
	}

	if ( Proxy_Expiration_Time == 0 ) {
		// First time through....
		Proxy_Expiration_Time = current_expiration_time;
		Initial_Proxy_Expiration_Time = current_expiration_time;
		dprintf(D_ALWAYS,
			"Condor-G proxy cert valid for %s\n",format_time(seconds_left));
	}

	// If our proxy expired in the past, make it seem like it expired
	// right now so we don't have to deal with time_t negative overflows
	if ( now > Proxy_Expiration_Time ) {
		Proxy_Expiration_Time = now;
	}
	if ( now > Initial_Proxy_Expiration_Time ) {
		Initial_Proxy_Expiration_Time = now;
	}

	// Check if we have a refreshed proxy
	if ( current_expiration_time > Proxy_Expiration_Time ) {
		// We have a refreshed proxy!

		/* Update our credential context for GRAM.  This will allow new
		 * jobmanagers which we subsequently startup to have the refreshed
		 * proxy.  Aren't we cool?  
		 */
    	OM_uint32	major_status;
    	OM_uint32 	minor_status;
		gss_cred_id_t old_gram_credential = globus_i_gram_http_credential;
		static bool first_cred_refresh = true;
		/* -- Now, acquire our new context.  We care about errors
		 * here, but we have no good way of handling them.  Dooohh! */
    	major_status = globus_gss_assist_acquire_cred(&minor_status,
                        GSS_C_BOTH,
                        &globus_i_gram_http_credential);
    	if (major_status != GSS_S_COMPLETE)
    	{
			// If we failed, perhaps the proxy file was being changed
			// right when we were reading it.  Bad luck!  So try
			// to sleep for one second and try again.
			sleep(1);
    		major_status = globus_gss_assist_acquire_cred(&minor_status,
                        GSS_C_BOTH,
                        &globus_i_gram_http_credential);
    		if (major_status != GSS_S_COMPLETE)
			{
				// We cannot read in the new proxy... revert to the old.
				globus_i_gram_http_credential = old_gram_credential;
			}
		}

		if ( major_status == GSS_S_COMPLETE ) {
			// Success!  We have read in the new context.

			/* -- First, release our old context.  Don't care about errors. 
			 * Do not release the first time through, because the 
			 * initial context is in use by all our listening sockets. */
			if ((old_gram_credential != GSS_C_NO_CREDENTIAL) &&
				(!first_cred_refresh))
			{
				OM_uint32 minor_status; 
				gss_release_cred(&minor_status,&old_gram_credential); 
				old_gram_credential = GSS_C_NO_CREDENTIAL;
			}

			// Print out a message
			int time_left = Proxy_Expiration_Time - now;
			if ( time_left < 0 ) {
				time_left = 0;
			}
			char *oldtime = strdup(format_time(time_left));
			char *newtime = strdup(format_time(seconds_left));
			dprintf(D_ALWAYS,
			   "Using refreshed cert - old valid for %s (%d), "
			   "new valid for %s (%d)\n", 
			   oldtime,time_left,newtime,seconds_left);
			free(oldtime);
			free(newtime);

			// Update some variables
			Proxy_Expiration_Time = current_expiration_time;
			first_cred_refresh = false;
		}
	
	}  // done handling a refreshed proxy

	// Verify our Initial Proxy is longer than the minimum allowed
	if ((Initial_Proxy_Expiration_Time - now) <= minProxy_time) 
	{
		// Initial Proxy is either already expired, or will expire in
		// a very short period of time.
		// Write something out to the log and send a signal to shutdown.
		// The schedd will try to restart us every few minutes.
		// TODO: should email the user somewhere around here....

		char *formated_minproxy = strdup(format_time(minProxy_time));
		dprintf(D_ALWAYS,
			"ERROR: Condor-G proxy expiring; valid for %s - minimum allowed is %s\n",
			X509Proxy, format_time((int)(Initial_Proxy_Expiration_Time - now) ), 
			formated_minproxy);
		free(formated_minproxy);

			// Shutdown with haste!
		daemonCore->Send_Signal(daemonCore->getpid(),DC_SIGQUIT);  
		return FALSE;
	}


	// Setup timer to automatically check it next time.  We want the
	// timer to go off either at the next user-specified interval,
	// or right before our credentials expire, whichever is less.
	int interval1 = MIN(checkProxy_interval, 
						Proxy_Expiration_Time - now - minProxy_time);
 	int interval = MIN(interval1, 
						Initial_Proxy_Expiration_Time - now - minProxy_time);	
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
ADD_JOBS_signalHandler( int signal )
{
	dprintf(D_FULLDEBUG,"Received ADD_JOBS signal\n");

	if ( !addJobsSignalled ) {
		RequestContactSchedd();
		addJobsSignalled = true;
	}

	return TRUE;
}

int
REMOVE_JOBS_signalHandler( int signal )
{
	dprintf(D_FULLDEBUG,"Received REMOVE_JOBS signal\n");

	if ( !removeJobsSignalled ) {
		RequestContactSchedd();
		removeJobsSignalled = true;
	}

	return TRUE;
}

int
doContactSchedd()
{
	int rc;
	int cluster_id;
	int proc_id;
	char buf[1024];
	Qmgr_connection *schedd;
	SceddUpdateAction *curr_action;
	GlobusJob *curr_job;
	ClassAd *next_ad;
	char expr_buf[_POSIX_PATH_MAX];
	char owner_buf[_POSIX_PATH_MAX];

	dprintf(D_FULLDEBUG,"in doContactSchedd()\n");

	contactScheddTid = TIMER_UNSET;

	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		curr_job = curr_action->job;

		if ( curr_actions->actions & UA_UPDATE_CONDOR_STATE ||
			 curr_actions->actions & UA_UPDATE_GLOBUS_STATE ||
			 curr_actions->actions & UA_UPDATE_CONTACT_STRING ||
			 curr_actions->actions & UA_DELETE_FROM_SCHEDD ) {
			contact_schedd = true;
		}
		if ( curr_actions->actions & UA_LOG_SUBMIT_EVENT ) {
			WriteGlobusSubmitEventToUserLog( curr_job );
		}
		if ( curr_actions->actions & UA_LOG_EXECUTE_EVENT ) {
			WriteGlobusExecuteEventToUserLog( curr_job );
		}
		if ( curr_actions->actions & UA_LOG_SUBMIT_FAILED_EVENT ) {
			WriteGlobusSubmitFailedToUserLog( curr_job );
		}
		if ( curr_actions->actions & UA_LOG_TERMINATE_EVENT ) {
			WriteTerminateToUserLog( curr_job );
		}
		if ( curr_actions->actions & UA_LOG_ABORT_EVENT ) {
			WriteAbortToUserLog( curr_job );
		}
		if ( curr_actions->actions & UA_LOG_EVICT_EVENT ) {
			WriteEvictToUserLog( curr_job );
		}

	}

	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, false );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd!\n");
		// Should we be retrying infinitely?
		RequestContactSchedd();
		return TRUE;
	}

	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		curr_job = curr_action->job;

		if ( curr_actions->actions & UA_UPDATE_CONDOR_STATE ) {
			int curr_status;
			GetAttributeInt( curr_job->procID.cluster,
							 curr_job->procID.proc,
							 ATTR_JOB_STATUS, &curr_status );

			if ( curr_status != REMOVED ) {
				// TODO: What about HELD jobs?
				SetAttributeInt( curr_job->procID.cluster,
								 curr_job->procID.proc,
								 ATTR_JOB_STATUS, curr_job->condorState );
			} else {
				curr_job->UpdateCondorState( curr_status );
			}
		}

		if ( curr_actions->actions & UA_UPDATE_GLOBUS_STATE ) {
			SetAttributeInt( curr_job->procID.cluster,
							 curr_job->procID.proc,
							 ATTR_GLOBUS_STATUS, curr_job->globusState );
		}

		if ( curr_actions->actions & UA_UPDATE_CONTACT_STRING ) {
			SetAttributeString( curr_job->procID.cluster,
								curr_job->procID.proc,
								ATTR_GLOBUS_CONTACT_STRING,
								curr_job->jobContact );
		}

		if ( curr_actions->actions & UA_DELETE_FROM_SCHEDD ) {
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
	if ( addJobsSignaled ) {
		int num_ads = 0;

		// Make sure we grab all Globus Universe jobs when we first start up
		// in case we're recovering from a shutdown/meltdown.
		if ( grabAllJobs ) {
			sprintf( expr_buf, "%s  && %s == %d",
					 owner_buf, ATTR_JOB_UNIVERSE, GLOBUS_UNIVERSE );
		} else {
			sprintf( expr_buf, "%s  && %s == %d && %s == %d",
					 owner_buf, ATTR_JOB_UNIVERSE, GLOBUS_UNIVERSE,
					 ATTR_GLOBUS_STATUS, G_UNSUBMITTED );
		}

		next_ad = GetNextJobByConstraint( expr_buf, 1 );
		while ( next_ad != NULL ) {
			PROC_ID procID;
			GlobusJob *old_job;
			ClassAd *old_ad;

			next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );

			if ( JobsByProcID->lookup( procID, old_job ) != 0 ) {

				int rc;
				char resource_name[200];
				GlobusResource *resource;

				resource_name[0] = '\0';
				next_ad->LookupString( ATTR_GLOBUS_RESOURCE, resource_name );

				if ( resource_name[0] == '\0' ) {

					dprintf( D_ALWAYS, "Job %d.%d has no Globus resource name!\n",
							 procID.cluster, procID.proc );
					// TODO: What do we do about this job?

				} else {

					rc = ResourcesByName->lookup( HashKey( resource name ),
												  resource );

					if ( rc != 0 ) {
						resource = new GlobusResource( resource_name );
						ASSERT(resource);
						ResourcesByName->insert( HashKey( resource_name ),
												 resource );
					} else {
						ASSERT(resource);
					}

					GlobusJob *new_job = new GlobusJob( next_ad, resource );
					ASSERT(new_job);
					JobsByProcID->insert( new_job->procID, new_job );
					num_ads++;

				}

			}

			delete next_ad;

			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}

		dprintf(D_FULLDEBUG,"Fetched %d new job ads from schedd\n",num_ads);

		grabAllJobs = false;
		addJobsSignalled = false;
	}
	/////////////////////////////////////////////////////

	// RemoveJobs
	/////////////////////////////////////////////////////
	if ( removeJobsSignalled ) {
		int num_ads = 0;

		sprintf( expr_buf, "%s && %s == %d && (%s == %d || $s == %d)",
				 owner_buf, ATTR_JOB_UNIVERSE, GLOBUS_UNIVERSE,
				 ATTR_JOB_STATUS, REMOVED, ATTR_JOB_STATUS, HELD );

		next_ad = GetNextJobByConstraint( expr_buf, 1 );
		while ( next_ad != NULL ) {
			PROC_ID procID;
			GlobusJob *next_job;

			next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );

			if ( JobsByProcID->lookup( procID, next_job ) == 0 ) {
				// Should probably skip jobs we already have marked for removal

				next_job->UpdateCondorState( REMOVED );
				num_ads++;

			} else {

				// If we don't know about the job, remove it immediately
				dprintf( D_ALWAYS, 
						 "Don't know about removed job %d.%d. "
						 "Deleting it immediately\n", procID.cluster,
						 procID.proc );
				DestroyProc( procID.cluster, procID.proc );

			}

			delete next_ad;
			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}

		dprintf(D_FULLDEBUG,"Fetched %d job ads from schedd\n",num_ads);

		removeJobsSignalled = false;
	}
	/////////////////////////////////////////////////////

	DisconnectQ( schedd );

	// Wake up jobs that had schedd updates pending
	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		curr_job = curr_action->job;

		if ( curr_action->request_id != 0 ) {
			completedScheddUpdates.insert( curr_job->procID, curr_action );
			curr_job->SetEvaluateState();
		}

	}

	pendingScheddUpdates.clear();

dprintf(D_FULLDEBUG,"leaving doContactSchedd()\n");
	return TRUE;
}

// Initialize a UserLog object for a given job and return a pointer to
// the UserLog object created.  This object can then be used to write
// events and must be deleted when you're done.  This returns NULL if
// the user didn't want a UserLog, so you must check for NULL before
// using the pointer you get back.
UserLog*
InitializeUserLog( GlobusJob *job )
{
	if( job->userLogFile == NULL ) {
		// User doesn't want a log
		return NULL;
	}

	UserLog *ULog = new UserLog();
	ULog->initialize(Owner, job->userLogFile, job->procID.cluster,
					 job->procID.proc, 0);
	return ULog;
}

bool
WriteExecuteToUserLog( GlobusJob *job )
{
	UserLog *ulog = InitializeUserLog( job );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	dprintf( D_FULLDEBUG, 
		"Writing execute record to user logfile=%s job=%d.%d owner=%s\n",
			 job->userLogFile, job->procID.cluster, job->procID.proc, Owner );

	int hostname_len = strcspn( job->rmContact, ":/" );

	ExecuteEvent event;
	strncpy( event.executeHost, job->rmContact, hostname_len );
	event.executeHost[hostname_len] = '\0';
	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS, "Unable to log ULOG_EXECUTE event\n" );
		return false;
	}

	return true;
}

bool
WriteAbortToUserLog( GlobusJob *job )
{
	UserLog *ulog = InitializeUserLog( job );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	dprintf( D_FULLDEBUG, 
		"Writing abort record to user logfile=%s job=%d.%d owner=%s\n",
			 job->userLogFile, job->procID.cluster, job->procID.proc, Owner );

	JobAbortedEvent event;
	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS, "Unable to log ULOG_ABORT event\n" );
		return false;
	}

	return true;
}

bool
WriteTerminateToUserLog( GlobusJob *job )
{
	UserLog *ulog = InitializeUserLog( job );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	dprintf( D_FULLDEBUG, 
		"Writing terminate record to user logfile=%s job=%d.%d owner=%s\n",
			 job->userLogFile, job->procID.cluster, job->procID.proc, Owner );

	JobTerminatedEvent event;
	event.coreFile[0] = '\0';
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
	event.returnValue = job->exitValue;

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_TERMINATED event\n" );
		return false;
	}

	return true;
}

bool
WriteEvictToUserLog( GlobusJob *job )
{
	UserLog *ulog = InitializeUserLog( job );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	dprintf( D_FULLDEBUG, 
		"Writing evict record to user logfile=%s job=%d.%d owner=%s\n",
			 job->userLogFile, job->procID.cluster, job->procID.proc, Owner );

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
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_EVICTED event\n" );
		return false;
	}

	return true;
}


bool
WriteGlobusSubmitEventToUserLog( GlobusJob *job )
{
	UserLog *ulog = InitializeUserLog( job );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	dprintf( D_FULLDEBUG, 
		"Writing globus submit record to user logfile=%s job=%d.%d owner=%s\n",
			 job->userLogFile, job->procID.cluster, job->procID.proc, Owner );

	GlobusSubmitEvent event;

	event.rmContact =  strnewp(job->rmContact);
	event.jmContact = strnewp(job->jobContact);
	event.restartableJM = job->newJM;

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS, "Unable to log ULOG_GLOBUS_SUBMIT event\n" );
		return false;
	}

	return true;
}

bool
WriteGlobusSubmitFailedEventToUserLog( GlobusJob *job )
{
	const char *unknown = "UNKNOWN";

	UserLog *ulog = InitializeUserLog( job );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	dprintf( D_FULLDEBUG, 
		"Writing submit-failed record to user logfile=%s job=%d.%d owner=%s\n",
			 job->userLogFile, job->procID.cluster, job->procID.proc, Owner );

	GlobusSubmitFailedEvent event;
	
	event.reason =  strnewp(job->errorString());

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS, "Unable to log ULOG_GLOBUS_SUBMIT_FAILED event\n" );
		return false;
	}

	return true;
}

bool
WriteGlobusResourceUpEventToUserLog( GlobusJob *job )
{
	UserLog *ulog = InitializeUserLog( job );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	dprintf( D_FULLDEBUG, 
		"Writing globus up record to user logfile=%s job=%d.%d owner=%s\n",
			 job->userLogFile, job->procID.cluster, job->procID.proc, Owner );

	GlobusResourceUpEvent event;

	event.rmContact =  strnewp(job->rmContact);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS, "Unable to log ULOG_GLOBUS_RESOURCE_UP event\n" );
		return false;
	}

	return true;
}

bool
WriteGlobusResourceDownEventToUserLog( GlobusJob *job )
{
	UserLog *ulog = InitializeUserLog( job );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	dprintf( D_FULLDEBUG, 
		"Writing globus down record to user logfile=%s job=%d.%d owner=%s\n",
			 job->userLogFile, job->procID.cluster, job->procID.proc, Owner );

	GlobusResourceDownEvent event;

	event.rmContact =  strnewp(job->rmContact);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS, "Unable to log ULOG_GLOBUS_RESOURCE_DOWN event\n" );
		return false;
	}

	return true;
}
