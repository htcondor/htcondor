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
#include "condor_attributes.h"
#include "condor_debug.h"
#include "env.h"
#include "condor_string.h"	// for strnewp and friends
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"

#include "gridmanager.h"
#include "gt3job.h"
#include "condor_config.h"
#include "globusjob.h" // for rsl_stringify


// GridManager job states
#define GM_INIT					0
#define GM_REGISTER				1
#define GM_STDIO_UPDATE			2
#define GM_UNSUBMITTED			3
#define GM_SUBMIT				4
#define GM_SUBMIT_SAVE			5
#define GM_SUBMIT_COMMIT		6
#define GM_SUBMITTED			7
#define GM_DONE_SAVE			8
#define GM_DONE_COMMIT			9
#define GM_CANCEL				10
#define GM_FAILED				11
#define GM_DELETE				12
#define GM_CLEAR_REQUEST		13
#define GM_HOLD					14
#define GM_PROXY_EXPIRED		15
#define GM_REFRESH_PROXY		16
#define GM_PROBE_JOBMANAGER		17
#define GM_START				18

static char *GMStateNames[] = {
	"GM_INIT",
	"GM_REGISTER",
	"GM_STDIO_UPDATE",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_SUBMIT_COMMIT",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_DONE_COMMIT",
	"GM_CANCEL",
	"GM_FAILED",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROXY_EXPIRED",
	"GM_REFRESH_PROXY",
	"GM_PROBE_JOBMANAGER",
	"GM_START"
};

// TODO: once we can set the jobmanager's proxy timeout, we should either
// let this be set in the config file or set it to
// GRIDMANAGER_MINIMUM_PROXY_TIME + 60
#define JM_MIN_PROXY_TIME		(minProxy_time + 60)

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define LOG_GLOBUS_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, globusState %d: %s returned Globus error %d\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],globusState, \
        func,error)

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
gt3GramCallbackHandler( void *user_arg, char *job_contact, int state,
					 int errorcode )
{
	int rc;
	GT3Job *this_job;
	MyString job_id;

	job_id.sprintf( "gt3 %s", job_contact );

	// Find the right job object
	rc = BaseJob::JobsByRemoteId.lookup( HashKey( job_id.Value() ),
										 (BaseJob*&)this_job );
	if ( rc != 0 || this_job == NULL ) {
		dprintf( D_ALWAYS, 
			"gt3GramCallbackHandler: Can't find record for globus job with "
			"contact %s on globus state %d, errorcode %d, ignoring\n",
			job_contact, state, errorcode );
		return;
	}

	dprintf( D_ALWAYS, "(%d.%d) gram callback: state %d, errorcode %d\n",
			 this_job->procID.cluster, this_job->procID.proc, state,
			 errorcode );

	this_job->GramCallback( state, errorcode );
}

/////////////////////////added for reorg
void GT3JobInit()
{
}

void GT3JobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	GT3Job::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_RESOURCE_PROBE_INTERVAL", 5 * 60 );
	GT3Resource::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	GT3Job::setGahpCallTimeout( tmp_int );
	GT3Resource::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	GT3Job::setConnectFailureRetry( tmp_int );

	// Tell all the resource objects to deal with their new config values
	GT3Resource *next_resource;

	GT3Resource::ResourcesByName.startIterations();

	while ( GT3Resource::ResourcesByName.iterate( next_resource ) != 0 ) {
		next_resource->Reconfig();
	}
}

bool GT3JobAdMatch( const ClassAd *job_ad ) {
	int universe;
	MyString resource;
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 strncasecmp( resource.Value(), "gt3 ", 4 ) == 0 ) {

		return true;
	}
	return false;
}

BaseJob *GT3JobCreate( ClassAd *jobad )
{
	return (BaseJob *)new GT3Job( jobad );
}
////////////////////////////////////////

int GT3Job::probeInterval = 300;			// default value
int GT3Job::submitInterval = 300;			// default value
int GT3Job::gahpCallTimeout = 300;			// default value
int GT3Job::maxConnectFailures = 3;			// default value

