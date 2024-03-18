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
#include "condor_version.h"
#include "condor_config.h"

#include "starter.h"
#include "jic_local.h"

#include "my_hostname.h"
#include "internet.h"
#include "condor_attributes.h"
#include "cred_dir.h"
#include "classad_command_util.h"
#include "directory.h"
#include "nullfile.h"
#include "basename.h"
#include "secure_file.h"

extern Starter *Starter;


JICLocal::JICLocal() 
	: JobInfoCommunicator()
	, m_refresh_sandbox_creds_tid(-1)
{
}


JICLocal::~JICLocal()
{
	if (daemonCore && (m_refresh_sandbox_creds_tid != -1)) {
		daemonCore->Cancel_Timer(m_refresh_sandbox_creds_tid);
	}
}


bool
JICLocal::init( void ) 
{ 
	dprintf( D_ALWAYS, "Starter running a local job with no shadow\n" );

	if( ! getLocalJobAd() ) {
		dprintf( D_ERROR,
				 "Failed to get local job ClassAd!\n" );
		return false;
	}

		// stash a copy of the unmodified job ad in case we decide
		// below that we want to write out an execution visa
	ClassAd orig_ad = *job_ad;	

		// now that we have the job ad, see if we should go into an
		// infinite loop, waiting for someone to attach w/ the
		// debugger.
	checkForStarterDebugging();

		// Grab all the interesting stuff out of the ClassAd we need
		// to know about the job itself.
	if( ! initJobInfo() ) { 
		dprintf( D_ERROR,
				 "Failed to initialize job info from ClassAd!\n" );
		return false;
	}

		// for now, this is a no-op.  someday it might give this info
		// to someone who really cares. :)
	registerStarterInfo();

		// Now that we have the job ad, figure out what the owner
		// should be and initialize our priv_state code:
	if( ! initUserPriv() ) {
		dprintf( D_ALWAYS, "ERROR: Failed to determine what user "
				 "to run this job as, aborting\n" );
		return false;
	}

		// Now that we have the user_priv, we can make the temp
		// execute dir
	if( ! Starter->createTempExecuteDir() ) { 
		return false;
	}

	initOutputAdFile();

		// Now that the user priv is setup and the temp execute dir
		// exists, we can initialize the LocalUserLog.
	if( ! initLocalUserLog() ) {
		return false;
	}

		// Drop a job ad "visa" into the job's IWD now if the job
		// requested it
	writeExecutionVisa(orig_ad);

		// If the job requests a user credential (OAuthServicesNeeded is
		// defined) then grab it the same way a shadow would.
	if (initUserCredentials() < 0) {
		return false;
	}


	return true;
}


void
JICLocal::config( void ) 
{ 
	// Nothing for us to do
}


void
JICLocal::setupJobEnvironment( void )
{ 
		// Nothing for us to do, so tell the parent class we succeeded
		// this will normally queue a hook or a timer to spawn the job
	JobInfoCommunicator::setupCompleted(0);
}


float
JICLocal::bytesSent( void )
{
		// no file transfer, always 0 for now. 
	return 0.0;
}


float
JICLocal::bytesReceived( void )
{
		// no file transfer, always 0 for now. 
	return 0.0;
}


void
JICLocal::Suspend( void )
{
		// We need the update ad for our job.  We'll use this for the
		// LocalUserLog, and maybe someday other notification to the
		// local user...
	ClassAd update_ad;
	publishUpdateAd( &update_ad );
	
		// See if the LocalUserLog wants it
	u_log->logSuspend( &update_ad );
}


void
JICLocal::Continue( void )
{
		// We need the update ad for our job.  We'll use this for the
		// LocalUserLog, and maybe someday other notification to the
		// local user...
	ClassAd update_ad;
	publishUpdateAd( &update_ad );

		// See if the LocalUserLog wants it
	u_log->logContinue( &update_ad );
}


void
JICLocal::allJobsGone( void )
{
		// Since there's no shadow to tell us to go away, we have to
		// exit ourselves.
	dprintf( D_ALWAYS, "All jobs have exited... starter exiting\n" );
	Starter->StarterExit( STARTER_EXIT_NORMAL );
}


int
JICLocal::reconnect( ReliSock* s, ClassAd* /*ad*/ )
{
		// Someday this might mean something, for now it doesn't.
	sendErrorReply( s, getCommandString(CA_RECONNECT_JOB), CA_FAILURE, 
					"Starter using JICLocal does not support reconnect" );
	return FALSE;
}

