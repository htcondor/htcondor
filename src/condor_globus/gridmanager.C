/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#include "globus_gram_client.h"
#include "globus_gss_assist.h"

#include "gm_common.h"
#include "gridmanager.h"

#include "sslutils.h"	// for proxy_get_filenames

extern gss_cred_id_t globus_i_gram_protocol_credential;

#define QMGMT_TIMEOUT 5

#define GLOBUS_POLL_INTERVAL	1
#define JOB_PROBE_INTERVAL		300
static int UPDATE_SCHEDD_DELAY = 5;
#define JM_RESTART_DELAY		60
#define JOB_PROBE_LENGTH		5

#define HASH_TABLE_SIZE			500


extern GridManager gridmanager;

List<JobUpdateEvent> JobUpdateEventQueue;
static bool updateScheddTimerSet = false;

template class HashTable<HashKey, GlobusJob *>;
template class HashBucket<HashKey, GlobusJob *>;
template class HashTable<PROC_ID, GlobusJob *>;
template class HashBucket<PROC_ID, GlobusJob *>;
template class HashTable<HashKey, char *>;
template class HashBucket<HashKey, char *>;
template class List<GlobusJob>;
template class Item<GlobusJob>;
template class List<JobUpdateEvent>;
template class Item<JobUpdateEvent>;
template class List<char *>;
template class Item<char *>;


void 
addJobUpdateEvent( GlobusJob *job, int event, int subtype = 0 )
{
	JobUpdateEvent *curr_event;
	
	// For some event types we 
	// check if there is already the same event on our list and then
	// remove it before adding the new event.
	// For example, if we are adding an event to set the status
	// to DONE, and there is already an event to set it to RUNNING,
	// we may as well remove the earlier event so we do not waste
	// the schedd's time.
#if 0
	if ( event == JOB_UE_DONE ||
		 event == JOB_UE_CANCELED )
	{
		JobUpdateEventQueue.Rewind();
		while ( JobUpdateEventQueue.Next( curr_event ) ) {
			if ( curr_event->job == job && 
				 curr_event->event == JOB_UE_RUNNING )
			{
				JobUpdateEventQueue.DeleteCurrent();
				delete curr_event;
			}
		}
	}
#endif

	JobUpdateEvent *new_event = new JobUpdateEvent;
	new_event->job = job;
	new_event->event = event;
	new_event->subtype = subtype;

	JobUpdateEventQueue.Append( new_event );
	setUpdate();
}

void removeJobUpdateEvents( GlobusJob *job ) {
	JobUpdateEvent *curr_event;

	JobUpdateEventQueue.Rewind();
	while ( JobUpdateEventQueue.Next( curr_event ) ) {
		if ( curr_event->job == job ) {
			JobUpdateEventQueue.DeleteCurrent();
			delete curr_event;
		}
	}
}

void
setUpdate()
{
	if ( updateScheddTimerSet == false 
			&& !JobUpdateEventQueue.IsEmpty() ) 
	{
		daemonCore->Register_Timer( UPDATE_SCHEDD_DELAY,
					(TimerHandlercpp)&GridManager::updateSchedd,
					"updateSchedd", (Service*) &gridmanager );
		updateScheddTimerSet = true;
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


GridManager::GridManager()
{
	grabAllJobs = true;
	updateScheddTimerSet = false;
	Owner = NULL;
	ScheddAddr = NULL;
	X509Proxy = NULL;
	m_cluster = 0;
	m_proc = 0;
	useDefaultProxy = true;
	checkProxy_tid = -1;
	Proxy_Expiration_Time = 0;
	Initial_Proxy_Expiration_Time = 0;

	JobsByContact = new HashTable <HashKey, GlobusJob *>( HASH_TABLE_SIZE,
														  hashFunction );
	JobsByProcID = new HashTable <PROC_ID, GlobusJob *>( HASH_TABLE_SIZE,
														 hashFuncPROC_ID );
	DeadMachines = new HashTable <HashKey, char *>( HASH_TABLE_SIZE,
													hashFunction );
}

GridManager::~GridManager()
{
	if ( Owner ) {
		free( Owner );
		Owner = NULL;
	}
	if ( ScheddAddr ) {
		free( ScheddAddr );
		ScheddAddr = NULL;
	}
	if ( X509Proxy ) {
		free( X509Proxy );
		X509Proxy = NULL;
	}

	if ( JobsByContact != NULL ) {
		delete JobsByContact;
		JobsByContact = NULL;
	}
	if ( JobsByProcID != NULL ) {
		GlobusJob *tmp;
		JobsByProcID->startIterations();
		while( JobsByProcID->iterate( tmp ) ) {
			delete tmp;
		}
		delete JobsByProcID;
		JobsByProcID = NULL;
	}
	if ( DeadMachines != NULL ) {
		delete DeadMachines;
		DeadMachines = NULL;
	}
}


void
GridManager::Init()
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
		proxy_get_filenames(NULL, 1, NULL, NULL, &X509Proxy, NULL, NULL);
	}
	ASSERT(X509Proxy);

	dprintf(D_ALWAYS,"Managing for owner %s - Proxy at %s\n",
		Owner, X509Proxy);

	if ( m_cluster ) {
		dprintf(D_ALWAYS,"Managing for specific job id %d.%d\n",
			m_cluster, m_proc);
		UPDATE_SCHEDD_DELAY = 1;
	}
}

void
GridManager::Register()
{
	daemonCore->Register_Signal( GRIDMAN_ADD_JOBS, "AddJobs",
		(SignalHandlercpp)&GridManager::ADD_JOBS_signalHandler,
		"ADD_JOBS_signalHandler", (Service*)this, WRITE );

	daemonCore->Register_Signal( GRIDMAN_SUBMIT_JOB, "AddJobs",
		(SignalHandlercpp)&GridManager::SUBMIT_JOB_signalHandler,
		"SUBMIT_JOB_signalHandler", (Service*)this, WRITE );

	daemonCore->Register_Signal( GRIDMAN_REMOVE_JOBS, "RemoveJobs",
		(SignalHandlercpp)&GridManager::REMOVE_JOBS_signalHandler,
		"REMOVE_JOBS_signalHandler", (Service*)this, WRITE );

	daemonCore->Register_Signal( GRIDMAN_CANCEL_JOB, "CancelJob",
		(SignalHandlercpp)&GridManager::CANCEL_JOB_signalHandler,
		"CANCEL_JOB_signalHandler", (Service*)this, WRITE );

	daemonCore->Register_Signal( GRIDMAN_COMMIT_JOB, "CommitJob",
		(SignalHandlercpp)&GridManager::COMMIT_JOB_signalHandler,
		"COMMIT_JOB_signalHandler", (Service*)this, WRITE );

	daemonCore->Register_Signal( GRIDMAN_RESTART_JM, "RestartJM",
		(SignalHandlercpp)&GridManager::RESTART_JM_signalHandler,
		"RESTART_JM_signalHandler", (Service*)this, WRITE );

	daemonCore->Register_Timer( 0, GLOBUS_POLL_INTERVAL,
		(TimerHandlercpp)&GridManager::globusPoll,
		"globusPoll", (Service*)this );

	daemonCore->Register_Timer( 10,
		(TimerHandlercpp)&GridManager::jobProbe,
		"jobProbe", (Service*)this );

	reconfig();
}