GT3Job::GT3Job( ClassAd *classad )
	: BaseJob( classad )
{
	int bool_value;
	char buff[4096];
	char buff2[_POSIX_PATH_MAX];
	char iwd[_POSIX_PATH_MAX];
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
	globusStateErrorCode = 0;
	globusStateBeforeFailure = 0;
	callbackGlobusState = 0;
	callbackGlobusStateErrorCode = 0;
	gmState = GM_INIT;
	globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	enteredCurrentGlobusState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	submitFailureCode = 0;
	jmProxyExpireTime = 0;
	connect_failure_counter = 0;
	resourceManagerString = NULL;
	myResource = NULL;
	jobProxy = NULL;
	gassServerUrl = NULL;
	gramCallbackContact = NULL;
	gahp = NULL;

	// In GM_HOLD, we assme HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	jobProxy = AcquireProxy( jobAd, error_string, evaluateStateTid );
	if ( jobProxy == NULL ) {
		if ( error_string == "" ) {
			error_string.sprintf( "%s is not set in the job ad",
								  ATTR_X509_USER_PROXY );
		}
		goto error_exit;
	}

	gahp_path = param("GT3_GAHP");
	if ( gahp_path == NULL ) {
		error_string = "GT3_GAHP not defined";
		goto error_exit;
	}
	snprintf( buff, sizeof(buff), "GT3/%s",
			  jobProxy->subject->subject_name );
	gahp = new GahpClient( buff, gahp_path );
	free( gahp_path );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_RESOURCE, buff );
	if ( buff[0] != '\0' ) {
		const char *token;
		MyString str = buff;

		str.Tokenize();

		token = str.GetNextToken( " ", false );
		if ( !token || stricmp( token, "gt3" ) ) {
			error_string.sprintf( "%S not of type gt3", ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = str.GetNextToken( " ", false );
		if ( token && *token ) {
			resourceManagerString = strdup( token );
		} else {
			error_string.sprintf( "%s missing GRAM Service URL",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

	} else {
		error_string.sprintf( "%s is not set in the job ad",
							  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff );
	if ( buff[0] != '\0' ) {
		SetRemoteJobId( strchr( buff, ' ' ) + 1 );
		job_already_submitted = true;
	}

	myResource = GT3Resource::FindOrCreateResource( resourceManagerString,
													jobProxy->subject->subject_name);
	if ( myResource == NULL ) {
		error_string = "Failed to initialized GT3Resource object";
		goto error_exit;
	}

	// RegisterJob() may call our NotifyResourceUp/Down(), so be careful.
	myResource->RegisterJob( this );
	if ( job_already_submitted ) {
		myResource->AlreadySubmitted( this );
	}

	jobAd->LookupInteger( ATTR_GLOBUS_STATUS, globusState );

	globusError = GLOBUS_SUCCESS;

	iwd[0] = '\0';
	if ( jobAd->LookupString(ATTR_JOB_IWD, iwd) && *iwd ) {
		int len = strlen(iwd);
		if ( len > 1 && iwd[len - 1] != '/' ) {
			strcat( iwd, "/" );
		}
	} else {
		strcpy( iwd, "/" );
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( jobAd->LookupString(ATTR_JOB_OUTPUT, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_OUTPUT, bool_value ) ||
			 bool_value ) {

			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localOutput = strdup( buff2 );

			bool_value = 1;
			jobAd->LookupBool( ATTR_STREAM_OUTPUT, bool_value );
			streamOutput = (bool_value != 0);
			stageOutput = !streamOutput;
		}
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( jobAd->LookupString(ATTR_JOB_ERROR, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_ERROR, bool_value ) ||
			 bool_value ) {

			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localError = strdup( buff2 );

			bool_value = 1;
			jobAd->LookupBool( ATTR_STREAM_ERROR, bool_value );
			streamError = (bool_value != 0);
			stageError = !streamError;
		}
	}

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

GT3Job::~GT3Job()
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
	if ( jobProxy ) {
		ReleaseProxy( jobProxy, evaluateStateTid );
	}
	if ( gassServerUrl ) {
		free( gassServerUrl );
	}
	if ( gramCallbackContact ) {
		free( gramCallbackContact );
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
}

void GT3Job::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

void GT3Job::SetRemoteJobId( const char *job_id )
{
	free( jobContact );
	if ( job_id ) {
		jobContact = strdup( job_id );
	} else {
		jobContact = NULL;
	}

	MyString full_job_id;
	if ( job_id ) {
		full_job_id.sprintf( "gt3 %s", job_id );
	}
	BaseJob::SetRemoteJobId( full_job_id.Value() );
}

int GT3Job::doEvaluateState()
{
	bool connect_failure = false;
	int old_gm_state;
	int old_globus_state;
	bool reevaluate_state = true;
	time_t now;	// make sure you set this before every use!!!

	bool done;
	int rc;
	int status;
	int error;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, globusState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],globusState);

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

			err = gahp->gt3_gram_client_callback_allow( gt3GramCallbackHandler,
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

			err = gahp->globus_gass_server_superez_init( &gassServerUrl, 0 );
			if ( err != GLOBUS_SUCCESS ) {
				dprintf( D_ALWAYS, "(%d.%d) Error enabling GASS server, err=%d\n",
						 procID.cluster, procID.proc, err );
				jobAd->Assign( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			gahp->setMode( saved_mode );

			gmState = GM_START;
			} break;
		case GM_START: {
			// This state is the real start of the state machine, after
			// one-time initialization has been taken care of.
			// If we think there's a running jobmanager
			// out there, we try to register for callbacks (in GM_REGISTER).
			errorString = "";
			if ( jobContact == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( wantResubmit || doResubmit ) {
				gmState = GM_CLEAR_REQUEST;
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
			CHECK_PROXY;
			rc = gahp->gt3_gram_client_job_callback_register( jobContact,
														gramCallbackContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_callback_register()", rc );
				globusError = rc;
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
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) {
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
			} else {
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
				jobAd->Assign( ATTR_HOLD_REASON,
							   "Attempts to submit failed" );
				gmState = GM_HOLD;
				break;
			}
			now = time(NULL);
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
					gmState = GM_HOLD;
					break;
				}
				rc = gahp->gt3_gram_client_job_create( 
										resourceManagerString,
										RSL->Value(),
										gramCallbackContact, &job_contact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				myResource->SubmitComplete(this);
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
					LOG_GLOBUS_ERROR( "globus_gram_client_job_create()", rc );
					dprintf(D_ALWAYS,"(%d.%d)    RSL='%s'\n",
							procID.cluster, procID.proc,RSL->Value());
					submitFailureCode = globusError = rc;
					WriteGlobusSubmitFailedEventToUserLog( jobAd,
														   submitFailureCode );
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
				done = requestScheddUpdate( this );
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
				CHECK_PROXY;
				rc = gahp->gt3_gram_client_job_start( jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_start()", rc );
					globusError = rc;
					WriteGlobusSubmitFailedEventToUserLog( jobAd,
														   globusError );
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
				gmState = GM_DONE_SAVE;
			} else if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				gmState = GM_FAILED;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				if ( GetCallbacks() == true ) {
					reevaluate_state = true;
					break;
				}
				if ( jmProxyExpireTime < jobProxy->expiration_time ) {
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
				CHECK_PROXY;
//				rc = gahp->gt3_gram_client_job_refresh_credentials(
//																jobContact );
//java gahp doesn't support this command yet.
rc=0;
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR("refresh_credentials()",rc);
					globusError = rc;
					gmState = GM_CANCEL;
					break;
				}
				jmProxyExpireTime = jobProxy->expiration_time;
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_PROBE_JOBMANAGER: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				CHECK_PROXY;
				rc = gahp->gt3_gram_client_job_status( jobContact,
													  &status );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "gt3_gram_client_job_status()", rc );
					globusError = rc;
					gmState = GM_CANCEL;
					break;
				}
				UpdateGlobusState( status, 0 );
				ClearCallbacks();
				lastProbeTime = time(NULL);
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_DONE_SAVE: {
			// Report job completion to the schedd.
			JobTerminated();
			if ( condorState == COMPLETED ) {
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
			}
			gmState = GM_DONE_COMMIT;
			} break;
		case GM_DONE_COMMIT: {
			// Tell the jobmanager it can clean up and exit.
			CHECK_PROXY;
			rc = gahp->gt3_gram_client_job_destroy( jobContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "gt3_gram_client_job_destroy()", rc );
				globusError = rc;
				gmState = GM_CANCEL;
				break;
			}
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				SetRemoteJobId( NULL );
				myResource->CancelSubmit( this );
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.
			if ( globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE &&
				 globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				CHECK_PROXY;
				rc = gahp->gt3_gram_client_job_destroy( jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_cancel()", rc );
					globusError = rc;
					gmState = GM_CLEAR_REQUEST;
					break;
				}
			}
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
			rc = gahp->gt3_gram_client_job_destroy( jobContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_END)", rc );
				globusError = rc;
				gmState = GM_CLEAR_REQUEST;
				break;
			}

			myResource->CancelSubmit( this );
			SetRemoteJobId( NULL );

			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				gmState = GM_CLEAR_REQUEST;
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
			if ( (jobContact != NULL || (globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED && globusStateErrorCode != GLOBUS_GRAM_PROTOCOL_ERROR_JOB_UNSUBMITTED)) 
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
			if ( globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) {
				globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
				jobAd->Assign( ATTR_GLOBUS_STATUS, globusState );
			}
			globusStateErrorCode = 0;
			globusError = 0;
			errorString = "";
			ClearCallbacks();
			SetRemoteJobId( NULL );
			myResource->CancelSubmit( this );
			JobIdle();
			if ( submitLogged ) {
				JobEvicted();
				if ( !evictLogged ) {
					WriteEvictEventToUserLog( jobAd );
					evictLogged = true;
				}
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
			done = requestScheddUpdate( this );
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
			if ( jobContact &&
				 globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN ) {
				globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN;
				jobAd->Assign( ATTR_GLOBUS_STATUS, globusState );
				//UpdateGlobusState( GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN, 0 );
			}
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
				if ( holdReason[0] == '\0' && globusStateErrorCode != 0 ) {
					snprintf( holdReason, 1024, "Globus error %d: %s",
							  globusStateErrorCode,
							  "" );
				}
				if ( holdReason[0] == '\0' && globusError != 0 ) {
					snprintf( holdReason, 1024, "Globus error %d: %s", globusError,
							  "" );
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
			now = time(NULL);
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
			if ( gahp ) {
				gahp->purgePendingRequests();
			}
			// If we were calling a globus call that used RSL, we're done
			// with it now, so free it.
			if ( RSL ) {
				delete RSL;
				RSL = NULL;
			}
			connect_failure_counter = 0;
		}

	} while ( reevaluate_state );

	if ( connect_failure && !resourceDown ) {
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
			RequestPing();
		}
	}

	return TRUE;
}

bool GT3Job::AllowTransition( int new_state, int old_state )
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


void GT3Job::UpdateGlobusState( int new_state, int new_error_code )
{
	bool allow_transition;

	allow_transition = AllowTransition( new_state, globusState );

	if ( allow_transition ) {
		// where to put logging of events: here or in EvaluateState?
		dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
				procID.cluster, procID.proc,
				GlobusJobStatusName(globusState),
				GlobusJobStatusName(new_state));

		if ( ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
			   new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ) &&
			 condorState == IDLE ) {
			JobRunning();
		}

		if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED &&
			 condorState == RUNNING ) {
			JobIdle();
		}

		if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED &&
			 !submitLogged && !submitFailedLogged ) {
			if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
					// TODO: should SUBMIT_FAILED_EVENT be used only on
					//   certain errors (ones we know are submit-related)?
				submitFailureCode = new_error_code;
				if ( !submitFailedLogged ) {
					WriteGlobusSubmitFailedEventToUserLog( jobAd,
														   submitFailureCode );
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
		if ( (new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
			  new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ||
			  new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ||
			  new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED)
			 && !executeLogged ) {
			WriteExecuteEventToUserLog( jobAd );
			executeLogged = true;
		}

		if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
			globusStateBeforeFailure = globusState;
		} else {
			jobAd->Assign( ATTR_GLOBUS_STATUS, new_state );
		}

		globusState = new_state;
		globusStateErrorCode = new_error_code;
		enteredCurrentGlobusState = time(NULL);

		requestScheddUpdate( this );

		SetEvaluateState();
	}
}

void GT3Job::GramCallback( int new_state, int new_error_code )
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

bool GT3Job::GetCallbacks()
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

void GT3Job::ClearCallbacks()
{
	callbackGlobusState = 0;
	callbackGlobusStateErrorCode = 0;
}

BaseResource *GT3Job::GetResource()
{
	return (BaseResource *)myResource;
}

MyString *GT3Job::buildSubmitRSL()
{
	int transfer;
	MyString *rsl = new MyString;
	MyString iwd = "";
	MyString riwd = "";
	MyString buff;
	char *attr_value = NULL;
	char *rsl_suffix = NULL;

	if ( jobAd->LookupString( ATTR_GLOBUS_RSL, &rsl_suffix ) &&
						   rsl_suffix[0] == '&' ) {
		*rsl = rsl_suffix;
		free( rsl_suffix );
		return rsl;
	}

	if ( jobAd->LookupString(ATTR_JOB_IWD, &attr_value) && *attr_value ) {
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

	//We're assuming all job clasads have a command attribute
	//First look for executable in the spool area.
	char *spooldir = param("SPOOL");
	if ( spooldir ) {
		char *source = gen_ckpt_name(spooldir,procID.cluster,ICKPT,0);
		free(spooldir);
		if ( access(source,F_OK | X_OK) >= 0 ) {
				// we can access an executable in the spool dir
			attr_value = strdup(source);
		}
	}
	if ( attr_value == NULL ) {
			// didn't find any executable in the spool directory,
			// so use what is explicitly stated in the job ad
		jobAd->LookupString( ATTR_JOB_CMD, &attr_value );
	}
	*rsl += "(executable=";
	if ( !jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer ) || transfer ) {
		buff = "$(GRIDMANAGER_GASS_URL)/";
		if ( attr_value[0] != '/' ) {
			buff += iwd;
		}
		buff += attr_value;
	} else {
		buff = attr_value;
	}
	*rsl += rsl_stringify( buff.Value() );
	free( attr_value );
	attr_value = NULL;

	if ( jobAd->LookupString(ATTR_JOB_REMOTE_IWD, &attr_value) && *attr_value ) {
		*rsl += ")(directory=";
		*rsl += rsl_stringify( attr_value );

		riwd = attr_value;
	} else if ( true ) {
		// The user didn't specify a remote IWD, so tell the jobmanager to
		// create a scratch directory in its default location and make that
		// the remote IWD.
		*rsl += ")(scratchdir='')(directory=$(SCRATCH_DIRECTORY)";

		riwd = "$(SCRATCH_DIRECTORY)";
	} else {
		// The jobmanager can't create a directory for us, so use its
		// default of $(HOME).
		riwd = "$(HOME)";
	}
	if ( riwd[riwd.Length()-1] != '/' ) {
		riwd += '/';
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	{
		ArgList args;
		MyString arg_errors;
		MyString rsl_args;
		if(!args.AppendArgsFromClassAd(jobAd,&arg_errors)) {
			dprintf(D_ALWAYS,"(%d.%d) Failed to read job arguments: %s\n",
					procID.cluster, procID.proc, arg_errors.Value());
			errorString.sprintf("Failed to read job arguments: %s\n",
					arg_errors.Value());
			return NULL;
		}
		if(args.Count() != 0) {
			if(args.InputWasV1()) {
					// In V1 syntax, the user's input _is_ RSL
				if(!args.GetArgsStringV1Raw(&rsl_args,&arg_errors)) {
					dprintf(D_ALWAYS,
							"(%d.%d) Failed to get job arguments: %s\n",
							procID.cluster,procID.proc,arg_errors.Value());
					errorString.sprintf("Failed to get job arguments: %s\n",
							arg_errors.Value());
					return NULL;
				}
			}
			else {
					// In V2 syntax, we convert the ArgList to RSL
				for(int i=0;i<args.Count();i++) {
					if(i) {
						rsl_args += ' ';
					}
					rsl_args += rsl_stringify(args.GetArg(i));
				}
			}
			*rsl += ")(arguments=";
			*rsl += rsl_args;
		}
	}

	if ( jobAd->LookupString(ATTR_JOB_INPUT, &attr_value) && *attr_value &&
		 strcmp( attr_value, NULL_FILE ) ) {
		*rsl += ")(stdin=";
		if ( !jobAd->LookupBool( ATTR_TRANSFER_INPUT, transfer ) || transfer ) {
			buff = "$(GRIDMANAGER_GASS_URL)/";
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
		buff.sprintf( "$(GRIDMANAGER_GASS_URL)/%s", localOutput );
		*rsl += rsl_stringify( buff.Value() );
	} else {
		if ( stageOutput ) {
			*rsl += ")(stdout=$(GLOBUS_CACHED_STDOUT)";
		} else {
			if ( jobAd->LookupString(ATTR_JOB_OUTPUT, &attr_value) &&
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
		buff.sprintf( "$(GRIDMANAGER_GASS_URL)/%s", localError );
		*rsl += rsl_stringify( buff.Value() );
	} else {
		if ( stageError ) {
			*rsl += ")(stderr=$(GLOBUS_CACHED_STDERR)";
		} else {
			if ( jobAd->LookupString(ATTR_JOB_ERROR, &attr_value) &&
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

	if ( jobAd->LookupString(ATTR_TRANSFER_INPUT_FILES, &attr_value) &&
		 *attr_value ) {
		StringList filelist( attr_value, "," );
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
				buff += condor_basename( filename );
				*rsl += rsl_stringify( buff );
				*rsl += ')';
			}
		}
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( ( jobAd->LookupString(ATTR_TRANSFER_OUTPUT_FILES, &attr_value) &&
		   *attr_value ) || stageOutput || stageError ) {
		StringList filelist( attr_value, "," );
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
				buff += condor_basename( filename );
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

	{
		Env envobj;
		MyString env_errors;
		if(!envobj.MergeFrom(jobAd,&env_errors)) {
			dprintf(D_ALWAYS,"(%d.%d) Failed to read job environment: %s\n",
					procID.cluster, procID.proc, env_errors.Value());
			errorString.sprintf("Failed to read job environment: %s\n",
					env_errors.Value());
			return NULL;
		}
		char **env_vec = envobj.getStringArray();
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
		deleteStringArray(env_vec);
	}

	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	buff.sprintf( ")(proxy_timeout=%d", JM_MIN_PROXY_TIME );
	*rsl += buff;

	buff.sprintf( ")"
				  "(remote_io_url=$(GRIDMANAGER_GASS_URL))",
				  JM_COMMIT_TIMEOUT );
	*rsl += buff;

	if ( rsl_suffix != NULL ) {
		*rsl += rsl_suffix;
		free( rsl_suffix );
	}

	return rsl;
}

void GT3Job::DeleteOutput()
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

