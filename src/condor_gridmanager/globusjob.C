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


/* TODO XXX adesmet: Notes on gridshell, TODO List

- Writing input classad to file in BuildSubmitRSL seems wrong.  But where?
  (And how to pass filename to BuildSubmitRSL)
- Log retrieval:
	- propagating things from the submit file to allow the user to
	  specify (it's all hard coded now).

*/


#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "environ.h"  // for Environ object
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"
#include "condor_holdcodes.h"
#include "gridmanager.h"
#include "globusjob.h"
#include "condor_config.h"
#include "condor_parameters.h"
#include "string_list.h"


// GridManager job states
#define GM_INIT					0
#define GM_REGISTER				1
#define GM_STDIO_UPDATE			2
#define GM_UNSUBMITTED			3
#define GM_SUBMIT				4
#define GM_SUBMIT_SAVE			5
#define GM_SUBMIT_COMMIT		6
#define GM_SUBMITTED			7
#define GM_CHECK_OUTPUT			8
#define GM_DONE_SAVE			9
#define GM_DONE_COMMIT			10
#define GM_STOP_AND_RESTART		11
#define GM_RESTART				12
#define GM_RESTART_SAVE			13
#define GM_RESTART_COMMIT		14
#define GM_CANCEL				15
#define GM_CANCEL_WAIT			16
#define GM_FAILED				17
#define GM_DELETE				18
#define GM_CLEAR_REQUEST		19
#define GM_HOLD					20
#define GM_PROXY_EXPIRED		21
#define GM_CLEAN_JOBMANAGER		22
#define GM_REFRESH_PROXY		23
#define GM_PROBE_JOBMANAGER		24
#define GM_OUTPUT_WAIT			25
#define GM_PUT_TO_SLEEP			26
#define GM_JOBMANAGER_ASLEEP	27

char *GMStateNames[] = {
	"GM_INIT",
	"GM_REGISTER",
	"GM_STDIO_UPDATE",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_SUBMIT_COMMIT",
	"GM_SUBMITTED",
	"GM_CHECK_OUTPUT",
	"GM_DONE_SAVE",
	"GM_DONE_COMMIT",
	"GM_STOP_AND_RESTART",
	"GM_RESTART",
	"GM_RESTART_SAVE",
	"GM_RESTART_COMMIT",
	"GM_CANCEL",
	"GM_CANCEL_WAIT",
	"GM_FAILED",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROXY_EXPIRED",
	"GM_CLEAN_JOBMANAGER",
	"GM_REFRESH_PROXY",
	"GM_PROBE_JOBMANAGER",
	"GM_OUTPUT_WAIT",
	"GM_PUT_TO_SLEEP",
	"GM_JOBMANAGER_ASLEEP"
};

#define MIN_SUPPORTED_GRAM_V GRAM_V_1_6
#define MIN_SUPPORTED_GRAM_V_STRING "1.6"

// TODO: once we can set the jobmanager's proxy timeout, we should either
// let this be set in the config file or set it to
// GRIDMANAGER_MINIMUM_PROXY_TIME + 60
// #define JM_MIN_PROXY_TIME		(minProxy_time + 60)
#define JM_MIN_PROXY_TIME		(180 + 60)

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define OUTPUT_WAIT_POLL_INTERVAL 1

#define LOG_GLOBUS_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, globusState %d: %s returned Globus error %d\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],globusState, \
        func,error)

/* Given a classad, write it to a file for later staging to the gridshell.
Returns true/false on success/failure.  If successful, out_filename contains
the filename of the classad.  If not successful, out_filename's contents are
undefined.
*/
static bool write_classad_input_file( ClassAd *classad, 
	const MyString & executable_path,
	MyString & out_filename )
{
	if( ! classad ) {
		dprintf(D_ALWAYS,"Internal Error: write_classad_input_file handed "
			"invalid ClassAd (null)\n");
		return false;
	}

	ClassAd tmpclassad(*classad);

	MyString CmdExpr;
	CmdExpr = ATTR_JOB_CMD;
	CmdExpr += "=\"";
	CmdExpr += basename( executable_path.GetCStr() );
	CmdExpr += '"';
	// TODO: Store old Cmd as OrigCmd?
	tmpclassad.InsertOrUpdate(CmdExpr.GetCStr());

	PROC_ID procID;
	if( ! tmpclassad.LookupInteger( ATTR_CLUSTER_ID, procID.cluster ) ) {
		dprintf(D_ALWAYS,"Internal Error: write_classad_input_file handed "
			"invalid ClassAd (Missing or malformed %s)\n", ATTR_CLUSTER_ID);
		return false;
	}
	if( ! tmpclassad.LookupInteger( ATTR_PROC_ID, procID.proc ) ) {
		dprintf(D_ALWAYS,"Internal Error: write_classad_input_file handed "
			"invalid ClassAd (Missing or malformed %s)\n", ATTR_PROC_ID);
		return false;
	}

	out_filename.sprintf("_condor_private_classad_in_%d.%d", 
		procID.cluster, procID.proc);

	MyString out_filename_full;
	{
		char * buff = NULL;
		if( ! tmpclassad.LookupString( ATTR_JOB_IWD, &buff ) ) {

			dprintf(D_ALWAYS,"(%d.%d) Internal Error: "
				"write_classad_input_file handed "
				"invalid ClassAd (Missing or malformed %s)\n",
				procID.cluster, procID.proc, ATTR_JOB_IWD);

			return false;
		}
		out_filename_full = buff;
		free(buff);
		out_filename_full += "/";
		out_filename_full += out_filename;
	}

		// Fix the universe, too, since the starter is going to expect
		// "VANILLA", not "GLOBUS"...
	tmpclassad.InsertOrUpdate( "JobUniverse = 5" );

	dprintf(D_FULLDEBUG,"(%d.%d) Writing ClassAd to file %s\n",
		procID.cluster, procID.proc, out_filename.GetCStr());

	// TODO: Test for file's existance, complain and die on existance?
	FILE * fp = fopen(out_filename_full.GetCStr(), "w");

	if( ! fp )
	{
		dprintf(D_ALWAYS,"(%d.%d) Failed to write ClassAd to file %s. "
			"Error number %d (%s).\n",
			procID.cluster, procID.proc, out_filename.GetCStr(),
			errno, strerror(errno));
		return false;
	}

	if( tmpclassad.fPrint(fp) ) {
		dprintf(D_ALWAYS,"(%d.%d) Failed to write ClassAd to file %s. "
			"Unknown error in ClassAd::fPrint.\n",
			procID.cluster, procID.proc, out_filename.GetCStr());
		fclose(fp);
		return false;
	} 

	fclose(fp);
	return true;
}

const char *rsl_stringify( const MyString& src )
{
	int src_len = src.Length();
	int src_pos = 0;
	int var_pos1;
	int var_pos2;
	int quote_pos;
	static MyString dst;

	if ( src_len == 0 ) {
		dst = "''";
	} else {
		dst = "";
	}

	while ( src_pos < src_len ) {
		var_pos1 = src.find( "$(", src_pos );
		var_pos2 = var_pos1 == -1 ? -1 : src.find( ")", var_pos1 );
		quote_pos = src.find( "'", src_pos );
		if ( var_pos2 == -1 && quote_pos == -1 ) {
			dst += "'";
			dst += src.Substr( src_pos, src.Length() - 1 );
			dst += "'";
			src_pos = src.Length();
		} else if ( var_pos2 == -1 ||
					(quote_pos != -1 && quote_pos < var_pos1 ) ) {
			if ( src_pos < quote_pos ) {
				dst += "'";
				dst += src.Substr( src_pos, quote_pos - 1 );
				dst += "'#";
			}
			dst += '"';
			while ( src[quote_pos] == '\'' ) {
				dst += "'";
				quote_pos++;
			}
			dst += '"';
			if ( quote_pos < src_len ) {
				dst += '#';
			}
			src_pos = quote_pos;
		} else {
			if ( src_pos < var_pos1 ) {
				dst += "'";
				dst += src.Substr( src_pos, var_pos1 - 1 );
				dst += "'#";
			}
			dst += src.Substr( var_pos1, var_pos2 );
			if ( var_pos2 + 1 < src_len ) {
				dst += '#';
			}
			src_pos = var_pos2 + 1;
		}
	}

	return dst.Value();
}
const char *rsl_stringify( const char *string )
{
	static MyString src;
	src = string;
	return rsl_stringify( src );
}

static bool merge_file_into_classad(const char * filename, ClassAd * ad)
{
	if( ! ad ) {
		// TODO dprintf?
		dprintf(D_ALWAYS, "Internal error: "
			"merge_file_into_classad called without ClassAd\n");
		return false;
	}
	if( ! filename ) {
		// TODO dprintf?
		dprintf(D_ALWAYS, "Internal error: "
			"merge_file_into_classad called without filename\n");
		return false;
	}
	if( ! strlen(filename) ) {
		// TODO dprintf?
		dprintf(D_ALWAYS, "Internal error: "
			"merge_file_into_classad called with empty filename\n");
		return false;
	}

	PROC_ID procID;
	if( ! ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster ) ) {
		dprintf(D_ALWAYS,"Internal Error: merge_file_into_classad handed "
			"invalid ClassAd (Missing or malformed %s)\n", ATTR_CLUSTER_ID);
		return false;
	}
	if( ! ad->LookupInteger( ATTR_PROC_ID, procID.proc ) ) {
		dprintf(D_ALWAYS,"Internal Error: merge_file_into_classad handed "
			"invalid ClassAd (Missing or malformed %s)\n", ATTR_PROC_ID);
		return false;
	}

	/* TODO: Is this the right solution?  I'm basically reimplementing
	   a subset of the ClassAd reading code.  Perhaps load into a ClassAd
	   and scan that? */
	{
		StringList SAVE_ATTRS;
		SAVE_ATTRS.append(ATTR_JOB_REMOTE_SYS_CPU);
		SAVE_ATTRS.append(ATTR_JOB_REMOTE_USER_CPU);
		SAVE_ATTRS.append(ATTR_IMAGE_SIZE);
		SAVE_ATTRS.append(ATTR_JOB_STATE);
		SAVE_ATTRS.append(ATTR_NUM_PIDS);
		SAVE_ATTRS.append(ATTR_ON_EXIT_BY_SIGNAL);
		SAVE_ATTRS.append(ATTR_ON_EXIT_CODE);
		SAVE_ATTRS.append(ATTR_ON_EXIT_SIGNAL);
		SAVE_ATTRS.append(ATTR_JOB_START_DATE);
		SAVE_ATTRS.append(ATTR_JOB_DURATION);


		/* TODO: COMPLETION_DATE isn't currently returned.  Who deals with it?
		   Is it our job?  gridshell's?  Condor-G never really supported it,
		   but it would be nice to have.  Update: Normally
		   condor_schedd.V6/qmgmt.C does it in DestroyProc.  Why isn't it?
		   */

		SAVE_ATTRS.append(ATTR_COMPLETION_DATE);

		MyString full_filename;
		{
			char * buff = NULL;
			if( ! ad->LookupString( ATTR_JOB_IWD, &buff ) ) {
				dprintf(D_ALWAYS,"(%d.%d) Internal Error: "
					"merge_file_into_classad handed "
					"invalid ClassAd (Missing or malformed %s)\n",
					procID.cluster, procID.proc, ATTR_JOB_IWD);
				return false;
			}
			full_filename = buff;
			free(buff);
			full_filename += "/";
			full_filename += filename;
		}
		
		FILE * fp = fopen(full_filename.GetCStr(), "r");
		if( ! fp ) {
			dprintf(D_ALWAYS, "Unable to read output ClassAd at %s.  "
				"Error number %d (%s).  "
				"Results will not be integrated into history.\n",
				filename, errno, strerror(errno));
			return false;
		}

		MyString line;
		while( line.readLine(fp) ) {
			line.chomp();
			int n = line.find(" = ");
			if(n < 1) {
				dprintf( D_ALWAYS,
					"Failed to parse \"%s\", ignoring.", line.GetCStr());
				continue;
			}
			MyString attr = line.Substr(0, n - 1);

			dprintf( D_ALWAYS, "FILE: %s\n", line.GetCStr() );
			if( ! SAVE_ATTRS.contains_anycase(attr.GetCStr()) ) {
				continue;
			}

			if( ! ad->Insert(line.GetCStr()) ) {
				dprintf( D_ALWAYS, "Failed to insert \"%s\" into ClassAd, "
						 "ignoring.\n", line.GetCStr() );
			}
		}
		fclose( fp );
	}

	return true;
}

