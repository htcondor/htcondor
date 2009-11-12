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
#include "baseshadow.h"
#include "condor_classad.h"      // for ClassAds.
#include "condor_qmgr.h"         // have to talk to schedd's Q manager
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_config.h"       // for param()
#include "condor_email.h"        // for (you guessed it) email stuff
#include "condor_version.h"
#include "condor_ver_info.h"
#include "enum_utils.h"
#include "condor_holdcodes.h"
#include "classad_helpers.h"

#include <math.h>

// these are declared static in baseshadow.h; allocate space here
UserLog BaseShadow::uLog;
BaseShadow* BaseShadow::myshadow_ptr = NULL;


// this appears at the bottom of this file:
extern "C" int display_dprintf_header(FILE *fp);

// some helper functions
int getJobAdExitCode(ClassAd *jad, int &exit_code);
int getJobAdExitedBySignal(ClassAd *jad, int &exited_by_signal);
int getJobAdExitSignal(ClassAd *jad, int &exit_signal);

BaseShadow::BaseShadow() {
	spool = NULL;
	fsDomain = uidDomain = NULL;
	jobAd = NULL;
	remove_requested = false;
	cluster = proc = -1;
	gjid = NULL;
	core_file_name = NULL;
	scheddAddr = NULL;
	job_updater = NULL;
	ASSERT( !myshadow_ptr );	// make cetain we're only instantiated once
	myshadow_ptr = this;
	exception_already_logged = false;
	began_execution = FALSE;
}

BaseShadow::~BaseShadow() {
	if (spool) free(spool);
	if (fsDomain) free(fsDomain);
	if (jobAd) FreeJobAd(jobAd);
	if (gjid) free(gjid); 
	if (scheddAddr) free(scheddAddr);
	if( job_updater ) delete job_updater;
}

void
BaseShadow::baseInit( ClassAd *job_ad, const char* schedd_addr, const char *xfer_queue_contact_info )
{
	int pending = FALSE;

	if( ! job_ad ) {
		EXCEPT("baseInit() called with NULL job_ad!");
	}
	jobAd = job_ad;

	if( ! is_valid_sinful(schedd_addr) ) {
		EXCEPT("schedd_addr not specified with valid address");
	}
	scheddAddr = strdup( schedd_addr );

	m_xfer_queue_contact_info = xfer_queue_contact_info;

	if ( !jobAd->LookupString(ATTR_OWNER, owner)) {
		EXCEPT("Job ad doesn't contain an %s attribute.", ATTR_OWNER);
	}

	if( !jobAd->LookupInteger(ATTR_CLUSTER_ID, cluster)) {
		EXCEPT("Job ad doesn't contain an %s attribute.", ATTR_CLUSTER_ID);
	}

	if( !jobAd->LookupInteger(ATTR_PROC_ID, proc)) {
		EXCEPT("Job ad doesn't contain an %s attribute.", ATTR_PROC_ID);
	}


		// Grab the GlobalJobId if we've got it.
	if( ! jobAd->LookupString(ATTR_GLOBAL_JOB_ID, &gjid) ) {
		gjid = NULL;
	}

	// grab the NT domain if we've got it
	jobAd->LookupString(ATTR_NT_DOMAIN, domain);
	if ( !jobAd->LookupString(ATTR_JOB_IWD, iwd)) {
		EXCEPT("Job ad doesn't contain an %s attribute.", ATTR_JOB_IWD);
	}

	if( !jobAd->LookupFloat(ATTR_BYTES_SENT, prev_run_bytes_sent) ) {
		prev_run_bytes_sent = 0;
	}
	if( !jobAd->LookupFloat(ATTR_BYTES_RECVD, prev_run_bytes_recvd) ) {
		prev_run_bytes_recvd = 0;
	}

		// construct the core file name we'd get if we had one.
	MyString tmp_name = iwd;
	tmp_name += DIR_DELIM_CHAR;
	tmp_name += "core.";
	tmp_name += cluster;
	tmp_name += '.';
	tmp_name += proc;
	core_file_name = strdup( tmp_name.Value() );

        // put the shadow's sinful string into the jobAd.  Helpful for
        // the mpi shadow, at least...and a good idea in general.
	MyString tmp_addr = ATTR_MY_ADDRESS;
	tmp_addr += "=\"";
	tmp_addr += daemonCore->InfoCommandSinfulString();
	tmp_addr += '"';
    if ( !jobAd->Insert( tmp_addr.Value() )) {
        EXCEPT( "Failed to insert %s!", ATTR_MY_ADDRESS );
    }

	DebugId = display_dprintf_header;
	
	config();

	initUserLog();

		// Make sure we've got enough swap space to run
	checkSwap();

		// register SIGUSR1 (condor_rm) for shutdown...
	daemonCore->Register_Signal( SIGUSR1, "SIGUSR1", 
		(SignalHandlercpp)&BaseShadow::handleJobRemoval, "HandleJobRemoval", 
		this);

	// handle system calls with Owner's privilege
// XXX this belong here?  We'll see...
	if ( !init_user_ids(owner.Value(), domain.Value())) {
		dprintf(D_ALWAYS, "init_user_ids() failed!\n");
		// uids.C will EXCEPT when we set_user_priv() now
		// so there's not much we can do at this point
	}
	set_user_priv();
	daemonCore->Register_Priv_State( PRIV_USER );

	dumpClassad( "BaseShadow::baseInit()", this->jobAd, D_JOB );

		// initialize the UserPolicy object
	shadow_user_policy.init( jobAd, this );

		// setup an object to keep our job ad updated to the schedd's
		// permanent job queue.  this clears all the dirty bits on our
		// copy of the classad, so anything we touch after this will
		// be updated to the schedd when appropriate.
	job_updater = new QmgrJobUpdater( jobAd, scheddAddr );

		// change directory; hold on failure
	if ( cdToIwd() == -1 ) {
		EXCEPT("Could not cd to initial working directory");
	}

		// CRUFT
		// we want this *after* we clear the dirty flags so that if we
		// change anything, we consider that change dirty so it'll get
		// updated the next time we connect to the job queue...
	checkFileTransferCruft();

		// check to see if this invocation of the shadow is just to write
		// a terminate event and exit since this job had been recorded as
		// pending termination, but somehow the job didn't leave the queue
		// and the schedd is trying to restart it again..
	if( jobAd->LookupInteger(ATTR_TERMINATION_PENDING, pending)) {
		if (pending == TRUE) {
			// If the classad of this job "thinks" that this job should be
			// finished already, let's enact that belief.
			// This function does not return.
			this->terminateJob(US_TERMINATE_PENDING);
		}
	}
}