void
GridManager::reconfig()
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
GridManager::checkProxy()
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
		gss_cred_id_t old_gram_credential = globus_i_gram_protocol_credential;
		static bool first_cred_refresh = true;
		/* -- Now, acquire our new context.  We care about errors
		 * here, but we have no good way of handling them.  Dooohh! */
    	major_status = globus_gss_assist_acquire_cred(&minor_status,
                        GSS_C_BOTH,
                        &globus_i_gram_protocol_credential);
    	if (major_status != GSS_S_COMPLETE)
    	{
			// If we failed, perhaps the proxy file was being changed
			// right when we were reading it.  Bad luck!  So try
			// to sleep for one second and try again.
			sleep(1);
    		major_status = globus_gss_assist_acquire_cred(&minor_status,
                        GSS_C_BOTH,
                        &globus_i_gram_protocol_credential);
    		if (major_status != GSS_S_COMPLETE)
			{
				// We cannot read in the new proxy... revert to the old.
				globus_i_gram_protocol_credential = old_gram_credential;
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
		daemonCore->Send_Signal(daemonCore->getpid(),SIGQUIT);  
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
		if ( checkProxy_tid != -1 ) {
			daemonCore->Reset_Timer(checkProxy_tid,interval);
		} else {
			checkProxy_tid = daemonCore->Register_Timer( interval,
				(TimerHandlercpp)&GridManager::checkProxy,
				"checkProxy", (Service*)this );
		}
	} else {
		// interval is 0, cancel any timer if we have one
		if (checkProxy_tid != -1 ) {
			daemonCore->Cancel_Timer(checkProxy_tid);
			checkProxy_tid = -1;
		}
	}

	return TRUE;
}


int
GridManager::ADD_JOBS_signalHandler( int signal )
{
	ClassAd *next_ad;
	char expr_buf[_POSIX_PATH_MAX];
	char owner_buf[_POSIX_PATH_MAX];
	int num_ads = 0;
	static bool got_job_already = false;

	dprintf(D_FULLDEBUG,"in ADD_JOBS_signalHandler\n");

	if ( m_cluster && got_job_already ) {
		dprintf(D_FULLDEBUG,
			"already got the job we want, leaving ADD_JOBS_signalHandler\n");
		return TRUE;
	}
	
	if(useDefaultProxy == false) {
		sprintf(owner_buf, "%s == \"%s\" && %s =?= \"%s\" ", ATTR_OWNER, Owner,
				ATTR_X509_USER_PROXY, X509Proxy);
	} else {
		sprintf(owner_buf, "%s == \"%s\" && %s =?= UNDEFINED ", ATTR_OWNER, 
						Owner, ATTR_X509_USER_PROXY);
	}

	// Make sure we grab all Globus Universe jobs when we first start up
	// in case we're recovering from a shutdown/meltdown.
	if ( grabAllJobs ) {
		sprintf( expr_buf, "%s  && %s == %d",
				owner_buf, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GRID );
	} else {
		sprintf( expr_buf, "%s  && %s == %d && %s == %d",
			owner_buf, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GRID,
			ATTR_GLOBUS_STATUS, G_UNSUBMITTED );
	}

	// If we are only managing one job, the above constraint is junk.
	if  ( m_cluster ) {
		sprintf( expr_buf,"job %d.%d\n",m_cluster,m_proc);
	}

	// Get all the new Grid job ads
	dprintf(D_FULLDEBUG,"ConnectQ w/ constraint=%s\n",expr_buf);
	Qmgr_connection *schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, true );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd! (in ADD_JOBS)\n");
		daemonCore->Register_Timer( UPDATE_SCHEDD_DELAY,
					(TimerHandlercpp)&GridManager::ADD_JOBS_signalHandler,
					"ADD_JOBS", (Service*) &gridmanager );
		return FALSE;
	}

	if ( m_cluster ) {
		next_ad = GetJobAd( m_cluster, m_proc );
	} else {
		next_ad = GetNextJobByConstraint( expr_buf, 1 );
	}

	while ( next_ad != NULL ) {
		PROC_ID procID;
		GlobusJob *old_job;
		ClassAd *old_ad;

		next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
		next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );

		if ( JobsByProcID->lookup( procID, old_job ) != 0 ) {

			GlobusJob *new_job = new GlobusJob( next_ad );
			ASSERT(new_job);
			JobsByProcID->insert( new_job->procID, new_job );
			JobsToSubmit.Append( new_job );
			num_ads++;
			got_job_already = true;

		}

		delete next_ad;

		if ( m_cluster ) {
			next_ad = NULL;
		} else {
			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}
	}

	DisconnectQ( schedd );
	dprintf(D_FULLDEBUG,"Fetched %d new job ads from schedd\n",num_ads);

	grabAllJobs = false;

	if ( !JobsToSubmit.IsEmpty() ) {
		daemonCore->Block_Signal( GRIDMAN_ADD_JOBS );
		daemonCore->Send_Signal( daemonCore->getpid(), GRIDMAN_SUBMIT_JOB );
	}

	return TRUE;
}

