

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "my_username.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"

#include "globus_gram_client.h"

#include "gridmanager.h"


#define QMGMT_TIMEOUT 300

#define GLOBUS_POLL_INTERVAL	5
#define JOB_PROBE_INTERVAL		300
#define UPDATE_SCHEDD_DELAY		5

#define HASH_TABLE_SIZE			500

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


GridManager::GridManager()
{
	grabAllJobs = true;
	updateScheddTimerSet = false;
	Owner = NULL;
	ScheddAddr = NULL;
	gramCallbackContact = NULL;

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
	if ( gramCallbackContact ) {
		free( gramCallbackContact );
		gramCallbackContact = NULL;
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

	rc = globus_module_activate( GLOBUS_GRAM_CLIENT_MODULE );
	if ( rc != GLOBUS_SUCCESS ) {
		EXCEPT( "Failed to activate globus" );
	}

	rc = globus_gram_client_callback_allow( GridManager::gramCallbackHandler,
											this, &gramCallbackContact );
	if ( rc != GLOBUS_SUCCESS ) {
		EXCEPT( "Failed to activate globus callbacks" );
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

	daemonCore->Register_Timer( 0, JOB_PROBE_INTERVAL,
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
	Qmgr_connection *schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, true );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd!\n");
		return FALSE;
	}

	next_ad = GetNextJobByConstraint( expr_buf, 1 );
	while ( next_ad != NULL ) {
		new_ads.Insert( next_ad );
		next_ad = GetNextJobByConstraint( expr_buf, 0 );
	}

	DisconnectQ( schedd );

	grabAllJobs = false;

	// Try to submit the new jobs
	new_ads.Open();

	while ( ( next_ad = new_ads.Next() ) != NULL ) {

		GlobusJob *new_job = new GlobusJob( next_ad );

		JobsByProcID->insert( new_job->procID, new_job );

		if ( new_job->jobState == G_UNSUBMITTED ) {

			if ( new_job->submit( gramCallbackContact ) == true ) {

				JobsByContact->insert( HashKey( new_job->jobContact ), new_job );

			}

			needsUpdate( new_job, JOB_STATE | JOB_CONTACT );

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

	snprintf( expr_buf, 1024, "%s == \"%s\" && %s == %d && %s == %d",
			  ATTR_OWNER, Owner, ATTR_JOB_UNIVERSE, GLOBUS_UNIVERSE,
			  ATTR_JOB_STATUS, REMOVED );

	// Get all the new Grid job ads
	Qmgr_connection *schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, true );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd!\n");
		return FALSE;
	}

	next_ad = GetNextJobByConstraint( expr_buf, 1 );
	while ( next_ad != NULL ) {
		new_ads.Insert( next_ad );
		next_ad = GetNextJobByConstraint( expr_buf, 0 );
	}

	DisconnectQ( schedd );

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

			if ( curr_job->cancel() == false ) {

				// Error

			}

			if ( curr_job->jobContact != NULL )
				JobsByContact->remove( HashKey( curr_job->jobContact ) );
			JobsByProcID->remove( curr_procid );
			needsUpdate( curr_job, JOB_REMOVED );

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
	GlobusJob *curr_job;

	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, false );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd!\n");
		// Should we be retrying infinitely?
		daemonCore->Register_Timer( UPDATE_SCHEDD_DELAY,
				(TimerHandlercpp)&GridManager::updateSchedd,
				"updateSchedd", (Service*)this );
		return TRUE;
	}

	JobsToUpdate.Rewind();

	while ( JobsToUpdate.Next( curr_job ) ) {

		if ( curr_job->updateFlags & JOB_STATE ) {

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

		}

		if ( curr_job->updateFlags & JOB_CONTACT ) {

			SetAttributeString( curr_job->procID.cluster,
								curr_job->procID.proc,
								ATTR_GLOBUS_CONTACT_STRING,
								curr_job->jobContact );

		}

		if ( curr_job->updateFlags & JOB_REMOVED ) {

			// Send "removed" command

			delete curr_job;

			// Since we just deleted the current job, skip the stuff
			// at the loop bottom that references it.
			JobsToUpdate.DeleteCurrent();
			continue;

		}

		curr_job->updateFlags = 0;

		JobsToUpdate.DeleteCurrent();
	}

	DisconnectQ( schedd );

	updateScheddTimerSet = false;

	return TRUE;
}