void
BaseShadow::checkFileTransferCruft()
{
		/*
		  If this job was a) submitted by a pre-6.3.3 condor_submit,
		  b) unix and c) vanilla, it was submitted with an incorrect
		  default value of "ON_EXIT" for ATTR_TRANSFER_FILES.  So, if
		  all of those conditions are met, we want to change the value
		  to be "NEVER" instead, so that we treat this like the old
		  shadow would treat it and rely on a shared file system.
		*/
#ifndef WIN32
	int universe; 
	char* version = NULL;
	bool is_old = false;
	if( ! jobAd->LookupInteger(ATTR_JOB_UNIVERSE, universe) ) {
		universe = CONDOR_UNIVERSE_VANILLA;
	}
	if( universe != CONDOR_UNIVERSE_VANILLA ) {
			// nothing to do
		return;
	}
	jobAd->LookupString( ATTR_VERSION, &version );
	if( version ) {
		CondorVersionInfo ver( version, "JOB" );
		if( ! ver.built_since_version(6,3,3) ) {
			is_old = true;
		}
		free( version );
		version = NULL;
	} else {
		dprintf( D_FULLDEBUG, "Job has no %s, assuming pre version 6.3.3\n",
				 ATTR_VERSION ); 
		is_old = true;
	}	
	if( ! is_old ) {
			// if we're new enough, nothing else to do
		return;
	}

		// see if ATTR_TRANSFER_FILES is already set to "NEVER"... 
	bool already_never;
	char* tmp = NULL;
	jobAd->LookupString( ATTR_TRANSFER_FILES, &tmp );
	if( tmp ) {
		already_never = ( stricmp(tmp, "NEVER") == 0 );
		free( tmp );
		if( already_never ) {
				// already have the right value, don't bother changing
				// it and updating the job queue, etc.
			return;
		}
	}

		// if we're still here, we've hit the nasty case, so change
		// the value...
	MyString new_attr;
	new_attr += ATTR_TRANSFER_FILES;
	new_attr += " = \"NEVER\"";

	dprintf( D_FULLDEBUG, "Unix Vanilla job is pre version 6.3.3, "
			 "setting '%s'\n", new_attr.Value() );

	if( ! jobAd->Insert(new_attr.Value()) ) {
		EXCEPT( "Insert of '%s' into job ad failed!", new_attr.Value() );
	}

		// also, add it to the list of attributes we want to update,
		// so we change it in the job queue, too.
	job_updater->watchAttribute( ATTR_TRANSFER_FILES );

#endif /* ! WIN32 */

}


