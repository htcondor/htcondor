

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "my_username.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"

#include "globus_gram_client.h"

#include "gm_common.h"
#include "gridmanager.h"


#define QMGMT_TIMEOUT 300

#define GLOBUS_POLL_INTERVAL	1
#define JOB_PROBE_INTERVAL		300
#define UPDATE_SCHEDD_DELAY		5

#define HASH_TABLE_SIZE			500


extern GridManager gridmanager;

List<JobUpdateEvent> JobUpdateEventQueue;
static bool updateScheddTimerSet = false;

// For developmental testing
int test_mode = 0;

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
template class List<GlobusJob *>;
template class Item<GlobusJob *>;
template class List<JobUpdateEvent>;
template class Item<JobUpdateEvent>;


void 
addJobUpdateEvent( GlobusJob *job, int event, int subtype = 0 )
{
	JobUpdateEvent *new_event = new JobUpdateEvent;
	new_event->job = job;
	new_event->event = event;
	new_event->subtype = subtype;

	JobUpdateEventQueue.Append( new_event );
	setUpdate();
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


GridManager::GridManager()
{
	grabAllJobs = true;
	updateScheddTimerSet = false;
	Owner = NULL;
	ScheddAddr = NULL;

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
	daemonCore->Register_Signal( ADD_JOBS, "AddJobs",
		(SignalHandlercpp)&GridManager::ADD_JOBS_signalHandler,
		"ADD_JOBS_signalHandler", (Service*)this, WRITE );

	daemonCore->Register_Signal( REMOVE_JOBS, "RemoveJobs",
		(SignalHandlercpp)&GridManager::REMOVE_JOBS_signalHandler,
		"REMOVE_JOBS_signalHandler", (Service*)this, WRITE );

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
	ClassAdList new_ads;
	ClassAd *next_ad;
	char expr_buf[1024];
	int num_ads = 0;

	dprintf(D_FULLDEBUG,"in ADD_JOBS_signalHandler\n");

	// Make sure we grab all Globus Universe jobs when we first start up
	// in case we're recovering from a shutdown/meltdown.
	if ( grabAllJobs ) {
		snprintf( expr_buf, 1024, "%s == \"%s\" && %s == %d",
				  ATTR_OWNER, Owner, ATTR_JOB_UNIVERSE, GLOBUS_UNIVERSE );
	} else {
		snprintf( expr_buf, 1024, "%s == \"%s\" && %s == %d && %s == %d",
				  ATTR_OWNER, Owner, ATTR_JOB_UNIVERSE, GLOBUS_UNIVERSE,
				  ATTR_GLOBUS_STATUS, G_UNSUBMITTED );
	}

	// Get all the new Grid job ads
	dprintf(D_FULLDEBUG,"ConnectQ w/ constraint=%s\n",expr_buf);
	Qmgr_connection *schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, true );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd!\n");
		return FALSE;
	}

	next_ad = GetNextJobByConstraint( expr_buf, 1 );
	while ( next_ad != NULL ) {
		new_ads.Insert( next_ad );
		num_ads++;
		next_ad = GetNextJobByConstraint( expr_buf, 0 );
	}

	DisconnectQ( schedd );
	dprintf(D_FULLDEBUG,"Fetched %d job ads from schedd\n",num_ads);

	grabAllJobs = false;

	// Try to submit the new jobs
	GlobusJob *new_job;
	PROC_ID procID;
	new_ads.Open();
	while ( ( next_ad = new_ads.Next() ) != NULL ) {

		next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
		next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );

		new_job = NULL;
		JobsByProcID->lookup( procID, new_job );
		if (!new_job) {
			// we have not seen this job before
			new_job = new GlobusJob( next_ad );
			ASSERT(new_job);
			JobsByProcID->insert( new_job->procID, new_job );
		}


		if ( new_job->jobState == G_UNSUBMITTED ) {
			dprintf(D_FULLDEBUG,"Attempting to submit job %d.%d\n",
				new_job->procID.cluster,new_job->procID.proc);

			if ( new_job->start() == true ) {
				JobsByContact->insert(HashKey(new_job->jobContact), new_job);
				dprintf(D_ALWAYS,"Submited job %d.%d\n",
					new_job->procID.cluster,new_job->procID.proc);
			} else {
				dprintf(D_ALWAYS,"ERROR: Job Submit failed because %s\n",
					new_job->errorString());
				// TODO : we just failed to submit a job; handle it better.
			}
		} else {
			dprintf(D_FULLDEBUG,
				"Job %d.%d already submitted to Globus\n",
				new_job->procID.cluster,new_job->procID.proc);
			// No worry about inserting the same job multiple
			// times into the JobsByContact table since all fetches from
			// the schedd after the first one require that jobState
			// is G_UNSUBMITTED.  So no need to do checks here.
			//
			// So here the job has already been submitted to Globus.
			// We need to place the contact string into our hashtable,
			// and setup to get async events.
			JobsByContact->insert(HashKey(new_job->jobContact), new_job);
			new_job->callback_register();
		}

		new_ads.Delete( next_ad );
		FreeJobAd( next_ad );
	}

	return TRUE;
}