void 
JICLocal::disconnect()
{
		// Someday this might mean something, for now it doesn't.
	dprintf(D_ALWAYS, "Starter using JICLocal does not support disconnect\n");
	return;
}

bool
JICLocal::periodicJobUpdate( ClassAd* update_ad )
{
	dprintf( D_FULLDEBUG, "Entering JICLocal::peridocJobUpdate()\n" );

	ClassAd local_ad;
	ClassAd* ad;
	if( update_ad ) {
			// we already have the update info, so don't bother trying
			// to publish another one.
		ad = update_ad;
	} else {
		ad = &local_ad;
		if( ! publishUpdateAd(ad) ) {
			dprintf( D_FULLDEBUG, "JICLocal::periodicJobUpdate(): "
			         "Didn't find any info to update!\n" );
			return false;
		}
	}
	bool r1, r2;
	r1 = writeUpdateAdFile(ad);
	r2 = JobInfoCommunicator::periodicJobUpdate(ad);
	return (r1 && r2);
}

void
JICLocal::notifyJobPreSpawn( void )
{
		// let the LocalUserLog know so it can log if necessary.  it
		// doesn't use the ClassAd for this event at all, so it's not
		// worth the trouble of creating one and publishing anything
		// into it.
	u_log->logExecute( NULL );
}


bool
JICLocal::notifyJobExit( int /*exit_status*/, int reason, UserProc*
						  user_proc )
{
	ClassAd ad;

		// TODO: this is a hack.  we need a better way to get the
		// pre/post info back to the right places...
	Starter->publishPreScriptUpdateAd( &ad );
	user_proc->PublishUpdateAd( &ad );
	Starter->publishPostScriptUpdateAd( &ad );

		// depending on the exit reason, we want a different event. 
	u_log->logJobExit( &ad, reason );

	writeOutputAdFile( &ad );

	return true;
}



bool
JICLocal::notifyStarterError( const char* err_msg, bool critical, int /*hold_reason_code*/, int /*hold_reason_subcode*/ )
{
	u_log->logStarterError( err_msg, critical );
	return true;
}


void
JICLocal::addToOutputFiles( const char* /*filename*/ )
{
		// there's no file transfer, so if something needs to get back
		// to the user, it's already going to be in the job's iwd...
}

void
JICLocal::removeFromOutputFiles( const char* /*filename*/ )
{
		// there's no file transfer, so there is nothing to clean up.
}

bool
JICLocal::registerStarterInfo( void )
{
		// someday, we might want to give info about the Starter to
		// someone who cares, for now, everything useful has already
		// been dprintf'ed, so we're done.
	return true;
}


bool
JICLocal::initUserPriv( void )
{
	bool rval = false;

#ifdef WIN32
		/*
		  If we're on windoze, and this is anything but a local
		  universe job, we should immediately try the windoze-specific
		  method for per-slot users, setting up a nobody account, etc,
		  and be done with it.  However, if it's local universe, we
		  basically want to do what we do for the unix case: find
		  ATTR_OWNER (and ATTR_NT_DOMAIN) from the job ad and
		  initialize ourselves with that...
		*/
	if( job_universe != CONDOR_UNIVERSE_LOCAL ) {
		return initUserPrivWindows();
	}
#endif

		// Before we go through any trouble, see if we even need
		// ATTR_OWNER to initialize user_priv.  If not, go ahead and
		// initialize it as appropriate.  
	if( initUserPrivNoOwner() ) {
		return true;
	}

	char* owner = NULL;
	if( job_ad->LookupString( ATTR_OWNER, &owner ) != 1 ) {
		dprintf( D_ALWAYS, "ERROR: %s not found in JobAd.  Aborting.\n", 
				 ATTR_OWNER );
		return false;
	}

#ifdef WIN32
		// we only care about or expect to find this attribute if
		// we're on windoze...
	char* domain = NULL;
	if( job_ad->LookupString( ATTR_NT_DOMAIN, &domain ) != 1 ) {
		dprintf( D_ALWAYS, "ERROR: %s not found in JobAd.  Aborting.\n", 
				 ATTR_NT_DOMAIN );
		return false;
	}

	if( ! init_user_ids(owner,domain) ) { 
		dprintf( D_ALWAYS,
				 "ERROR: Bad or missing credential for user \"%s@%s\"\n",
				 owner, domain );
	} else {  
		rval = true;
		dprintf( D_FULLDEBUG, "Initialized user_priv as \"%s@%s\"\n", 
				 owner, domain );
	}
	if( domain ) {
		free( domain );
		domain = NULL;
	}

#else /* UNIX */

	if( ! init_user_ids_quiet(owner) ) { 
		dprintf( D_ALWAYS, "ERROR: Uid for \"%s\" not found in "
				 "passwd database for a local job\n", owner ); 
	} else {
		rval = true;
		dprintf( D_FULLDEBUG, "Initialized user_priv as \"%s\"\n", 
				 owner );
	}

#endif

		// deallocate owner string so we don't leak memory.
	free( owner );
	owner = NULL;

	if( rval ) {
		user_priv_is_initialized = true;
	}
	return rval;
}