void BaseShadow::config()
{
	char *tmp;

	if (spool) free(spool);
	spool = param("SPOOL");
	if (!spool) {
		EXCEPT("SPOOL not specified in config file.");
	}

	if (fsDomain) free(fsDomain);
	fsDomain = param( "FILESYSTEM_DOMAIN" );
	if (!fsDomain) {
		EXCEPT("FILESYSTEM_DOMAIN not specified in config file.");
	}

	if (uidDomain) free(uidDomain);
	uidDomain = param( "UID_DOMAIN" );
	if (!uidDomain) {
		EXCEPT("UID_DOMAIN not specified in config file.");
	}

	reconnect_ceiling = param_integer( "RECONNECT_BACKOFF_CEILING", 300 );

	reconnect_e_factor = 0.0;
	reconnect_e_factor = param_double( "RECONNECT_BACKOFF_FACTOR", 2.0, 0.0 );
	if( !reconnect_e_factor ) {
    	reconnect_e_factor = 2.0;
    }

	m_cleanup_retry_tid = -1;
	m_num_cleanup_retries = 0;
		// NOTE: these config knobs are very similar to
		// LOCAL_UNIVERSE_MAX_JOB_CLEANUP_RETRIES and
		// LOCAL_UNIVERSE_JOB_CLEANUP_RETRY_DELAY in the local starter.
	m_max_cleanup_retries = param_integer("SHADOW_MAX_JOB_CLEANUP_RETRIES", 5);
	m_cleanup_retry_delay = param_integer("SHADOW_JOB_CLEANUP_RETRY_DELAY", 30);

	m_lazy_queue_update = param_boolean("SHADOW_LAZY_QUEUE_UPDATE", true);
}


int BaseShadow::cdToIwd() {
	if (chdir(iwd.Value()) < 0) {
		int chdir_errno = errno;
		dprintf(D_ALWAYS, "\n\nPath does not exist.\n"
				"He who travels without bounds\n"
				"Can't locate data.\n\n" );
		MyString hold_reason;
		hold_reason.sprintf("Cannot access initial working directory %s: %s",
		                    iwd.Value(), strerror(chdir_errno));
		dprintf( D_ALWAYS, "%s\n",hold_reason.Value());
		holdJob(hold_reason.Value(),CONDOR_HOLD_CODE_IwdError,chdir_errno);
		return -1;
	}
	return 0;
}


void
BaseShadow::shutDown( int reason ) 
{
		// exit now if there is no job ad
	if ( !getJobAd() ) {
		DC_Exit( reason );
	}
	
		// if we are being called from the exception handler, return
		// now to prevent infinite loop in case we call EXCEPT below.
	if ( reason == JOB_EXCEPTION ) {
		return;
	}

		// Only if the job is trying to leave the queue should we
		// evaluate the user job policy...
	if( reason == JOB_EXITED || reason == JOB_COREDUMPED ) {
		shadow_user_policy.checkAtExit();
	}
	else {
		// if we aren't trying to evaluate the user's policy, we just
		// want to evict this job.
		evictJob( reason );
	}
}


int
BaseShadow::nextReconnectDelay( int attempts )
{
	if( ! attempts ) {
			// first time, do it right away
		return 0;
	}
	int n = (int)ceil(::pow(reconnect_e_factor, (attempts+2)));
	if( n > reconnect_ceiling || n < 0 ) {
		n = reconnect_ceiling;
	}
	return n;
}


void
BaseShadow::reconnectFailed( const char* reason )
{
		// try one last time to release the claim, write a UserLog event
		// about it, and exit with a special status. 
	dprintf( D_ALWAYS, "Reconnect FAILED: %s\n", reason );
	
	logReconnectFailedEvent( reason );

		// does not return
	DC_Exit( JOB_SHOULD_REQUEUE );
}


void
BaseShadow::holdJob( const char* reason, int hold_reason_code, int hold_reason_subcode )
{
	dprintf( D_ALWAYS, "Job %d.%d going into Hold state (code %d,%d): %s\n", 
			 getCluster(), getProc(), hold_reason_code, hold_reason_subcode,reason );

	if( ! jobAd ) {
		dprintf( D_ALWAYS, "In HoldJob() w/ NULL JobAd!" );
		DC_Exit( JOB_SHOULD_HOLD );
	}

		// cleanup this shadow (kill starters, etc)
	cleanUp();

		// Put the reason in our job ad.
	jobAd->Assign( ATTR_HOLD_REASON, reason );
	jobAd->Assign( ATTR_HOLD_REASON_CODE, hold_reason_code );
	jobAd->Assign( ATTR_HOLD_REASON_SUBCODE, hold_reason_subcode );

		// try to send email (if the user wants it)
	emailHoldEvent( reason );

		// update the job queue for the attributes we care about
	if( !updateJobInQueue(U_HOLD) ) {
			// trouble!  TODO: should we do anything else?
		dprintf( D_ALWAYS, "Failed to update job queue!\n" );
	}

		// finally, exit and tell the schedd what to do
	DC_Exit( JOB_SHOULD_HOLD );
}


void
BaseShadow::removeJob( const char* reason )
{
	if( ! jobAd ) {
		dprintf( D_ALWAYS, "In removeJob() w/ NULL JobAd!" );
	}
	dprintf( D_ALWAYS, "Job %d.%d is being removed: %s\n", 
			 getCluster(), getProc(), reason );

		// cleanup this shadow (kill starters, etc)
	cleanUp();

		// Put the reason in our job ad.
	int size = strlen( reason ) + strlen( ATTR_REMOVE_REASON ) + 4;
	char* buf = (char*)malloc( size * sizeof(char) );
	if( ! buf ) {
		EXCEPT( "Out of memory!" );
	}
	sprintf( buf, "%s=\"%s\"", ATTR_REMOVE_REASON, reason );
	jobAd->Insert( buf );
	free( buf );

	emailRemoveEvent( reason );

		// update the job ad in the queue with some important final
		// attributes so we know what happened to the job when using
		// condor_history...
	if( !updateJobInQueue(U_REMOVE) ) {
			// trouble!  TODO: should we do anything else?
		dprintf( D_ALWAYS, "Failed to update job queue!\n" );
	}

		// does not return.
	DC_Exit( JOB_SHOULD_REMOVE );
}