int GlobusJob::probeInterval = 300;		// default value
int GlobusJob::submitInterval = 300;	// default value
int GlobusJob::restartInterval = 60;	// default value
int GlobusJob::gahpCallTimeout = 300;	// default value
int GlobusJob::maxConnectFailures = 3;	// default value
int GlobusJob::outputWaitGrowthTimeout = 15;	// default value

GlobusJob::GlobusJob( GlobusJob& copy )
{
	dprintf(D_ALWAYS, "GlobusJob copy constructor called. This is a No-No!\n");
	ASSERT( 0 );
}

GlobusJob::GlobusJob( ClassAd *classad, GlobusResource *resource )
{
	int bool_value;
	char buff[4096];
	char buff2[_POSIX_PATH_MAX];
	char iwd[_POSIX_PATH_MAX];
	bool job_already_submitted = false;

	RSL = NULL;
	callbackRegistered = false;
	classad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
	classad->LookupInteger( ATTR_PROC_ID, procID.proc );
	jobContact = NULL;
	localOutput = NULL;
	localError = NULL;
	streamOutput = false;
	streamError = false;
	stageOutput = false;
	stageError = false;
	globusStateErrorCode = 0;
	globusStateBeforeFailure = 0;
	callbackGlobusState = 0;
	callbackGlobusStateErrorCode = 0;
	submitLogged = false;
	executeLogged = false;
	submitFailedLogged = false;
	terminateLogged = false;
	abortLogged = false;
	evictLogged = false;
	holdLogged = false;
	stateChanged = false;
	jmVersion = GRAM_V_UNKNOWN;
	restartingJM = false;
	restartWhen = 0;
	gmState = GM_INIT;
	globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
	jmUnreachable = false;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	enteredCurrentGlobusState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	submitFailureCode = 0;
	lastRestartReason = 0;
	lastRestartAttempt = 0;
	numRestartAttempts = 0;
	numRestartAttemptsThisSubmit = 0;
	jmProxyExpireTime = 0;
	connect_failure_counter = 0;
	outputWaitLastGrowth = 0;
	// HACK!
	retryStdioSize = true;
	gahp_proxy_id_set = false;
	communicationTimeoutTid = -1;
	ad = classad;

	useGridShell = false;

	{
		int use_gridshell;
		if( classad->LookupBool(ATTR_USE_GRID_SHELL, use_gridshell) ) {
			useGridShell = use_gridshell;
		}
	}

	evaluateStateTid = daemonCore->Register_Timer( TIMER_NEVER,
								(TimerHandlercpp)&GlobusJob::doEvaluateState,
								"doEvaluateState", (Service*) this );;

	gahp.setNotificationTimerId( evaluateStateTid );
	gahp.setMode( GahpClient::normal );
	gahp.setTimeout( gahpCallTimeout );

	myProxy = NULL;
	buff[0] = '\0';
	ad->LookupString( ATTR_X509_USER_PROXY, buff );
	if ( buff[0] != '\0' ) {
		myProxy = AcquireProxy( buff, evaluateStateTid );
		if ( myProxy == NULL ) {
			dprintf( D_ALWAYS, "(%d.%d) error acquiring proxy!\n",
					 procID.cluster, procID.proc );
		}
	} else {
		dprintf( D_ALWAYS, "(%d.%d) %s not set in job ad!\n",
				 procID.cluster, procID.proc, ATTR_X509_USER_PROXY );
	}

	// In GM_HOLD, we assme HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( ad->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		UpdateJobAd( ATTR_HOLD_REASON, "UNDEFINED" );
	}

	ad->LookupInteger( ATTR_GLOBUS_GRAM_VERSION, jmVersion );

	buff[0] = '\0';
	ad->LookupString( ATTR_GLOBUS_RESOURCE, buff );
	if ( buff[0] != '\0' ) {
		resourceManagerString = strdup( buff );
	} else {
		EXCEPT( "No GlobusResource defined for job %d.%d",
				procID.cluster, procID.proc );
	}

	buff[0] = '\0';
	ad->LookupString( ATTR_GLOBUS_CONTACT_STRING, buff );
	if ( buff[0] != '\0' && strcmp( buff, NULL_JOB_CONTACT ) != 0 ) {
		if ( jmVersion == GRAM_V_UNKNOWN ) {
			dprintf(D_ALWAYS,
					"(%d.%d) Non-NULL contact string and unknown gram version!\n",
					procID.cluster, procID.proc);
		}
		rehashJobContact( this, jobContact, buff );
		SetJobContact(buff);
		job_already_submitted = true;
	}

	resourceDown = false;
	resourceStateKnown = false;
	myResource = resource;
	// RegisterJob() may call our NotifyResourceUp/Down(), so be careful.
	myResource->RegisterJob( this, job_already_submitted );

	useGridJobMonitor = true;

	ad->LookupInteger( ATTR_GLOBUS_STATUS, globusState );
	ad->LookupInteger( ATTR_JOB_STATUS, condorState );

	globusError = GLOBUS_SUCCESS;

	iwd[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_IWD, iwd) && *iwd ) {
		int len = strlen(iwd);
		if ( len > 1 && iwd[len - 1] != '/' ) {
			strcat( iwd, "/" );
		}
	} else {
		strcpy( iwd, "/" );
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_OUTPUT, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {

		if ( !ad->LookupBool( ATTR_TRANSFER_OUTPUT, bool_value ) ||
			 bool_value ) {

			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localOutput = strdup( buff2 );

			bool_value = 1;
			ad->LookupBool( ATTR_STREAM_OUTPUT, bool_value );
			streamOutput = (bool_value != 0);
			stageOutput = !streamOutput;
		}
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_ERROR, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {

		if ( !ad->LookupBool( ATTR_TRANSFER_ERROR, bool_value ) ||
			 bool_value ) {

			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localError = strdup( buff2 );

			bool_value = 1;
			ad->LookupBool( ATTR_STREAM_ERROR, bool_value );
			streamError = (bool_value != 0);
			stageError = !streamError;
		}
	}

	wantRematch = 0;
	doResubmit = 0;		// set if gridmanager wants to resubmit job
	wantResubmit = 0;	// set if user wants to resubmit job via RESUBMIT_CHECK
	ad->EvalBool(ATTR_GLOBUS_RESUBMIT_CHECK,NULL,wantResubmit);

	ad->ClearAllDirtyFlags();
}

GlobusJob::~GlobusJob()
{
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	if ( resourceManagerString ) {
		free( resourceManagerString );
	}
	if ( jobContact ) {
		free( jobContact );
	}
	if ( RSL ) {
		delete RSL;
	}
	if ( localOutput ) {
		free( localOutput );
	}
	if ( localError ) {
		free( localError );
	}
	if ( myProxy ) {
		ReleaseProxy( myProxy, evaluateStateTid );
	}
	if (daemonCore) {
		daemonCore->Cancel_Timer( evaluateStateTid );
		if ( communicationTimeoutTid != -1 ) {
			daemonCore->Cancel_Timer(communicationTimeoutTid);
			communicationTimeoutTid = -1;
		}
	}
	if ( ad ) {
		delete ad;
	}
}

void GlobusJob::Reconfig()
{
	gahp.setTimeout( gahpCallTimeout );
}

void GlobusJob::UpdateJobAd( const char *name, const char *value )
{
	char buff[1024];
	sprintf( buff, "%s = %s", name, value );
	ad->InsertOrUpdate( buff );
}

void GlobusJob::UpdateJobAdInt( const char *name, int value )
{
	char buff[16];
	sprintf( buff, "%d", value );
	UpdateJobAd( name, buff );
}

void GlobusJob::UpdateJobAdFloat( const char *name, float value )
{
	char buff[16];
	sprintf( buff, "%f", value );
	UpdateJobAd( name, buff );
}

void GlobusJob::UpdateJobAdBool( const char *name, int value )
{
	if ( value ) {
		UpdateJobAd( name, "TRUE" );
	} else {
		UpdateJobAd( name, "FALSE" );
	}
}

void GlobusJob::UpdateJobAdString( const char *name, const char *value )
{
	char buff[1024];
	sprintf( buff, "\"%s\"", value );
	UpdateJobAd( name, buff );
}

void GlobusJob::SetEvaluateState()
{
	daemonCore->Reset_Timer( evaluateStateTid, 0 );
}

int GlobusJob::doEvaluateState()
{
	bool connect_failure = false;
	int old_gm_state;
	int old_globus_state;
	bool reevaluate_state = true;
	int schedd_actions = 0;
	time_t now;	// make sure you set this before every use!!!

	bool done;
	int rc;
	int status;
	int error;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, globusState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],globusState);

	if ( !myProxy ) {
		UpdateJobAdString( ATTR_HOLD_REASON,"Proxy file missing or corrupted" );
		UpdateJobAdInt(ATTR_HOLD_REASON_CODE, CONDOR_HOLD_CODE_CorruptedCredential);
		UpdateJobAdInt(ATTR_HOLD_REASON_SUBCODE, 0);
		gmState = GM_HOLD;
	} else {
		if ( myProxy->gahp_proxy_id == -1 ) {
			dprintf( D_ALWAYS, "(%d.%d) proxy not cached yet, waiting...\n",
					 procID.cluster, procID.proc );
			return TRUE;
		} else if ( gahp_proxy_id_set == false ) {
			gahp.setDelegProxyCacheId( myProxy->gahp_proxy_id );
			gahp_proxy_id_set = true;
		}

			// For a job remove operation, we will try to use any proxy that is still
			// valid.  For any other purpose, however, we require that the proxy
			// at least has a minimum number of seconds left on it.
			// This allows the user to perform a condor_rm on a job which has been put
			// on hold when the proxy is about to expire (as long as they are quick
			// about it!).  
		if ( ((PROXY_NEAR_EXPIRED( myProxy ) && condorState != REMOVED) ||	// min # of seconds
			  (PROXY_IS_EXPIRED( myProxy ) && condorState == REMOVED))		// any time at all
			  && gmState != GM_PROXY_EXPIRED ) {
			dprintf( D_ALWAYS, "(%d.%d) proxy is about to expire, changing state to GM_PROXY_EXPIRED\n",
					 procID.cluster, procID.proc );
			gmState = GM_PROXY_EXPIRED;
		}
	}	// of myProxy != NULL

	if ( jmUnreachable || resourceDown ) {
		gahp.setMode( GahpClient::results_only );
	} else {
		gahp.setMode( GahpClient::normal );
	}

	do {
		reevaluate_state = false;
		old_gm_state = gmState;
		old_globus_state = globusState;

		switch ( gmState ) {
		case GM_INIT: {
			// This is the state all jobs start in when the GlobusJob object
			// is first created. If we think there's a running jobmanager
			// out there, we try to register for callbacks (in GM_REGISTER).
			// The one way jobs can end up back in this state is if we
			// attempt a restart of a jobmanager only to be told that the
			// old jobmanager process is still alive.
			errorString = "";

			if(jmVersion != GRAM_V_UNKNOWN && jmVersion < MIN_SUPPORTED_GRAM_V) {
				const char * OLD_GRAM_ERROR 
					= "Job is attempting to use obsolete GRAM protocol.";
				dprintf(D_ALWAYS, "(%d.%d) %s\n", procID.cluster, procID.proc, 
					OLD_GRAM_ERROR);
				errorString = OLD_GRAM_ERROR;
				gmState = GM_HOLD;
			}

			if ( resourceStateKnown == false ) {
				break;
			}
			if ( jobContact == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( wantResubmit || doResubmit ) {
				gmState = GM_CLEAN_JOBMANAGER;
			} else {
				if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_IN ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
					submitLogged = true;
				}
				if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
					executeLogged = true;
				}

				gmState = GM_REGISTER;
			}
			} break;
		case GM_REGISTER: {
			// Register for callbacks from an already-running jobmanager.
			rc = gahp.globus_gram_client_job_callback_register( jobContact,
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact, &status,
										&error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				globusError = GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER;
				gmState = GM_RESTART;
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND ) {
				gmState = GM_RESTART;
				break;
			} 
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL ) {
				// the job completed or failed while we were not around -- now
				// the jobmanager is sitting in a state where all it will permit
				// is a status query or a commit to exit.  switch into 
				// GM_SUBMITTED state and do an immediate probe to figure out
				// if the state is done or failed, and move on from there.
				probeNow = true;
				gmState = GM_SUBMITTED;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_callback_register()", rc );
				globusError = rc;
				gmState = GM_STOP_AND_RESTART;
				break;
			}
				// Now handle the case of we got GLOBUS_SUCCESS...
			callbackRegistered = true;
			UpdateGlobusState( status, error );
			gmState = GM_STDIO_UPDATE;
			} break;
		case GM_STDIO_UPDATE: {
			// Update an already-running jobmanager to send its I/O to us
			// instead a previous incarnation.
			if ( RSL == NULL ) {
				RSL = buildStdioUpdateRSL();
			}
			rc = gahp.globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_UPDATE,
								RSL->Value(), &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure = true;
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL ) {
				// the job completed or failed while we were not around -- now
				// the jobmanager is sitting in a state where all it will permit
				// is a status query or a commit to exit.  switch into 
				// GM_SUBMITTED state and do an immediate probe to figure out
				// if the state is done or failed, and move on from there.
				probeNow = true;
				gmState = GM_SUBMITTED;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STDIO_UPDATE)", rc );
				globusError = rc;
				gmState = GM_STOP_AND_RESTART;
				break;
			}
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) {
				gmState = GM_SUBMIT_COMMIT;
			} else {
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_UNSUBMITTED: {
			// There are no outstanding gram submissions for this job (if
			// there is one, we've given up on it).
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else if ( condorState == HELD ) {
				addScheddUpdateAction( this, UA_FORGET_JOB, GM_UNSUBMITTED );
				// This object will be deleted when the update occurs
				break;
			} else {
				gmState = GM_SUBMIT;
			}
			} break;
		case GM_SUBMIT: {
			// Start a new gram submission for this job.
			char *job_contact;
			if ( condorState == REMOVED || condorState == HELD ) {
				myResource->CancelSubmit(this);
				gmState = GM_UNSUBMITTED;
				break;
			}
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
				// Don't set HOLD_REASON here --- that way, the reason will get set
				// to the globus error that caused the submission failure.
				// UpdateJobAdString( ATTR_HOLD_REASON,"Attempts to submit failed" );

				gmState = GM_HOLD;
				break;
			}
			now = time(NULL);
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {
				// Once RequestSubmit() is called at least once, you must
				// CancelRequest() once you're done with the request call
				if ( myResource->RequestSubmit(this) == false ) {
					break;
				}
				if ( RSL == NULL ) {
					RSL = buildSubmitRSL();
				}
				if ( RSL == NULL ) {
					gmState = GM_HOLD;
					break;
				}
				rc = gahp.globus_gram_client_job_request( 
										resourceManagerString,
										RSL->Value(),
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact, &job_contact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				myResource->SubmitComplete(this);
				lastSubmitAttempt = time(NULL);
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ||
					 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED ) {
					gmState = GM_PROXY_EXPIRED;
					break;
				}
				numSubmitAttempts++;
				jmProxyExpireTime = myProxy->expiration_time;
				if ( rc == GLOBUS_SUCCESS ) {
					// Previously this supported GRAM 1.0
					dprintf(D_ALWAYS, "(%d.%d) Unexpected remote response.  GRAM %s is now required.\n", procID.cluster, procID.proc, MIN_SUPPORTED_GRAM_V_STRING);
					errorString.sprintf("Unexpected remote response.  Remote server must speak GRAM %s", MIN_SUPPORTED_GRAM_V_STRING);
					gmState = GM_HOLD;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT ) {
					if ( jmVersion == GRAM_V_UNKNOWN ) {
						jmVersion = GRAM_V_1_6;
						UpdateJobAdInt( ATTR_GLOBUS_GRAM_VERSION, GRAM_V_1_6 );
					}
					callbackRegistered = true;
					rehashJobContact( this, jobContact, job_contact );
					SetJobContact(job_contact);
					UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
									   job_contact );
					gahp.globus_gram_client_job_contact_free( job_contact );
					gmState = GM_SUBMIT_SAVE;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_RSL_EVALUATION_FAILED &&
							(jmVersion == GRAM_V_UNKNOWN || jmVersion >= GRAM_V_1_6) ) {
					// Previously this supported GRAM 1.5
					dprintf(D_ALWAYS, "(%d.%d) Unexpected remote response.  GRAM %s is now required.\n", procID.cluster, procID.proc, MIN_SUPPORTED_GRAM_V_STRING);
					errorString.sprintf("Unexpected remote response.  Remote server must speak GRAM %s", MIN_SUPPORTED_GRAM_V_STRING);
					gmState = GM_HOLD;
				} else {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_request()", rc );
					dprintf(D_ALWAYS,"(%d.%d)    RSL='%s'\n",
							procID.cluster, procID.proc,RSL->Value());
					submitFailureCode = globusError = rc;
					WriteGlobusSubmitFailedEventToUserLog( ad,
														   submitFailureCode );
					jobAd->EvalBool(ATTR_REMATCH_CHECK,NULL,wantRematch);
					if ( wantRematch ) {
						// We go to CLEAR_REQUEST here so that wantRematch
						// will be acted upon
						doResubmit = 1;
						gmState = GM_CLEAR_REQUEST;
					} else {
						// We don't go to CLEAR_REQUEST here because it will
						// clear out globusError, which we want to save once
						// we run out of submit attempts.
						// TODO We need a better solution to this, so that
						//   we always go to the same state.
						gmState = GM_UNSUBMITTED;
					}
					reevaluate_state = true;
				}
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_UNSUBMITTED;
			} else {
				unsigned int delay = 0;
				if ( (lastSubmitAttempt + submitInterval) > now ) {
					delay = (lastSubmitAttempt + submitInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_SUBMIT_SAVE: {
			// Save the jobmanager's contact for a new gram submission.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				done = addScheddUpdateAction( this, UA_UPDATE_JOB_AD,
											GM_SUBMIT_SAVE );
				if ( !done ) {
					break;
				}
				gmState = GM_SUBMIT_COMMIT;
			}
			} break;
		case GM_SUBMIT_COMMIT: {
			// Now that we've saved the jobmanager's contact, commit the
			// gram job submission.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				if ( GetCallbacks() ) {
					if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED &&
						 (globusStateErrorCode == GLOBUS_GRAM_PROTOCOL_ERROR_COMMIT_TIMED_OUT ||
						  globusStateErrorCode == 0) ) {
dprintf(D_FULLDEBUG,"(%d.%d) jobmanager timed out on commit, clearing request\n",procID.cluster, procID.proc);
						doResubmit = 1;
						gmState = GM_CLEAR_REQUEST;
						break;
					}
				}
				rc = gahp.globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST,
								NULL, &status, &error );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_REQUEST)", rc );
					globusError = rc;
					WriteGlobusSubmitFailedEventToUserLog( ad, globusError );
					gmState = GM_CANCEL;
				} else {
					gmState = GM_SUBMITTED;
				}
			}
			} break;
		case GM_SUBMITTED: {
			// The job has been submitted (or is about to be by the
			// jobmanager). Wait for completion or failure, and probe the
			// jobmanager occassionally to make it's still alive.
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
				gmState = GM_CHECK_OUTPUT;
			} else if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				gmState = GM_FAILED;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else if ( JmShouldSleep() ) {
				gmState = GM_PUT_TO_SLEEP;
			} else {
				if ( GetCallbacks() == true ) {
					reevaluate_state = true;
					break;
				}
				if ( jmProxyExpireTime < myProxy->expiration_time) {
					gmState = GM_REFRESH_PROXY;
					break;
				}
				now = time(NULL);
				if ( lastProbeTime < enteredCurrentGmState ) {
					lastProbeTime = enteredCurrentGmState;
				}
				if ( probeNow ) {
					lastProbeTime = 0;
					probeNow = false;
				}
				if ( now >= lastProbeTime + probeInterval ) {
					gmState = GM_PROBE_JOBMANAGER;
					break;
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + probeInterval) > now ) {
					delay = (lastProbeTime + probeInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_REFRESH_PROXY: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				rc = gahp.globus_gram_client_job_refresh_credentials(
																jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR("refresh_credentials()",rc);
					globusError = rc;
					gmState = GM_STOP_AND_RESTART;
					break;
				}
				jmProxyExpireTime = myProxy->expiration_time;
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_PROBE_JOBMANAGER: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				rc = gahp.globus_gram_client_job_status( jobContact,
														 &status, &error );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_status()", rc );
					globusError = rc;
					gmState = GM_STOP_AND_RESTART;
					break;
				}
				UpdateGlobusState( status, error );
				ClearCallbacks();
				lastProbeTime = time(NULL);
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_CHECK_OUTPUT: {
			// The job has completed. Make sure we got all the output.
			// If we haven't, stop and restart the jobmanager to prompt
			// sending of the rest.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_DONE_COMMIT;
			} else {
				char size_str[128];
				if ( streamOutput == false && streamError == false &&
					 stageOutput == false && stageError == false ) {
					gmState = GM_DONE_SAVE;
					break;
				}
				int output_size = -1;
				int error_size = -1;
				GetOutputSize( output_size, error_size );
				sprintf( size_str, "%d %d", output_size, error_size );
				rc = gahp.globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_SIZE,
									size_str, &status, &error );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc == GLOBUS_SUCCESS ) {
					// HACK!
					retryStdioSize = true;
					gmState = GM_DONE_SAVE;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_STILL_STREAMING ) {
					dprintf( D_FULLDEBUG, "(%d.%d) ERROR_STILL_STREAMING, will wait and retry\n", procID.cluster, procID.proc);
					gmState = GM_OUTPUT_WAIT;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_STDIO_SIZE ) {
					// HACK!
					if ( retryStdioSize ) {
						dprintf( D_FULLDEBUG, "(%d.%d) ERROR_STDIO_SIZE, will wait and retry\n", procID.cluster, procID.proc);
						retryStdioSize = false;
						gmState = GM_OUTPUT_WAIT;
					} else {
					// HACK!
					retryStdioSize = true;
					globusError = rc;
					gmState = GM_STOP_AND_RESTART;
					dprintf( D_FULLDEBUG, "(%d.%d) Requesting jobmanager restart because of GLOBUS_GRAM_PROTOCOL_ERROR_STDIO_SIZE\n",
							 procID.cluster, procID.proc);
					dprintf( D_FULLDEBUG, "(%d.%d) output_size = %d, error_size = %d\n",
							 procID.cluster, procID.proc,output_size, error_size );
					}
				} else {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STDIO_SIZE)", rc );
					// HACK!
					retryStdioSize = true;
					globusError = rc;
					gmState = GM_STOP_AND_RESTART;
					dprintf( D_FULLDEBUG, "(%d.%d) Requesting jobmanager restart because of unknown error\n",
							 procID.cluster, procID.proc);
				}
			}
			} break;
		case GM_OUTPUT_WAIT: {
			// We haven't received all the output from the job, but we
			// think the jobmanager is still trying to send it. Wait until
			// we think it's all here, then check again with the jobmanager.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_DONE_COMMIT;
				break;
			}
			now = time(NULL);
			if ( outputWaitLastGrowth == 0 ) {
				outputWaitLastGrowth = now;
				outputWaitOutputSize = 0;
				outputWaitErrorSize = 0;
				GetOutputSize( outputWaitOutputSize, outputWaitErrorSize );
				ClearCallbacks();
			} else {
				int new_output_size, new_error_size;

				GetOutputSize( new_output_size, new_error_size );

				if ( new_output_size > outputWaitOutputSize ) {
dprintf(D_FULLDEBUG,"(%d.%d) saw new output size %d\n",procID.cluster,procID.proc,new_output_size);
					outputWaitOutputSize = new_output_size;
					outputWaitLastGrowth = now;
				}

				if ( new_error_size > outputWaitErrorSize ) {
dprintf(D_FULLDEBUG,"(%d.%d) saw new error size %d\n",procID.cluster,procID.proc,new_error_size);
					outputWaitErrorSize = new_error_size;
					outputWaitLastGrowth = now;
				}
			}
			if ( now > outputWaitLastGrowth + outputWaitGrowthTimeout ) {
dprintf(D_FULLDEBUG,"(%d.%d) no new output/error for %d seconds, retrying STDIO_SIZE\n",procID.cluster,procID.proc,outputWaitGrowthTimeout);
				outputWaitLastGrowth = 0;
				gmState = GM_CHECK_OUTPUT;
			} else if ( GetCallbacks() ) {
dprintf(D_FULLDEBUG,"(%d.%d) got a callback, retrying STDIO_SIZE\n",procID.cluster,procID.proc);
				outputWaitLastGrowth = 0;
				gmState = GM_CHECK_OUTPUT;
			} else {
				daemonCore->Reset_Timer( evaluateStateTid,
										 OUTPUT_WAIT_POLL_INTERVAL );
			}
			} break;
		case GM_DONE_SAVE: {
			// Report job completion to the schedd.
			if ( condorState != HELD && condorState != REMOVED ) {
				if ( condorState != COMPLETED ) {
					condorState = COMPLETED;
					UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
				}

				if(useGridShell) {
					if( ! merge_file_into_classad(outputClassadFilename.GetCStr(), ad) ) {
						/* TODO: put job on hold or otherwise don't let it
						   quietly pass into the great beyond? */
						dprintf(D_ALWAYS,"(%d.%d) Failed to add job result attributes to job's classad.  Job's history will lack run information.\n",procID.cluster,procID.proc);
					}
				}

				done = addScheddUpdateAction( this, UA_UPDATE_JOB_AD,
											  GM_DONE_SAVE );
				if ( !done ) {
					break;
				}
			}
			gmState = GM_DONE_COMMIT;
			} break;
		case GM_DONE_COMMIT: {
			// Tell the jobmanager it can clean up and exit.
			rc = gahp.globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END,
								NULL, &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_END)", rc );
				globusError = rc;
				gmState = GM_STOP_AND_RESTART;
				break;
			}
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				// And even if we decide to go to GM_DELETE, that may
				// not actually remove the job from the queue if 
				// leave_job_in_queue attribute evals to TRUE --- and then
				// the job will errantly go on hold when the user
				// subsequently does a condor_rm.
			if ( jobContact != NULL ) {
				rehashJobContact( this, jobContact, NULL );
				myResource->CancelSubmit( this );
				SetJobContact(NULL);
				UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
								   NULL_JOB_CONTACT );
				addScheddUpdateAction( this, UA_UPDATE_JOB_AD, 0 );
			}
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_STOP_AND_RESTART: {
			// Something has wrong with the jobmanager and we want to stop
			// it and restart it to clear up the problem.
			rc = gahp.globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STOP_MANAGER,
								NULL, &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STOP_MANAGER)", rc );
				globusError = rc;
				gmState = GM_CANCEL;
			} else {
				gmState = GM_RESTART;
			}
			} break;
		case GM_RESTART: {
			// Something has gone wrong with the jobmanager and we need to
			// restart it.
			now = time(NULL);
			if ( jobContact == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( globusError != 0 && globusError == lastRestartReason ) {
				dprintf( D_FULLDEBUG, "(%d.%d) Restarting jobmanager for same reason "
						 "two times in a row: %d, aborting request\n",
						 procID.cluster, procID.proc,globusError );
				gmState = GM_CLEAR_REQUEST;
			} else if ( now < lastRestartAttempt + restartInterval ) {
				// After a restart, wait at least restartInterval before
				// trying another one.
				daemonCore->Reset_Timer( evaluateStateTid,
								(lastRestartAttempt + restartInterval) - now );
			} else {
				char *job_contact;

				// Once RequestSubmit() is called at least once, you must
				// CancelRequest() once you're done with the request call
				if ( myResource->RequestSubmit(this) == false ) {
					break;
				}
				if ( RSL == NULL ) {
					RSL = buildRestartRSL();
				}
				rc = gahp.globus_gram_client_job_request(
										resourceManagerString,
										RSL->Value(),
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact, &job_contact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				myResource->SubmitComplete(this);
				lastRestartAttempt = time(NULL);
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ||
					 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED ) {
					gmState = GM_PROXY_EXPIRED;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_NO_STATE_FILE &&
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) 
				{
						// Here we tried to restart, but the jobmanager claimed
						// there was no state file.  
						// If we still think we are in UNSUBMITTED state, the
						// odds are overwhelming that the jobmanager bailed
						// out and deleted the state file before we sent 
						// a commit signal.  So resubmit.
						// HOWEVER ---- this is evil and wrong!!!  What
						// should really happen is the Globus jobmanager
						// should *not* delete the state file if it fails
						// to receive the commit within 5 minutes.  
						// Don't understand why?  
						// Ask Todd T <tannenba@cs.wisc.edu>
					gmState = GM_CLEAR_REQUEST;
					doResubmit = 1;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_NO_STATE_FILE &&
					 condorState == COMPLETED ) {
					// Our restart attempt failed because the jobmanager
					// couldn't find the state file. If the job is marked
					// COMPLETED, then it's almost certain that we told the
					// jobmanager to clean up but died before we could
					// remove the job from the queue. So let's just remove
					// it now.
					gmState = GM_DELETE;
					break;
				}
				// TODO: What should be counted as a restart attempt and
				// what shouldn't?
				numRestartAttempts++;
				numRestartAttemptsThisSubmit++;
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_OLD_JM_ALIVE ) {
					// TODO: need to avoid an endless loop of old jm not
					// responding, start new jm, new jm says old one still
					// running, try to contact old jm again. How likely is
					// this to happen?
					gmState = GM_INIT;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_EXE ) {
					gmState = GM_CLEAR_REQUEST;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT ) {
					jmProxyExpireTime = myProxy->expiration_time;
					rehashJobContact( this, jobContact, job_contact );
					SetJobContact(job_contact);
					UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
									   job_contact );
					gahp.globus_gram_client_job_contact_free( job_contact );
					if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
						globusState = globusStateBeforeFailure;
					}
					globusStateErrorCode = 0;
					lastRestartReason = globusError;
					ClearCallbacks();
					gmState = GM_RESTART_SAVE;
				} else {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_request()", rc );
					globusError = rc;
					gmState = GM_CLEAR_REQUEST;
				}
			}
			} break;
		case GM_RESTART_SAVE: {
			// Save the restarted jobmanager's contact string on the schedd.
			done = addScheddUpdateAction( this, UA_UPDATE_JOB_AD,
										GM_RESTART_SAVE );
			if ( !done ) {
				break;
			}
			gmState = GM_RESTART_COMMIT;
			} break;
		case GM_RESTART_COMMIT: {
			// Tell the jobmanager it can proceed with the restart.
			rc = gahp.globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST,
								NULL, &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_REQUEST)", rc );
				globusError = rc;
				gmState = GM_STOP_AND_RESTART;
			} else {
				if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE &&
					 ( status == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ||
					   status == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ) ) {
					// TODO: should this get sent to the schedd in
					//   GM_RESTART_SAVE??
					dprintf(D_FULLDEBUG,"(%d.%d) jobmanager's job state went from DONE to %s across a restart, do the same here",
							procID.cluster, procID.proc, GlobusJobStatusName(status) );
					globusState = status;
					UpdateJobAdInt( ATTR_GLOBUS_STATUS, status );
					enteredCurrentGlobusState = time(NULL);
					addScheddUpdateAction( this, UA_UPDATE_JOB_AD, 0 );
				}
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.
			if ( globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE &&
				 globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				rc = gahp.globus_gram_client_job_cancel( jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL ) {
						// The jobmanager is already in either FAILED or
						// DONE state and ready to shut down. Go to
						// GM_CANCEL_WAIT, where we'll deal with it
						// appropriately.
					gmState = GM_CANCEL_WAIT;
					break;
				} else if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_cancel()", rc );
					globusError = rc;
					gmState = GM_CLEAR_REQUEST;
					break;
				}
			}
			if ( callbackRegistered ) {
				gmState = GM_CANCEL_WAIT;
			} else {
				// This can happen if we're restarting and fail to
				// reattach to a running jobmanager
				if ( condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_CLEAR_REQUEST;
				}
			}
			} break;
		case GM_CANCEL_WAIT: {
			// A cancel has been successfully issued. Wait for the
			// accompanying FAILED callback. Probe the jobmanager
			// occassionally to make sure it hasn't died on us.
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
				gmState = GM_DONE_COMMIT;
			} else if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				gmState = GM_FAILED;
			} else {
				now = time(NULL);
				if ( lastProbeTime < enteredCurrentGmState ) {
					lastProbeTime = enteredCurrentGmState;
				}
				if ( probeNow ) {
					lastProbeTime = 0;
					probeNow = false;
				}
				if ( now >= lastProbeTime + probeInterval ) {
					rc = gahp.globus_gram_client_job_status( jobContact,
														&status, &error );
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}
					if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
						 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
						 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
						connect_failure = true;
						break;
					}
					if ( rc != GLOBUS_SUCCESS ) {
						// unhandled error
						LOG_GLOBUS_ERROR( "globus_gram_client_job_status()", rc );
						globusError = rc;
						gmState = GM_CLEAR_REQUEST;
					}
					UpdateGlobusState( status, error );
					ClearCallbacks();
					lastProbeTime = now;
				} else {
					GetCallbacks();
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + probeInterval) > now ) {
					delay = (lastProbeTime + probeInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_FAILED: {
			// The jobmanager's job state has moved to FAILED. Send a
			// commit if necessary and take appropriate action.
			if ( globusStateErrorCode ==
				 GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED ) {

				gmState = GM_PROXY_EXPIRED;

			} else if ( FailureIsRestartable( globusStateErrorCode ) ) {

				// The job may still be submitted and/or recoverable,
				// so stop the jobmanager and restart it.
				if ( FailureNeedsCommit( globusStateErrorCode ) ) {
					globusError = globusStateErrorCode;
					gmState = GM_STOP_AND_RESTART;
				} else {
					gmState = GM_RESTART;
				}

			} else {

				if ( FailureNeedsCommit( globusStateErrorCode ) ) {

					// Sending a COMMIT_END here means we no longer care
					// about this job submission. Either we know the job
					// isn't pending/running or the user has told us to
					// forget lost job submissions.
					rc = gahp.globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END,
									NULL, &status, &error );
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}
					if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
						 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
						 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
						connect_failure = true;
						break;
					}
					if ( rc != GLOBUS_SUCCESS ) {
						// unhandled error
						LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_END)", rc );
						globusError = rc;
						gmState = GM_STOP_AND_RESTART;
						break;
					}

				}

				rehashJobContact( this, jobContact, NULL );
				myResource->CancelSubmit( this );
				SetJobContact(NULL);
				UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
								   NULL_JOB_CONTACT );
				jmVersion = GRAM_V_UNKNOWN;
				UpdateJobAdInt( ATTR_GLOBUS_GRAM_VERSION,
								jmVersion );
				addScheddUpdateAction( this, UA_UPDATE_JOB_AD, 0 );

				if ( condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_CLEAR_REQUEST;
				}

			}
			} break;
		case GM_DELETE: {
			// The job has completed or been removed. Delete it from the
			// schedd.
			schedd_actions = UA_DELETE_FROM_SCHEDD | UA_FORGET_JOB;
			if ( condorState == REMOVED ) {
				schedd_actions |= UA_LOG_ABORT_EVENT;
			} else if ( condorState == COMPLETED ) {
				schedd_actions |= UA_LOG_TERMINATE_EVENT;
			}
			addScheddUpdateAction( this, schedd_actions, GM_DELETE );
			// This object will be deleted when the update occurs
			} break;
		case GM_CLEAN_JOBMANAGER: {
			// Tell the jobmanager to cleanup an un-restartable job submission

			// Once RequestSubmit() is called at least once, you must
			// CancelRequest() once you're done with the request call
			char cleanup_rsl[4096];
			char *dummy_job_contact;
			if ( myResource->RequestSubmit(this) == false ) {
				break;
			}
			snprintf( cleanup_rsl, sizeof(cleanup_rsl), "&(cleanup=%s)", jobContact );
			rc = gahp.globus_gram_client_job_request( 
										resourceManagerString, 
										cleanup_rsl,
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact,
										&dummy_job_contact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			myResource->SubmitComplete(this);
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ||
				 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure = true;
				break;
			}
			if ( rc == GLOBUS_SUCCESS ||
				 rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CANCEL_FAILED ) {
				 // All is good... deed is done.
				gmState = GM_CLEAR_REQUEST;
			} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_EXE ) {
				// Jobmanager doesn't support cleanup attribute.
				gmState = GM_CLEAR_REQUEST;
			} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_NO_STATE_FILE ) {
				// State file disappeared.  Could be a normal condition
				// if we never successfully committed a submit (and
				// thus the jobmanager removed the state file when it timed
				// out waiting for the submit commit signal).
				gmState = GM_CLEAR_REQUEST;
			} else {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_request()", rc );
				dprintf(D_ALWAYS,"(%d.%d)    RSL='%s'\n",
						procID.cluster, procID.proc,cleanup_rsl);
				globusError = rc;
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CLEAR_REQUEST: {
			// Remove all knowledge of any previous or present job
			// submission, in both the gridmanager and the schedd.

			// If we are doing a rematch, we are simply waiting around
			// for the schedd to be updated and subsequently this globus job
			// object to be destroyed.  So there is nothing to do.
			if ( wantRematch ) {
				break;
			}

			// For now, put problem jobs on hold instead of
			// forgetting about current submission and trying again.
			// TODO: Let our action here be dictated by the user preference
			// expressed in the job ad.
			if ( (jobContact != NULL || (globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED && globusStateErrorCode != GLOBUS_GRAM_PROTOCOL_ERROR_JOB_UNSUBMITTED)) 
				     // && condorState != REMOVED 
					 && wantResubmit == 0 
					 && doResubmit == 0 ) {
				gmState = GM_HOLD;
				break;
			}
			// Only allow a rematch *if* we are also going to perform a resubmit
			if ( wantResubmit || doResubmit ) {
				ad->EvalBool(ATTR_REMATCH_CHECK,NULL,wantRematch);
			}
			if ( wantResubmit ) {
				wantResubmit = 0;
				dprintf(D_ALWAYS,
						"(%d.%d) Resubmitting to Globus because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_GLOBUS_RESUBMIT_CHECK );
			}
			if ( doResubmit ) {
				doResubmit = 0;
				dprintf(D_ALWAYS,
					"(%d.%d) Resubmitting to Globus (last submit failed)\n",
						procID.cluster, procID.proc );
			}
			schedd_actions = 0;
			if ( globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) {
				globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
				UpdateJobAdInt( ATTR_GLOBUS_STATUS, globusState );
				schedd_actions |= UA_UPDATE_JOB_AD;
			}
			globusStateErrorCode = 0;
			globusError = 0;
			lastRestartReason = 0;
			numRestartAttemptsThisSubmit = 0;
			jmVersion = GRAM_V_UNKNOWN;
			errorString = "";
			ClearCallbacks();
			useGridJobMonitor = true;
			// HACK!
			retryStdioSize = true;
			if ( jobContact != NULL ) {
				rehashJobContact( this, jobContact, NULL );
				myResource->CancelSubmit( this );
				SetJobContact(NULL);
				UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
								   NULL_JOB_CONTACT );
				schedd_actions |= UA_UPDATE_JOB_AD;
			}
			if ( condorState == RUNNING ) {
				condorState = IDLE;
				UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
				schedd_actions |= UA_UPDATE_JOB_AD;
			}
			if ( submitLogged ) {
				schedd_actions |= UA_LOG_EVICT_EVENT;
			}
			
			if ( wantRematch ) {
				dprintf(D_ALWAYS,
						"(%d.%d) Requesting schedd to rematch job because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_REMATCH_CHECK );

				// Set ad attributes so the schedd finds a new match.
				int dummy;
				if ( ad->LookupBool( ATTR_JOB_MATCHED, dummy ) != 0 ) {
					UpdateJobAdBool( ATTR_JOB_MATCHED, 0 );
					UpdateJobAdInt( ATTR_CURRENT_HOSTS, 0 );
					schedd_actions |= UA_UPDATE_JOB_AD;
				}

				// If we are rematching, we need to forget about this job
				// cuz we wanna pull a fresh new job ad, with a fresh new match,
				// from the all-singing schedd.
				schedd_actions |= UA_FORGET_JOB;
			}
			
			// If there are no updates to be done when we first enter this
			// state, addScheddUpdateAction will return done immediately
			// and not waste time with a needless connection to the
			// schedd. If updates need to be made, they won't show up in
			// schedd_actions after the first pass through this state
			// because we modified our local variables the first time
			// through. However, since we registered update events the
			// first time, addScheddUpdateAction won't return done until
			// they've been committed to the schedd.
			done = addScheddUpdateAction( this, schedd_actions,
										  GM_CLEAR_REQUEST );
			if ( !done ) {
				break;
			}
			DeleteOutput();
			submitLogged = false;
			executeLogged = false;
			submitFailedLogged = false;
			terminateLogged = false;
			abortLogged = false;
			evictLogged = false;
			gmState = GM_UNSUBMITTED;
			} break;
		case GM_HOLD: {
			// Put the job on hold in the schedd.
			// TODO: what happens if we learn here that the job is removed?
			condorState = HELD;
			UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
			schedd_actions = UA_HOLD_JOB | UA_FORGET_JOB | UA_UPDATE_JOB_AD;
			if ( jobContact &&
				 globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN ) {
				globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN;
				UpdateJobAdInt( ATTR_GLOBUS_STATUS, globusState );
				//UpdateGlobusState( GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN, 0 );
			}
			// Set the hold reason as best we can
			// TODO: set the hold reason in a more robust way.
			char holdReason[1024];
			int holdCode = 0;
			int holdSubCode = 0;
			holdReason[0] = '\0';
			holdReason[sizeof(holdReason)-1] = '\0';
			ad->LookupString( ATTR_HOLD_REASON, holdReason,
							  sizeof(holdReason) - 1 );
			ad->LookupInteger(ATTR_HOLD_REASON_CODE,holdCode);
			ad->LookupInteger(ATTR_HOLD_REASON_SUBCODE,holdSubCode);
			if ( holdReason[0] == '\0' && errorString != "" ) {
				strncpy( holdReason, errorString.Value(),
						 sizeof(holdReason) - 1 );
			}
			if ( holdReason[0] == '\0' && globusStateErrorCode != 0 ) {
				snprintf( holdReason, 1024, "Globus error %d: %s",
						  globusStateErrorCode,
						  gahp.globus_gram_client_error_string( globusStateErrorCode ) );
				holdCode = CONDOR_HOLD_CODE_GlobusGramError;
				holdSubCode = globusStateErrorCode;
			}
			if ( holdReason[0] == '\0' && globusError != 0 ) {
				snprintf( holdReason, 1024, "Globus error %d: %s", globusError,
						gahp.globus_gram_client_error_string( globusError ) );
				holdCode = CONDOR_HOLD_CODE_GlobusGramError;
				holdSubCode = globusError;
			}
			if ( holdReason[0] == '\0' ) {
				strncpy( holdReason, "Unspecified gridmanager error",
						 sizeof(holdReason) - 1 );
			}
			UpdateJobAdString( ATTR_HOLD_REASON, holdReason );
			UpdateJobAdInt(ATTR_HOLD_REASON_CODE, holdCode);
			UpdateJobAdInt(ATTR_HOLD_REASON_SUBCODE, holdSubCode);
			addScheddUpdateAction( this, schedd_actions, GM_HOLD );
			// This object will be deleted when the update occurs
			} break;
		case GM_PROXY_EXPIRED: {
			// The proxy for this job is either expired or about to expire.
			// If requested, put the job on hold. Otherwise, wait for the
			// proxy to be refreshed, then resume handling the job.
			bool hold_if_credential_expired = 
				param_boolean(PARAM_HOLD_IF_CRED_EXPIRED,true);
			if ( hold_if_credential_expired ) {
					// set hold reason via Globus cred expired error code
				globusStateErrorCode =
					GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED;
				gmState = GM_HOLD;
				break;
			}
			now = time(NULL);
			// if ( myProxy->expiration_time > JM_MIN_PROXY_TIME + now ) {
			if ( myProxy->expiration_time > (minProxy_time + 60) + now ) {
				// resume handling the job.
				gmState = GM_INIT;
			} else {
				// Do nothing. Our proxy is about to expire.
			}
			} break;
		case GM_PUT_TO_SLEEP: {
			rc = gahp.globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STOP_MANAGER,
								NULL, &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 // rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STOP_MANAGER)", rc );
				globusError = rc;
				gmState = GM_CANCEL;
			} else {
				gmState = GM_JOBMANAGER_ASLEEP;
			}
			} break;
		case GM_JOBMANAGER_ASLEEP: {
			if ( callbackGlobusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED &&
				 callbackGlobusStateErrorCode == GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED ) {
				// Small hack to ignore the callback caused by the STOP
				// signal we sent to GM_PUT_TO_SLEEP
				ClearCallbacks();
			} else {
				GetCallbacks();
			}
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				// The GRAM poll script returned FAILED, which the
				// jobmanager considers a permanent, fatal error.
				// Disable the grid monitor for this job submission so
				// that we don't put the jobmanager right back to sleep
				// after we restart it. We want it to do a poll itself
				// and enter the FAILED state.
				useGridJobMonitor = false;
			}
			globusError = 0;
			if ( JmShouldSleep() == false ) {
				gmState = GM_RESTART;
			}
			} break;
		default:
			EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster,procID.proc,
					gmState );
		}

		if ( gmState != old_gm_state || globusState != old_globus_state ) {
			reevaluate_state = true;
		}
		if ( globusState != old_globus_state ) {
//			dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
//					procID.cluster, procID.proc,
//					GlobusJobStatusName(old_globus_state),
//					GlobusJobStatusName(globusState));
			enteredCurrentGlobusState = time(NULL);
		}
		if ( gmState != old_gm_state ) {
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
			// If we were waiting for a pending globus call, we're not
			// anymore so purge it.
			gahp.purgePendingRequests();
			// If we were calling a globus call that used RSL, we're done
			// with it now, so free it.
			if ( RSL ) {
				delete RSL;
				RSL = NULL;
			}
			connect_failure_counter = 0;
		}

	} while ( reevaluate_state );

	if ( connect_failure && !jmUnreachable && !resourceDown ) {
		if ( connect_failure_counter < maxConnectFailures ) {
				// We are seeing a lot of failures to connect
				// with Globus 2.2 libraries, often due to GSI not able 
				// to authenticate.
			connect_failure_counter++;
			int retry_secs = param_integer(
				"GRIDMANAGER_CONNECT_FAILURE_RETRY_INTERVAL",5);
			dprintf(D_FULLDEBUG,
				"(%d.%d) Connection failure (try #%d), retrying in %d secs\n",
				procID.cluster,procID.proc,connect_failure_counter,retry_secs);
			daemonCore->Reset_Timer( evaluateStateTid, retry_secs );
		} else {
			dprintf(D_FULLDEBUG,
				"(%d.%d) Connection failure, requesting a ping of the resource\n",
				procID.cluster,procID.proc);
			jmUnreachable = true;
			myResource->RequestPing( this );
		}
	}

	return TRUE;
}