bool
JICLocal::initJobInfo( void ) 
{
		// Give our base class a chance.
	if (!JobInfoCommunicator::initJobInfo()) return false;

	std::string orig_job_iwd;

	if( ! job_ad ) {
		EXCEPT( "JICLocal::initJobInfo() called with NULL job ad!" );
	}

		// stash the executable name in orig_job_name
	if( ! job_ad->LookupString(ATTR_JOB_CMD, &orig_job_name) ) {
		dprintf( D_ALWAYS, "Error in JICLocal::initJobInfo(): "
				 "Can't find %s in job ad\n", ATTR_JOB_CMD );
		return false;
	} else {
			// put the orig job name in class ad
		dprintf(D_ALWAYS, "setting the orig job name in starter\n");
		job_ad->Assign (ATTR_ORIG_JOB_CMD,orig_job_name);
	}

		// stash the iwd name in orig_job_iwd
	if( ! job_ad->LookupString(ATTR_JOB_IWD, orig_job_iwd) ) {
		dprintf(D_ALWAYS, "%s not found in job ad, setting to %s\n",
				ATTR_JOB_IWD, Starter->GetWorkingDir(0));
		job_ad->Assign(ATTR_JOB_IWD, Starter->GetWorkingDir(0));
	} else {
			// put the orig job iwd in class ad
		dprintf(D_ALWAYS, "setting the orig job iwd in starter\n");
		job_ad->Assign(ATTR_ORIG_JOB_IWD,orig_job_iwd);
	}

	if( ! job_ad->LookupInteger(ATTR_JOB_UNIVERSE, job_universe) ) {
		dprintf( D_ALWAYS, 
				 "Job doesn't specify universe, assuming VANILLA\n" ); 
		job_universe = CONDOR_UNIVERSE_VANILLA;
	}
		// also, make sure the universe we've got is valid, since in
		// certain cases we may not check this until now, and we want
		// to make sure we bail out if we can't support it.
	if( ! checkUniverse(job_universe) ) {
		return false;
	}

	if( Starter->isGridshell() ) { 
		job_ad->Assign( ATTR_JOB_IWD, Starter->origCwd() );
	}
	job_ad->LookupString( ATTR_JOB_IWD, &job_iwd );
	if( ! job_iwd ) {
		dprintf( D_ALWAYS, "Can't find job's IWD, aborting\n" );
		return false;
	}

		// Figure out the cluster and proc 
	initJobId();

	if( ! fullpath(orig_job_name) ) {
			// add the job's iwd to the job_cmd, so exec will work. 
		dprintf( D_FULLDEBUG, "warning: %s not specified as full path, "
				 "prepending job's IWD (%s)\n", ATTR_JOB_CMD, job_iwd );
		std::string job_cmd;
		formatstr( job_cmd, "%s%c%s", job_iwd, DIR_DELIM_CHAR, orig_job_name );
		job_ad->Assign( ATTR_JOB_CMD, job_cmd );
	}
		
		// now that we have the real iwd we'll be using, we can
		// initialize the std files...
	if( ! job_input_name ) {
		job_input_name = getJobStdFile( ATTR_JOB_INPUT );
	}
	if( ! job_output_name ) {
		job_output_name = getJobStdFile( ATTR_JOB_OUTPUT );
	}
	if( ! job_error_name ) {
		job_error_name = getJobStdFile( ATTR_JOB_ERROR );
	}
	return true;
}


void
JICLocal::initJobId( void ) 
{
	if( job_cluster < 0 ) {
			// not on command-line, try classad
		if( !job_ad->LookupInteger(ATTR_CLUSTER_ID, job_cluster) ) { 
			dprintf( D_FULLDEBUG, "Job's cluster ID not specified "
					 "in ClassAd or on command-line, using '1'\n" );
			job_cluster = 1;
		}
	}
	if( job_proc < 0 ) {
			// not on command-line, try classad
		if( !job_ad->LookupInteger(ATTR_PROC_ID, job_proc) ) { 
			dprintf( D_FULLDEBUG, "Job's proc ID not specified "
					 "in ClassAd or on command-line, using '0'\n" );
			job_proc = 0;
		}
	}
	if( job_subproc < 0 ) {
			// not on command-line, try classad
		if( !job_ad->LookupInteger(ATTR_NODE, job_subproc) ) { 
			job_subproc = 0;
		}
	}
}