void
BaseShadow::retryJobCleanup( void )
{
	m_num_cleanup_retries++;
	if (m_num_cleanup_retries > m_max_cleanup_retries) {
		dprintf(D_ALWAYS,
		        "Maximum number of job cleanup retry attempts "
		        "(SHADOW_MAX_JOB_CLEANUP_RETRIES=%d) reached"
		        "; Forcing job requeue!\n",
		        m_max_cleanup_retries);
		DC_Exit(JOB_SHOULD_REQUEUE);
	}
	ASSERT(m_cleanup_retry_tid == -1);
	m_cleanup_retry_tid = daemonCore->Register_Timer(m_cleanup_retry_delay, 0,
					(TimerHandlercpp)&BaseShadow::retryJobCleanupHandler,
					"retry job cleanup", this);
	dprintf(D_FULLDEBUG, "Will retry job cleanup in "
	        "SHADOW_JOB_CLEANUP_RETRY_DELAY=%d seconds\n",
	        m_cleanup_retry_delay);
}


int
BaseShadow::retryJobCleanupHandler( void )
{
	m_cleanup_retry_tid = -1;
	dprintf(D_ALWAYS, "Retrying job cleanup, calling terminateJob()\n");
	terminateJob();
	return TRUE;
}

void
BaseShadow::terminateJob( update_style_t kind ) // has a default argument of US_NORMAL
{
	int reason;
	bool signaled;
	MyString str;

	if( ! jobAd ) {
		dprintf( D_ALWAYS, "In terminateJob() w/ NULL JobAd!" );
	}

	/* The first thing we do is record that we are in a termination pending
		state. */
	if (kind == US_NORMAL) {
		str.sprintf("%s = TRUE", ATTR_TERMINATION_PENDING);
		jobAd->Insert(str.Value());
	}

	if (kind == US_TERMINATE_PENDING) {
		// In this case, the job had already completed once and the
		// status had been saved to the job queue, however, for
		// some reason, the shadow didn't exit with a good value and
		// the job had been requeued. When this style of update
		// is used, it is a shortcut from the very birth of the shadow
		// to here, and so there will not be a remote resource or
		// anything like that set up. In this situation, we just
		// want to write the log event and mail the user and exit
		// with a good exit code so the schedd removes the job from
		// the queue. If for some reason the logging fails once again,
		// the process continues to repeat. 
		// This means at least once semantics for the termination event
		// and user email, but at no time should the job actually execute
		// again.

		int exited_by_signal = FALSE;
		int exit_signal = 0;
		int exit_code = 0;

		getJobAdExitedBySignal(jobAd, exited_by_signal);
		if (exited_by_signal == TRUE) {
			getJobAdExitSignal(jobAd, exit_signal);
		} else {
			getJobAdExitCode(jobAd, exit_code);
		}

		if (exited_by_signal == TRUE) {
			reason = JOB_COREDUMPED;
			str.sprintf("%s = \"%s\"", ATTR_JOB_CORE_FILENAME, core_file_name);
			jobAd->Insert(str.Value());
		} else {
			reason = JOB_EXITED;
		}

		dprintf( D_ALWAYS, "Job %d.%d terminated: %s %d\n",
	 		getCluster(), getProc(), 
	 		exited_by_signal? "killed by signal" : "exited with status",
	 		exited_by_signal ? exit_signal : exit_code );
		
			// write stuff to user log, but get values from jobad
		logTerminateEvent( reason, kind );

			// email the user, but get values from jobad
		emailTerminateEvent( reason, kind );

		DC_Exit( reason );
	}

	// the default path when kind == US_NORMAL

	// cleanup this shadow (kill starters, etc)
	cleanUp();

	reason = getExitReason();
	signaled = exitedBySignal();

	/* also store the corefilename into the jobad so we can recover this 
		during a termination pending scenario. */
	if( reason == JOB_COREDUMPED ) {
		str.sprintf("%s = \"%s\"", ATTR_JOB_CORE_FILENAME, getCoreName());
		jobAd->Insert(str.Value());
	}

    // Update final Job committed time
    int last_ckpt_time = 0;
    jobAd->LookupInteger(ATTR_LAST_CKPT_TIME, last_ckpt_time);
    int current_start_time = 0;
    jobAd->LookupInteger(ATTR_JOB_CURRENT_START_DATE, current_start_time);
    int int_value = (last_ckpt_time > current_start_time) ?
                        last_ckpt_time : current_start_time;

    if( int_value > 0 ) {
        int job_committed_time = 0;
        jobAd->LookupInteger(ATTR_JOB_COMMITTED_TIME, job_committed_time);
        job_committed_time += (int)time(NULL) - int_value;
        jobAd->Assign(ATTR_JOB_COMMITTED_TIME, job_committed_time);
    }


	// update the job ad in the queue with some important final
	// attributes so we know what happened to the job when using
	// condor_history...
	if( !updateJobInQueue(U_TERMINATE) ) {
		dprintf( D_ALWAYS, 
				 "Failed to perform final update to job queue!\n");
		retryJobCleanup();
		return;
	}

	// Let's maximize the effectiveness of that queue synchronization and
	// only record the job as done if the update to the queue was successful.
	// If any of these next operations fail and the shadow exits with an
	// exit code which causes the job to get requeued, it will be in the
	// "terminate pending" state marked by the ATTR_TERMINATION_PENDING
	// attribute.

	dprintf( D_ALWAYS, "Job %d.%d terminated: %s %d\n",
	 	getCluster(), getProc(), 
	 	signaled ? "killed by signal" : "exited with status",
	 	signaled ? exitSignal() : exitCode() );

	// write stuff to user log:
	logTerminateEvent( reason );

	// email the user
	emailTerminateEvent( reason );

	if( reason == JOB_EXITED && claimIsClosing() ) {
			// Startd not accepting any more jobs on this claim.
			// We do this here to avoid having to treat this case
			// identically to JOB_EXITED in the code leading up to
			// this point.
		dprintf(D_FULLDEBUG,"Startd is closing claim, so no more jobs can be run on it.\n");
		reason = JOB_EXITED_AND_CLAIM_CLOSING;
	}

	// does not return.
	DC_Exit( reason );
}