int GlobusJob::CommunicationTimeout()
{
	// This function is called by a daemonCore timer if the resource
	// object has been waiting too long for a gatekeeper ping to 
	// succeed.
	// For now, put the job on hold.

	globusStateErrorCode = GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED;
	gmState = GM_HOLD;
	communicationTimeoutTid = -1;
	return TRUE;
}

void GlobusJob::NotifyResourceDown()
{
	resourceStateKnown = true;
	if ( resourceDown == false ) {
		WriteGlobusResourceDownEventToUserLog( ad );
	}
	resourceDown = true;
	jmUnreachable = false;
	SetEvaluateState();
	// set a timeout timer, so we don't wait forever for this
	// resource to reappear.
	if ( communicationTimeoutTid != -1 ) {
		// timer already set, our work is done
		return;
	}
	int timeout = param_integer(PARAM_GLOBUS_GATEKEEPER_TIMEOUT,60*60*24*5);
	int time_of_death = 0;
	unsigned int now = time(NULL);
	ad->LookupInteger( ATTR_GLOBUS_RESOURCE_UNAVAILABLE_TIME, time_of_death );
	if ( time_of_death ) {
		timeout = timeout - (now - time_of_death);
	}

	if ( timeout > 0 ) {
		communicationTimeoutTid = daemonCore->Register_Timer( timeout,
							(TimerHandlercpp)&GlobusJob::CommunicationTimeout,
							"CommunicationTimeout", (Service*) this );
	} else {
		// timeout must have occurred while the gridmanager was down
		CommunicationTimeout();
	}

	if (!time_of_death) {
		UpdateJobAdInt(ATTR_GLOBUS_RESOURCE_UNAVAILABLE_TIME,now);
		addScheddUpdateAction( this, UA_UPDATE_JOB_AD, 0 );
	}
}

