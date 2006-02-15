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
#include "baseshadow.h"
#include "condor_classad.h"      // for ClassAds.
#include "condor_qmgr.h"         // have to talk to schedd's Q manager
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_config.h"       // for param()
#include "condor_email.h"        // for (you guessed it) email stuff
#include "condor_version.h"
#include "condor_ver_info.h"
#include "enum_utils.h"


// these are declared static in baseshadow.h; allocate space here
UserLog BaseShadow::uLog;
BaseShadow* BaseShadow::myshadow_ptr = NULL;


// this appears at the bottom of this file:
extern "C" int display_dprintf_header(FILE *fp);

BaseShadow::BaseShadow() {
	spool = NULL;
	fsDomain = uidDomain = NULL;
	ckptServerHost = NULL;
	useAFS = useNFS = useCkptServer = false;
	jobAd = NULL;
	remove_requested = false;
	cluster = proc = -1;
	gjid = NULL;
	owner[0] = '\0';
	iwd[0] = '\0';
	core_file_name = NULL;
	scheddAddr = NULL;
	job_updater = NULL;
	ASSERT( !myshadow_ptr );	// make cetain we're only instantiated once
	myshadow_ptr = this;
	exception_already_logged = false;
}

BaseShadow::~BaseShadow() {
	if (spool) free(spool);
	if (fsDomain) free(fsDomain);
	if (ckptServerHost) free(ckptServerHost);
	if (jobAd) FreeJobAd(jobAd);
	if (gjid) free(gjid); 
	if (scheddAddr) free(scheddAddr);
	if( job_updater ) delete job_updater;
}

void
BaseShadow::baseInit( ClassAd *job_ad, const char* schedd_addr )
{
	if( ! job_ad ) {
		EXCEPT("baseInit() called with NULL job_ad!");
	}
	jobAd = job_ad;

	if( ! is_valid_sinful(schedd_addr) ) {
		EXCEPT("schedd_addr not specified with valid address");
	}
	scheddAddr = strdup( schedd_addr );

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
		this, IMMEDIATE_FAMILY);

	// handle system calls with Owner's privilege
// XXX this belong here?  We'll see...
	if ( !init_user_ids(owner, domain)) {
		dprintf(D_ALWAYS, "init_user_ids() failed!\n");
		// uids.C will EXCEPT when we set_user_priv() now
		// so there's not much we can do at this point
	}
	set_user_priv();
	daemonCore->Register_Priv_State( PRIV_USER );

		// change directory, send mail if failure:
	if ( cdToIwd() == -1 ) {
		EXCEPT("Could not cd to initial working directory");
	}

	dumpClassad( "BaseShadow::baseInit()", this->jobAd, D_JOB );

		// initialize the UserPolicy object
	shadow_user_policy.init( jobAd, this );

		// setup an object to keep our job ad updated to the schedd's
		// permanent job queue.  this clears all the dirty bits on our
		// copy of the classad, so anything we touch after this will
		// be updated to the schedd when appropriate.
	job_updater = new QmgrJobUpdater( jobAd, scheddAddr );

		// CRUFT
		// we want this *after* we clear the dirty flags so that if we
		// change anything, we consider that change dirty so it'll get
		// updated the next time we connect to the job queue...
	checkFileTransferCruft();
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

	tmp = param("USE_AFS");
	if (tmp && (tmp[0] == 'T' || tmp[0] == 't')) {
		useAFS = true;
	} else {
		useAFS = false;
	}
	if (tmp) free(tmp);

	tmp = param("USE_NFS");
	if (tmp && (tmp[0] == 'T' || tmp[0] == 't')) {
		useNFS = true;
	} else {
		useNFS = false;
	}
	if (tmp) free(tmp);

	if (ckptServerHost) free(ckptServerHost);
	ckptServerHost = param("CKPT_SERVER_HOST");
	tmp = param("USE_CKPT_SERVER");
	if (tmp && ckptServerHost && (tmp[0] == 'T' || tmp[0] == 't')) {
		useCkptServer = true;
	} else {
		useCkptServer = false;
	}
	if (tmp) free(tmp);

	reconnect_ceiling = 0;
	tmp = param( "RECONNECT_BACKOFF_CEILING" );
	if( tmp ) {
		reconnect_ceiling = atoi( tmp );
		free( tmp );
	} 
	if( !reconnect_ceiling ) {
		reconnect_ceiling = 300;
	}

	reconnect_e_factor = 0;
	tmp = param( "RECONNECT_BACKOFF_FACTOR" );
    if( tmp ) {
        reconnect_e_factor = atof( tmp );
		free( tmp );
    } 
	if( !reconnect_e_factor ) {
    	reconnect_e_factor = 2.0;
    }
}