void
BaseShadow::evictJob( int reason )
{
	MyString from_where;
	MyString machine;
	if( getMachineName(machine) ) {
		from_where.sprintf(" from %s",machine.Value());
	}
	dprintf( D_ALWAYS, "Job %d.%d is being evicted%s\n",
			 getCluster(), getProc(), from_where.Value() );

	if( ! jobAd ) {
		dprintf( D_ALWAYS, "In evictJob() w/ NULL JobAd!" );
		DC_Exit( reason );
	}

		// cleanup this shadow (kill starters, etc)
	cleanUp();

		// write stuff to user log:
	logEvictEvent( reason );

		// record the time we were vacated into the job ad 
	char buf[64];
	sprintf( buf, "%s = %d", ATTR_LAST_VACATE_TIME, (int)time(0) ); 
	jobAd->Insert( buf );

		// update the job ad in the queue with some important final
		// attributes so we know what happened to the job when using
		// condor_history...
	if( !updateJobInQueue(U_EVICT) ) {
			// trouble!  TODO: should we do anything else?
		dprintf( D_ALWAYS, "Failed to update job queue!\n" );
	}

		// does not return.
	DC_Exit( reason );
}


void
BaseShadow::requeueJob( const char* reason )
{
	if( ! jobAd ) {
		dprintf( D_ALWAYS, "In requeueJob() w/ NULL JobAd!" );
	}
	dprintf( D_ALWAYS, 
			 "Job %d.%d is being put back in the job queue: %s\n", 
			 getCluster(), getProc(), reason );

		// cleanup this shadow (kill starters, etc)
	cleanUp();

		// Put the reason in our job ad.
	int size = strlen( reason ) + strlen( ATTR_REQUEUE_REASON ) + 4;
	char* buf = (char*)malloc( size * sizeof(char) );
	if( ! buf ) {
		EXCEPT( "Out of memory!" );
	}
	sprintf( buf, "%s=\"%s\"", ATTR_REQUEUE_REASON, reason );
	jobAd->Insert( buf );
	free( buf );

		// write stuff to user log:
	logRequeueEvent( reason );

		// update the job ad in the queue with some important final
		// attributes so we know what happened to the job when using
		// condor_history...
	if( !updateJobInQueue(U_REQUEUE) ) {
			// trouble!  TODO: should we do anything else?
		dprintf( D_ALWAYS, "Failed to update job queue!\n" );
	}

		// does not return.
	DC_Exit( JOB_SHOULD_REQUEUE );
}


void
BaseShadow::emailHoldEvent( const char* reason ) 
{
	Email mailer;
	mailer.sendHold( jobAd, reason );
}


void
BaseShadow::emailRemoveEvent( const char* reason ) 
{
	Email mailer;
	mailer.sendRemove( jobAd, reason );
}


FILE*
BaseShadow::emailUser( const char *subjectline )
{
	dprintf(D_FULLDEBUG, "BaseShadow::emailUser() called.\n");
	if( !jobAd ) {
		return NULL;
	}
	return email_user_open( jobAd, subjectline );
}


