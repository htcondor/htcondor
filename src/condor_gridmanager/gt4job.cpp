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
#include "condor_attributes.h"
#include "condor_debug.h"
#include "env.h"
#include "condor_string.h"	// for strnewp and friends
#include "condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"
#include "filename_tools.h"
#include "job_lease.h"
#include "directory.h"

#include "gridmanager.h"
#include "gt4job.h"
#include "condor_config.h"
#include "my_username.h"


// GridManager job states
#define GM_INIT					0
#define GM_REGISTER				1
#define GM_STDIO_UPDATE			2
#define GM_UNSUBMITTED			3
#define GM_SUBMIT				4
#define GM_SUBMIT_SAVE			5
#define GM_SUBMITTED			6
#define GM_DONE_SAVE			7
#define GM_DONE_COMMIT			8
#define GM_CANCEL				9
#define GM_FAILED				10
#define GM_DELETE				11
#define GM_CLEAR_REQUEST		12
#define GM_HOLD					13
#define GM_PROXY_EXPIRED		14
#define GM_EXTEND_LIFETIME		15
#define GM_PROBE_JOBMANAGER		16
#define GM_START				17
#define GM_GENERATE_ID			18
#define GM_DELEGATE_PROXY		19
#define GM_SUBMIT_ID_SAVE		20

static const char *GMStateNames[] = {
	"GM_INIT",
	"GM_REGISTER",
	"GM_STDIO_UPDATE",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_DONE_COMMIT",
	"GM_CANCEL",
	"GM_FAILED",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROXY_EXPIRED",
	"GM_EXTEND_LIFETIME",
	"GM_PROBE_JOBMANAGER",
	"GM_START",
	"GM_GENERATE_ID",
	"GM_DELEGATE_PROXY",
	"GM_SUBMIT_ID_SAVE",
};

#define GT4_JOB_STATE_PENDING		"Pending"
#define GT4_JOB_STATE_ACTIVE		"Active"
#define GT4_JOB_STATE_FAILED		"Failed"
#define GT4_JOB_STATE_DONE			"Done"
#define GT4_JOB_STATE_SUSPENDED		"Suspended"
#define GT4_JOB_STATE_UNSUBMITTED	"Unsubmitted"
#define GT4_JOB_STATE_STAGE_IN		"StageIn"
#define GT4_JOB_STATE_STAGE_OUT		"StageOut"
#define GT4_JOB_STATE_CLEAN_UP		"CleanUp"


// TODO: once we can set the jobmanager's proxy timeout, we should either
// let this be set in the config file or set it to
// GRIDMANAGER_MINIMUM_PROXY_TIME + 60
#define JM_MIN_PROXY_TIME		(minProxy_time + 60)

#define DEFAULT_LEASE_DURATION	12*60*60

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define LOG_GLOBUS_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, globusState %s: %s %s\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],globusState.Value(), \
        func,error==GAHPCLIENT_COMMAND_TIMED_OUT?"timed out":"failed")

#define CHECK_PROXY \
{ \
	if ( PROXY_NEAR_EXPIRED( jobProxy ) && gmState != GM_PROXY_EXPIRED ) { \
		dprintf( D_ALWAYS, "(%d.%d) proxy is about to expire\n", \
				 procID.cluster, procID.proc ); \
		gmState = GM_PROXY_EXPIRED; \
		break; \
	} \
}

void
gt4GramCallbackHandler( void * /* user_arg */, const char *job_contact,
						const char *state, const char *fault,
						const int exit_code )
{
	int rc;
	GT4Job *this_job;
	MyString job_id;

	job_id.sprintf( "gt4 %s", job_contact );


	// Find the right job object
	rc = BaseJob::JobsByRemoteId.lookup( HashKey( job_id.Value() ),
										 (BaseJob*&)this_job );
	if ( rc != 0 || this_job == NULL ) {
		dprintf( D_ALWAYS, 
			"gt4GramCallbackHandler: Can't find record for globus job with "
			"contact %s on globus state %s, ignoring\n",
			job_contact, state );
		return;
	}

	dprintf( D_ALWAYS, "(%d.%d) gram callback: state %s, fault %s, exit code %d\n",
			 this_job->procID.cluster, this_job->procID.proc, state,
			 fault ? fault : "(null)", exit_code );

	this_job->GramCallback( state, fault, exit_code );
}

void GT4JobInit()
{
}

void GT4JobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	GT4Job::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_RESOURCE_PROBE_INTERVAL", 5 * 60 );
	GT4Resource::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	GT4Job::setGahpCallTimeout( tmp_int );
	GT4Resource::setGahpCallTimeout( tmp_int );

	// Tell all the resource objects to deal with their new config values
	GT4Resource *next_resource;

	GT4Resource::ResourcesByName.startIterations();

	while ( GT4Resource::ResourcesByName.iterate( next_resource ) != 0 ) {
		next_resource->Reconfig();
	}
}

bool GT4JobAdMatch( const ClassAd *job_ad ) {
	int universe;
	MyString resource;
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 strncasecmp( resource.Value(), "gt4 ", 4 ) == 0 ) {

		return true;
	}
	return false;
}

BaseJob *GT4JobCreate( ClassAd *jobad )
{
	return (BaseJob *)new GT4Job( jobad );
}


static
const char *xml_stringify( const char *string )
{
	static MyString dst;

	dst = string;
	dst.replaceString( "&", "&amp;" );
	dst.replaceString( "<", "&lt;" );
	dst.replaceString( ">", "&gt;" );

	return dst.Value();
}

/* Not currently used
static
const char *xml_stringify( const MyString& src )
{
	return xml_stringify( src.Value() );
}
*/

int Gt4JobStateToInt( const char *status ) {
	if ( status == NULL ) {
		return 0;
	} else if ( !strcmp( status, GT4_JOB_STATE_PENDING ) ) {
		return 1;
	} else if ( !strcmp( status, GT4_JOB_STATE_ACTIVE ) ) {
		return 2;
	} else if ( !strcmp( status, GT4_JOB_STATE_FAILED ) ) {
		return 4;
	} else if ( !strcmp( status, GT4_JOB_STATE_DONE ) ) {
		return 8;
	} else if ( !strcmp( status, GT4_JOB_STATE_SUSPENDED ) ) {
		return 16;
	} else if ( !strcmp( status, GT4_JOB_STATE_UNSUBMITTED ) ) {
		return 32;
	} else if ( !strcmp( status, GT4_JOB_STATE_STAGE_IN ) ) {
		return 64;
	} else if ( !strcmp( status, GT4_JOB_STATE_STAGE_OUT ) ) {
		return 128;
	} else if ( !strcmp( status, GT4_JOB_STATE_CLEAN_UP ) ) {
		return 256;
	} else {
		return 0;
	}
}

int GT4Job::probeInterval = 300;			// default value
int GT4Job::submitInterval = 300;			// default value
int GT4Job::gahpCallTimeout = 300;			// default value