int
GridManager::SUBMIT_JOB_signalHandler( int signal )
{
	// Try to submit the new jobs
	GlobusJob *new_job = NULL;
	PROC_ID procID;

	dprintf(D_FULLDEBUG,"in SUBMIT_JOB_signalHandler\n");

	if ( JobsToSubmit.IsEmpty() ) {
		daemonCore->Unblock_Signal( GRIDMAN_ADD_JOBS );
		return TRUE;
	}

	if ( !JobsToCancel.IsEmpty() || !JobsToCommit.IsEmpty() ) {
		dprintf(D_FULLDEBUG,"Running jobs need servicing. Delaying submit.\n");
		daemonCore->Block_Signal( GRIDMAN_ADD_JOBS );
		daemonCore->Send_Signal( daemonCore->getpid(), GRIDMAN_SUBMIT_JOB );
		return TRUE;
	}

	JobsToSubmit.Rewind();
	JobsToSubmit.Next( new_job );

	char *tmp;
	if ( DeadMachines->lookup( HashKey(new_job->rmContact), tmp ) == 0 ) {

		dprintf(D_FULLDEBUG,
				"Resource %s is dead, delaying submit of job %d.%d\n",
				new_job->rmContact,new_job->procID.cluster,
				new_job->procID.proc);
		WriteGlobusResourceDownEventToUserLog( new_job );
		WaitingToSubmit.Append( new_job );

	} else {

		if ( new_job->jobState == G_UNSUBMITTED ) {
			dprintf(D_FULLDEBUG,"Attempting to submit job %d.%d\n",
					new_job->procID.cluster,new_job->procID.proc);

			if ( new_job->start() == true ) {
				dprintf(D_ALWAYS,"Submitted job %d.%d\n",
						new_job->procID.cluster,new_job->procID.proc);
			} else {
				if ( new_job->errorCode ==
					 GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ) {

					// The machine is unreachable. Mark it dead and
					// delay the submit.
					dprintf( D_ALWAYS,
							 "Resource %s unreachable. Marking as dead\n",
							 new_job->rmContact );
					//DeadMachines->insert( HashKey(new_job->rmContact), (char*)NULL );
					RecordDeath( new_job->rmContact, (char*)NULL );
					WaitingToSubmit.Append( new_job );

				} else {

					dprintf(D_ALWAYS,
							"ERROR: Job %d.%d Submit failed because %s (%d)\n",
							new_job->procID.cluster, new_job->procID.proc,
							new_job->errorString(), new_job->errorCode );
					if (new_job->RSL) {
						dprintf(D_ALWAYS,"Job %d.%d RSL is: %s\n",
								new_job->procID.cluster,new_job->procID.proc,
								new_job->RSL);
					}
					WriteGlobusSubmitFailedEventToUserLog( new_job );
					// TODO : we just failed to submit a job; handle it better.
				}
			}
		} else if ( new_job->jobContact ) {
			dprintf(D_FULLDEBUG,
					"Job %d.%d already submitted to Globus\n",
					new_job->procID.cluster,new_job->procID.proc);
			// So here the job has already been submitted to Globus.
			// We need to place the contact string into our hashtable,
			// and setup to get async events.
			GlobusJob *tmp_job = NULL;
			// see if this contact string already in our hash table
			JobsByContact->lookup(HashKey(new_job->jobContact), tmp_job);
			if ( tmp_job == NULL ) {
				// nope, this string is not in our hash table. Register
				// a callback for it (and insert it in the hash table).
				if ( new_job->callback_register() == false ) {

					if ( new_job->errorCode ==
						 GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {

						// The jobmanager is unreachable. See if the same is
						// true for the entire machine.
						if ( globus_gram_client_ping(new_job->rmContact) !=
							 GLOBUS_SUCCESS ) {

							// The machine is unreachable. Mark it dead and
							// delay the cancel.
							dprintf( D_ALWAYS,
								 "Resource %s unreachable. Marking as dead\n",
									 new_job->rmContact );
							RecordDeath( new_job->rmContact, (char*)NULL );
							//DeadMachines->insert( HashKey(new_job->rmContact),
							//					  (char*)NULL );
							WaitingToSubmit.Append( new_job );

						} else {

							// Try to restart the jobmanager
							dprintf(D_FULLDEBUG,
								"Can't contact jobmanager for job %d.%d. "
								"Will try to restart\n",
								new_job->procID.cluster,new_job->procID.proc);
							addRestartJM( new_job );

						}

					} else {
						// Error
						dprintf(D_ALWAYS,
							"ERROR: JM register for %d.%d failed because %s (%d)\n",
							new_job->procID.cluster, new_job->procID.proc,
							new_job->errorString(), new_job->errorCode );
						// TODO what now?  try later ?
						// Here is what to do: Register a timer to probe jobs
						// sooner rather than later, so we can either restart or
						// resubmit this job.
						daemonCore->Register_Timer( 10,
									(TimerHandlercpp)&GridManager::jobProbe,
									"jobProbe", (Service*)this );
					}
				}
			}
		} else if ( new_job->jobState == G_CANCELED ) {
			addJobUpdateEvent( new_job, JOB_UE_CANCELED );
		} else {
			// bad state
			dprintf(D_ALWAYS,
				"ERROR: job %d.%d is in state %d but has no contact string\n",
					new_job->procID.cluster, new_job->procID.proc,
					new_job->jobState);
		}

	}

	JobsToSubmit.DeleteCurrent();

	if ( !JobsToSubmit.IsEmpty() ) {
		dprintf(D_FULLDEBUG,"More jobs to submit. Resignaling SUBMIT_JOB\n");
		daemonCore->Block_Signal( GRIDMAN_ADD_JOBS );
		daemonCore->Send_Signal( daemonCore->getpid(), GRIDMAN_SUBMIT_JOB );
	} else {
		daemonCore->Unblock_Signal( GRIDMAN_ADD_JOBS );
	}

	return TRUE;
}

int
GridManager::REMOVE_JOBS_signalHandler( int signal )
{
	ClassAd *next_ad;
	char expr_buf[_POSIX_PATH_MAX];
	char owner_buf[_POSIX_PATH_MAX];
	int num_ads = 0;
	bool must_disconnect_from_queue = false;
	Qmgr_connection *schedd = NULL;

	dprintf(D_FULLDEBUG,"in REMOVE_JOBS_signalHandler\n");

	if(useDefaultProxy == false) {
		sprintf(owner_buf, "%s == \"%s\" && %s =?= \"%s\" ", ATTR_OWNER, Owner,
				ATTR_X509_USER_PROXY, X509Proxy);
	} else {
		sprintf(owner_buf, "%s == \"%s\" && %s =?= UNDEFINED ", ATTR_OWNER, 
						Owner, ATTR_X509_USER_PROXY);
	}

	sprintf( expr_buf, "%s && %s == %d && %s == %d",
		owner_buf, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GRID,
		ATTR_JOB_STATUS, REMOVED );


	// Get all the new Grid job ads
	if ( m_cluster ) {
		next_ad = (ClassAd *)1;	// Horrible SC01 hack! But CVS friendly...
	} else {
		dprintf(D_FULLDEBUG,"ConnectQ w/ constraint=%s\n",expr_buf);
		schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, false );
		must_disconnect_from_queue = true;
		if ( !schedd ) {
			dprintf( D_ALWAYS, "Failed to connect to schedd! (in REMOVE_JOBS)\n");
			daemonCore->Register_Timer( UPDATE_SCHEDD_DELAY,
					(TimerHandlercpp)&GridManager::REMOVE_JOBS_signalHandler,
					"REMOVE_JOBS", (Service*) &gridmanager );
			return FALSE;
		}
		next_ad = GetNextJobByConstraint( expr_buf, 1 );
	}

	while ( next_ad != NULL ) {
		PROC_ID procID;
		GlobusJob *next_job;

		if ( m_cluster ) {
			procID.cluster = m_cluster;
			procID.proc = m_proc;
		} else {
			next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );
		}

		if ( JobsByProcID->lookup( procID, next_job ) == 0 ) {
			// Should probably skip jobs we already have marked for removal

			JobsToSubmit.Delete( next_job );
			next_job->removedByUser = true;
			JobsToCancel.Append( next_job );
			num_ads++;

		} else {

			// If we don't know about the job, remove it immediately
			dprintf( D_ALWAYS, 
				"Don't know about removed job %d.%d. "
				"Deleting it immediately\n", procID.cluster, procID.proc );

			// if we are managing a specifc job, we did not connnect to the
			// queue above, so do it now.
			if ( m_cluster ) {
				dprintf(D_FULLDEBUG,"ConnectQ w/ constraint=%s\n",expr_buf);
				schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, false );
				must_disconnect_from_queue = true;
				if ( !schedd ) {
					dprintf( D_ALWAYS, 
						"Failed to connect to schedd! (in REMOVE_JOBS)\n");
					daemonCore->Register_Timer( UPDATE_SCHEDD_DELAY,
						(TimerHandlercpp)&GridManager::REMOVE_JOBS_signalHandler,
						"REMOVE_JOBS", (Service*) &gridmanager );
					return FALSE;
				}
			}
			
			DestroyProc( procID.cluster, procID.proc );

		}

		if ( m_cluster ) {
			next_ad = NULL;
		} else {
			delete next_ad;
			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}
	}

	if ( must_disconnect_from_queue ) {
		DisconnectQ( schedd );
	}
	dprintf(D_FULLDEBUG,"Fetched %d job ads from schedd\n",num_ads);

	if ( !JobsToCancel.IsEmpty() ) {
		daemonCore->Block_Signal( GRIDMAN_REMOVE_JOBS );
		daemonCore->Send_Signal( daemonCore->getpid(), GRIDMAN_CANCEL_JOB );
	}

	return TRUE;
}