void GlobusJob::NotifyResourceUp()
{
	if ( communicationTimeoutTid != -1 ) {
		daemonCore->Cancel_Timer(communicationTimeoutTid);
		communicationTimeoutTid = -1;
	}
	resourceStateKnown = true;
	if ( resourceDown == true ) {
		WriteGlobusResourceUpEventToUserLog( ad );
	}
	resourceDown = false;
	if ( jmUnreachable ) {
		globusError = GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER;
		gmState = GM_RESTART;
		enteredCurrentGmState = time(NULL);
		if ( RSL ) {
			delete RSL;
			RSL = NULL;
		}
	}
	jmUnreachable = false;
	SetEvaluateState();
	int time_of_death = 0;
	ad->LookupInteger( ATTR_GLOBUS_RESOURCE_UNAVAILABLE_TIME, time_of_death );
	if ( time_of_death ) {
		UpdateJobAd(ATTR_GLOBUS_RESOURCE_UNAVAILABLE_TIME,"UNDEFINED");
		addScheddUpdateAction( this, UA_UPDATE_JOB_AD, 0 );
	}
}

// We're assuming that any updates that come through UpdateCondorState()
// are coming from the schedd, so we don't need to update it.
void GlobusJob::UpdateCondorState( int new_state )
{
	if ( new_state != condorState ) {
		condorState = new_state;
		UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
		SetEvaluateState();
	}
}