GT4Job::GT4Job( ClassAd *classad )
	: BaseJob( classad )
{
	int bool_value;
	int int_value;
	MyString iwd;
	MyString job_output;
	MyString job_error;
	MyString job_proxy_subject;
	MyString grid_resource;
	MyString grid_job_id;
	MyString gridftp_url_base;
	MyString globus_delegation_uri;
	bool job_already_submitted = false;
	MyString error_string = "";
	char *gahp_path = NULL;

	RSL = NULL;
	callbackRegistered = false;
	jobContact = NULL;
	localOutput = NULL;
	localError = NULL;
	streamOutput = false;
	streamError = false;
	stageOutput = false;
	stageError = false;
	gmState = GM_INIT;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	enteredCurrentGlobusState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	jmProxyExpireTime = 0;
	jmLifetime = 0;
	resourceManagerString = NULL;
	jobmanagerType = NULL;
	myResource = NULL;
	jobProxy = NULL;
	gramCallbackContact = NULL;
	gahp = NULL;
	delegatedCredentialURI = NULL;
	gridftpServer = NULL;
	m_isGram42 = false;

	// In GM_HOLD, we assme HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	// TODO This is bad. The job may already be on hold with a valid hold
	//   reason, and here we'll clear it out (and propogate to the schedd).
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	jobProxy = AcquireProxy( jobAd, error_string,
							 (TimerHandlercpp)&GT4Job::ProxyCallback, this );
	if ( jobProxy == NULL ) {
		if ( error_string == "" ) {
			error_string.sprintf( "%s is not set in the job ad",
								  ATTR_X509_USER_PROXY );
		}
		goto error_exit;
	}

	gahp_path = param("GT4_GAHP");
	if ( gahp_path == NULL ) {
		error_string = "GT4_GAHP not defined";
		goto error_exit;
	}
	job_proxy_subject.sprintf( "GT4/%s", jobProxy->subject->fqan );
	gahp = new GahpClient( job_proxy_subject.Value(), gahp_path );
	free( gahp_path );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	jobAd->LookupString( ATTR_GRID_RESOURCE, grid_resource );
	if ( grid_resource.Length() ) {
		const char *token;

		grid_resource.Tokenize();

		token = grid_resource.GetNextToken( " ", false );
		if ( !token || stricmp( token, "gt4" ) ) {
			error_string.sprintf( "%s not of type gt4", ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = grid_resource.GetNextToken( " ", false );
		if ( token && *token ) {
			// If the resource url is missing a scheme, insert one
			if ( strncmp( token, "http://", 7 ) == 0 ||
				 strncmp( token, "https://", 8 ) == 0 ) {
				resourceManagerString = strdup( token );
			} else {
				MyString urlbuf;
				urlbuf.sprintf("https://%s", token );
				resourceManagerString = strdup( urlbuf.Value() );
			}
		} else {
			error_string.sprintf( "%s missing GRAM Service URL",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = grid_resource.GetNextToken( " ", false );
		if ( token && *token ) {
			jobmanagerType = strdup( token );
		} else {
			error_string.sprintf( "%s missing JobManager type",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

	} else {
		error_string.sprintf( "%s is not set in the job ad",
							  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	jobAd->LookupString( ATTR_GRID_JOB_ID, grid_job_id );
	if ( grid_job_id.Length() ) {
		SetRemoteJobId( strchr( grid_job_id.Value(), ' ' ) + 1 );
		job_already_submitted = true;
	}

		// Find/create an appropriate GT4Resource for this job
	myResource = GT4Resource::FindOrCreateResource( resourceManagerString,
													jobProxy );
	if ( myResource == NULL ) {
		error_string = "Failed to initialize GT4Resource object";
		goto error_exit;
	}

	// RegisterJob() may call our NotifyResourceUp/Down(), so be careful.
	myResource->RegisterJob( this );
	if ( job_already_submitted ) {
		myResource->AlreadySubmitted( this );
	}

	gridftpServer = GridftpServer::FindOrCreateServer( jobProxy );

	if ( jobContact ) {
		jobAd->LookupString( ATTR_GRIDFTP_URL_BASE, gridftp_url_base );
	}

	gridftpServer->RegisterClient( evaluateStateTid, gridftp_url_base.Length() ? gridftp_url_base.Value() : NULL);

	if ( jobContact &&
		 jobAd->LookupString( ATTR_GLOBUS_DELEGATION_URI, globus_delegation_uri ) ) {

		delegatedCredentialURI = strdup( globus_delegation_uri.Value() );
		myResource->registerDelegationURI( delegatedCredentialURI, jobProxy );
	}

	jobAd->LookupString( ATTR_GRID_JOB_STATUS, globusState );

	globusErrorString = "";

	if ( jobAd->LookupInteger( ATTR_JOB_LEASE_EXPIRATION, int_value ) ) {
		jmLifetime = int_value;
	}

	if ( jobAd->LookupString(ATTR_JOB_IWD, iwd) && iwd.Length() ) {
		int len = iwd.Length();
		if ( len > 1 && iwd[len - 1] != '/' ) {
			iwd += "/";
		}
	} else {
		iwd = "/";
	}

	if ( jobAd->LookupString(ATTR_JOB_OUTPUT, job_output) && job_output.Length() &&
		 strcmp( job_output.Value(), NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_OUTPUT, bool_value ) ||
			 bool_value ) {
			MyString full_job_output;

			if ( job_output[0] != '/' ) {
				full_job_output = iwd;
			}

			full_job_output += job_output;
			localOutput = strdup( full_job_output.Value() );

			bool_value = 0;
			jobAd->LookupBool( ATTR_STREAM_OUTPUT, bool_value );
			streamOutput = (bool_value != 0);
			stageOutput = !streamOutput;
		}
	}

	if ( jobAd->LookupString(ATTR_JOB_ERROR, job_error) && job_error.Length() &&
		 strcmp( job_error.Value(), NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_ERROR, bool_value ) ||
			 bool_value ) {
			MyString full_job_error;

			if ( job_error[0] != '/' ) {
				full_job_error = iwd;
			}

			full_job_error += job_error;
			localError = strdup( full_job_error.Value() );

			bool_value = 0;
			jobAd->LookupBool( ATTR_STREAM_ERROR, bool_value );
			streamError = (bool_value != 0);
			stageError = !streamError;
		}
	}

		// Trigger recreation of the empty scratch directory for
		// already-submitted jobs, in case it was deleted. This is
		// a simple-but-crude way to fix this problem.
	getDummyJobScratchDir();

	return;

 error_exit:
		// We must ensure that the code-path from GM_HOLD doesn't depend
		// on any initialization that's been skipped.
	gmState = GM_HOLD;
	if ( !error_string.IsEmpty() ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string.Value() );
	}
	return;
}

GT4Job::~GT4Job()
{
	if ( gridftpServer ) {
		gridftpServer->UnregisterClient( evaluateStateTid );
	}
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
	if ( jobProxy ) {
		ReleaseProxy( jobProxy, (TimerHandlercpp)&GT4Job::ProxyCallback, this );
	}
	if ( gramCallbackContact ) {
		free( gramCallbackContact );
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( jobmanagerType != NULL ) {
		free( jobmanagerType );
	}
	if ( delegatedCredentialURI != NULL) {
		free( delegatedCredentialURI );
	}

}

void GT4Job::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

int GT4Job::ProxyCallback()
{
	if ( gmState == GM_PROXY_EXPIRED ) {
		SetEvaluateState();
	}
	return 0;
}

void GT4Job::doEvaluateState()
{
	int old_gm_state;
	MyString old_globus_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	bool attr_exists;
	bool attr_dirty;
	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, globusState %s\n",
			procID.cluster,procID.proc,GMStateNames[gmState],
			globusState.Value());

	if ( gahp ) {
		if ( !resourceStateKnown || resourcePingPending || resourceDown ) {
			gahp->setMode( GahpClient::results_only );
		} else {
			gahp->setMode( GahpClient::normal );
		}
	}

	do {
		reevaluate_state = false;
		old_gm_state = gmState;
		old_globus_state = globusState;

		switch ( gmState ) {
		case GM_INIT: {
			// This is the state all jobs start in when the GlobusJob object
			// is first created. Here, we do things that we didn't want to
			// do in the constructor because they could block (the
			// constructor is called while we're connected to the schedd).
			int err;

			if ( gahp->Initialize( jobProxy ) == false ) {
				dprintf( D_ALWAYS, "(%d.%d) Error initializing GAHP\n",
						 procID.cluster, procID.proc );
				
				jobAd->Assign( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			gahp->setDelegProxy( jobProxy );

			GahpClient::mode saved_mode = gahp->getMode();
			gahp->setMode( GahpClient::blocking );

			err = gahp->gt4_gram_client_callback_allow( gt4GramCallbackHandler,
														NULL,
														&gramCallbackContact );
			if ( err != GLOBUS_SUCCESS ) {
				dprintf( D_ALWAYS,
						 "(%d.%d) Error enabling GRAM callback, err=%d\n", 
						 procID.cluster, procID.proc, err );
				jobAd->Assign( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			gahp->setMode( saved_mode );

			if ( m_isGram42 == false && myResource->IsGram42() == true ) {
				if ( SwitchToGram42() == false ) {
					gmState = GM_HOLD;
					break;
				}
			}

			gmState = GM_START;
			} break;
		case GM_START: {
			// This state is the real start of the state machine, after
			// one-time initialization has been taken care of.
			// If we think there's a running jobmanager
			// out there, we try to register for callbacks (in GM_REGISTER).
			// The one way jobs can end up back in this state is if we
			// attempt a restart of a jobmanager only to be told that the
			// old jobmanager process is still alive.
			errorString = "";
			if ( jobContact == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( wantResubmit || doResubmit ) {
				gmState = GM_CLEAR_REQUEST;
			} else {
				if ( strncmp( jobContact, "uuid:", 5 ) == 0 ) {
						// JEF Make sure our proxy delegation and lease time
						//   are set up correctly. This may require going
						//   to a different state. Also, make sure we try
						//   to cancel the remote job if the local status
						//   is HELD or REMOVED
					gmState = GM_GENERATE_ID;
				} else {
					if ( globusState == GT4_JOB_STATE_STAGE_IN ||
						 globusState == GT4_JOB_STATE_PENDING ||
						 globusState == GT4_JOB_STATE_ACTIVE ||
						 globusState == GT4_JOB_STATE_SUSPENDED ||
						 globusState == GT4_JOB_STATE_STAGE_OUT ||
						 globusState == GT4_JOB_STATE_DONE ) {
						submitLogged = true;
					}
					if ( globusState == GT4_JOB_STATE_ACTIVE ||
						 globusState == GT4_JOB_STATE_SUSPENDED ||
						 globusState == GT4_JOB_STATE_STAGE_OUT ||
						 globusState == GT4_JOB_STATE_DONE ) {
						executeLogged = true;
					}

					gmState = GM_REGISTER;
				}
			}
			} break;
		case GM_REGISTER: {
			// Register for callbacks from an already-running jobmanager.
			CHECK_PROXY;
			rc = gahp->gt4_gram_client_job_callback_register( jobContact,
														gramCallbackContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "gt4_gram_client_job_callback_register()", rc );
				globusErrorString = gahp->getErrorString();
				gmState = GM_CANCEL;
				break;
			}
				// Now handle the case of we got GLOBUS_SUCCESS...
			callbackRegistered = true;
			probeNow = true;
			//gmState = GM_STDIO_UPDATE;
//			gmState = GM_CANCEL;
			gmState = GM_SUBMITTED;
			} break;
		case GM_STDIO_UPDATE: {
/*
			// Update an already-running jobmanager to send its I/O to us
			// instead a previous incarnation.
			CHECK_PROXY;
			if ( RSL == NULL ) {
				RSL = buildStdioUpdateRSL();
			}
			rc = gahp->globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_UPDATE,
								RSL->Value(), &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
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
			if ( globusState == GT4_JOB_STATE_UNSUBMITTED ) {
				gmState = GM_SUBMIT_COMMIT;
			} else {
				gmState = GM_SUBMITTED;
			}
*/
			gmState = GM_CANCEL;
			} break;
 		case GM_UNSUBMITTED: {
			// There are no outstanding gram submissions for this job (if
			// there is one, we've given up on it).
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else if ( condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			} else if ( gridftpServer->GetErrorMessage() ) {
				errorString = gridftpServer->GetErrorMessage();
				gmState = GM_HOLD;
				break;
			} else if ( gridftpServer->GetUrlBase() ) {
				jobAd->Assign( ATTR_GRIDFTP_URL_BASE, gridftpServer->GetUrlBase() );
				gmState = GM_DELEGATE_PROXY;
			}
			} break;
 		case GM_DELEGATE_PROXY: {
			const char *deleg_uri;
			const char *error_msg;
				// TODO What happens if Gt4Resource can't delegate proxy?
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			}
			if ( delegatedCredentialURI != NULL ) {
				gmState = GM_GENERATE_ID;
				break;
			}
			if ( (error_msg = myResource->getDelegationError( jobProxy )) ) {
					// There's a problem delegating the proxy
				errorString = error_msg;
				gmState = GM_HOLD;
			}
			deleg_uri = myResource->getDelegationURI( jobProxy );
			if ( deleg_uri == NULL ) {
					// proxy still needs to be delegated. Wait.
				break;
			}
			delegatedCredentialURI = strdup( deleg_uri );
			gmState = GM_GENERATE_ID;
			jobAd->Assign( ATTR_GLOBUS_DELEGATION_URI,
						   delegatedCredentialURI );
		} break;
		case GM_GENERATE_ID: {
			char *submit_id = NULL;

			// TODO: allow REMOVED or HELD jobs to break out of this state
			if (jobContact) {
				gmState = GM_SUBMIT_ID_SAVE;
				break;
			}

			rc = gahp->gt4_generate_submit_id( &submit_id );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				dprintf( D_ALWAYS, "(%d.%d) Error generating submit id\n",
						 procID.cluster, procID.proc );
				errorString = "Failed to generate submit id";
				gmState = GM_HOLD;
			} else {
					// Calculate the lease we want to give to the remote job
				int new_lease;	// CalculateJobLease needs an int
				ASSERT( CalculateJobLease( jobAd, new_lease,
										   DEFAULT_LEASE_DURATION ) );
				jmLifetime = new_lease;
				UpdateJobLeaseSent( jmLifetime );

				SetRemoteJobId( submit_id );
				//jobAd->Assign( ATTR_GLOBUS_SUBMIT_ID, submit_id );
				gmState = GM_SUBMIT_ID_SAVE;
			}
			free( submit_id );
		} break;
		case GM_SUBMIT_ID_SAVE: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				jobAd->GetDirtyFlag( ATTR_GRID_JOB_ID, &attr_exists, &attr_dirty );
				if ( attr_exists && attr_dirty ) {
					requestScheddUpdate( this, true );
					break;
				}
				gmState = GM_SUBMIT;
			}
		} break;
		case GM_SUBMIT: {
			// Start a new gram submission for this job.
 			char *job_contact = NULL;
			if ( condorState == REMOVED || condorState == HELD ) {
				myResource->CancelSubmit(this);
				gmState = GM_UNSUBMITTED;
				break;
			}
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
				if ( globusErrorString == "" ) {
					globusErrorString = "Attempts to submit failed";
				}
				gmState = GM_HOLD;
				break;
			}
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {
				CHECK_PROXY;
				// Once RequestSubmit() is called at least once, you must
				// CancelRequest() once you're done with the request call
				if ( myResource->RequestSubmit(this) == false ) {
					break;
				}
				if ( RSL == NULL ) {
					RSL = buildSubmitRSL();
				}
				if ( RSL == NULL ) {
					myResource->CancelSubmit(this);
					gmState = GM_HOLD;
					break;
				}

				rc = gahp->gt4_gram_client_job_create( 
													  jobContact,
										resourceManagerString,
										jobmanagerType,
										gramCallbackContact,
										RSL->Value(),
										jmLifetime,
										&job_contact );

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;
				jmProxyExpireTime = jobProxy->expiration_time;
				if ( rc == GLOBUS_SUCCESS ) {
					callbackRegistered = true;
					SetRemoteJobId( job_contact );
					free( job_contact );
					gmState = GM_SUBMIT_SAVE;
				} else {
					// unhandled error
					LOG_GLOBUS_ERROR( "gt4_gram_client_job_create()", rc );
					dprintf(D_ALWAYS,"(%d.%d)    RSL='%s'\n",
							procID.cluster, procID.proc,RSL->Value());
					globusErrorString = gahp->getErrorString();
					WriteGlobusSubmitFailedEventToUserLog( jobAd, 0,
													gahp->getErrorString() );
					myResource->CancelSubmit( this );
					gmState = GM_UNSUBMITTED;
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
				jobAd->GetDirtyFlag( ATTR_GRID_JOB_ID, &attr_exists, &attr_dirty );
				if ( attr_exists && attr_dirty ) {
					requestScheddUpdate( this, true );
					break;
				}
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_SUBMITTED: {
			// The job has been submitted (or is about to be by the
			// jobmanager). Wait for completion or failure, and probe the
			// jobmanager occassionally to make it's still alive.
			if ( globusState == GT4_JOB_STATE_DONE ) {
				gmState = GM_DONE_SAVE;
			} else if ( globusState == GT4_JOB_STATE_FAILED ) {
				gmState = GM_FAILED;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
					// Check that our gridftp server is healthy
				if ( gridftpServer->GetErrorMessage() ) {
					errorString = gridftpServer->GetErrorMessage();
					gmState = GM_HOLD;
					break;
				}
				MyString url_base;
				jobAd->LookupString( ATTR_GRIDFTP_URL_BASE, url_base );
				if ( gridftpServer->GetUrlBase() &&
					 strcmp( url_base.Value(),
							 gridftpServer->GetUrlBase() ) ) {
					gmState = GM_CANCEL;
					break;
				}

				if ( GetCallbacks() == true ) {
					reevaluate_state = true;
					break;
				}
				int new_lease;	// CalculateJobLease needs an int
				if ( CalculateJobLease( jobAd, new_lease,
										DEFAULT_LEASE_DURATION ) ) {
					jmLifetime = new_lease;
					gmState = GM_EXTEND_LIFETIME;
					break;
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
		case GM_EXTEND_LIFETIME: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				CHECK_PROXY;
				time_t new_lifetime = jmLifetime - now;
				rc = gahp->gt4_set_termination_time( jobContact,
													 new_lifetime );

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR("refresh_credentials()",rc);
					globusErrorString = gahp->getErrorString();
					gmState = GM_CANCEL;
					break;
				}
				jmLifetime = new_lifetime;
				UpdateJobLeaseSent( jmLifetime );
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_PROBE_JOBMANAGER: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				char *status = NULL;
				char *fault = NULL;
				int exit_code = -1;
				CHECK_PROXY;
				rc = gahp->gt4_gram_client_job_status( jobContact, &status,
													   &fault, &exit_code );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "gt4_gram_client_job_status()", rc );
					globusErrorString = gahp->getErrorString();
					gmState = GM_CANCEL;
					if ( status ) {
						free( status );
					}
					break;
				}
				MyString new_status = status;
				MyString new_fault = fault;
				UpdateGlobusState( new_status, new_fault );
				if ( status ) {
					free( status );
				}
				if ( fault ) {
					free( fault );
				}
				// Globus folks reporting a frequent case where
				// callback for Done arrives at the same time
				// as the results of a poll saying StageOut.
				// Try not clearing callbacks, since old
				// job statuses will be rejected by
				// AllowTransition().
				//ClearCallbacks();
				lastProbeTime = time(NULL);
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_DONE_SAVE: {
			// Report job completion to the schedd.
			JobTerminated();
			if ( condorState == COMPLETED ) {
				jobAd->GetDirtyFlag( ATTR_JOB_STATUS, &attr_exists, &attr_dirty );
				if ( attr_exists && attr_dirty ) {
					requestScheddUpdate( this, true );
					break;
				}
			}
			gmState = GM_DONE_COMMIT;
			} break;
		case GM_DONE_COMMIT: {
			// Tell the jobmanager it can clean up and exit.
			CHECK_PROXY;
			if ( myResource->RequestDestroy( this ) == false ) {
				break;
			}
			rc = gahp->gt4_gram_client_job_destroy( jobContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			myResource->DestroyComplete( this );
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "gt4_gram_client_job_destroy()", rc );
				globusErrorString = gahp->getErrorString();
				gmState = GM_CANCEL;
				break;
			}
			myResource->CancelSubmit( this );
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				SetRemoteJobId( NULL );
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				if ( jobContact != NULL ) {
					SetRemoteJobId( NULL );
					globusState = "";
					jobAd->Assign( ATTR_GLOBUS_STATUS, 32 );
					SetRemoteJobStatus( NULL );
					requestScheddUpdate( this, false );
				}
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.
			if ( globusState != GT4_JOB_STATE_DONE &&
				 globusState != GT4_JOB_STATE_FAILED ) {
				CHECK_PROXY;
				if ( myResource->RequestDestroy( this ) == false ) {
					break;
				}
				rc = gahp->gt4_gram_client_job_destroy( jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				myResource->DestroyComplete( this );
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "gt4_gram_client_job_destroy()", rc );
					globusErrorString = gahp->getErrorString();
					gmState = GM_CLEAR_REQUEST;
					break;
				}
				myResource->CancelSubmit( this );
				SetRemoteJobId( NULL );
			}
			myResource->DestroyComplete( this );
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_FAILED: {
			// The jobmanager's job state has moved to FAILED. Send a
			// commit if necessary and take appropriate action.

			// Sending a COMMIT_END here means we no longer care
			// about this job submission. Either we know the job
			// isn't pending/running or the user has told us to
			// forget lost job submissions.
			CHECK_PROXY;
			if ( myResource->RequestDestroy( this ) == false ) {
				break;
			}
			rc = gahp->gt4_gram_client_job_destroy( jobContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			myResource->DestroyComplete( this );
			if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
				LOG_GLOBUS_ERROR( "gt4_gram_client_job_destroy", rc );
				globusErrorString = gahp->getErrorString();
				gmState = GM_CLEAR_REQUEST;
				break;
			}

			SetRemoteJobId( NULL );
			myResource->CancelSubmit( this );
			globusState = "";
			jobAd->Assign( ATTR_GLOBUS_STATUS, 32 );
			SetRemoteJobStatus( NULL );
			requestScheddUpdate( this, false );

			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				//gmState = GM_CLEAR_REQUEST;
				gmState= GM_HOLD;
			}
			} break;
		case GM_DELETE: {
			// We are done with the job. Propagate any remaining updates
			// to the schedd, then delete this object.
			DoneWithJob();
			// This object will be deleted when the update occurs
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
			if ( (jobContact != NULL || globusState == GT4_JOB_STATE_FAILED) 
				     && condorState != REMOVED 
					 && wantResubmit == 0 
					 && doResubmit == 0 ) {
				gmState = GM_HOLD;
				break;
			}
			// Only allow a rematch *if* we are also going to perform a resubmit
			if ( wantResubmit || doResubmit ) {
				jobAd->EvalBool(ATTR_REMATCH_CHECK,NULL,wantRematch);
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
			if ( globusState != "" ) {
				globusState = "";
				SetRemoteJobStatus( NULL );
				jobAd->Assign( ATTR_GLOBUS_STATUS, 32 );
			}
			globusStateFaultString = "";
			globusErrorString = "";
			errorString = "";
			ClearCallbacks();
			jmLifetime = 0;
			UpdateJobLeaseSent( -1 );
			myResource->CancelSubmit( this );
			myResource->DestroyComplete( this );
			if ( jobContact != NULL ) {
				SetRemoteJobId( NULL );
			}
			JobIdle();
			if ( submitLogged ) {
				JobEvicted();
				if ( !evictLogged ) {
					WriteEvictEventToUserLog( jobAd );
					evictLogged = true;
				}
			}
			free( delegatedCredentialURI );
			delegatedCredentialURI = NULL;
			MyString val;
			if ( jobAd->LookupString( ATTR_GLOBUS_DELEGATION_URI, val ) ) {
				jobAd->AssignExpr( ATTR_GLOBUS_DELEGATION_URI, "Undefined" );
			}
			if ( jobAd->LookupString( ATTR_GRIDFTP_URL_BASE, val ) ) {
				jobAd->AssignExpr( ATTR_GRIDFTP_URL_BASE, "Undefined" );
			}
			
			if ( wantRematch ) {
				dprintf(D_ALWAYS,
						"(%d.%d) Requesting schedd to rematch job because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_REMATCH_CHECK );

				// Set ad attributes so the schedd finds a new match.
				int dummy;
				if ( jobAd->LookupBool( ATTR_JOB_MATCHED, dummy ) != 0 ) {
					jobAd->Assign( ATTR_JOB_MATCHED, false );
					jobAd->Assign( ATTR_CURRENT_HOSTS, 0 );
				}

				// If we are rematching, we need to forget about this job
				// cuz we wanna pull a fresh new job ad, with a fresh new match,
				// from the all-singing schedd.
				gmState = GM_DELETE;
				break;
			}
			
			// If there are no updates to be done when we first enter this
			// state, requestScheddUpdate will return done immediately
			// and not waste time with a needless connection to the
			// schedd. If updates need to be made, they won't show up in
			// schedd_actions after the first pass through this state
			// because we modified our local variables the first time
			// through. However, since we registered update events the
			// first time, requestScheddUpdate won't return done until
			// they've been committed to the schedd.
			const char *name;
			ExprTree *expr;
			jobAd->ResetExpr();
			if ( jobAd->NextDirtyExpr(name, expr) ) {
				requestScheddUpdate( this, true );
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
			// If the condor state is already HELD, then someone already
			// HELD it, so don't update anything else.
			if ( condorState != HELD ) {

				// Set the hold reason as best we can
				// TODO: set the hold reason in a more robust way.
				char holdReason[1024];
				holdReason[0] = '\0';
				holdReason[sizeof(holdReason)-1] = '\0';
				jobAd->LookupString( ATTR_HOLD_REASON, holdReason,
									 sizeof(holdReason) - 1 );
				if ( holdReason[0] == '\0' && errorString != "" ) {
					strncpy( holdReason, errorString.Value(),
							 sizeof(holdReason) - 1 );
				}
				if ( holdReason[0] == '\0' &&
					 !globusStateFaultString.IsEmpty() ) {

					snprintf( holdReason, 1024, "Globus error: %s",
							  globusStateFaultString.Value() );
				}
				if ( holdReason[0] == '\0' && !globusErrorString.IsEmpty() ) {
					snprintf( holdReason, 1024, "Globus error: %s",
							  globusErrorString.Value() );
				}
				if ( holdReason[0] == '\0' ) {
					strncpy( holdReason, "Unspecified gridmanager error",
							 sizeof(holdReason) - 1 );
				}

				JobHeld( holdReason );
			}
			gmState = GM_DELETE;
			} break;
		case GM_PROXY_EXPIRED: {
			// The proxy for this job is either expired or about to expire.
			// If requested, put the job on hold. Otherwise, wait for the
			// proxy to be refreshed, then resume handling the job.
			if ( jobProxy->expiration_time > JM_MIN_PROXY_TIME + now ) {
				gmState = GM_START;
			} else {
				// Do nothing. Our proxy is about to expire.
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
//					old_globus_state.Value(),
//					globusState.Value());
			enteredCurrentGlobusState = time(NULL);
		}
		if ( gmState != old_gm_state ) {
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
			// If we were waiting for a pending globus call, we're not
			// anymore so purge it.
			if ( gahp ) {
				gahp->purgePendingRequests();
			}
			// If we were calling a globus call that used RSL, we're done
			// with it now, so free it.
			if ( RSL ) {
				delete RSL;
				RSL = NULL;
			}
		}

	} while ( reevaluate_state );
}

bool GT4Job::AllowTransition( const MyString &new_state,
							  const MyString &old_state )
{

	// Prevent non-transitions or transitions that go backwards in time.
	// The jobmanager shouldn't do this, but notification of events may
	// get re-ordered (callback and probe results arrive backwards).
	if ( old_state == "" ) {
		return true;
	}
    if ( new_state == old_state ||
		 new_state == GT4_JOB_STATE_UNSUBMITTED ||
		 old_state == GT4_JOB_STATE_DONE ||
		 old_state == GT4_JOB_STATE_FAILED ||
		 ( new_state == GT4_JOB_STATE_STAGE_IN &&
		   old_state != GT4_JOB_STATE_UNSUBMITTED) ||
		 ( new_state == GT4_JOB_STATE_PENDING &&
		   old_state != GT4_JOB_STATE_UNSUBMITTED &&
		   old_state != GT4_JOB_STATE_STAGE_IN) ||
		 ( old_state == GT4_JOB_STATE_STAGE_OUT &&
		   new_state != GT4_JOB_STATE_DONE &&
		   new_state != GT4_JOB_STATE_FAILED ) ) {
		return false;
	}

	return true;
}


void GT4Job::UpdateGlobusState( const MyString &new_state, 
								const MyString &new_fault )
{
	bool allow_transition;

	allow_transition = AllowTransition( new_state, globusState );

		// We want to call SetRemoteJobStatus() even if the status
		// hasn't changed, so that BaseJob will know that we got a
		// status update.
	if ( allow_transition || new_state == globusState ) {
		SetRemoteJobStatus( globusState.Value() );
	}

	if ( allow_transition ) {
		// where to put logging of events: here or in EvaluateState?
		dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
				procID.cluster, procID.proc,
				globusState.Value(), new_state.Value());

		if ( ( new_state == GT4_JOB_STATE_ACTIVE ||
			   new_state == GT4_JOB_STATE_STAGE_OUT ) &&
			 condorState == IDLE ) {
			JobRunning();
		}

		if ( new_state == GT4_JOB_STATE_SUSPENDED &&
			 condorState == RUNNING ) {
			JobIdle();
		}

		if ( ( globusState == GT4_JOB_STATE_UNSUBMITTED ||
			   globusState == "" ) &&
			 !submitLogged && !submitFailedLogged ) {
			if ( new_state == GT4_JOB_STATE_FAILED ) {
					// TODO: should SUBMIT_FAILED_EVENT be used only on
					//   certain errors (ones we know are submit-related)?
				if ( !submitFailedLogged ) {
					WriteGlobusSubmitFailedEventToUserLog( jobAd, 0,
														   new_fault.Value() );
					submitFailedLogged = true;
				}
			} else {
					// The request was successfuly submitted. Write it to
					// the user-log and increment the globus submits count.
				int num_globus_submits = 0;
				if ( !submitLogged ) {
						// The GlobusSubmit event is now deprecated
					WriteGlobusSubmitEventToUserLog( jobAd );
					WriteGridSubmitEventToUserLog( jobAd );
					submitLogged = true;
				}
				jobAd->LookupInteger( ATTR_NUM_GLOBUS_SUBMITS,
									  num_globus_submits );
				num_globus_submits++;
				jobAd->Assign( ATTR_NUM_GLOBUS_SUBMITS, num_globus_submits );
			}
		}
		if ( (new_state == GT4_JOB_STATE_ACTIVE ||
			  new_state == GT4_JOB_STATE_STAGE_OUT ||
			  new_state == GT4_JOB_STATE_DONE ||
			  new_state == GT4_JOB_STATE_SUSPENDED)
			 && !executeLogged ) {
			WriteExecuteEventToUserLog( jobAd );
			executeLogged = true;
		}

		jobAd->Assign( ATTR_GLOBUS_STATUS,
					   Gt4JobStateToInt( new_state.Value() ) );

		globusState = new_state;
		globusStateFaultString = new_fault;
		enteredCurrentGlobusState = time(NULL);

		requestScheddUpdate( this, false );

		SetEvaluateState();
	}
}

void GT4Job::GramCallback( const char *new_state, const char *new_fault,
						   const int new_exit_code )
{
	MyString new_state_str = new_state;
	if ( AllowTransition(new_state_str,
						 callbackGlobusState != "" ?
						 callbackGlobusState :
						 globusState ) ) {

		callbackGlobusState = new_state_str;
		callbackGlobusStateFaultString = new_fault;

		if ( new_state_str == GT4_JOB_STATE_DONE ) {
			jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );
			jobAd->Assign( ATTR_ON_EXIT_CODE, new_exit_code );
		}

		SetEvaluateState();
	}
}

bool GT4Job::GetCallbacks()
{
	if ( callbackGlobusState != "" ) {
		UpdateGlobusState( callbackGlobusState,
						   callbackGlobusStateFaultString );

		ClearCallbacks();
		return true;
	} else {
		return false;
	}
}

void GT4Job::ClearCallbacks()
{
	callbackGlobusState = "";
	callbackGlobusStateFaultString = "";
}

BaseResource *GT4Job::GetResource()
{
	return (BaseResource *)myResource;
}

void GT4Job::SetRemoteJobId( const char *job_id )
{
	free( jobContact );
	if ( job_id ) {
		jobContact = strdup( job_id );
	} else {
		jobContact = NULL;
	}

	MyString full_job_id;
	if ( job_id ) {
		full_job_id.sprintf( "gt4 %s", job_id );
	}
	BaseJob::SetRemoteJobId( full_job_id.Value() );
}

// Build submit RSL.. er... XML
MyString *GT4Job::buildSubmitRSL()
{
	MyString *rsl = new MyString;
	MyString local_iwd = "";
	MyString remote_iwd = "";
	MyString riwd_parent="";
	MyString local_executable = "";
	MyString remote_executable = "";
	MyString local_stdout = "";
	MyString local_stderr = "";
	bool create_remote_iwd = false;
	bool transfer_executable = true;
	bool transfer_stdout = true;
	bool transfer_stderr = true;
	MyString buff;
	char *attr_value = NULL;
	char *rsl_suffix = NULL;
	StringList stage_in_list;
	StringList stage_out_list;
	bool staging_input = false;
	MyString local_url_base = "";

	if ( !jobAd->LookupString( ATTR_GRIDFTP_URL_BASE, local_url_base ) ) {
		errorString.sprintf( "%s not defined", ATTR_GRIDFTP_URL_BASE );
		delete rsl;
		return NULL;
	}

		// Once we add streaming support, remove this
	if ( streamOutput || streamError ) {
		errorString = "Streaming not supported";
		delete rsl;
		return NULL;
	}

	MyString delegation_epr;
	MyString delegation_uri = delegatedCredentialURI;
	int pos = delegation_uri.FindChar( '?' );
	if ( m_isGram42 == false ) {
			// GRAM 4.0
		delegation_epr.sprintf( "<ns01:Address xmlns:ns01=\"http://schemas.xmlsoap.org/ws/2004/03/addressing\">%s</ns01:Address>", delegation_uri.Substr( 0, pos-1 ).Value() );
		delegation_epr.sprintf_cat( "<ns01:ReferenceProperties xmlns:ns01=\"http://schemas.xmlsoap.org/ws/2004/03/addressing\">" );
		delegation_epr.sprintf_cat( "<ns1:DelegationKey xmlns:ns1=\"http://www.globus.org/08/2004/delegationService\">%s</ns1:DelegationKey>", delegation_uri.Substr( pos+1, delegation_uri.Length()-1 ).Value() );
		delegation_epr.sprintf_cat( "</ns01:ReferenceProperties>" );
		delegation_epr.sprintf_cat( "<ns01:ReferenceParameters xmlns:ns01=\"http://schemas.xmlsoap.org/ws/2004/03/addressing\"/>" );
	} else {
			// GRAM 4.2
		delegation_epr.sprintf( "<ns01:Address xmlns:ns01=\"http://www.w3.org/2005/08/addressing\">%s</ns01:Address>", delegation_uri.Substr( 0, pos-1 ).Value() );
		delegation_epr.sprintf_cat( "<ns01:ReferenceParameters xmlns:ns01=\"http://www.w3.org/2005/08/addressing\">" );
		delegation_epr.sprintf_cat( "<ns1:DelegationKey xmlns:ns1=\"http://www.globus.org/08/2004/delegationService\">%s</ns1:DelegationKey>", delegation_uri.Substr( pos+1, delegation_uri.Length()-1 ).Value() );
		delegation_epr.sprintf_cat( "</ns01:ReferenceParameters>" );
	}

		// Set our local job working directory
	if ( jobAd->LookupString(ATTR_JOB_IWD, &attr_value) && *attr_value ) {
		local_iwd = attr_value;
		int len = strlen(attr_value);
		if ( len > 1 && attr_value[len - 1] != '/' ) {
			local_iwd += '/';
		}
	} else {
		local_iwd = "/";
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

		// Set our remote job working directory
	if ( jobAd->LookupString(ATTR_JOB_REMOTE_IWD, &attr_value) && *attr_value ) {
			// The user gave us a remote IWD, use it.
		create_remote_iwd = false;
		remote_iwd = attr_value;
	} else {
		// The user didn't specify a remote IWD, so tell the jobmanager to
		// create a scratch directory in its default location and make that
		// the remote IWD.

		// Note that ${SCRATCH_DIR} is no longer supported by globus
		// Instead we'll upload an empty directory to be our scratch dir
		
		create_remote_iwd = true;
		ASSERT (jobContact);	// append job id for uniqueness, fool
		char *unique_id = jobContact;
			// Avoid having a colon in our job scratch directory name, for
			// any systems that may not like that.
		if ( strncmp( unique_id, "uuid:", 5 ) == 0 ) {
			unique_id = &unique_id[5];
		}
		// The GT4 server expects a leading slash *before* variable
		// substitution is completed for some reason. See globus bugzilla
		// ticket 2846 for details. To keep it happy, throw in an extra
		// slash, even though this'll result in file:////....
		remote_iwd.sprintf ("/${GLOBUS_SCRATCH_DIR}/job_%s", unique_id);
		// The default GLOBUS_SCRATCH_DIR (~/.globus/scratch) is not
		// created by Globus, so our attempt to create a sub-directory
		// will fail. As a work-around, try to create it. This will
		// succeed even if the directory already exists and is not
		// writable by the user.
		// http://bugzilla.globus.org/globus/show_bug.cgi?id=3802
		// http://bugzilla.globus.org/globus/show_bug.cgi?id=3803
		riwd_parent.sprintf("/${GLOBUS_SCRATCH_DIR}");
	}

	if ( remote_iwd[remote_iwd.Length()-1] != '/' ) {
		remote_iwd += '/';
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	//Start off the RSL
	rsl->sprintf( "<job>" );


	//We're assuming all job clasads have a command attribute
	jobAd->LookupString( ATTR_JOB_CMD, &attr_value );

	transfer_executable = true;
	jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer_executable );

	if ( transfer_executable == false ) {
			// The executable is already on the remote machine. Leave the
			// path alone.
		remote_executable = attr_value;
	} else {
			// The executable needs to be transferred. Construct the
			// pathname of the transferred file.
		remote_executable = remote_iwd;
		remote_executable += condor_basename( attr_value );

			// Figure out where the local copy of the executable is
		char *spooldir = param("SPOOL");
		if ( spooldir ) {
			char *source = gen_ckpt_name(spooldir,procID.cluster,ICKPT,0);
			free(spooldir);
			if ( access(source,F_OK | X_OK) >= 0 ) {
					// we can access an executable in the spool dir
				local_executable = source;
			}
		}
		if ( local_executable.IsEmpty() ) {
				// didn't find any executable in the spool directory,
				// so use what is explicitly stated in the job ad

				// We're assuming here that the submitter gives us full
				// paths for executables that are to be transferred.
			local_executable = attr_value;
		}
	}

	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	*rsl += printXMLParam( "executable", remote_executable.Value() );
	
	*rsl += printXMLParam( "directory", remote_iwd.Value() );

		// parse arguments
	{
		ArgList args;
		MyString arg_errors;
		MyString rsl_args;
		if(!args.AppendArgsFromClassAd(jobAd,&arg_errors)) {
			dprintf(D_ALWAYS,"(%d.%d) Failed to read job arguments: %s\n",
					procID.cluster, procID.proc, arg_errors.Value());
			errorString.sprintf("Failed to read job arguments: %s\n",
					arg_errors.Value());
			delete rsl;
			return NULL;
		}
		for(int a=0; a<args.Count(); a++) {
			char const *arg = args.GetArg(a);
			*rsl += printXMLParam ("argument", xml_stringify(arg));
		}
	}

		// Start building the list of files to be staged in
	if ( jobAd->LookupString(ATTR_TRANSFER_INPUT_FILES, &attr_value) &&
		 *attr_value ) {
		stage_in_list.initializeFromString( attr_value );
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

		// Start building the list of files to be staged out
	if ( jobAd->LookupString(ATTR_TRANSFER_OUTPUT_FILES, &attr_value) &&
		 *attr_value ) {
		stage_out_list.initializeFromString( attr_value );
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

		// For stdin/out/err, always give a full pathname, because GT4
		// doesn't resolve relative paths before handing to the local
		// scheduler, which can interpret relative paths as relative to
		// the user's home directory. Details in globus bugzilla ticket
		// 2886.

		// standard input
	if ( jobAd->LookupString(ATTR_JOB_INPUT, &attr_value) && *attr_value &&
		 strcmp( attr_value, NULL_FILE ) ) {

		bool transfer = true;
		jobAd->LookupBool( ATTR_TRANSFER_INPUT, transfer );

		if ( transfer == false ) {
			if ( attr_value[0] != '/' ) {
				buff.sprintf( "%s/%s", remote_iwd.Value(), attr_value );
			} else {
				buff.sprintf( "%s", attr_value );
			}
			*rsl += printXMLParam( "stdin", buff.Value() );
		} else {
			buff.sprintf( "%s/%s", remote_iwd.Value(),
						condor_basename(attr_value) );
			*rsl += printXMLParam( "stdin", buff.Value() );
			stage_in_list.append( attr_value );
		}
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

		// standard output
		// If we don't find a real filename for Output, then we don't
		// want to try transferring it.
	transfer_stdout = false;
	if ( jobAd->LookupString(ATTR_JOB_OUTPUT, &attr_value) && *attr_value &&
		 strcmp( attr_value, NULL_FILE ) ) {

		transfer_stdout = true;
		jobAd->LookupBool( ATTR_TRANSFER_OUTPUT, transfer_stdout );

		if ( transfer_stdout == false ) {
			if ( attr_value[0] != '/' ) {
				buff.sprintf( "%s/%s", remote_iwd.Value(), attr_value );
			} else {
				buff.sprintf( "%s", attr_value );
			}
			*rsl += printXMLParam( "stdout", buff.Value() );
		} else {
			buff.sprintf( "%s/%s", remote_iwd.Value(),
						condor_basename(attr_value) );
			*rsl += printXMLParam( "stdout", buff.Value() );
			local_stdout = attr_value;
		}
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

		// standard error
		// If we don't find a real filename for Error, then we don't
		// want to try transferring it.
	transfer_stderr = false;
	if ( jobAd->LookupString(ATTR_JOB_ERROR, &attr_value) && *attr_value &&
		 strcmp( attr_value, NULL_FILE ) ) {

		transfer_stderr = true;
		jobAd->LookupBool( ATTR_TRANSFER_ERROR, transfer_stderr );

		if ( transfer_stderr == false ) {
			if ( attr_value[0] != '/' ) {
				buff.sprintf( "%s/%s", remote_iwd.Value(), attr_value );
			} else {
				buff.sprintf( "%s", attr_value );
			}
			*rsl += printXMLParam( "stderr", buff.Value() );
		} else {
			buff.sprintf( "%s/%s", remote_iwd.Value(),
						condor_basename(attr_value) );
			*rsl += printXMLParam( "stderr", buff.Value() );
			local_stderr = attr_value;
		}
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

		// Now add the input staging directives to the job description
	if ( create_remote_iwd || transfer_executable ||
		 !stage_in_list.isEmpty() ) {

		staging_input = true;

		*rsl += "<fileStageIn>";
		*rsl += "<maxAttempts>5</maxAttempts>";

		*rsl += "<transferCredentialEndpoint>";
		*rsl += delegation_epr;
		*rsl += "</transferCredentialEndpoint>";

		// First upload an emtpy dummy directory
		// This will be the job's sandbox directory
		if ( create_remote_iwd ) {
			if ( getDummyJobScratchDir() == NULL ) {
				errorString = "Failed to create empty stage-in directory";
				delete rsl;
				return NULL;
			}
			if ( riwd_parent != "" ) {
				*rsl += "<transfer>";
				buff.sprintf( "%s%s/", local_url_base.Value(),
							  getDummyJobScratchDir() );
				*rsl += printXMLParam( "sourceUrl", buff.Value() );
				buff.sprintf( "file://%s", riwd_parent.Value() );
				*rsl += printXMLParam( "destinationUrl", buff.Value());
				if ( gridftpServer->UseSelfCred() ) {
					*rsl += "<rftOptions>";
					*rsl += printXMLParam( "sourceSubjectName",
										   jobProxy->subject->subject_name );
					*rsl += "</rftOptions>";
				}
				*rsl += "</transfer>";
			}
			*rsl += "<transfer>";
			buff.sprintf( "%s%s/", local_url_base.Value(),
						  getDummyJobScratchDir() );
			*rsl += printXMLParam( "sourceUrl", buff.Value() );
			buff.sprintf( "file://%s", remote_iwd.Value() );
			*rsl += printXMLParam( "destinationUrl", buff.Value());
			if ( gridftpServer->UseSelfCred() ) {
				*rsl += "<rftOptions>";
				*rsl += printXMLParam( "sourceSubjectName",
									   jobProxy->subject->subject_name );
				*rsl += "</rftOptions>";
			}
			*rsl += "</transfer>";
		}

		// Next, add the executable if needed
		if ( transfer_executable ) {
			*rsl += "<transfer>";
			buff.sprintf( "%s%s", local_url_base.Value(),
						  local_executable.Value() );
			*rsl += printXMLParam( "sourceUrl", buff.Value() );
			buff.sprintf( "file://%s", remote_executable.Value() );
			*rsl += printXMLParam( "destinationUrl", buff.Value());
			if ( gridftpServer->UseSelfCred() ) {
				*rsl += "<rftOptions>";
				*rsl += printXMLParam( "sourceSubjectName",
									   jobProxy->subject->subject_name );
				*rsl += "</rftOptions>";
			}
			*rsl += "</transfer>";
		}

		// Finally, deal with any other files we might wish to transfer
		if ( !stage_in_list.isEmpty() ) {
			char *filename;

			stage_in_list.rewind();
			while ( (filename = stage_in_list.next()) != NULL ) {

				*rsl += "<transfer>";
				buff.sprintf( "%s%s%s", local_url_base.Value(),
							  filename[0] == '/' ? "" : local_iwd.Value(),
							  filename );
				*rsl += printXMLParam ("sourceUrl", 
									   buff.Value());
				buff.sprintf ("file://%s%s",
							  remote_iwd.Value(),
							  condor_basename (filename));
				*rsl += printXMLParam ("destinationUrl", 
									   buff.Value());
				if ( gridftpServer->UseSelfCred() ) {
					*rsl += "<rftOptions>";
					*rsl += printXMLParam( "sourceSubjectName",
										   jobProxy->subject->subject_name );
					*rsl += "</rftOptions>";
				}
				*rsl += "</transfer>";

			}
		}

		*rsl += "</fileStageIn>";
	}

		// Now add the output staging directives to the job description
		// All the files we need are in stage_out_list
	if ( !stage_out_list.isEmpty() || transfer_stdout || transfer_stderr ) {
		char *filename;

		*rsl += "<fileStageOut>";
		*rsl += "<maxAttempts>5</maxAttempts>";

		*rsl += "<transferCredentialEndpoint>";
		*rsl += delegation_epr;
		*rsl += "</transferCredentialEndpoint>";

		if ( transfer_stdout ) {
			*rsl += "<transfer>";
			buff.sprintf ("file://%s%s",
						  remote_iwd.Value(),
						  condor_basename( local_stdout.Value() ) );
			*rsl += printXMLParam ("sourceUrl", 
								   buff.Value());
			buff.sprintf( "%s%s%s", local_url_base.Value(),
						  local_stdout[0] == '/' ? "" : local_iwd.Value(),
						  local_stdout.Value() );
			*rsl += printXMLParam ("destinationUrl", 
								   buff.Value());
			if ( gridftpServer->UseSelfCred() ) {
				*rsl += "<rftOptions>";
				*rsl += printXMLParam( "destinationSubjectName",
									   jobProxy->subject->subject_name );
				*rsl += "</rftOptions>";
			}
			*rsl += "</transfer>";
		}

		if ( transfer_stderr ) {
			*rsl += "<transfer>";
			buff.sprintf ("file://%s%s",
						  remote_iwd.Value(),
						  condor_basename( local_stderr.Value() ) );
			*rsl += printXMLParam ("sourceUrl", 
								   buff.Value());
			buff.sprintf( "%s%s%s", local_url_base.Value(),
						  local_stderr[0] == '/' ? "" : local_iwd.Value(),
						  local_stderr.Value() );
			*rsl += printXMLParam ("destinationUrl", 
								   buff.Value());
			if ( gridftpServer->UseSelfCred() ) {
				*rsl += "<rftOptions>";
				*rsl += printXMLParam( "destinationSubjectName",
									   jobProxy->subject->subject_name );
				*rsl += "</rftOptions>";
			}
			*rsl += "</transfer>";
		}

		char *remaps = NULL;
		MyString new_name;
		jobAd->LookupString( ATTR_TRANSFER_OUTPUT_REMAPS, &remaps );

		stage_out_list.rewind();
		while ( (filename = stage_out_list.next()) != NULL ) {

			*rsl += "<transfer>";
			buff.sprintf ("file://%s%s",
						  filename[0]=='/' ? "" : remote_iwd.Value(),
						  filename);
			*rsl += printXMLParam ("sourceUrl", 
								   buff.Value());
			if ( remaps && filename_remap_find( remaps, filename,
												new_name ) ) {
				buff.sprintf( "%s%s%s", local_url_base.Value(),
							  new_name[0] == '/' ? "" : local_iwd.Value(),
							  new_name.Value() );
			} else {
				buff.sprintf( "%s%s%s", local_url_base.Value(),
							  local_iwd.Value(),
							  condor_basename( filename ) );
			}
			*rsl += printXMLParam ("destinationUrl", 
								   buff.Value());
			if ( gridftpServer->UseSelfCred() ) {
				*rsl += "<rftOptions>";
				*rsl += printXMLParam( "destinationSubjectName",
									   jobProxy->subject->subject_name );
				*rsl += "</rftOptions>";
			}
			*rsl += "</transfer>";

		}
		free( remaps );

		*rsl += "</fileStageOut>";
	}

		// Add a cleanup directive to remove the remote iwd,
		// if we created one
	if ( create_remote_iwd ) {
		*rsl += "<fileCleanUp>";
		*rsl += "<maxAttempts>5</maxAttempts>";
		*rsl += "<transferCredentialEndpoint>";
		*rsl += delegation_epr;
		*rsl += "</transferCredentialEndpoint>";
		*rsl += "<deletion><file>file://" + remote_iwd +
			"</file></deletion></fileCleanUp>";
	}

	{
		Env envobj;
		MyString env_errors;
		if(!envobj.MergeFrom(jobAd,&env_errors)) {
			dprintf(D_ALWAYS,"(%d.%d) Failed to read job environment: %s\n",
					procID.cluster, procID.proc, env_errors.Value());
			errorString.sprintf("Failed to read job environment: %s\n",
					env_errors.Value());
			delete rsl;
			return NULL;
		}
		char **env_vec = envobj.getStringArray();

		for ( int i = 0; env_vec[i]; i++ ) {
			char *equals = strchr(env_vec[i],'=');
			if ( !equals ) {
				// this environment entry has no equals sign!?!?
				continue;
			}
			
			*equals = '\0';

			*rsl += "<environment>";
			*rsl += printXMLParam ("name", env_vec[i]);
			*rsl += printXMLParam ("value", equals + 1);
			*rsl += "</environment>";

			*equals = '=';
		}
		deleteStringArray(env_vec);
	}

	*rsl += "<jobCredentialEndpoint>";
	*rsl += delegation_epr;
	*rsl += "</jobCredentialEndpoint>";

	*rsl += "<stagingCredentialEndpoint>";
	*rsl += delegation_epr;
	*rsl += "</stagingCredentialEndpoint>";

	if ( jobAd->LookupString( ATTR_GLOBUS_XML, &rsl_suffix ) ) {
		*rsl += rsl_suffix;
		free( rsl_suffix );
	}

	*rsl += "</job>";

	return rsl;
}

void GT4Job::DeleteOutput()
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

const char * 
GT4Job::printXMLParam (const char * name, const char * value) {
	static MyString buff;
		// TODO should perform escaping of special characters in value
	buff.sprintf ("<%s>%s</%s>", name, value, name);
	return buff.Value();
}

// Create an per-user empty directory, or return one if
// one already exist. The directory will be created under
// gridmanger scratch dir. NOT thread-safe.
const char*
GT4Job::getDummyJobScratchDir() {
	const int dir_mode = 0500;
	const char *return_val = NULL;
	static MyString dirname;
	static time_t last_check = 0;

	if ( dirname != "" && time(NULL) < last_check + 60 ) {
		return dirname.Value();
	}

	if ( dirname == "" ) {
		char *prefix = temp_dir_path();
		ASSERT( prefix );
#ifndef WIN32 
		dirname.sprintf ("%s%ccondor_g_empty_dir_u%d", // <scratch>/condorg_empty_dir_u<uid>
						 prefix,
						 DIR_DELIM_CHAR,
						 geteuid());
#else // Windows
		char *user = my_username();
		dirname.sprintf ("%s%ccondor_g_empty_dir_u%s", // <scratch>\empty_dir_u<username>
						 prefix, 
						 DIR_DELIM_CHAR,
						 user);
		free(user);
		user = NULL;
#endif
		free( prefix );
	}

	StatInfo *dir_stat = new StatInfo( dirname.Value() );

	if ( dir_stat->Error() == SINoFile ) {
		delete dir_stat;
		dir_stat = NULL;
		if ( mkdir (dirname.Value(), dir_mode) < 0 ) {
			dprintf (D_ALWAYS, "Unable to create scratch directory %s, errno=%d (%s)\n", 
					 dirname.Value(), errno, strerror(errno));
			goto error_exit;
		}
		dir_stat = new StatInfo( dirname.Value() );
	}

	if ( dir_stat->Error() != SIGood ) {
		dprintf( D_ALWAYS, "Unable to stat scratch directory %s, errno=%d (%s)\n",
				 dirname.Value(), dir_stat->Errno(),
				 strerror( dir_stat->Errno() ) );
		goto error_exit;
	}
	if ( dir_stat->IsSymlink() ) {
		dprintf( D_ALWAYS, "Scratch directory %s is a symlink\n",
				 dirname.Value() );
		goto error_exit;
	}
	if ( !dir_stat->IsDirectory() ) {
		dprintf( D_ALWAYS, "Scratch directory %s isn't a directory\n",
				 dirname.Value() );
		goto error_exit;
	}
#ifndef WIN32 
	if ( dir_stat->GetOwner() != get_user_uid() ) {
		dprintf( D_ALWAYS, "Scratch directory %s not owned by user\n",
				 dirname.Value() );
		goto error_exit;
	}
#endif
	if ( ( dir_stat->GetMode() & 0777 ) != dir_mode ) {
		dprintf( D_ALWAYS, "Scratch directory %s has wrong permissions 0%o\n",
				 dirname.Value(), dir_stat->GetMode() & 0777 );
		goto error_exit;
	}

	return_val = dirname.Value();

	last_check = time(NULL);

 error_exit:
	if ( dir_stat ) {
		delete dir_stat;
	}
	if ( return_val == NULL ) {
		dirname = "";
	}
	return return_val;
}

bool
GT4Job::SwitchToGram42()
{
	if ( m_isGram42 ) {
		return true;
	}

dprintf(D_ALWAYS,"(%d.%d) JEF: Switching to Gram 42\n",procID.cluster,procID.proc);
	GahpClient *new_gahp = NULL;

	MyString gahp_name;
	char *gahp_path = param("GT42_GAHP");
	if ( gahp_path == NULL ) {
		gmState = GM_HOLD;
		jobAd->Assign( ATTR_HOLD_REASON, "GT42_GAHP not defined" );
		SetEvaluateState();
		return false;
	}
	gahp_name.sprintf( "GT42/%s", jobProxy->subject->fqan );
	new_gahp = new GahpClient( gahp_name.Value(), gahp_path );
	free( gahp_path );

	new_gahp->setNotificationTimerId( evaluateStateTid );
	new_gahp->setTimeout( gahpCallTimeout );

	if ( new_gahp->Initialize( jobProxy ) == false ) {
		dprintf( D_ALWAYS, "(%d.%d) Error initializing GAHP\n",
				 procID.cluster, procID.proc );
		jobAd->Assign( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
		gmState = GM_HOLD;
		SetEvaluateState();
		delete new_gahp;
		return false;
	}

	new_gahp->setDelegProxy( jobProxy );

	new_gahp->setMode( GahpClient::blocking );

	int err;
	err = new_gahp->gt4_gram_client_callback_allow( gt4GramCallbackHandler,
													NULL,
													&gramCallbackContact );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Error enabling GRAM callback, err=%d\n", 
				 procID.cluster, procID.proc, err );
		jobAd->Assign( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
		gmState = GM_HOLD;
		delete new_gahp;
		SetEvaluateState();
		return false;
	}

	new_gahp->setMode( gahp->getMode() );

	m_isGram42 = true;

		// If we're trying to do a submit, we need to recreate the RSL,
		// since the format is slightly different between 4.0 and 4.2.
	delete RSL;
	RSL = NULL;

	delete gahp;
	gahp = new_gahp;

	SetEvaluateState();

	return true;
}