int
GridManager::CANCEL_JOB_signalHandler( int signal )
{
	// Try to cancel a removed job
	GlobusJob *curr_job = NULL;
	PROC_ID procID;

	dprintf( D_FULLDEBUG, "in CANCEL_JOB_signalHandler\n" );

	if ( JobsToCancel.IsEmpty() ) {
		daemonCore->Unblock_Signal( GRIDMAN_REMOVE_JOBS );
		return TRUE;
	}

	JobsToCancel.Rewind();
	JobsToCancel.Next( curr_job );

	JobsToSubmit.Delete( curr_job );

	if ( curr_job->jobContact != NULL ) {
		char *tmp;
		if ( DeadMachines->lookup(HashKey(curr_job->rmContact),tmp) == 0 ) {

			dprintf(D_FULLDEBUG,
					"Resource %s is dead, delaying cancel of job %d.%d\n",
					curr_job->rmContact,curr_job->procID.cluster,
					curr_job->procID.proc);
			WaitingToCancel.Append( curr_job );

		} else if ( curr_job->restartingJM == false &&
					curr_job->restartWhen == 0 ) {
			// If we're in the middle of restarting the jobmanager, don't
			// send a cancel because it'll just cancel the new jobmanager and
			// not the job. Once the restart is complete, the job will be
			// re-added to the cancel queue (by the COMMIT_JOB handler).

			dprintf(D_FULLDEBUG,"Attempting to remove job %d.%d\n",
					curr_job->procID.cluster,curr_job->procID.proc);

			if ( curr_job->cancel() == false ) {

				if ( curr_job->errorCode ==
					 GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {

					// The jobmanager is unreachable. See if the same is
					// true for the entire machine.
					if ( globus_gram_client_ping(curr_job->rmContact) !=
						 GLOBUS_SUCCESS ) {

						// The machine is unreachable. Mark it dead and
						// delay the cancel.
						dprintf( D_ALWAYS,
								 "Resource %s unreachable. Marking as dead\n",
								 curr_job->rmContact );
						RecordDeath( curr_job->rmContact, (char*)NULL );
						//DeadMachines->insert( HashKey(curr_job->rmContact),
						//					  (char*)NULL );
						WaitingToCancel.Append( curr_job );

					} else {

						// Try to restart the jobmanager
						dprintf(D_FULLDEBUG,
							"Can't contact jobmanager for job %d.%d. "
							"Will try to restart\n",
							curr_job->procID.cluster,curr_job->procID.proc);
						addRestartJM( curr_job );

					}

				} else if ( curr_job->errorCode !=
							GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL ) {
					// JOB_QUERY_DENIAL means the job is done and the
					// jobmanager is cleaning up. In that case, wait for
					// the final callback.
					// Error
					dprintf(D_ALWAYS,
						"ERROR: Job cancel for %d.%d failed because %s (%d)\n",
						curr_job->procID.cluster, curr_job->procID.proc,
						curr_job->errorString(), curr_job->errorCode );
					// TODO what now?  try later ?
				}
			} else {
				// Success.  We don't actually remove it until
				// we receive the status update from the job manager.
				dprintf(D_ALWAYS,"Removed job %d.%d\n",
						curr_job->procID.cluster,curr_job->procID.proc);

				JobsToCommit.Delete( curr_job );
				removeJobUpdateEvents( curr_job );

				if ( curr_job->durocRequest ) {
					curr_job->jobState = G_CANCELED;
					addJobUpdateEvent( curr_job, JOB_UE_CANCELED );
				}
			}

		}

	} else {

		// If the state is DONE or FAILED with no contact string, then
		// the job is done and we just need to update the schedd. Allow
		// the job to complete.
		if ( curr_job->jobState != G_DONE && curr_job->jobState != G_FAILED ) {
			// If there is no contact, remove the job now
			dprintf(D_ALWAYS,"Removed job %d.%d (it had no contact string)\n",
					curr_job->procID.cluster,curr_job->procID.proc);
			removeJobUpdateEvents( curr_job );
			addJobUpdateEvent( curr_job, JOB_UE_CANCELED );
		}

	}

	JobsToCancel.DeleteCurrent();

	if ( !JobsToCancel.IsEmpty() ) {
		dprintf(D_FULLDEBUG, "More jobs to cancel, Resignalling CANCEL_JOB\n");

		daemonCore->Block_Signal( GRIDMAN_REMOVE_JOBS );
		daemonCore->Send_Signal( daemonCore->getpid(), GRIDMAN_CANCEL_JOB );
	} else {
		daemonCore->Unblock_Signal( GRIDMAN_REMOVE_JOBS );
	}

	return TRUE;
}

int
GridManager::COMMIT_JOB_signalHandler( int signal )
{
	// Try to commit a job submission or completion
	bool resignal_cancel = false;
	GlobusJob *curr_job = NULL;
	PROC_ID procID;

	dprintf( D_FULLDEBUG, "in COMMIT_JOB_signalHandler\n" );

	if ( JobsToCommit.IsEmpty() ) {
		return TRUE;
	}

	JobsToCommit.Rewind();
	JobsToCommit.Next( curr_job );

	char *tmp;
	if ( DeadMachines->lookup( HashKey(curr_job->rmContact), tmp ) == 0 ) {

		dprintf(D_FULLDEBUG,
				"Resource %s is dead, delaying commit of job %d.%d\n",
				curr_job->rmContact,curr_job->procID.cluster,
				curr_job->procID.proc);
		WaitingToCommit.Append( curr_job );

	} else if ( curr_job->jobContact != NULL ) {

		if ( curr_job->restartingJM && curr_job->removedByUser ) {
			// If we're restarting a jobmanager for a removed job, we need
			// to retry canceling the job.
			resignal_cancel = true;
		}

		dprintf(D_FULLDEBUG,"Attempting to send commit for job %d.%d\n",
				curr_job->procID.cluster, curr_job->procID.proc);

		if ( curr_job->commit() == false ) {

			if ( curr_job->errorCode ==
				 GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {

				// The jobmanager is unreachable. See if the same is
				// true for the entire machine.
				if ( globus_gram_client_ping(curr_job->rmContact) !=
					 GLOBUS_SUCCESS ) {

					// The machine is unreachable. Mark it dead and
					// delay the commit.
					dprintf( D_ALWAYS,
							 "Resource %s unreachable. Marking as dead\n",
							 curr_job->rmContact );
					RecordDeath( curr_job->rmContact, (char*)NULL );
					//	DeadMachines->insert( HashKey(curr_job->rmContact),
					//				  (char*)NULL );
					WaitingToCommit.Append( curr_job );

				} else {

					// Try to restart the jobmanager
					dprintf(D_FULLDEBUG,
							"Can't contact jobmanager for job %d.%d. "
							"Will try to restart\n",
							curr_job->procID.cluster,curr_job->procID.proc);
					addRestartJM( curr_job );


				}

			} else {
				// Error
				dprintf(D_ALWAYS,
						"ERROR: Job commit for %d.%d failed because %s (%d)\n",
						curr_job->procID.cluster, curr_job->procID.proc,
						curr_job->errorString(), curr_job->errorCode );
				// TODO what now?  try later ?
			}

		} else {

			dprintf(D_FULLDEBUG,"Commit successful for job %d.%d\n",
					curr_job->procID.cluster,curr_job->procID.proc);

			if ( resignal_cancel ) {

				JobsToCancel.Append( curr_job );
				daemonCore->Send_Signal( daemonCore->getpid(),
										 GRIDMAN_CANCEL_JOB );

			}

		}

	}

	JobsToCommit.DeleteCurrent();

	// This job may have crept into the commit list more than once, but there
	// should be at most one pending commit at any time per job. Make sure
	// we only send one commit.
	while ( JobsToCommit.Delete( curr_job ) );

	if ( !JobsToCommit.IsEmpty() ) {
		dprintf(D_FULLDEBUG, "More jobs to commit, Resignalling COMMIT_JOB\n");

		daemonCore->Send_Signal( daemonCore->getpid(), GRIDMAN_COMMIT_JOB );
	}

	return TRUE;
}

bool
GridManager::stopJM(GlobusJob *job)
{

	// We are being asked to kill the JobManager associated
	// with job (perhaps because its credential is going to expire).

	// Handle this by first canceling all pending events on this
	// job, so we don't do something silly like kill the JobManager
	// and then have a pending event restart it.
	cancelAllPendingEvents(job);

	// Now kill the JM.  Once we do this, we will no longer listen
	// to any updates from the JM, in case updates will trigger
	// events which will restart it.
	job->stop_job_manager();
	

}

void
GridManager::cancelAllPendingEvents(GlobusJob *curr_job)
{
	// Cancel any pending events on this job.
	JobsToSubmit.Delete( curr_job );
	JobsToCancel.Delete( curr_job );
	JobsToCommit.Delete( curr_job );
	JMsToRestart.Delete( curr_job );
	JobsToProbe.Delete( curr_job );
	WaitingToSubmit.Delete( curr_job );
	WaitingToCancel.Delete( curr_job );
	WaitingToCommit.Delete( curr_job );
	WaitingToRestart.Delete( curr_job );
}

void
GridManager::addRestartJM( GlobusJob *job )
{
	// restart will fail for an old jobmanager, so just do what the restart
	// function would do on a restart failure
	if ( job->newJM == false ) {
		JobsByContact->remove(HashKey(job->jobContact));
		free( job->jobContact );
		job->jobContact = NULL;

		if ( job->jobState == G_CANCELED ||
			 job->removedByUser ) {

			job->jobState = G_CANCELED;
			removeJobUpdateEvents( job );
			addJobUpdateEvent( job, JOB_UE_CANCELED );

		} else {

			job->jobState = G_UNSUBMITTED;
			addJobUpdateEvent( job, JOB_UE_RESUBMIT );

		}
		return;
	}

	bool need_signal = JMsToRestart.IsEmpty();

	if ( job->restartWhen == 0 ) {
		job->restartWhen = time(NULL) + JM_RESTART_DELAY;
		JMsToRestart.Append( job );

		if ( need_signal ) {
			daemonCore->Send_Signal( daemonCore->getpid(),
									 GRIDMAN_RESTART_JM );
		}
	}
}

int
GridManager::RESTART_JM_signalHandler( int signal = 0 )
{
	// Try to restart a dead jobmanager
	GlobusJob *curr_job = NULL;
	PROC_ID procID;

	dprintf( D_FULLDEBUG, "in RESTART_JM_signalHandler\n" );

	if ( JMsToRestart.IsEmpty() ) {
		return TRUE;
	}

	JMsToRestart.Rewind();
	JMsToRestart.Next( curr_job );

	if ( curr_job->restartWhen > time(NULL) ) {
		dprintf( D_FULLDEBUG, "not time to restart job yet, waiting\n");

		daemonCore->Register_Timer( curr_job->restartWhen - time(NULL),
					(TimerHandlercpp)&GridManager::RESTART_JM_signalHandler,
					"RESTART_JM_signalHandler", (Service*)&gridmanager );
		return TRUE;
	}

	char *tmp;
	if ( DeadMachines->lookup( HashKey(curr_job->rmContact), tmp ) == 0 ) {

		dprintf(D_FULLDEBUG,
				"Resource %s is dead, delaying restart of job %d.%d\n",
				curr_job->rmContact,curr_job->procID.cluster,
				curr_job->procID.proc);
		WaitingToRestart.Append( curr_job );

	} else if ( curr_job->restartWhen != 0 ) {

		curr_job->restartWhen = 0;

		dprintf(D_FULLDEBUG,"Attempting to restart jobmanager for job %d.%d\n",
				curr_job->procID.cluster, curr_job->procID.proc);

		if ( curr_job->restart() == false ) {

			if ( curr_job->errorCode ==
				 GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ) {

				// The machine is unreachable. Mark it dead and
				// delay the restart.
				dprintf( D_ALWAYS,
						 "Resource %s unreachable. Marking as dead\n",
						 curr_job->rmContact );
				RecordDeath( curr_job->rmContact, (char*)NULL );
				//DeadMachines->insert( HashKey(curr_job->rmContact),
				//					  (char*)NULL );
				WaitingToRestart.Append( curr_job );

			} else {

				// Can't restart, so give up on this job
				// submission. Resubmit the job (or let a
				// cancel complete).
				if ( curr_job->newJM ) {
					dprintf(D_ALWAYS,
						"JM restart failed for job %d.%d because %s (%d)\n",
						curr_job->procID.cluster,
						curr_job->procID.proc,
						curr_job->errorString(),
						curr_job->errorCode);
				}

				JobsByContact->remove(HashKey(curr_job->jobContact));
				free( curr_job->jobContact );
				curr_job->jobContact = NULL;

				if ( curr_job->jobState == G_CANCELED ||
					 curr_job->removedByUser ) {

					curr_job->jobState = G_CANCELED;
					removeJobUpdateEvents( curr_job );
					addJobUpdateEvent( curr_job, JOB_UE_CANCELED );

				} else {

					curr_job->jobState = G_UNSUBMITTED;
					addJobUpdateEvent( curr_job, JOB_UE_RESUBMIT );

				}

			}

		} else {

			dprintf(D_FULLDEBUG,"Restarted jobmanager for job %d.%d\n",
					curr_job->procID.cluster,curr_job->procID.proc);

		}

	}

	JMsToRestart.DeleteCurrent();

	// This job may have crept into the restart list more than once (closely-
	// spaced commit and probe failures, for example). Make sure we only
	// restart it once.
	while ( JMsToRestart.Delete( curr_job ) );

	if ( !JMsToRestart.IsEmpty() ) {
		dprintf(D_FULLDEBUG, "More JMs to restart, Resignalling RESTART_JM\n");

		JMsToRestart.Next( curr_job );

		if ( curr_job->restartWhen > time(NULL) ) {
			daemonCore->Register_Timer( curr_job->restartWhen - time(NULL),
					(TimerHandlercpp)&GridManager::RESTART_JM_signalHandler,
					"RESTART_JM_signalHandler", (Service*)&gridmanager );
		} else {
			daemonCore->Send_Signal( daemonCore->getpid(),
									 GRIDMAN_RESTART_JM );
		}
	}

	return TRUE;
}


static void
update_remote_wall_clock(int cluster, int proc, int bday)
{
		// update ATTR_JOB_REMOTE_WALL_CLOCK. 
	if (bday) {
		float accum_time = 0;
		GetAttributeFloat(cluster, proc,
						  ATTR_JOB_REMOTE_WALL_CLOCK,&accum_time);
		accum_time += (float)( time(NULL) - bday );
		SetAttributeFloat(cluster, proc,
						  ATTR_JOB_REMOTE_WALL_CLOCK,accum_time);
		DeleteAttribute(cluster, proc, ATTR_JOB_WALL_CLOCK_CKPT);
		SetAttributeInt(cluster, proc, ATTR_SHADOW_BIRTHDATE, 0);
	}
}


int
GridManager::updateSchedd()
{
	int rc;
	int cluster_id;
	int proc_id;
	char buf[1024];
	Qmgr_connection *schedd;
	JobUpdateEvent *curr_event;
	GlobusJob *curr_job;
	bool handled;
	List <GlobusJob> jobs_to_delete;


	dprintf(D_FULLDEBUG,"in updateSchedd()\n");

	updateScheddTimerSet = false;

	JobUpdateEventQueue.Rewind();

	while ( JobUpdateEventQueue.Next( curr_event ) ) {

		curr_job = curr_event->job;
		handled = false;

		switch ( curr_event->event) {
		case JOB_UE_SUBMITTED:
			if ( curr_job->jobState != G_UNSUBMITTED ) {
				WriteGlobusSubmitEventToUserLog( curr_job );
			}
			break;
		case JOB_UE_RUNNING:
			if ( !curr_job->executeLogged ) {
				WriteExecuteToUserLog( curr_job );
				curr_job->executeLogged = true;
			}
			break;
		case JOB_UE_DONE:
		case JOB_UE_FAILED:
			if ( curr_job->newJM && curr_job->jobContact != NULL ) {
				break;
			}
			if ( curr_event->event == JOB_UE_FAILED && 
				 ( curr_job->jmFailureCode == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_EXECUTION_FAILED ||
				   curr_job->jmFailureCode == GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_QUEUE ) ) {
				// This error means the submission to the local scheduler
				// failed. Treat it as a submission error.
				curr_job->errorCode = curr_job->jmFailureCode;
				WriteGlobusSubmitFailedEventToUserLog( curr_job );
			} else {
				if ( !curr_job->executeLogged ) {
					WriteExecuteToUserLog( curr_job );
					curr_job->executeLogged = true;
				}
				if ( !curr_job->exitLogged ) {
					WriteTerminateToUserLog( curr_job );
					curr_job->exitLogged = true;
				}
			}
			break;
		case JOB_UE_CANCELED:
			if ( curr_job->newJM && curr_job->jobContact != NULL ) {
				break;
			}
			if ( !curr_job->exitLogged ) {
				WriteAbortToUserLog( curr_job );
				curr_job->exitLogged = true;
			}
			break;
		case JOB_UE_RESUBMIT:
			if ( curr_job->executeLogged ) {
				curr_job->executeLogged = false;
				WriteEvictToUserLog( curr_job );
			}
			break;
		}

		if ( handled ) {
			JobUpdateEventQueue.DeleteCurrent();
			delete curr_event;
		}
	}

	// Any items left in the list at this point must be qmgmt ops.
	// Before connecting to the schedd queue, check if the list
	// is empty.  If so, we're done.
	if ( JobUpdateEventQueue.IsEmpty() ) {
		return TRUE;
	}

	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, false );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd!\n");
		// Should we be retrying infinitely?
		setUpdate();
		return TRUE;
	}

	JobUpdateEventQueue.Rewind();

	while ( JobUpdateEventQueue.Next( curr_event ) ) {

		curr_job = curr_event->job;
		handled = false;

		if ( curr_job->stateChanged ) {

			SetAttributeInt( curr_job->procID.cluster,
							 curr_job->procID.proc,
							 ATTR_GLOBUS_STATUS, curr_job->jobState );

			int curr_status;
			GetAttributeInt( curr_job->procID.cluster,
							 curr_job->procID.proc,
							 ATTR_JOB_STATUS, &curr_status );

			switch ( curr_job->jobState ) {
			case G_UNSUBMITTED:
			case G_SUBMITTED:
			case G_PENDING:
				// Send new job state command
				if ( curr_status != REMOVED ) {
					SetAttributeInt( curr_job->procID.cluster,
									 curr_job->procID.proc,
									 ATTR_JOB_STATUS, IDLE );
				}
				break;
			case G_ACTIVE:
				// Send new job state command
				if ( curr_status != REMOVED ) {
					SetAttributeInt( curr_job->procID.cluster,
									 curr_job->procID.proc,
									 ATTR_JOB_STATUS, RUNNING );
					int current_time = (int)time(NULL);
					SetAttributeInt( curr_job->procID.cluster,
									 curr_job->procID.proc,
									 ATTR_SHADOW_BIRTHDATE, current_time );
					curr_job->shadow_birthday = current_time;
				}
				break;
			case G_FAILED:
//dprintf(D_ALWAYS,"TODD DEBUGCHECK %s:%d\n",__FILE__,__LINE__);

				// Send new job state command
				if ( curr_job->removedByUser ) {
//dprintf(D_ALWAYS,"TODD DEBUGCHECK %s:%d\n",__FILE__,__LINE__);

					SetAttributeInt( curr_job->procID.cluster,
									 curr_job->procID.proc,
									 ATTR_JOB_STATUS, REMOVED );
				} else if ( curr_status != REMOVED || curr_job->exitLogged ) {
//dprintf(D_ALWAYS,"TODD DEBUGCHECK %s:%d\n",__FILE__,__LINE__);

					SetAttributeInt( curr_job->procID.cluster,
									 curr_job->procID.proc,
									 ATTR_JOB_STATUS, COMPLETED );
				}
				break;
			case G_CANCELED:
				// This shouldn't be necessary, but what the hell
				SetAttributeInt( curr_job->procID.cluster,
								 curr_job->procID.proc,
								 ATTR_JOB_STATUS, REMOVED );
				break;
			case G_DONE:
				// Send new job state command
				if ( curr_status != REMOVED || curr_job->exitLogged ) {
					SetAttributeInt( curr_job->procID.cluster,
									 curr_job->procID.proc,
									 ATTR_JOB_STATUS, COMPLETED );
				}
				break;
			case G_SUSPENDED:
				// Send new job state command
				if ( curr_status != REMOVED ) {
					SetAttributeInt( curr_job->procID.cluster,
									 curr_job->procID.proc,
									 ATTR_JOB_STATUS, IDLE );
				}
				break;
			}

			curr_job->stateChanged = false;

			// If the job is now set at anything but RUNNING, and
			// yet we have a shadow_birthday (for reporting run_time in
			// condor_q), we need to commit the run time.
			if ( (curr_job->jobState != G_ACTIVE || curr_status == REMOVED)
					&& curr_job->shadow_birthday ) 
			{
				update_remote_wall_clock(curr_job->procID.cluster, 
					curr_job->procID.proc, curr_job->shadow_birthday);
				curr_job->shadow_birthday = 0;
			}

		}

		switch ( curr_event->event ) {
		case JOB_UE_STATE_CHANGE:
			handled = true;
			break;
		case JOB_UE_SUBMITTED:
			if ( curr_job->jobContact ) {
				SetAttributeString( curr_job->procID.cluster,
									curr_job->procID.proc,
									ATTR_GLOBUS_CONTACT_STRING,
									curr_job->jobContact );
			}
			// If the job state is G_UNSUBMITTED, then we're doing a 2-phase
			// commit to the jobmanager. Leave the event unhandled so we can
			// register a commit event after updating the schedd.
			if ( curr_job->jobState != G_UNSUBMITTED ) {
				handled = true;
			}
			break;
		case JOB_UE_JM_RESTARTED:
			if ( curr_job->jobContact ) {
				SetAttributeString( curr_job->procID.cluster,
									curr_job->procID.proc,
									ATTR_GLOBUS_CONTACT_STRING,
									curr_job->jobContact );
			}
			break;
		case JOB_UE_RUNNING:
			// The job's been marked as running. Nothing else to do.
			handled = true;
			break;
		case JOB_UE_FAILED:
		case JOB_UE_DONE:
		case JOB_UE_CANCELED:
			// If we're dealing with a new jobmanager and the contact string
			// isn't NULL, we need to commit the end of job execution
			// before removing the job from the schedd's queue.
			if ( curr_job->newJM && curr_job->jobContact != NULL ) {
				break;
			}
			// Remove the job from the schedd queue
			// (The differences between done and removed were already
			//  handled above.)

			// Before calling DestroyProc, first call
			// CloseConnection().  This will commit the transaction.
			// Then call BeginTransaction before calling DestroyProc.
			// We do this song-and-dance because in DestroryProc, removing
			// the ClassAd from the queue is transaction aware, but 
			// moving the ad to the history file is not (it happens 
			// immediately, not when the transaction is committed).
			// So, if we do not do this goofy procedure, the job ad
			// will show up in the history file as still running, etc.
			update_remote_wall_clock(curr_job->procID.cluster, 
					curr_job->procID.proc, curr_job->shadow_birthday);
			curr_job->shadow_birthday = 0;
			CloseConnection();
			BeginTransaction();
			DestroyProc(curr_job->procID.cluster,
						curr_job->procID.proc);

			jobs_to_delete.Append( curr_job );

			handled = true;
			break;
		case JOB_UE_RESUBMIT:
		case JOB_UE_NOT_SUBMITTED:
			SetAttributeString( curr_job->procID.cluster,
								curr_job->procID.proc,
								ATTR_GLOBUS_CONTACT_STRING,
								NULL_JOB_CONTACT );
			break;
		}

		if ( handled ) {
			JobUpdateEventQueue.DeleteCurrent();
			delete curr_event;
		}
	}

	DisconnectQ( schedd );

	JobUpdateEventQueue.Rewind();

	while ( JobUpdateEventQueue.Next( curr_event ) ) {

		curr_job = curr_event->job;
		handled = false;

		switch ( curr_event->event ) {
		case JOB_UE_SUBMITTED:
		case JOB_UE_JM_RESTARTED:
		case JOB_UE_DONE:
		case JOB_UE_FAILED:
		case JOB_UE_CANCELED:
			JobsToCommit.Append( curr_job );
			daemonCore->Send_Signal( daemonCore->getpid(),
									 GRIDMAN_COMMIT_JOB );
			handled = true;
			break;
		case JOB_UE_RESUBMIT:
		case JOB_UE_NOT_SUBMITTED:
			JobsToSubmit.Append( curr_job );
			daemonCore->Send_Signal( daemonCore->getpid(),
									 GRIDMAN_SUBMIT_JOB );
			handled = true;
			break;
		}

		if ( handled ) {
			JobUpdateEventQueue.DeleteCurrent();
			delete curr_event;
		}
	}

	jobs_to_delete.Rewind();

	while ( jobs_to_delete.Next( curr_job ) ) {

		// For jobs we just removed from the schedd's queue, remove all
		// knowledge of them and delete them. We do this after disconnecting
		// from the schedd because some of the lists we need to search could
		// be very long. Most of these lists shouldn't contain the job, but
		// we're being safe.
		if ( curr_job->jobContact != NULL ) {
			JobsByContact->remove( HashKey( curr_job->jobContact ) );
		}
		JobsByProcID->remove( curr_job->procID );
		cancelAllPendingEvents( curr_job);
		delete curr_job;

	}

	// If we are managing a specific job, may as well bail out
	// once it is done & gone.
	if ( m_cluster && (JobsByProcID->getNumElements() == 0) ) {
		dprintf( D_ALWAYS, "No jobs left, shutting down\n" );
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}

	dprintf(D_FULLDEBUG,"leaving updateSchedd()\n");
	return TRUE;
}