bool GlobusJob::AllowTransition( int new_state, int old_state )
{

	// Prevent non-transitions or transitions that go backwards in time.
	// The jobmanager shouldn't do this, but notification of events may
	// get re-ordered (callback and probe results arrive backwards).
    if ( new_state == old_state ||
		 new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ||
		 old_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ||
		 old_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ||
		 ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_IN &&
		   old_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED) ||
		 ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING &&
		   old_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED &&
		   old_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_IN) ||
		 ( old_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT &&
		   new_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE &&
		   new_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) ) {
		return false;
	}

	return true;
}


void GlobusJob::UpdateGlobusState( int new_state, int new_error_code )
{
	bool allow_transition;

	allow_transition = AllowTransition( new_state, globusState );

	if ( allow_transition ) {
		// where to put logging of events: here or in EvaluateState?
		int update_actions = UA_UPDATE_JOB_AD;

		dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
				procID.cluster, procID.proc,
				GlobusJobStatusName(globusState),
				GlobusJobStatusName(new_state));

		if ( ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
			   new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ) &&
			 condorState == IDLE ) {
			condorState = RUNNING;
			UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
		}

		if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED &&
			 condorState == RUNNING ) {
			condorState = IDLE;
			UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
		}

		if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED &&
			 !submitLogged && !submitFailedLogged ) {
			if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
					// TODO: should SUBMIT_FAILED_EVENT be used only on
					//   certain errors (ones we know are submit-related)?
				submitFailureCode = new_error_code;
				update_actions |= UA_LOG_SUBMIT_FAILED_EVENT;
			} else {
					// The request was successfuly submitted. Write it to
					// the user-log and increment the globus submits count.
				int num_globus_submits = 0;
				update_actions |= UA_LOG_SUBMIT_EVENT;
				ad->LookupInteger( ATTR_NUM_GLOBUS_SUBMITS,
								   num_globus_submits );
				num_globus_submits++;
				UpdateJobAdInt( ATTR_NUM_GLOBUS_SUBMITS, num_globus_submits );
			}
		}
		if ( (new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
			  new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ||
			  new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ||
			  new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED)
			 && !executeLogged ) {
			update_actions |= UA_LOG_EXECUTE_EVENT;
		}

		if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
			globusStateBeforeFailure = globusState;
		} else {
			UpdateJobAdInt( ATTR_GLOBUS_STATUS, new_state );
		}

		globusState = new_state;
		globusStateErrorCode = new_error_code;
		enteredCurrentGlobusState = time(NULL);

		addScheddUpdateAction( this, update_actions, 0 );

		SetEvaluateState();
	}
}