void BaseShadow::initUserLog()
{
	MyString logfilename;
	int  use_xml;
	if ( getPathToUserLog(jobAd, logfilename) ) {
		uLog.initialize (owner.Value(), domain.Value(), logfilename.Value(), cluster, proc, 0, gjid);
		if (jobAd->LookupBool(ATTR_ULOG_USE_XML, use_xml)
			&& use_xml) {
			uLog.setUseXML(true);
		} else {
			uLog.setUseXML(false);
		}
		dprintf(D_FULLDEBUG, "%s = %s\n", ATTR_ULOG_FILE, logfilename.Value());
	} else {
		dprintf(D_FULLDEBUG, "no %s found\n", ATTR_ULOG_FILE);
	}
}


// returns TRUE if attribute found.
int getJobAdExitCode(ClassAd *jad, int &exit_code)
{
	if( ! jad->LookupInteger(ATTR_ON_EXIT_CODE, exit_code) ) {
		return FALSE;
	}

	return TRUE;
}

// returns TRUE if attribute found.
int getJobAdExitedBySignal(ClassAd *jad, int &exited_by_signal)
{
	if( ! jad->LookupInteger(ATTR_ON_EXIT_BY_SIGNAL, exited_by_signal) ) {
		return FALSE;
	}

	return TRUE;
}

// returns TRUE if attribute found.
int getJobAdExitSignal(ClassAd *jad, int &exit_signal)
{
	if( ! jad->LookupInteger(ATTR_ON_EXIT_SIGNAL, exit_signal) ) {
		return FALSE;
	}

	return TRUE;
}

// kind defaults to US_NORMAL.
void
BaseShadow::logTerminateEvent( int exitReason, update_style_t kind )
{
	struct rusage run_remote_rusage;
	JobTerminatedEvent event;
	MyString corefile;

	memset( &run_remote_rusage, 0, sizeof(struct rusage) );

	switch( exitReason ) {
	case JOB_EXITED:
	case JOB_COREDUMPED:
		break;
	default:
		dprintf( D_ALWAYS, 
				 "logTerminateEvent with unknown reason (%d), aborting\n",
				 exitReason ); 
		return;
	}

	if (kind == US_TERMINATE_PENDING) {

		float float_value;
		int exited_by_signal = FALSE;
		int exit_signal = 0;
		int exit_code = 0;

		getJobAdExitedBySignal(jobAd, exited_by_signal);
		if (exited_by_signal == TRUE) {
			getJobAdExitSignal(jobAd, exit_signal);
			event.normal = false;
			event.signalNumber = exit_signal;
		} else {
			getJobAdExitCode(jobAd, exit_code);
			event.normal = true;
			event.returnValue = exit_code;
		}

		/* grab usage information out of job ad */
		if( jobAd->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, float_value) ) {
			run_remote_rusage.ru_stime.tv_sec = (int) float_value;
		}

		if( jobAd->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, float_value) ) {
			run_remote_rusage.ru_utime.tv_sec = (int) float_value;
		}

		event.run_remote_rusage = run_remote_rusage;
		event.total_remote_rusage = run_remote_rusage;
	
		/*
		  we want to log the events from the perspective of the user
		  job, so if the shadow *sent* the bytes, then that means the
		  user job *received* the bytes
		*/
		jobAd->LookupFloat(ATTR_BYTES_SENT, event.recvd_bytes);
		jobAd->LookupFloat(ATTR_BYTES_RECVD, event.sent_bytes);

		event.total_recvd_bytes = event.recvd_bytes;
		event.total_sent_bytes = event.sent_bytes;
	
		if( exited_by_signal == TRUE ) {
			jobAd->LookupString(ATTR_JOB_CORE_FILENAME, corefile);
			event.setCoreFile( corefile.Value() );
		}

		if (!uLog.writeEvent (&event,jobAd)) {
			dprintf (D_ALWAYS,"Unable to log "
				 	"ULOG_JOB_TERMINATED event\n");
		}

		return;
	}

	// the default kind == US_NORMAL path

	run_remote_rusage = getRUsage();
	
	if( exitedBySignal() ) {
		event.normal = false;
		event.signalNumber = exitSignal();
	} else {
		event.normal = true;
		event.returnValue = exitCode();
	}

		// TODO: fill in local/total rusage
		// event.run_local_rusage = r;
	event.run_remote_rusage = run_remote_rusage;
		// event.total_local_rusage = r;
	event.total_remote_rusage = run_remote_rusage;
	
		/*
		  we want to log the events from the perspective of the user
		  job, so if the shadow *sent* the bytes, then that means the
		  user job *received* the bytes
		*/
	event.recvd_bytes = bytesSent();
	event.sent_bytes = bytesReceived();

	event.total_recvd_bytes = prev_run_bytes_recvd + bytesSent();
	event.total_sent_bytes = prev_run_bytes_sent + bytesReceived();
	
	if( exitReason == JOB_COREDUMPED ) {
		event.setCoreFile( core_file_name );
	}
	
	if (!uLog.writeEvent (&event,jobAd)) {
		dprintf (D_ALWAYS,"Unable to log "
				 "ULOG_JOB_TERMINATED event\n");
	}
}