int
GridManager::REMOVE_JOBS_signalHandler( int signal )
{
	ClassAdList new_ads;
	ClassAd *next_ad;
	char expr_buf[1024];
	int num_ads = 0;

	dprintf(D_FULLDEBUG,"in REMOVE_JOBS_signalHandler\n");

	snprintf( expr_buf, 1024, "%s == \"%s\" && %s == %d && %s == %d",
			  ATTR_OWNER, Owner, ATTR_JOB_UNIVERSE, GLOBUS_UNIVERSE,
			  ATTR_JOB_STATUS, REMOVED );

	// Get all the new Grid job ads
	dprintf(D_FULLDEBUG,"ConnectQ w/ constraint=%s\n",expr_buf);
	Qmgr_connection *schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, true );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd!\n");
		return FALSE;
	}

	next_ad = GetNextJobByConstraint( expr_buf, 1 );
	while ( next_ad != NULL ) {
		new_ads.Insert( next_ad );
		num_ads++;
		next_ad = GetNextJobByConstraint( expr_buf, 0 );
	}

	DisconnectQ( schedd );
	dprintf(D_FULLDEBUG,"Fetched %d job ads from schedd\n",num_ads);

	// Try to cancel the jobs
	new_ads.Open();

	while ( ( next_ad = new_ads.Next() ) != NULL ) {

		int rc;
		PROC_ID curr_procid;
		GlobusJob *curr_job;

		next_ad->LookupInteger( ATTR_CLUSTER_ID, curr_procid.cluster );
		next_ad->LookupInteger( ATTR_PROC_ID, curr_procid.proc );

		JobsByProcID->lookup( curr_procid, curr_job );
		if ( rc != 0 || curr_job == NULL ) {
			new_ads.Delete( next_ad );
			FreeJobAd( next_ad );
			continue;
		}

		if ( curr_job->jobContact != NULL ) {

			dprintf(D_FULLDEBUG,"Attempting to remove job %d.%d\n",
				curr_job->procID.cluster,curr_job->procID.proc);
			
			if ( curr_job->cancel() == false ) {

				// Error
				dprintf(D_ALWAYS,"ERROR: Job cancel failed because %s\n",
					globus_gram_client_error_string(curr_job->errorCode));

			}

			if ( curr_job->jobContact != NULL )
				JobsByContact->remove( HashKey( curr_job->jobContact ) );
			JobsByProcID->remove( curr_procid );
			addJobUpdateEvent( curr_job, JOB_REMOVED );
		}

		new_ads.Delete( next_ad );
		FreeJobAd( next_ad );

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

	dprintf(D_FULLDEBUG,"in updateSchedd()\n");

	JobUpdateEventQueue.Rewind();

	while ( JobUpdateEventQueue.Next( curr_event ) ) {
		curr_job = curr_event->job;

		switch ( curr_event->event) {
		case JOB_UE_ULOG_EXECUTE:
			WriteExecuteToUserLog( curr_job );

			break;
		case JOB_UE_ULOG_TERMINATE:
			WriteTerminateToUserLog( curr_job );

			break;
		case JOB_UE_EMAIL:
			break;
		case JOB_UE_CALLBACK:
			curr_job->callback();

			break;
		}

	}

	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, false );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd!\n");
		// Should we be retrying infinitely?
		daemonCore->Register_Timer( UPDATE_SCHEDD_DELAY,
				(TimerHandlercpp)&GridManager::updateSchedd,
				"updateSchedd", (Service*)this );
		return TRUE;
	}

	JobUpdateEventQueue.Rewind();

	while ( JobUpdateEventQueue.Next( curr_event ) ) {

		curr_job = curr_event->job;

		switch ( curr_event->event) {
		case JOB_UE_UPDATE_STATE:
			SetAttributeInt( curr_job->procID.cluster,
							 curr_job->procID.proc,
							 ATTR_GLOBUS_STATUS, curr_job->jobState );

			switch ( curr_job->jobState ) {
			case G_PENDING:
				// Send new job state command
				SetAttributeInt( curr_job->procID.cluster,
								 curr_job->procID.proc,
								 ATTR_JOB_STATUS, IDLE );
				break;
			case G_ACTIVE:
				// Send new job state command
				SetAttributeInt( curr_job->procID.cluster,
								 curr_job->procID.proc,
								 ATTR_JOB_STATUS, RUNNING );
				break;
			case G_FAILED:
				// Send new job state command
				SetAttributeInt( curr_job->procID.cluster,
								 curr_job->procID.proc,
								 ATTR_JOB_STATUS, COMPLETED );
				break;
			case G_DONE:
				// Send new job state command
				SetAttributeInt( curr_job->procID.cluster,
								 curr_job->procID.proc,
								 ATTR_JOB_STATUS, COMPLETED );
				break;
			case G_SUSPENDED:
				// Send new job state command
				SetAttributeInt( curr_job->procID.cluster,
								 curr_job->procID.proc,
								 ATTR_JOB_STATUS, IDLE );
				break;
			}

			break;
		case JOB_UE_UPDATE_CONTACT:
			SetAttributeString( curr_job->procID.cluster,
								curr_job->procID.proc,
								ATTR_GLOBUS_CONTACT_STRING,
								curr_job->jobContact );

			break;
		case JOB_UE_REMOVE_JOB:
			// Remove the job from the schedd queue

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
			if ( curr_job->jobContact ) {
				JobsByContact->remove(HashKey(curr_job->jobContact));
			}
			JobsByProcID->remove( curr_job->procID );
			delete curr_job;

			break;
		case JOB_UE_HOLD_JOB:
			// put job on hold

			break;
		}
		
		JobUpdateEventQueue.DeleteCurrent();
	}

	DisconnectQ( schedd );

	updateScheddTimerSet = false;


	return TRUE;
}

