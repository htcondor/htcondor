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

#include "globus_gram_client.h"

#include "gm_common.h"
#include "gridmanager.h"


#define QMGMT_TIMEOUT 5

#define GLOBUS_POLL_INTERVAL	1
#define JOB_PROBE_INTERVAL		300
#define UPDATE_SCHEDD_DELAY		5

#define HASH_TABLE_SIZE			500


extern GridManager gridmanager;

List<JobUpdateEvent> JobUpdateEventQueue;
static bool updateScheddTimerSet = false;

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
template class List<GlobusJob>;
template class Item<GlobusJob>;
// template class List<GlobusJob *>;
// template class Item<GlobusJob *>;
template class List<JobUpdateEvent>;
template class Item<JobUpdateEvent>;


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

	JobsByContact = new HashTable <HashKey, GlobusJob *>( HASH_TABLE_SIZE,
														  hashFunction );
	JobsByProcID = new HashTable <PROC_ID, GlobusJob *>( HASH_TABLE_SIZE,
														 procIDHash );
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
		while( JobsByProcID->iterate( tmp ) )
			delete tmp;
		delete JobsByProcID;
		JobsByProcID = NULL;
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

	daemonCore->Register_Timer( 0, GLOBUS_POLL_INTERVAL,
		(TimerHandlercpp)&GridManager::globusPoll,
		"globusPoll", (Service*)this );

	daemonCore->Register_Timer( 10, JOB_PROBE_INTERVAL,
		(TimerHandlercpp)&GridManager::jobProbe,
		"jobProbe", (Service*)this );
}

void
GridManager::reconfig()
{
}