void GlobusJob::GramCallback( int new_state, int new_error_code )
{
	if ( AllowTransition(new_state,
						 callbackGlobusState ?
						 callbackGlobusState :
						 globusState ) ) {

		callbackGlobusState = new_state;
		callbackGlobusStateErrorCode = new_error_code;

		SetEvaluateState();
	}
}

bool GlobusJob::GetCallbacks()
{
	if ( callbackGlobusState != 0 ) {
		UpdateGlobusState( callbackGlobusState,
						   callbackGlobusStateErrorCode );

		callbackGlobusState = 0;
		callbackGlobusStateErrorCode = 0;
		return true;
	} else {
		return false;
	}
}

void GlobusJob::ClearCallbacks()
{
	callbackGlobusState = 0;
	callbackGlobusStateErrorCode = 0;
}

GlobusResource *GlobusJob::GetResource()
{
	return myResource;
}

void GlobusJob::SetJobContact(const char * contact)
{
	free(jobContact);
	if(contact)
		jobContact = strdup( contact );
	else
		jobContact = NULL;
}

bool GlobusJob::IsExitStatusValid()
{
	/* Using gridshell?  They're valid.  No gridshell?  Not available. */
	return useGridShell;
}

MyString *GlobusJob::buildSubmitRSL()
{
	int transfer;
	MyString *rsl = new MyString;
	MyString iwd = "";
	MyString riwd = "";
	MyString buff;
	char *attr_value = NULL;
	char *rsl_suffix = NULL;

	if(useGridShell) {
		dprintf(D_FULLDEBUG, "(%d.%d) Using gridshell\n",
			procID.cluster, procID.proc );
	}

	if ( ad->LookupString( ATTR_GLOBUS_RSL, &rsl_suffix ) &&
						   rsl_suffix[0] == '&' ) {
		*rsl = rsl_suffix;
		free( rsl_suffix );
		return rsl;
	}

	if ( ad->LookupString(ATTR_JOB_IWD, &attr_value) && *attr_value ) {
		iwd = attr_value;
		int len = strlen(attr_value);
		if ( len > 1 && attr_value[len - 1] != '/' ) {
			iwd += '/';
		}
	} else {
		iwd = "/";
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	//Start off the RSL
	rsl->sprintf( "&(rsl_substitution=(GRIDMANAGER_GASS_URL %s))",
				  gassServerUrl );

	//We're assuming all job classads have a command attribute
	//First look for executable in the spool area.
	MyString executable_path;
	char *spooldir = param("SPOOL");
	if ( spooldir ) {
		char *source = gen_ckpt_name(spooldir,procID.cluster,ICKPT,0);
		free(spooldir);
		if ( access(source,F_OK | X_OK) >= 0 ) {
				// we can access an executable in the spool dir
			executable_path = source;
		}
	}
	if( executable_path.IsEmpty() ) {
			// didn't find any executable in the spool directory,
			// so use what is explicitly stated in the job ad
		if( ! ad->LookupString( ATTR_JOB_CMD, &attr_value ) ) {
			attr_value = (char *)malloc(1);
			attr_value[0] = 0;
		}
		executable_path = attr_value;
		free(attr_value);
		attr_value = NULL;
	}
	*rsl += "(executable=";

	int transfer_executable = 0;
	if( ! ad->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer ) ) {
		transfer_executable = 1;
	}

	MyString input_classad_filename;
	MyString output_classad_filename;
	MyString gridshell_log_filename = "condor_gridshell.log.";
	gridshell_log_filename += procID.cluster;
	gridshell_log_filename += '.';
	gridshell_log_filename += procID.proc;

	if( useGridShell ) {
		// We always transfer the gridshell executable.

		/* TODO XXX adesmet: I'm probably stomping all over the GRAM 1.4
		   detection and work around.  For example, I assume I can stick the
		   real executable into the transfer_input_files, but 1.4 doesn't
		   support that (1.6 does).  Make sure this is worked out.  */

		/* TODO XXX adesmet: This assumes that the local gridshell can run on
		 * the 
		   remote side.  For cross-architecture/OS jobs, this is false.  We'll
		   need to more intelligently select a gridshell binary, perhaps
		   autodetectings (surprisingly hard), or forcing user to specify
		   CPU/OS (and defaulting to local one if not specified).*/

		buff = "$(GRIDMANAGER_GASS_URL)";
		char * tmp = param("GRIDSHELL");
		if( tmp ) {
			buff += tmp;
			free(tmp);
		} else {

			/* TODO XXX adesmet: Put job on hold, then bail.  Also add test to
			   condor_submit.  If job.gridshell == TRUE, then condor_config_val
			   GRIDSHELL must be defined. */

		}

		bool bsuccess = write_classad_input_file( ad, executable_path, input_classad_filename );
		if( ! bsuccess ) {
			/* TODO XXX adesmet: Writing to file failed?  Bail. */
			dprintf(D_ALWAYS, "(%d.%d) Attempt to write gridshell file %s failed.\n", 
				procID.cluster, procID.proc, input_classad_filename.GetCStr() );
		}

		output_classad_filename.sprintf("%s.OUT", input_classad_filename.GetCStr());
		outputClassadFilename = output_classad_filename;


	} else if ( transfer_executable ) {
		buff = "$(GRIDMANAGER_GASS_URL)";
		if ( executable_path[0] != '/' ) {
			buff += iwd;
		}
		buff += executable_path;

	} else {
		buff = executable_path;
	}

	*rsl += rsl_stringify( buff.Value() );

	if ( ad->LookupString(ATTR_JOB_REMOTE_IWD, &attr_value) && *attr_value ) {
		*rsl += ")(directory=";
		*rsl += rsl_stringify( attr_value );

		riwd = attr_value;
	} else {
		// The user didn't specify a remote IWD, so tell the jobmanager to
		// create a scratch directory in its default location and make that
		// the remote IWD.
		*rsl += ")(scratchdir='')(directory=$(SCRATCH_DIRECTORY)";

		riwd = "$(SCRATCH_DIRECTORY)";
	}
	if ( riwd[riwd.Length()-1] != '/' ) {
		riwd += '/';
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	if(useGridShell) {
		/* for gridshell, pass the gridshell the filename of the input
		   classad.  The real arguments will be in the classad, so we
		   don't need to pass them. */
		*rsl += ")(arguments=-gridshell -job-input-ad ";
		*rsl += input_classad_filename;
		*rsl += " -job-output-ad ";
		*rsl += output_classad_filename;
		*rsl += " -job-stdin - -job-stdout - -job-stderr -";
	} else if ( ad->LookupString(ATTR_JOB_ARGUMENTS, &attr_value)
				&& *attr_value ) {
		*rsl += ")(arguments=";
		*rsl += attr_value;
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	if( ad->LookupString(ATTR_JOB_INPUT, &attr_value) && *attr_value &&
		strcmp(attr_value, NULL_FILE) ) {
		*rsl += ")(stdin=";
		if ( !ad->LookupBool( ATTR_TRANSFER_INPUT, transfer ) || transfer ) {
			buff = "$(GRIDMANAGER_GASS_URL)";
			if ( attr_value[0] != '/' ) {
				buff += iwd;
			}
			buff += attr_value;
		} else {
			buff = attr_value;
		}
		*rsl += rsl_stringify( buff.Value() );
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( streamOutput ) {
		*rsl += ")(stdout=";
		buff.sprintf( "$(GRIDMANAGER_GASS_URL)%s", localOutput );
		*rsl += rsl_stringify( buff.Value() );
	} else {
		if ( stageOutput ) {
			*rsl += ")(stdout=$(GLOBUS_CACHED_STDOUT)";
		} else {
			if ( ad->LookupString(ATTR_JOB_OUTPUT, &attr_value) &&
				 *attr_value && strcmp( attr_value, NULL_FILE ) ) {
				*rsl += ")(stdout=";
				*rsl += rsl_stringify( attr_value );
			}
			if ( attr_value != NULL ) {
				free( attr_value );
				attr_value = NULL;
			}
		}
	}

	if ( streamError ) {
		*rsl += ")(stderr=";
		buff.sprintf( "$(GRIDMANAGER_GASS_URL)%s", localError );
		*rsl += rsl_stringify( buff.Value() );
	} else {
		if ( stageError ) {
			*rsl += ")(stderr=$(GLOBUS_CACHED_STDERR)";
		} else {
			if ( ad->LookupString(ATTR_JOB_ERROR, &attr_value) &&
				 *attr_value && strcmp( attr_value, NULL_FILE ) ) {
				*rsl += ")(stderr=";
				*rsl += rsl_stringify( attr_value );
			}
			if ( attr_value != NULL ) {
				free( attr_value );
				attr_value = NULL;
			}
		}
	}

	bool has_input_files = ad->LookupString(ATTR_TRANSFER_INPUT_FILES, &attr_value) && *attr_value;

	if ( ( useGridShell && transfer_executable ) || has_input_files) {
		StringList filelist( NULL, "," );
		if( attr_value ) {
			filelist.initializeFromString( attr_value );
		}
		if( useGridShell ) {
			filelist.append(input_classad_filename.GetCStr());
			if(transfer_executable) {
				filelist.append(executable_path.GetCStr());
			}
		}
		if ( !filelist.isEmpty() ) {
			char *filename;
			*rsl += ")(file_stage_in=";
			filelist.rewind();
			while ( (filename = filelist.next()) != NULL ) {
				// append file pairs to rsl
				*rsl += '(';
				buff = "$(GRIDMANAGER_GASS_URL)";
				if ( filename[0] != '/' ) {
					buff += iwd;
				}
				buff += filename;
				*rsl += rsl_stringify( buff );
				*rsl += ' ';
				buff = riwd;
				buff += basename( filename );
				*rsl += rsl_stringify( buff );
				*rsl += ')';
			}
		}
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( ( ad->LookupString(ATTR_TRANSFER_OUTPUT_FILES, &attr_value) &&
		   *attr_value ) || stageOutput || stageError || useGridShell) {
		StringList filelist( NULL, "," );
		if( attr_value ) {
			filelist.initializeFromString( attr_value );
		}
		if( useGridShell ) {
				// If we're the grid shell, we want to append some
				// files to  be transfered back: the final status
				// ClassAd from the gridshell

			ASSERT( output_classad_filename.GetCStr() );
			filelist.append( output_classad_filename.GetCStr() );
			filelist.append( gridshell_log_filename.GetCStr() );
		}
		if ( !filelist.isEmpty() || stageOutput || stageError ) {
			char *filename;
			*rsl += ")(file_stage_out=";

			if ( stageOutput ) {
				*rsl += "($(GLOBUS_CACHED_STDOUT) ";
				buff.sprintf( "$(GRIDMANAGER_GASS_URL)%s", localOutput );
				*rsl += rsl_stringify( buff );
				*rsl += ')';
			}

			if ( stageError ) {
				*rsl += "($(GLOBUS_CACHED_STDERR) ";
				buff.sprintf( "$(GRIDMANAGER_GASS_URL)%s", localError );
				*rsl += rsl_stringify( buff );
				*rsl += ')';
			}

			filelist.rewind();
			while ( (filename = filelist.next()) != NULL ) {
				// append file pairs to rsl
				*rsl += '(';
				buff = riwd;
				buff += basename( filename );
				*rsl += rsl_stringify( buff );
				*rsl += ' ';
				buff = "$(GRIDMANAGER_GASS_URL)";
				if ( filename[0] != '/' ) {
					buff += iwd;
				}
				buff += filename;
				*rsl += rsl_stringify( buff );
				*rsl += ')';
			}
		}
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( useGridShell ) {
		*rsl += ")(environment=";
		*rsl += "(CONDOR_CONFIG 'only_env')";
		*rsl += "(_CONDOR_GRIDSHELL_DEBUG 'D_FULLDEBUG')";
		*rsl += "(_CONDOR_GRIDSHELL_LOG '";
		*rsl += gridshell_log_filename.GetCStr();
		*rsl += "')";
	} else if ( ad->LookupString(ATTR_JOB_ENVIRONMENT, &attr_value)
				&& *attr_value ) {
		Environ env_obj;
		env_obj.add_string(attr_value);
		char **env_vec = env_obj.get_vector();
		int i = 0;
		if ( env_vec[0] ) {
			*rsl += ")(environment=";
		}
		for ( i = 0; env_vec[i]; i++ ) {
			char *equals = strchr(env_vec[i],'=');
			if ( !equals ) {
				// this environment entry has no equals sign!?!?
				continue;
			}
			*equals = '\0';
			buff.sprintf( "(%s %s)", env_vec[i],
							 rsl_stringify(equals + 1) );
			*rsl += buff;
		}
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	buff.sprintf( ")(proxy_timeout=%d", JM_MIN_PROXY_TIME );
	*rsl += buff;

	int commit_timeout = param_integer("GRIDMANAGER_GLOBUS_COMMIT_TIMEOUT", JM_COMMIT_TIMEOUT);

	buff.sprintf( ")(save_state=yes)(two_phase=%d)"
				  "(remote_io_url=$(GRIDMANAGER_GASS_URL))",
				  commit_timeout);
	*rsl += buff;

	if ( rsl_suffix != NULL ) {
		*rsl += rsl_suffix;
		free( rsl_suffix );
	}

	dprintf( D_FULLDEBUG, "Final RSL: %s\n", rsl->GetCStr() );
	return rsl;
}

MyString *GlobusJob::buildRestartRSL()
{
	MyString *rsl = new MyString;
	MyString buff;

	DeleteOutput();

	rsl->sprintf( "&(rsl_substitution=(GRIDMANAGER_GASS_URL %s))(restart=%s)"
				  "(remote_io_url=$(GRIDMANAGER_GASS_URL))", gassServerUrl,
				  jobContact );
	if ( streamOutput ) {
		*rsl += "(stdout=";
		buff.sprintf( "$(GRIDMANAGER_GASS_URL)%s", localOutput );
		*rsl += rsl_stringify( buff.Value() );
		*rsl += ")(stdout_position=0)";
	}
	if ( streamError ) {
		*rsl += "(stderr=";
		buff.sprintf( "$(GRIDMANAGER_GASS_URL)%s", localError );
		*rsl += rsl_stringify( buff.Value() );
		*rsl += ")(stderr_position=0)";
	}

	buff.sprintf( "(proxy_timeout=%d)", JM_MIN_PROXY_TIME );
	*rsl += buff;

	return rsl;
}

MyString *GlobusJob::buildStdioUpdateRSL()
{
	MyString *rsl = new MyString;
	MyString buff;
	char *attr_value = NULL; /* in case the classad lookups fail */

	DeleteOutput();

	rsl->sprintf( "&(remote_io_url=%s)", gassServerUrl );
	if ( streamOutput ) {
		*rsl += "(stdout=";
		buff.sprintf( "%s%s", gassServerUrl, localOutput );
		*rsl += rsl_stringify( buff.Value() );
		*rsl += ")(stdout_position=0)";
	}
	if ( streamError ) {
		*rsl += "(stderr=";
		buff.sprintf( "%s%s", gassServerUrl, localError );
		*rsl += rsl_stringify( buff.Value() );
		*rsl += ")(stderr_position=0)";
	}

	if ( ad->LookupString(ATTR_TRANSFER_INPUT_FILES, &attr_value) &&
		 *attr_value ) {
		// GRAM 1.6 won't let you change file transfer info in a
		// stdio-update, so force it to fail, resulting in a stop-and-
		// restart
		*rsl += "(invalid=bad)";
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( ( ad->LookupString(ATTR_TRANSFER_OUTPUT_FILES, &attr_value) &&
		   *attr_value ) || stageOutput || stageError ) {
		// GRAM 1.6 won't let you change file transfer info in a
		// stdio-update, so force it to fail, resulting in a stop-and-
		// restart
		*rsl += "(invalid=bad)";
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	return rsl;
}

bool GlobusJob::GetOutputSize( int& output_size, int& error_size )
{
	int rc;
	struct stat file_status;
	bool retval = true;

	if ( streamOutput || stageOutput ) {
		rc = stat( localOutput, &file_status );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS,
					 "(%d.%d) stat failed for output file %s (errno=%d)\n",
					 procID.cluster, procID.proc, localOutput, errno );
			output_size = 0;
			retval = false;
		} else {
			output_size = file_status.st_size;
		}
	}

	if ( streamError || stageError ) {
		rc = stat( localError, &file_status );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS,
					 "(%d.%d) stat failed for error file %s (errno=%d)\n",
					 procID.cluster, procID.proc, localError, errno );
			error_size = 0;
			retval = false;
		} else {
			error_size = file_status.st_size;
		}
	}

	return retval;
}

void GlobusJob::DeleteOutput()
{
	int rc;
	struct stat fs;

	mode_t old_umask = umask(0);

	if ( streamOutput ) {
		rc = stat( localOutput, &fs );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) stat(%s) failed, errno=%d\n",
					 procID.cluster, procID.proc, localOutput, errno );
			fs.st_mode = S_IRWXU;
		}
		fs.st_mode &= S_IRWXU | S_IRWXG | S_IRWXO;
		rc = unlink( localOutput );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) unlink(%s) failed, errno=%d\n",
					 procID.cluster, procID.proc, localOutput, errno );
		}
		rc = creat( localOutput, fs.st_mode );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) creat(%s,%d) failed, errno=%d\n",
					 procID.cluster, procID.proc, localOutput, fs.st_mode,
					 errno );
		} else {
			close( rc );
		}
	}

	if ( streamError ) {
		rc = stat( localError, &fs );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) stat(%s) failed, errno=%d\n",
					 procID.cluster, procID.proc, localError, errno );
			fs.st_mode = S_IRWXU;
		}
		fs.st_mode &= S_IRWXU | S_IRWXG | S_IRWXO;
		rc = unlink( localError );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) unlink(%s) failed, errno=%d\n",
					 procID.cluster, procID.proc, localError, errno );
		}
		rc = creat( localError, fs.st_mode );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) creat(%s,%d) failed, errno=%d\n",
					 procID.cluster, procID.proc, localError, fs.st_mode,
					 errno );
		} else {
			close( rc );
		}
	}

	umask( old_umask );
}