char* 
JICLocal::getJobStdFile( const char* attr_name )
{
	char* tmp = NULL;
	std::string filename;

		// the only magic here is to make sure we have full paths for
		// these, by prepending the job's iwd if the filename doesn't
		// start with a '/'
	job_ad->LookupString( attr_name, &tmp );
	if( ! tmp ) {
		return NULL;
	}
	if ( !nullFile(tmp) ) {
		if( ! fullpath(tmp) ) { 
			formatstr( filename, "%s%c", job_iwd, DIR_DELIM_CHAR );
		}
		filename += tmp;
	}
	free( tmp );
	if (!filename.empty()) { 
		return strdup(filename.c_str());
	}
	return NULL;
}


bool
JICLocal::publishUpdateAd( ClassAd* ad )
{
		// No info from JICLocal itself.  Just get our Starter object
		// to publish about all the jobs.  This will walk through all
		// the UserProcs and have those publish, as well.  It returns
		// true if there was anything published, false if not.
	return Starter->publishUpdateAd( ad );
}


bool
JICLocal::checkUniverse( int univ ) 
{
	switch( univ ) {
	case CONDOR_UNIVERSE_LOCAL:
	case CONDOR_UNIVERSE_VANILLA:
	case CONDOR_UNIVERSE_JAVA:
			// for now, we don't support much. :)
		return true;

	case CONDOR_UNIVERSE_SCHEDULER:
	case CONDOR_UNIVERSE_MPI:
	case CONDOR_UNIVERSE_GRID:
			// these are at least valid tries, but we don't work with
			// any of them in stand-alone starter mode... yet.
		dprintf( D_ALWAYS, "ERROR: %s %s (%d) not supported without the "
				 "schedd and/or shadow, aborting\n", ATTR_JOB_UNIVERSE,
				 CondorUniverseName(univ), univ );
		return false;

	default:
			// downright unsupported universes
		dprintf( D_ALWAYS, "ERROR: %s %s (%d) is not supported\n", 
				 ATTR_JOB_UNIVERSE, CondorUniverseName(univ), univ );
		return false;

	}
}


bool
JICLocal::initLocalUserLog( void )
{
	return u_log->initFromJobAd( job_ad );
}

int
JICLocal::initUserCredentials()
{
	// check to see if the job needs any OAuth services (scitokens)
	// if so, call the function that does that.
	std::string services_needed;
	if (job_ad->LookupString(ATTR_OAUTH_SERVICES_NEEDED, services_needed)) {
		dprintf(D_ALWAYS, "initUserCredentials: job needs OAuth services %s\n", services_needed.c_str());
	} else {
		dprintf(D_FULLDEBUG, "Job needs no OAuth services.\n");
		return 0;
	}

	// set/reset timer
	int sec_cred_refresh = param_integer("SEC_CREDENTIAL_REFRESH", 300);
	if (sec_cred_refresh > 0) {
		if (m_refresh_sandbox_creds_tid == -1) {
			m_refresh_sandbox_creds_tid = daemonCore->Register_Timer(
				sec_cred_refresh,
				(TimerHandlercpp)&JICLocal::refreshSandboxCredentials_from_timer,
				"refreshSandboxCredentials",
				this );
		} else {
			daemonCore->Reset_Timer(m_refresh_sandbox_creds_tid, sec_cred_refresh);
		}
		dprintf(D_SECURITY, "CREDS: will refresh credentials again in %i seconds\n", sec_cred_refresh);
	} else {
		dprintf(D_SECURITY, "CREDS: cred refresh is DISABLED.\n");
	}

	// setup .condor_creds directory in sandbox (may already exist).
	std::string sandbox_dir_name;
	dircat(Starter->GetWorkingDir(0), ".condor_creds", sandbox_dir_name);

	htcondor::LocalUnivCredDirCreator creds(*job_ad, sandbox_dir_name);
	CondorError err;
	if (!creds.PrepareCredDir(err)) {
		return -1;
	}

	// only need to do this once
	if( ! getCredPath()) {
		dprintf(D_SECURITY, "CREDS: setting env _CONDOR_CREDS to %s\n", sandbox_dir_name.c_str());
		setCredPath(sandbox_dir_name.c_str());
	}

	return 1;
}