int
GridManager::ADD_JOBS_signalHandler( int signal )
{
	ClassAd *next_ad;
	char expr_buf[_POSIX_PATH_MAX];
	char owner_buf[_POSIX_PATH_MAX];
	int num_ads = 0;

	dprintf(D_FULLDEBUG,"in ADD_JOBS_signalHandler\n");
	
	if(X509Proxy) {
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
				owner_buf, ATTR_JOB_UNIVERSE, GLOBUS_UNIVERSE );
	} else {
		sprintf( expr_buf, "%s  && %s == %d && %s == %d",
			owner_buf, ATTR_JOB_UNIVERSE, GLOBUS_UNIVERSE,
			ATTR_GLOBUS_STATUS, G_UNSUBMITTED );
	}

	// Get all the new Grid job ads
	dprintf(D_FULLDEBUG,"ConnectQ w/ constraint=%s\n",expr_buf);
	Qmgr_connection *schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, true );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd! (in ADD_JOBS)\n");
		return FALSE;
	}

	next_ad = GetNextJobByConstraint( expr_buf, 1 );
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

		}

		delete next_ad;

		next_ad = GetNextJobByConstraint( expr_buf, 0 );
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

	JobsToSubmit.Rewind();
	JobsToSubmit.Next( new_job );

	if ( new_job->jobState == G_UNSUBMITTED ) {
		dprintf(D_FULLDEBUG,"Attempting to submit job %d.%d\n",
				new_job->procID.cluster,new_job->procID.proc);

		if ( new_job->start() == true ) {
			dprintf(D_ALWAYS,"Submited job %d.%d\n",
					new_job->procID.cluster,new_job->procID.proc);
		} else {
			dprintf(D_ALWAYS,"ERROR: Job %d.%d Submit failed because %s\n",
					new_job->procID.cluster,new_job->procID.proc,
					new_job->errorString());
			if (new_job->RSL) {
				dprintf(D_ALWAYS,"Job %d.%d RSL is: %s\n",
						new_job->procID.cluster,new_job->procID.proc,
						new_job->RSL);
			}
			// TODO : we just failed to submit a job; handle it better.
		}
	} else {
		dprintf(D_FULLDEBUG,
				"Job %d.%d already submitted to Globus\n",
				new_job->procID.cluster,new_job->procID.proc);
		// So here the job has already been submitted to Globus.
		// We need to place the contact string into our hashtable,
		// and setup to get async events.
		if ( new_job->jobContact ) {
			GlobusJob *tmp_job = NULL;
			// see if this contact string already in our hash table
			JobsByContact->lookup(HashKey(new_job->jobContact), tmp_job);
			if ( tmp_job == NULL ) {
				// nope, this string is not in our hash table. Register
				// a callback for it (and insert it in the hash table).
				new_job->callback_register();
			}
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

	dprintf(D_FULLDEBUG,"in REMOVE_JOBS_signalHandler\n");

	if(X509Proxy) {
		sprintf(owner_buf, "%s == \"%s\" && %s =?= \"%s\" ", ATTR_OWNER, Owner,
				ATTR_X509_USER_PROXY, X509Proxy);
	} else {
		sprintf(owner_buf, "%s == \"%s\" && %s =?= UNDEFINED ", ATTR_OWNER, 
						Owner, ATTR_X509_USER_PROXY);
	}

	sprintf( expr_buf, "%s && %s == %d && %s == %d",
		owner_buf, ATTR_JOB_UNIVERSE, GLOBUS_UNIVERSE,
		ATTR_JOB_STATUS, REMOVED );

	// Get all the new Grid job ads
	dprintf(D_FULLDEBUG,"ConnectQ w/ constraint=%s\n",expr_buf);
	Qmgr_connection *schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, true );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd! (in REMOVE_JOBS)\n");
		return FALSE;
	}

	next_ad = GetNextJobByConstraint( expr_buf, 1 );
	while ( next_ad != NULL ) {
		PROC_ID procID;
		GlobusJob *next_job;

		next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
		next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );

		if ( JobsByProcID->lookup( procID, next_job ) == 0 ) {
			// Should probably skip jobs we already have marked for removal

			JobsToSubmit.Delete( next_job );
			next_job->removedByUser = true;
			JobsToRemove.Append( next_job );
			num_ads++;

		} else {

			// If we don't know about the job, remove it immediately
			dprintf( D_ALWAYS, "Don't know about removed job %d.%d. Deleting it immediately\n", procID.cluster, procID.proc );
			DestroyProc( procID.cluster, procID.proc );

		}

		delete next_ad;
		next_ad = GetNextJobByConstraint( expr_buf, 0 );
	}

	DisconnectQ( schedd );
	dprintf(D_FULLDEBUG,"Fetched %d job ads from schedd\n",num_ads);

	if ( !JobsToRemove.IsEmpty() ) {
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

	if ( JobsToRemove.IsEmpty() ) {
		daemonCore->Unblock_Signal( GRIDMAN_REMOVE_JOBS );
		return TRUE;
	}

	JobsToRemove.Rewind();
	JobsToRemove.Next( curr_job );

	JobsToSubmit.Delete( curr_job );

	if ( curr_job->jobContact != NULL ) {

		dprintf(D_FULLDEBUG,"Attempting to remove job %d.%d\n",
				curr_job->procID.cluster,curr_job->procID.proc);

		if ( curr_job->cancel() == false ) {
			// Error
			dprintf(D_ALWAYS,"ERROR: Job cancel failed because %s\n",
					globus_gram_client_error_string(curr_job->errorCode));

			// TODO what now?  try later ?
		} else {
			// Success.  We don't actually remove it until
			// we receive the status update from the job manager.
			dprintf(D_ALWAYS,"Removed job %d.%d\n",
					curr_job->procID.cluster,curr_job->procID.proc);

			JobsToCommit.Delete( curr_job );
			removeJobUpdateEvents( curr_job );
		}
		/*
		  if ( curr_job->jobContact != NULL )
		  JobsByContact->remove( HashKey( curr_job->jobContact ) );
		  JobsByProcID->remove( curr_procid );
		  addJobUpdateEvent( curr_job, JOB_REMOVED );
		*/
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

	JobsToRemove.DeleteCurrent();

	if ( !JobsToRemove.IsEmpty() ) {
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
	GlobusJob *curr_job = NULL;
	PROC_ID procID;

	dprintf( D_FULLDEBUG, "in COMMIT_JOB_signalHandler\n" );

	if ( JobsToCommit.IsEmpty() ) {
		return TRUE;
	}

	JobsToCommit.Rewind();
	JobsToCommit.Next( curr_job );

	curr_job->commit();
	// Need to deal with failure...

	JobsToCommit.DeleteCurrent();

	if ( !JobsToCommit.IsEmpty() ) {
		dprintf(D_FULLDEBUG, "More jobs to commit, Resignalling COMMIT_JOB\n");

		daemonCore->Send_Signal( daemonCore->getpid(), GRIDMAN_COMMIT_JOB );
	}

	return TRUE;
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

	dprintf(D_FULLDEBUG,"in updateSchedd()\n");

	updateScheddTimerSet = false;

	JobUpdateEventQueue.Rewind();

	while ( JobUpdateEventQueue.Next( curr_event ) ) {

		curr_job = curr_event->job;
		handled = false;

		switch ( curr_event->event) {
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
			if ( !curr_job->executeLogged ) {
				WriteExecuteToUserLog( curr_job );
				curr_job->executeLogged = true;
			}
			if ( !curr_job->exitLogged ) {
				WriteTerminateToUserLog( curr_job );
				curr_job->exitLogged = true;
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
			CloseConnection();
			BeginTransaction();
			DestroyProc(curr_job->procID.cluster,
						curr_job->procID.proc);

			// Remove all knowledge we may have about this job
			JobsByProcID->remove( curr_job->procID );
			JobsToRemove.Delete( curr_job );
			delete curr_job;

			handled = true;
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
		case JOB_UE_DONE:
		case JOB_UE_FAILED:
		case JOB_UE_CANCELED:
			JobsToCommit.Append( curr_job );
			daemonCore->Send_Signal( daemonCore->getpid(),
									 GRIDMAN_COMMIT_JOB );
			handled = true;
			break;
		}

		if ( handled ) {
			JobUpdateEventQueue.DeleteCurrent();
			delete curr_event;
		}
	}

	return TRUE;
}

int
GridManager::globusPoll()
{
	if ( JobsToSubmit.IsEmpty() && JobsToRemove.IsEmpty() &&
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
	GlobusJob *next_job;

	//dprintf(D_FULLDEBUG,"in jobProbe elements=%d\n",JobsByContact->getNumElements() );
	if ( JobsByProcID->getNumElements() == 0 ) {
		dprintf( D_ALWAYS, "No jobs left, shutting down\n" );
		daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGTERM );
		return TRUE;
	}

	JobsByContact->startIterations();

	while ( JobsByContact->iterate( next_job ) != 0 ) {

		if ( next_job->jobContact != NULL ) {

			dprintf(D_FULLDEBUG,"calling jobProbe for job %d.%d\n",
						 next_job->procID.cluster, next_job->procID.proc );

			if ( next_job->probe() == false && next_job->errorCode ==
				     GLOBUS_GRAM_CLIENT_ERROR_CONTACTING_JOB_MANAGER ) {

				dprintf( D_ALWAYS, 
						"Globus JobManager unreachable for job %d.%d\n",
						 next_job->procID.cluster, next_job->procID.proc );
				// job manager is unreachable
				// resubmit or fail?
				// bad stuff
				// TODO: INSERT JAMIE'S RE-ATTACH TO JOBMANAGER CODE HERE

				// For now, check if we can contact the gatekeeper.
				// If we can contact the gatekeeper, and not the jobmanager,
				// we know the jobmanager is gone.  So either 
				// resubmit the job or place it on hold.
				// Note: It is possible to get the final callback from the
				//   jobmanager inside the ping call. If we do, then the
				//   jobmanager exitted normally and we shouldn't flag an
				//   error. For now, we test this by seeing if jobContact
				//   is still defined (meaning we still expect a running
				//   jobmanager on the other end).
				int err=globus_gram_client_ping(next_job->rmContact);
				if ( err == GLOBUS_SUCCESS && next_job->jobContact != NULL ) {
					// jobmanager definitely gone.
					// make it appear like it exited with status 1
					dprintf( D_ALWAYS, 
						"Job %d.%d exiting with status 1 because JobManager gone\n",
						 next_job->procID.cluster, next_job->procID.proc );
					next_job->exitValue = 1;
					next_job->callback(GLOBUS_GRAM_CLIENT_JOB_STATE_DONE);
				} 

			}

		}

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
			"Can't find record for globus job with contact %s on globus event %d\n",
				 job_contact, state );
		return;
	}

	dprintf( D_ALWAYS, "   job is %d.%d\n", this_job->procID.cluster,
			 this_job->procID.proc );

	this_job->callback( state, errorcode );

	dprintf(D_FULLDEBUG,"gramCallbackHandler() returning\n");
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

	dprintf( D_FULLDEBUG, "Writing execute record to user logfile=%s job=%d.%d owner=%s\n",
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

	dprintf( D_FULLDEBUG, "Writing abort record to user logfile=%s job=%d.%d owner=%s\n",
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

	dprintf( D_FULLDEBUG, "Writing terminate record to user logfile=%s job=%d.%d owner=%s\n",
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