bool GlobusJob::FailureIsRestartable( int error_code )
{
	switch( error_code ) {
		// Normally, 0 isn't a valid error_code, but it can be returned
		// when a poll of the job by the jobmanager fails, which is not
		// restartable.
	case 0:
	case GLOBUS_GRAM_PROTOCOL_ERROR_EXECUTABLE_NOT_FOUND:
	case GLOBUS_GRAM_PROTOCOL_ERROR_STDIN_NOT_FOUND:
	case GLOBUS_GRAM_PROTOCOL_ERROR_STAGING_EXECUTABLE:
	case GLOBUS_GRAM_PROTOCOL_ERROR_STAGING_STDIN:
	case GLOBUS_GRAM_PROTOCOL_ERROR_EXECUTABLE_PERMISSIONS:
	case GLOBUS_GRAM_PROTOCOL_ERROR_BAD_DIRECTORY:
	case GLOBUS_GRAM_PROTOCOL_ERROR_COMMIT_TIMED_OUT:
	case GLOBUS_GRAM_PROTOCOL_ERROR_USER_CANCELLED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_SUBMIT_UNKNOWN:
	case GLOBUS_GRAM_PROTOCOL_ERROR_JOBTYPE_NOT_SUPPORTED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_TEMP_SCRIPT_FILE_FAILED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_RSL_DIRECTORY:
	case GLOBUS_GRAM_PROTOCOL_ERROR_RSL_EXECUTABLE:
	case GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDIN:
	case GLOBUS_GRAM_PROTOCOL_ERROR_RSL_ENVIRONMENT:
	case GLOBUS_GRAM_PROTOCOL_ERROR_RSL_ARGUMENTS:
	case GLOBUS_GRAM_PROTOCOL_ERROR_JOB_EXECUTION_FAILED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_COUNT:
	case GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_CACHE:
	case GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_USER_PROXY:
	case GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SCHEDULER_SPECIFIC:
		return false;
	case GLOBUS_GRAM_PROTOCOL_ERROR_STAGE_OUT_FAILED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_TTL_EXPIRED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED:
	default:
		return true;
	}
}

bool GlobusJob::FailureNeedsCommit( int error_code )
{
	switch( error_code ) {
	case GLOBUS_GRAM_PROTOCOL_ERROR_COMMIT_TIMED_OUT:
	case GLOBUS_GRAM_PROTOCOL_ERROR_TTL_EXPIRED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED:
	case GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED:
		return false;
	default:
		return true;
	}
}

bool
GlobusJob::JmShouldSleep()
{
	if ( jmProxyExpireTime < myProxy->expiration_time ) {
		return false;
	}
	if ( condorState != IDLE && condorState != RUNNING ) {
		return false;
	}
	if ( useGridJobMonitor == false ) {
		return false;
	}

	if ( myResource->GridJobMonitorActive() == false ) {
		return false;
	}

	switch ( globusState ) {
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING:
		return true;
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE:
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED:
		if ( !streamOutput && !streamError ) {
			return true;
		} else {
			return false;
		}
	default:
		return false;
	}
}