void
BaseShadow::logEvictEvent( int exitReason )
{
	struct rusage run_remote_rusage;
	memset( &run_remote_rusage, 0, sizeof(struct rusage) );

	run_remote_rusage = getRUsage();

	switch( exitReason ) {
	case JOB_CKPTED:
	case JOB_NOT_CKPTED:
	case JOB_KILLED:
		break;
	default:
		dprintf( D_ALWAYS, 
				 "logEvictEvent with unknown reason (%d), aborting\n",
				 exitReason ); 
		return;
	}

	JobEvictedEvent event;
	event.checkpointed = (exitReason == JOB_CKPTED);
	
		// TODO: fill in local rusage
		// event.run_local_rusage = ???
			
		// remote rusage
	event.run_remote_rusage = run_remote_rusage;
	
		/*
		  we want to log the events from the perspective of the user
		  job, so if the shadow *sent* the bytes, then that means the
		  user job *received* the bytes
		*/
	event.recvd_bytes = bytesSent();
	event.sent_bytes = bytesReceived();
	
	if (!uLog.writeEvent (&event,jobAd)) {
		dprintf (D_ALWAYS, "Unable to log ULOG_JOB_EVICTED event\n");
	}
}


void
BaseShadow::logRequeueEvent( const char* reason )
{
	struct rusage run_remote_rusage;
	memset( &run_remote_rusage, 0, sizeof(struct rusage) );

	run_remote_rusage = getRUsage();

	int exit_reason = getExitReason();

	JobEvictedEvent event;

	event.terminate_and_requeued = true;

	if( exitedBySignal() ) {
		event.normal = false;
		event.signal_number = exitSignal();
	} else {
		event.normal = true;
		event.return_value = exitCode();
	}
			
	if( exit_reason == JOB_COREDUMPED ) {
		event.setCoreFile( core_file_name );
	}

	if( reason ) {
		event.setReason( reason );
	}

		// TODO: fill in local rusage
		// event.run_local_rusage = r;
	event.run_remote_rusage = run_remote_rusage;

		/* we want to log the events from the perspective 
		   of the user job, so if the shadow *sent* the 
		   bytes, then that means the user job *received* 
		   the bytes */
	event.recvd_bytes = bytesSent();
	event.sent_bytes = bytesReceived();
	
	if (!uLog.writeEvent (&event,jobAd)) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_EVICTED "
				 "(and requeued) event\n" );
	}
}


void
BaseShadow::checkSwap( void )
{
	int	reserved_swap, free_swap;
		// Reserved swap is specified in megabytes
	reserved_swap = param_integer( "RESERVED_SWAP", 5 );
	reserved_swap *= 1024;

	if( reserved_swap == 0 ) {
			// We're not supposed to care about swap space at all, so
			// none of the rest of the checks matter at all.
		return;
	}

	free_swap = sysapi_swap_space();

	dprintf( D_FULLDEBUG, "*** Reserved Swap = %d\n", reserved_swap );
	dprintf( D_FULLDEBUG, "*** Free Swap = %d\n", free_swap );

	if( free_swap < reserved_swap ) {
		dprintf( D_ALWAYS, "Not enough reserved swap space\n" );
		DC_Exit( JOB_NO_MEM );
	}
}	


// Note: log_except is static
void
BaseShadow::log_except(const char *msg)
{
	// log shadow exception event
	ShadowExceptionEvent event;
	bool exception_already_logged = false;

	if(!msg) msg = "";
	sprintf(event.message, msg);

	if ( BaseShadow::myshadow_ptr ) {
		BaseShadow *shadow = BaseShadow::myshadow_ptr;

		// we want to log the events from the perspective of the
		// user job, so if the shadow *sent* the bytes, then that
		// means the user job *received* the bytes
		event.recvd_bytes = shadow->bytesSent();
		event.sent_bytes = shadow->bytesReceived();
		exception_already_logged = shadow->exception_already_logged;

		if (shadow->began_execution) {
			event.began_execution = TRUE;
		}

	} else {
		event.recvd_bytes = 0.0;
		event.sent_bytes = 0.0;
	}

	if (!exception_already_logged && !uLog.writeEvent (&event,NULL))
	{
		::dprintf (D_ALWAYS, "Unable to log ULOG_SHADOW_EXCEPTION event\n");
	}
}


bool
BaseShadow::updateJobAttr( const char *name, const char *expr )
{
	return job_updater->updateAttr( name, expr, false );
}


bool
BaseShadow::updateJobAttr( const char *name, int value )
{
	return job_updater->updateAttr( name, value, false );
}


bool
BaseShadow::updateJobInQueue( update_t type )
{
		// insert the bytes sent/recv'ed by this job into our job ad.
		// we want this from the perspective of the job, so it's
		// backwards from the perspective of the shadow.  if this
		// value hasn't changed, it won't show up as dirty and we
		// won't actually connect to the job queue for it.  we do this
		// here since we want it for all kinds of updates...
	MyString buf;
	buf.sprintf( "%s = %f", ATTR_BYTES_SENT, (prev_run_bytes_sent +
											  bytesReceived()) );
	jobAd->Insert( buf.Value() );
	buf.sprintf( "%s = %f", ATTR_BYTES_RECVD, (prev_run_bytes_recvd +
											   bytesSent()) );
	jobAd->Insert( buf.Value() );

		// Now that the ad is current, just let our QmgrJobUpdater
		// object take care of the rest...
	return job_updater->updateJob( type );
}