int
GridManager::globusPoll()
{
	globus_poll();
	return TRUE;
}

int
GridManager::jobProbe()
{
	GlobusJob *next_job;

	JobsByContact->startIterations();

	while ( JobsByContact->iterate( next_job ) != 0 ) {

		if ( next_job->jobContact != NULL ) {

			dprintf(D_FULLDEBUG,"calling jobProbe for job %d.%d\n",
						 next_job->procID.cluster, next_job->procID.proc );

			if ( next_job->probe() == false ) {
				dprintf( D_ALWAYS, 
						"Globus jobmanager unreachable for job %d.%d\n",
						 next_job->procID.cluster, next_job->procID.proc );
				// job manager is unreachable
				// resubmit or fail?
				// bad stuff
				// TODO: INSERT JAMIE'S RE-ATTACH TO JOBMANAGER CODE HERE
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

	this_job->callback( state, errorcode );

	if ( this_job->jobContact == NULL ) {
		this_->JobsByContact->remove( HashKey( job_contact ) );
	} else if ( strcmp( this_job->jobContact, job_contact ) != 0 ) {
		this_->JobsByContact->remove( HashKey( job_contact ) );
		this_->JobsByContact->insert( HashKey( this_job->jobContact ),
									  this_job );
	}

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

	// Normally, if the user log is specified with a relative pathname, it's
	// relative to the IWD of the job. But if we consider the IWD to be on
	// the remote machine, where on the local machine do we put the log
	// file? For now, assume the CWD.

	dprintf( D_FULLDEBUG, "Writing record to user logfile=%s owner=%s\n",
			 job->userLogFile, Owner );

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
GridManager::WriteTerminateToUserLog( GlobusJob *job )
{
	UserLog *ulog = InitializeUserLog( job );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

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
	event.returnValue = 0;

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_TERMINATED event\n" );
		return false;
	}

	return true;
}