int
GridManager::globusPoll()
{
fprintf(stderr,"globusPoll (%d)\n", JobsByContact->getNumElements());
	globus_poll();
	return TRUE;
}

int
GridManager::jobProbe()
{
fprintf(stderr,"jobProbe\n");
	GlobusJob *next_job;

	JobsByContact->startIterations();

	while ( JobsByContact->iterate( next_job ) != 0 ) {

		if ( next_job->jobContact != NULL ) {

			if ( next_job->probe() == false ) {
				dprintf( D_ALWAYS, "Globus jobmanager unreachable for job %d.%d\n",
						 next_job->procID.cluster, next_job->procID.proc );
				// job manager is unreachable
				// resubmit or fail?
				// bad stuff
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

fprintf(stderr,"callback: job %s, state %d\n", job_contact, state);
	// Find the right job object
	rc = this_->JobsByContact->lookup( HashKey( job_contact ), this_job );
	if ( rc != 0 || this_job == NULL ) {
		dprintf( D_ALWAYS, "Can't find record for globus job with contact %s on globus event %d\n",
				 job_contact, state );
		return;
	}

	if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_PENDING ) {
		if ( this_job->jobState != G_PENDING ) {
			this_job->jobState = G_PENDING;

			this_->needsUpdate( this_job, JOB_STATE );
		}
	} else if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_ACTIVE ) {
		if ( this_job->jobState != G_ACTIVE ) {
			this_job->jobState = G_ACTIVE;

			this_->needsUpdate( this_job, JOB_STATE );

			// put this in updateSchedd?
			this_->WriteExecuteToUserLog( this_job );
		}
	} else if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_FAILED ) {
		// Not sure what the right thing to do on a FAILED message from
		// globus. For now, we treat it like a completed job.

		// JobsByContact with a state of FAILED shouldn't be in the hash table,
		// but we'll check anyway.
		if ( this_job->jobState != G_FAILED ) {
			this_job->jobState = G_FAILED;

			this_->needsUpdate( this_job, JOB_STATE );

			// put the following stuff in updateSchedd?

			// userlog entry (what type of event is this?)

			// email saying that the job failed?

			this_->JobsByContact->remove( HashKey( this_job->jobContact ) );
		}
	} else if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_DONE ) {
		// JobsByContact with a state of DONE shouldn't be in the hash table,
		// but we'll check anyway.
		if ( this_job->jobState != G_DONE ) {
			this_job->jobState = G_DONE;

			this_->needsUpdate( this_job, JOB_STATE );

			// put the following in updateSchedd?

			this_->WriteTerminateToUserLog( this_job );

			// email saying the job is done?

			this_->JobsByContact->remove( HashKey( this_job->jobContact ) );
		}
	} else if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_SUSPENDED ) {
		if ( this_job->jobState != G_SUSPENDED ) {
			this_job->jobState = G_SUSPENDED;

			this_->needsUpdate( this_job, JOB_STATE );
		}
	} else {
		dprintf( D_ALWAYS, "Unknown globus job state %d for job %d.%d\n",
				 state, this_job->procID.cluster, this_job->procID.proc );
	}

fprintf(stderr,"callback returning\n");
}

void
GridManager::needsUpdate( GlobusJob *job, unsigned int flags )
{
    if ( !flags )
	return;

    if ( !job->updateFlags )
	JobsToUpdate.Append( job );

    job->updateFlags |= flags;

    if ( updateScheddTimerSet == false ) {
	daemonCore->Register_Timer( UPDATE_SCHEDD_DELAY,
		    (TimerHandlercpp)&GridManager::updateSchedd,
		    "updateSchedd", (Service*)this );
	updateScheddTimerSet = true;
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