int
GridManager::globusPoll()
{
	if ( JobsToSubmit.IsEmpty() && JobsToCancel.IsEmpty() &&
		 JobsToCommit.IsEmpty() ) {

		time_t stop = time(NULL) + 1;
		globus_abstime_t wait;
		do {
			wait.tv_sec = stop;
			wait.tv_nsec = 0;
			globus_callback_poll( &wait );
		} while ( time(NULL) < stop );

	} else {

		globus_poll();

	}

	return TRUE;
}

int
GridManager::jobProbe()
{
	char *next_contact;
	GlobusJob *next_job;
	time_t end_time = time(NULL) + JOB_PROBE_LENGTH;

	dprintf(D_FULLDEBUG,"in jobProbe\n");
	if ( JobsByProcID->getNumElements() == 0 ) {
		dprintf( D_ALWAYS, "No jobs left, shutting down\n" );
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
		return TRUE;
	}

	if ( MachinesToProbe.IsEmpty() && JobsToProbe.IsEmpty() ) {

		HashKey next_key;

		DeadMachines->startIterations();

		while ( DeadMachines->iterate( next_key, next_contact ) != 0 ) {
			next_contact = strdup( next_key.value() );
			MachinesToProbe.Append( next_contact );
		}

		JobsByProcID->startIterations();

		while ( JobsByProcID->iterate( next_job ) != 0 ) {
			JobsToProbe.Append( next_job );
		}

	}

	dprintf(D_FULLDEBUG,"machines to probe:%d jobs to probe:%d\n",
		MachinesToProbe.Number(),JobsToProbe.Number() );

	// Ping all the unreachable machines to see if they're back
	MachinesToProbe.Rewind();

	while ( MachinesToProbe.Next( next_contact ) && time(NULL) <= end_time ) {

		dprintf(D_FULLDEBUG,"Trying to ping dead resource %s\n",next_contact);

		if ( globus_gram_client_ping(next_contact) == GLOBUS_SUCCESS ) {

			dprintf( D_ALWAYS,
				"Ping succeeded for resource %s. Removing from dead list\n",
					 next_contact );

			// Bring this machine back from the dead, and note that in all the
			// jobs running there.

			RecordRebirth(next_contact); 

			if ( !WaitingToSubmit.IsEmpty() ) {
				WaitingToSubmit.Rewind();
				while ( WaitingToSubmit.Next( next_job ) ) {
					if ( strcmp( next_job->rmContact, next_contact ) == 0 ) {
						WaitingToSubmit.DeleteCurrent();
						JobsToSubmit.Append( next_job );
					}
				}
				daemonCore->Send_Signal( daemonCore->getpid(),
										 GRIDMAN_SUBMIT_JOB );
			}

			if ( !WaitingToCancel.IsEmpty() ) {
				WaitingToCancel.Rewind();
				while ( WaitingToCancel.Next( next_job ) ) {
					if ( strcmp( next_job->rmContact, next_contact ) == 0 ) {
						WaitingToCancel.DeleteCurrent();
						JobsToCancel.Append( next_job );
					}
				}
				daemonCore->Send_Signal( daemonCore->getpid(),
										 GRIDMAN_CANCEL_JOB );
			}

			if ( !WaitingToCommit.IsEmpty() ) {
				WaitingToCommit.Rewind();
				while ( WaitingToCommit.Next( next_job ) ) {
					if ( strcmp( next_job->rmContact, next_contact ) == 0 ) {
						WaitingToCommit.DeleteCurrent();
						JobsToCommit.Append( next_job );
					}
				}
				daemonCore->Send_Signal( daemonCore->getpid(),
										 GRIDMAN_COMMIT_JOB );
			}

			if ( !WaitingToRestart.IsEmpty() ) {
				WaitingToRestart.Rewind();
				while ( WaitingToRestart.Next( next_job ) ) {
					if ( strcmp( next_job->rmContact, next_contact ) == 0 ) {
						WaitingToRestart.DeleteCurrent();
						JMsToRestart.Append( next_job );
					}
				}
				daemonCore->Send_Signal( daemonCore->getpid(),
										 GRIDMAN_RESTART_JM );
			}

		} else {
			dprintf(D_FULLDEBUG,"Ping failed, %s still dead\n",next_contact);
		}

		MachinesToProbe.DeleteCurrent();
		free( next_contact );

	}

	JobsToProbe.Rewind();

	while ( JobsToProbe.Next( next_job ) && time(NULL) <= end_time ) {

		if ( next_job->jobContact != NULL ) {

			char *tmp;
			if ( DeadMachines->lookup( HashKey(next_job->rmContact),
									   tmp ) == 0 ) {
				JobsToProbe.DeleteCurrent();
				continue;
			}

			dprintf(D_FULLDEBUG,"calling jobProbe for job %d.%d\n",
						 next_job->procID.cluster, next_job->procID.proc );

			if ( next_job->probe() == false && next_job->errorCode ==
				     GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {

				dprintf( D_ALWAYS, 
						"Globus Jobmanager unreachable for job %d.%d\n",
						 next_job->procID.cluster, next_job->procID.proc );

				// Check if we can contact the gatekeeper. If we can
				// contact to the gatekeeper, and not the jobmanager,
				// we know the jobmanager is gone.  So attempt to
				// restart the jobmanager.
				int err=globus_gram_client_ping(next_job->rmContact);
				if ( err == GLOBUS_SUCCESS ) {
					// It is possible to get the final callback from the
					// jobmanager inside the ping call. If we do, then the
					// jobmanager exitted normally and we shouldn't flag an
					// error. For now, we test this by seeing if jobContact
					// is still defined (meaning we still expect a running
					// jobmanager on the other end).
					if ( next_job->jobContact != NULL ) {

						// Try to restart the jobmanager
						dprintf(D_FULLDEBUG,
							"Can't contact jobmanager for job %d.%d. "
							"Will try to restart\n",
							next_job->procID.cluster,next_job->procID.proc);

						// Cancel any pending submit, cancel, or commit action.
						// They will conflict with the restart.
						JobsToSubmit.Delete( next_job );
						JobsToCancel.Delete( next_job );
						JobsToCommit.Delete( next_job );

						addRestartJM( next_job );

					}
				} else {
					// Mark the machine as dead
					dprintf( D_ALWAYS,
							 "Resource %s unreachable. Marking as dead\n",
							 next_job->rmContact );
					RecordDeath( next_job->rmContact, (char*)NULL );
					//DeadMachines->insert( HashKey(next_job->rmContact), (char*)NULL );
				}

			}

		}

		JobsToProbe.DeleteCurrent();

	}

	if ( MachinesToProbe.IsEmpty() && JobsToProbe.IsEmpty() ) {
		dprintf(D_FULLDEBUG,
				"No more probes to do, resignalling probe in %d seconds\n",
				JOB_PROBE_INTERVAL);

		daemonCore->Register_Timer( JOB_PROBE_INTERVAL,
									(TimerHandlercpp)&GridManager::jobProbe,
									"jobProbe", (Service*)this );
	} else {
		dprintf(D_FULLDEBUG,
				"More probes to do, resignalling probe immediately\n");

		daemonCore->Register_Timer( 0,
									(TimerHandlercpp)&GridManager::jobProbe,
									"jobProbe", (Service*)this );
	}

	return TRUE;
}

void
GridManager::gramCallbackHandler( void *user_arg, char *job_contact,
								  int state, int errorcode )
{
	int rc;
	int new_state;
	int cluster_id;
	int proc_id;
	char buf[1024];
	GlobusJob *this_job;
	GridManager *this_ = (GridManager *)user_arg;

	dprintf(D_ALWAYS,"gramCallbackHandler: job %s, state %d\n", 
		job_contact, state);
	// Find the right job object
	rc = this_->JobsByContact->lookup( HashKey( job_contact ), this_job );
	if ( rc != 0 || this_job == NULL ) {
		dprintf( D_ALWAYS, 
			"Can't find record for globus job with contact %s "
			"on globus event %d\n", job_contact, state );
		return;
	}

	dprintf( D_ALWAYS, "   job is %d.%d\n", this_job->procID.cluster,
			 this_job->procID.proc );

	// If we get a failed while waiting to commit a job, then it timed out
	// waiting for our commit signal. Remove the pending commit.
	if(state==GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED){
		this_->JobsToCommit.Delete(this_job);
	}

	this_job->callback( state, errorcode );

	dprintf(D_FULLDEBUG,"gramCallbackHandler() returning\n");
}

void
GridManager::RecordDeath( char *contact, char *second) {
	GlobusJob *tmp;

	DeadMachines->insert( HashKey(contact), second );

	JobsByProcID->startIterations();
	while( JobsByProcID->iterate( tmp ) ) {
		if ( strcmp(tmp->rmContact, contact ) == 0 ) {
			WriteGlobusResourceDownEventToUserLog( tmp );
		}
	}
}

void
GridManager::RecordRebirth(char *contact) {
	GlobusJob *tmp;

	DeadMachines->remove( HashKey(contact) );

	JobsByProcID->startIterations();
	while( JobsByProcID->iterate( tmp ) ) {
		if ( strcmp(tmp->rmContact, contact ) == 0 ) {
			WriteGlobusResourceUpEventToUserLog( tmp );
		}
	}

}

// Initialize a UserLog object for a given job and return a pointer to
// the UserLog object created.  This object can then be used to write
// events and must be deleted when you're done.  This returns NULL if
// the user didn't want a UserLog, so you must check for NULL before
// using the pointer you get back.
UserLog*
GridManager::InitializeUserLog( GlobusJob *job )
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
GridManager::WriteExecuteToUserLog( GlobusJob *job )
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
GridManager::WriteAbortToUserLog( GlobusJob *job )
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
GridManager::WriteTerminateToUserLog( GlobusJob *job )
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
GridManager::WriteEvictToUserLog( GlobusJob *job )
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
GridManager::WriteGlobusSubmitEventToUserLog( GlobusJob *job )
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
GridManager::WriteGlobusSubmitFailedEventToUserLog( GlobusJob *job )
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
GridManager::WriteGlobusResourceUpEventToUserLog( GlobusJob *job )
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
GridManager::WriteGlobusResourceDownEventToUserLog( GlobusJob *job )
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