void
BaseShadow::evalPeriodicUserPolicy( void )
{
	shadow_user_policy.checkPeriodic();
}


/**
 * Shared code that should be run once the shadow is sure that
 * everything it's watching over has actually started running.  In the
 * uni-shadow case (vanilla, java, etc) this is called immediately
 * as soon as the starter sends the CONDOR_begin_execution RSC.  For
 * multi-shadow (parallel, MPI), this is invoked once all of the
 * starters have reported in.
 */
void
BaseShadow::resourceBeganExecution( RemoteResource* /* rr */ )
{
		// Set our flag to remember we've really started.
	began_execution = true;

		// Start the timer for the periodic user job policy evaluation.
	shadow_user_policy.startTimer();
		
		// Start the timer for updating the job queue for this job.
	startQueueUpdateTimer();

		// Update our copy of NumJobStarts, so that the periodic user
		// policy expressions, etc, all have the correct value.
	int job_start_cnt = 0;
	jobAd->LookupInteger(ATTR_NUM_JOB_STARTS, job_start_cnt);
	job_start_cnt++;
	jobAd->Assign(ATTR_NUM_JOB_STARTS, job_start_cnt);
	dprintf(D_FULLDEBUG, "Set %s to %d\n", ATTR_NUM_JOB_STARTS, job_start_cnt);

		// Update NumJobStarts in the schedd.  We honor a config knob
		// for this since there's a big trade-off: if we update
		// agressively and fsync() it to the job queue as soon as we
		// hear from the starter, the semantics are about as solid as
		// we can hope for, but it's a schedd scalability problem.  If
		// we do a lazy update, there's no additional cost to the
		// schedd, but it means that condor_q and quill won't see the
		// change for N minutes, and if we happen to crash during that
		// time, the attribute is never incremented.  However, the
		// semantics aren't 100% solid, even if we don't update lazy,
		// and since the copy in RAM is already updated, all the
		// periodic user policy expressions will work right, so the
		// default is to do it lazy.
	if (m_lazy_queue_update) {
			// For lazy update, we just want to make sure the
			// job_updater object knows about this attribute (which we
			// already updated our copy of).
		job_updater->watchAttribute(ATTR_NUM_JOB_STARTS);
	}
	else {
			// They want it now, so do the qmgmt operation directly.
		updateJobAttr(ATTR_NUM_JOB_STARTS, job_start_cnt);
	}
}


const char*
BaseShadow::getCoreName( void )
{
	if( core_file_name ) {
		return core_file_name;
	} 
	return "unknown";
}


void
BaseShadow::startQueueUpdateTimer( void )
{
	job_updater->startUpdateTimer();
}


void
BaseShadow::publishShadowAttrs( ClassAd* ad )
{
	MyString tmp;
	tmp = ATTR_SHADOW_IP_ADDR;
	tmp += "=\"";
	tmp += daemonCore->InfoCommandSinfulString();
    tmp += '"';
	ad->Insert( tmp.Value() );

	tmp = ATTR_SHADOW_VERSION;
	tmp += "=\"";
	tmp += CondorVersion();
    tmp += '"';
	ad->Insert( tmp.Value() );

	char* my_uid_domain = param( "UID_DOMAIN" );
	if( my_uid_domain ) {
		tmp = ATTR_UID_DOMAIN;
		tmp += "=\"";
		tmp += my_uid_domain;
		tmp += '"';
		ad->Insert( tmp.Value() );
		free( my_uid_domain );
	}
}


void BaseShadow::dprintf_va( int flags, char* fmt, va_list args )
{
		// Print nothing in this version.  A subclass like MPIShadow
		// might like to say ::dprintf( flags, "(res %d)"
	::_condor_dprintf_va( flags, fmt, args );
}

void BaseShadow::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	this->dprintf_va( flags, fmt, args );
	va_end( args );
}

// This is declared in main.C, and is a pointer to one of the 
// various flavors of derived classes of BaseShadow.  
// It is only needed for this last function.
extern BaseShadow *Shadow;

// This function is called by dprintf - always display our job, proc,
// and pid in our log entries. 
extern "C" 
int
display_dprintf_header(FILE *fp)
{
	static pid_t mypid = 0;
	static int mycluster = -1;
	static int myproc = -1;

	if (!mypid) {
		mypid = daemonCore->getpid();
	}

	if (mycluster == -1) {
		mycluster = Shadow->getCluster();
		myproc = Shadow->getProc();
	}

	if ( mycluster != -1 ) {
		fprintf( fp, "(%d.%d) (%ld): ", mycluster, myproc, (long)mypid );
	} else {
		fprintf( fp, "(?.?) (%ld): ", (long)mypid );
	}	

	return TRUE;
}

bool
BaseShadow::getMachineName( MyString & /*machineName*/ )
{
	return false;
}