int BaseShadow::cdToIwd() {
	if (chdir(iwd) < 0) {
		dprintf(D_ALWAYS, "\n\nPath does not exist.\n"
				"He who travels without bounds\n"
				"Can't locate data.\n\n" );
		dprintf( D_ALWAYS, "(Can't chdir to %s)\n", iwd);
		char *buf = new char [strlen(iwd)+20];
		sprintf(buf, "Can't access \"%s\".", iwd);
		FILE *mailer = NULL;
		if ( (mailer=emailUser(buf)) ) {
			fprintf(mailer,"Your job %d.%d specified an initial working\n",
				getCluster(),getProc());
			fprintf(mailer,"directory of %s.\nThis directory currently does\n",
				iwd);
			fprintf(mailer,"not exist.  If this directory is on a shared\n"
				"filesystem, this could be just a temporary problem.  Thus\n"
				"I will try again later\n");
			email_close(mailer);
		}
		delete buf;
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
			// This will not return.  it'll take all desired actions
			// and will eventually call DC_Exit()...
		shadow_user_policy.checkAtExit();
	}

		// if we aren't trying to evaluate the user's policy, we just
		// want to evict this job.
	evictJob( reason );
}


int
BaseShadow::nextReconnectDelay( int attempts )
{
	if( ! attempts ) {
			// first time, do it right away
		return 0;
	}
	int n = (int)ceil(pow(reconnect_e_factor, (attempts+2)));
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
BaseShadow::holdJob( const char* reason )
{
	dprintf( D_ALWAYS, "Job %d.%d going into Hold state: %s\n", 
			 getCluster(), getProc(), reason );

	if( ! jobAd ) {
		dprintf( D_ALWAYS, "In HoldJob() w/ NULL JobAd!" );
		DC_Exit( JOB_SHOULD_HOLD );
	}

		// cleanup this shadow (kill starters, etc)
	cleanUp();

		// Put the reason in our job ad.
	int size = strlen( reason ) + strlen( ATTR_HOLD_REASON ) + 4;
	char* buf = (char*)malloc( size * sizeof(char) );
	if( ! buf ) {
		EXCEPT( "Out of memory!" );
	}
	sprintf( buf, "%s=\"%s\"", ATTR_HOLD_REASON, reason );
	jobAd->Insert( buf );
	free( buf );

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
BaseShadow::terminateJob( void )
{
	if( ! jobAd ) {
		dprintf( D_ALWAYS, "In terminateJob() w/ NULL JobAd!" );
	}

		// cleanup this shadow (kill starters, etc)
	cleanUp();

	int reason;
	reason = getExitReason();

	bool signaled = exitedBySignal();
	dprintf( D_ALWAYS, "Job %d.%d terminated: %s %d\n",
			 getCluster(), getProc(), 
			 signaled ? "killed by signal" : "exited with status",
			 signaled ? exitSignal() : exitCode() );

		// email the user
	emailTerminateEvent( reason );

		// write stuff to user log:
	logTerminateEvent( reason );

		// update the job ad in the queue with some important final
		// attributes so we know what happened to the job when using
		// condor_history...
	if( !updateJobInQueue(U_TERMINATE) ) {
			// trouble!  TODO: should we do anything else?
		dprintf( D_ALWAYS, "Failed to update job queue!\n" );
	}

		// does not return.
	DC_Exit( reason );
}


void
BaseShadow::evictJob( int reason )
{
	dprintf( D_ALWAYS, "Job %d.%d is being evicted\n",
			 getCluster(), getProc() );

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
	char tmp[_POSIX_PATH_MAX], logfilename[_POSIX_PATH_MAX];
	int  use_xml;
	if (jobAd->LookupString(ATTR_ULOG_FILE, tmp) == 1) {
		if ( tmp[0] == '/' || tmp[0]=='\\' || (tmp[1]==':' && tmp[2]=='\\') ) {
			strcpy(logfilename, tmp);
		} else {
			sprintf(logfilename, "%s/%s", iwd, tmp);
		}
		uLog.initialize (owner, domain, logfilename, cluster, proc, 0);
		if (jobAd->LookupBool(ATTR_ULOG_USE_XML, use_xml)
			&& use_xml) {
			uLog.setUseXML(true);
		} else {
			uLog.setUseXML(false);
		}
		dprintf(D_FULLDEBUG, "%s = %s\n", ATTR_ULOG_FILE, logfilename);
	} else {
		dprintf(D_FULLDEBUG, "no %s found\n", ATTR_ULOG_FILE);
	}
}


void
BaseShadow::logTerminateEvent( int exitReason )
{
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

	struct rusage run_remote_rusage;
	memset( &run_remote_rusage, 0, sizeof(struct rusage) );

	run_remote_rusage = getRUsage();
	
	JobTerminatedEvent event;
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
	
	if (!uLog.writeEvent (&event)) {
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
	
	if (!uLog.writeEvent (&event)) {
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
	
	if (!uLog.writeEvent (&event)) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_EVICTED "
				 "(and requeued) event\n" );
	}
}


void
BaseShadow::checkSwap( void )
{
	int	reserved_swap, free_swap;
	char* tmp;
	tmp = param( "RESERVED_SWAP" );
	if( tmp ) {
			// Reserved swap is specified in megabytes
		reserved_swap = atoi( tmp ) * 1024;	
		free( tmp );
	} else {
		reserved_swap = 5 * 1024;
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
BaseShadow::log_except(char *msg)
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
	} else {
		event.recvd_bytes = 0.0;
		event.sent_bytes = 0.0;
	}

	if (!exception_already_logged && !uLog.writeEvent (&event))
	{
		::dprintf (D_ALWAYS, "Unable to log ULOG_SHADOW_EXCEPTION event\n");
	}
}


bool
BaseShadow::updateJobAttr( const char *name, const char *expr )
{
	return job_updater->updateAttr( name, expr );
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
