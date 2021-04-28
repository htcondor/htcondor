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
#include "condor_daemon_core.h"
#include "basename.h"
#include "spooled_job_files.h"
#include "filename_tools.h"

#include "gridmanager.h"
#include "creamjob.h"
#include "condor_config.h"
#include "my_username.h"

// GridManager job states
#define GM_INIT					0
#define GM_UNSUBMITTED			1
#define GM_SUBMIT				2
#define GM_SUBMIT_SAVE			3
#define GM_SUBMIT_COMMIT		4
#define GM_SUBMITTED			5
#define GM_DONE_SAVE			6
#define GM_DONE_COMMIT			7
#define GM_CANCEL				8
#define GM_PURGE				9
#define GM_DELETE				10
#define GM_CLEAR_REQUEST		11
#define GM_HOLD					12
#define GM_PROXY_EXPIRED		13
#define GM_POLL_JOB_STATE		14
#define GM_START				15
#define GM_DELEGATE_PROXY		16
#define GM_CLEANUP				17
#define GM_SET_LEASE			18
#define GM_RECOVER_POLL			19
#define GM_STAGE_IN				20
#define GM_STAGE_OUT			21

static const char *GMStateNames[] = {
	"GM_INIT",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_SUBMIT_COMMIT",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_DONE_COMMIT",
	"GM_CANCEL",
	"GM_PURGE",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROXY_EXPIRED",
	"GM_POLL_JOB_STATE",
	"GM_START",
	"GM_DELEGATE_PROXY",
	"GM_CLEANUP",
	"GM_SET_LEASE",
	"GM_RECOVER_POLL",
	"GM_STAGE_IN",
	"GM_STAGE_OUT",
};

#define CREAM_JOB_STATE_UNSET			""
#define CREAM_JOB_STATE_REGISTERED		"REGISTERED"
#define CREAM_JOB_STATE_PENDING			"PENDING"
#define CREAM_JOB_STATE_IDLE			"IDLE"
#define CREAM_JOB_STATE_RUNNING			"RUNNING"
#define CREAM_JOB_STATE_REALLY_RUNNING	"REALLY-RUNNING"
#define CREAM_JOB_STATE_CANCELLED		"CANCELLED"
#define CREAM_JOB_STATE_HELD			"HELD"
#define CREAM_JOB_STATE_ABORTED			"ABORTED"
#define CREAM_JOB_STATE_DONE_OK			"DONE-OK"
#define CREAM_JOB_STATE_DONE_FAILED		"DONE-FAILED"
#define CREAM_JOB_STATE_UNKNOWN			"UNKNOWN"
#define CREAM_JOB_STATE_PURGED			"PURGED"

const char *ATTR_CREAM_UPLOAD_URL = "CreamUploadUrl";
const char *ATTR_CREAM_DOWNLOAD_URL = "CreamDownloadUrl";
const char *ATTR_CREAM_DELEGATION_URI = "CreamDelegationUri";
const char *ATTR_CREAM_LEASE_ID = "CreamLeaseId";

// TODO: once we can set the jobmanager's proxy timeout, we should either
// let this be set in the config file or set it to
// GRIDMANAGER_MINIMUM_PROXY_TIME + 60
#define JM_MIN_PROXY_TIME		(minProxy_time + 60)

#define DEFAULT_LEASE_DURATION	6*60*60 //6 hr

#define CLEANUP_DELAY	5
#define MAX_CLEANUP_ATTEMPTS 3

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define LOG_CREAM_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, remoteState %s: %s %s\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],remoteState.c_str(), \
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

void CreamJobInit()
{
}

void CreamJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_RESOURCE_PROBE_INTERVAL", 5 * 60 );
	CreamResource::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	CreamJob::setGahpCallTimeout( tmp_int );
	CreamResource::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	CreamJob::setConnectFailureRetry( tmp_int );

	// Tell all the resource objects to deal with their new config values
	for (auto &elem : CreamResource::ResourcesByName) {
		elem.second->Reconfig();
	}
}

bool CreamJobAdMatch( const ClassAd *job_ad ) {
	int universe;
	std::string resource;
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 strncasecmp( resource.c_str(), "cream ", 6 ) == 0 ) {

		return true;
	}
	return false;
}

BaseJob *CreamJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new CreamJob( jobad );
}

int CreamJob::submitInterval = 300;			// default value
int CreamJob::gahpCallTimeout = 300;		// default value
int CreamJob::maxConnectFailures = 3;		// default value

CreamJob::CreamJob( ClassAd *classad )
	: BaseJob( classad )
{

	bool bool_value;
	char buff[4096];
	std::string buff2;
	std::string iwd;
	std::string job_output;
	std::string job_error;
	std::string grid_resource;
	bool job_already_submitted = false;
	std::string error_string = "";
	char *gahp_path = NULL;

	creamAd = NULL;
	remoteJobId = NULL;
	remoteState = CREAM_JOB_STATE_UNSET;
	localOutput = NULL;
	localError = NULL;
	streamOutput = false;
	streamError = false;
	stageOutput = false;
	stageError = false;
	remoteStateFaultString = "";
	gmState = GM_INIT;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	enteredCurrentRemoteState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	jmProxyExpireTime = 0;
	resourceManagerString = NULL;
	myResource = NULL;
	jobProxy = NULL;
	uploadUrl = NULL;
	downloadUrl = NULL;
	gahp = NULL;
	delegatedCredentialURI = NULL;
	leaseId = NULL;
	connectFailureCount = 0;
	doActivePoll = false;
	m_xfer_request = NULL;
	m_numCleanupAttempts = 0;
	resourceBatchSystemString = NULL;
	resourceQueueString = NULL;

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	// TODO This is bad. The job may already be on hold with a valid hold
	//   reason, and here we'll clear it out (and propogate to the schedd).
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}
	
	jobProxy = AcquireProxy( jobAd, error_string,
							 (TimerHandlercpp)&CreamJob::ProxyCallback, this );
	if ( jobProxy == NULL ) {
		if ( error_string == "" ) {
			formatstr( error_string, "%s is not set in the job ad",
								  ATTR_X509_USER_PROXY );
		}
		dprintf(D_ALWAYS, "errorstring %s\n", error_string.c_str());
		goto error_exit;
	}

	if ( jobProxy->subject->has_voms_attrs == false ) {
		error_string = "Job proxy has no VOMS attributes";
		goto error_exit;
	}

	gahp_path = param("CREAM_GAHP");
	if ( gahp_path == NULL ) {
		error_string = "CREAM_GAHP not defined";
		goto error_exit;
	}
	snprintf( buff, sizeof(buff), "CREAM/%s/%s",
			  jobProxy->subject->subject_name, jobProxy->subject->first_fqan );

	gahp = new GahpClient( buff, gahp_path );
	free( gahp_path );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );
	
	jobAd->LookupString( ATTR_GRID_RESOURCE, grid_resource );

	if ( grid_resource.length() ) {
		const char *token;

		Tokenize( grid_resource );

		token = GetNextToken( " ", false );
		if ( !token || strcasecmp( token, "cream" ) ) {
			formatstr( error_string, "%s not of type cream", ATTR_GRID_RESOURCE );
			goto error_exit;
		}

			/* TODO Make port and '/ce-cream/services/CREAM' optional */
		token = GetNextToken( " ", false );
		if ( token && *token ) {
			// If the resource url is missing a scheme, insert one
			if ( strncmp( token, "http://", 7 ) == 0 ||
				 strncmp( token, "https://", 8 ) == 0 ) {
				resourceManagerString = strdup( token );
			} else {
				std::string urlbuf;
				formatstr( urlbuf, "https://%s", token );
				resourceManagerString = strdup( urlbuf.c_str() );
			}
		} else {
			formatstr( error_string, "%s missing CREAM Service URL",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = GetNextToken( " ", false );
		if ( token && *token ) {
			resourceBatchSystemString = strdup( token );
		} else {
			formatstr( error_string, "%s missing batch system (LRMS) type.",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = GetNextToken( " ", false );
		if ( token && *token ) {
			resourceQueueString = strdup( token );
		} else {
			formatstr( error_string, "%s missing LRMS queue name.",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

	} else {
		formatstr( error_string, "%s is not set in the job ad",
							  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	buff[0] = '\0';
	
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff, sizeof(buff) );
	if ( buff[0] != '\0' ) {
			//since GridJobId = <cream> <ResourceManager> <jobid>
		SetRemoteJobId(strchr((strchr(buff, ' ') + 1), ' ') + 1);
		job_already_submitted = true;
	}
	
		// Find/create an appropriate CreamResource for this job
	myResource = CreamResource::FindOrCreateResource( resourceManagerString,
													  jobProxy );
	if ( myResource == NULL ) {
		error_string = "Failed to initialize CreamResource object";
		goto error_exit;
	}

	// RegisterJob() may call our NotifyResourceUp/Down(), so be careful.
	myResource->RegisterJob( this );
	if ( job_already_submitted ) {
		myResource->AlreadySubmitted( this );
	}

	if ( job_already_submitted &&
		 jobAd->LookupString( ATTR_CREAM_DELEGATION_URI, buff, sizeof(buff) ) ) {

		delegatedCredentialURI = strdup( buff );
		myResource->registerDelegationURI( delegatedCredentialURI, jobProxy );
	}

	if ( job_already_submitted ) {
		jobAd->LookupString( ATTR_CREAM_UPLOAD_URL, &uploadUrl );
		jobAd->LookupString( ATTR_CREAM_DOWNLOAD_URL, &downloadUrl );
		jobAd->LookupString( ATTR_CREAM_LEASE_ID, &leaseId );
	}

	jobAd->LookupString( ATTR_GRID_JOB_STATUS, remoteState );

	gahpErrorString = "";

	if ( jobAd->LookupString(ATTR_JOB_IWD, iwd) && iwd.length() ) {
		int len = iwd.length();
		if ( len > 1 && iwd[len - 1] != '/' ) {
			iwd += "/";
		}
	} else {
		iwd = "/";
	}

	buff2 = "";
	if ( jobAd->LookupString(ATTR_JOB_OUTPUT, job_output) && job_output.length() &&
		 strcmp( job_output.c_str(), NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_OUTPUT, bool_value ) ||
			 bool_value ) {

			if ( job_output[0] != '/' ) {
				buff2 = iwd;
			}

			buff2 += job_output;
			localOutput = strdup( buff2.c_str() );

			bool_value = false;
			jobAd->LookupBool( ATTR_STREAM_OUTPUT, bool_value );
			streamOutput = bool_value;
			stageOutput = !streamOutput;
		}
	}

	buff2 = "";
	if ( jobAd->LookupString(ATTR_JOB_ERROR, job_error) && job_error.length() &&
		 strcmp( job_error.c_str(), NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_ERROR, bool_value ) ||
			 bool_value ) {

			if ( job_error[0] != '/' ) {
				buff2 = iwd;
			}

			buff2 += job_error;
			localError = strdup( buff2.c_str() );

			bool_value = false;
			jobAd->LookupBool( ATTR_STREAM_ERROR, bool_value );
			streamError = bool_value;
			stageError = !streamError;
		}
	}

	return;

 error_exit:
		// We must ensure that the code-path from GM_HOLD doesn't depend
		// on any initialization that's been skipped.
	gmState = GM_HOLD;
	if ( !error_string.empty() ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string );
	}
	return;
}

CreamJob::~CreamJob()
{
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	if ( resourceManagerString ) {
		free( resourceManagerString );
	}
	if ( resourceBatchSystemString ) {
		free( resourceBatchSystemString );
	}
	if ( resourceQueueString ) {
		free( resourceQueueString );
	}
	if ( remoteJobId ) {
		free( remoteJobId );
	}
	if ( creamAd ) {
		free( creamAd );
	}
	if ( localOutput ) {
		free( localOutput );
	}
	if ( localError ) {
		free( localError );
	}
	if ( jobProxy ) {
		ReleaseProxy( jobProxy, (TimerHandlercpp)&CreamJob::ProxyCallback, this );
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( delegatedCredentialURI != NULL) {
		free( delegatedCredentialURI );
	}
	if ( uploadUrl != NULL ) {
		free( uploadUrl );
	}
	free( downloadUrl );
	free( leaseId );
	delete m_xfer_request;
}

void CreamJob::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

void CreamJob::ProxyCallback()
{
	if ( gmState == GM_DELEGATE_PROXY || gmState == GM_PROXY_EXPIRED ) {
		SetEvaluateState();
	}
}

void CreamJob::doEvaluateState()
{
	bool connect_failure = false;
	int old_gm_state;
	std::string old_remote_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );
	dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, creamState %s\n",
			procID.cluster,procID.proc,GMStateNames[gmState],
			remoteState.c_str());

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
		old_remote_state = remoteState;
		ASSERT ( gahp != NULL || gmState == GM_HOLD || gmState == GM_DELETE );

		switch ( gmState ) {
		  
		case GM_INIT: {
			// This is the state all jobs start in when the CreamJob object
			// is first created. Here, we do things that we didn't want to
			// do in the constructor because they could block (the
			// constructor is called while we're connected to the schedd).

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

			gahp->setMode( saved_mode );

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
			if ( remoteJobId == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( wantResubmit || doResubmit ) {
				gmState = GM_CLEAR_REQUEST;
			} else {
					// TODO we should save the cream job state in the job
					//   ad and use it to set submitLogged and
					//   executeLogged here
				submitLogged = true;
				if ( condorState == RUNNING ) {
					executeLogged = true;
				}
				
				if ( condorState == COMPLETED ) {
					gmState = GM_DONE_COMMIT;
				} else if ( remoteState == CREAM_JOB_STATE_UNSET ||
					 remoteState == CREAM_JOB_STATE_REGISTERED ) {
					gmState = GM_RECOVER_POLL;
				} else {
					probeNow = true;
					gmState = GM_SUBMITTED;
				}
			}
			} break;
		case GM_RECOVER_POLL: {
			char *status = NULL;
			char *fault = NULL;
			int exit_code = -1;
			int stagein_time = 0;
			CHECK_PROXY;

			rc = gahp->cream_job_status( resourceManagerString,
										 remoteJobId, &status,
										 &exit_code, &fault );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				if ( !resourcePingComplete && IsConnectionError( gahp->getErrorString() ) ) {
					connect_failure = true;
					break;
				}
				// unhandled error
				LOG_CREAM_ERROR( "cream_job_status()", rc );
				gahpErrorString = gahp->getErrorString();
				gmState = GM_CANCEL;
				if ( status ) {
					free( status );
				}
				if ( fault ) {
					free (fault);
				}
				break;
			}

			NewCreamState( status, exit_code, fault );
			if ( status ) {
				free( status );
			}
			if ( fault ) {
				free( fault );
			}

			if ( remoteState == CREAM_JOB_STATE_REGISTERED ) {
				if ( jobAd->LookupInteger( ATTR_STAGE_IN_FINISH, stagein_time ) ) {
					probeNow = true;
					gmState = GM_SUBMIT_COMMIT;
				} else {
					gmState = GM_STAGE_IN;
				}
			} else {
				gmState = GM_SUBMITTED;
			}
		} break;
 		case GM_UNSUBMITTED: {
			// There are no outstanding submissions for this job (if
			// there is one, we've given up on it).
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else if ( condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			} else {
				gmState = GM_DELEGATE_PROXY;
			}
		} break;
 		case GM_DELEGATE_PROXY: {
			const char *deleg_uri;
			const char *error_msg;
				// TODO What happens if CreamResource can't delegate proxy?
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			}
			if ( delegatedCredentialURI != NULL ) {
				gmState = GM_SET_LEASE;
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
			gmState = GM_SET_LEASE;
			
			jobAd->Assign( ATTR_CREAM_DELEGATION_URI,
						   delegatedCredentialURI );
		} break;
		case GM_SET_LEASE: {
			const char *lease_id;
			const char *error_msg;
				// TODO What happens if CreamResource can't set a lease?
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			}
			if ( leaseId != NULL ) {
				gmState = GM_SUBMIT;
				break;
			}
			if ( (error_msg = myResource->getLeaseError()) ) {
					// There's a problem setting the lease
				errorString = error_msg;
				gmState = GM_HOLD;
			}
			lease_id = myResource->getLeaseId();
			if ( lease_id == NULL ) {
					// lease still needs to be set. Wait.
				break;
			}
			leaseId = strdup( lease_id );
			gmState = GM_SUBMIT;
		} break;
		case GM_SUBMIT: {
			// Start a new cream submission for this job.
			char *job_id = NULL;
			char *upload_url = NULL;
			char *download_url = NULL;
			if ( condorState == REMOVED || condorState == HELD ) {
				myResource->CancelSubmit(this);
				gmState = GM_UNSUBMITTED;
				break;
			}
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
				if ( gahpErrorString == "" ) {
					gahpErrorString = "Attempts to submit failed";
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
				if ( creamAd == NULL ) {
					creamAd = buildSubmitAd();
				}
				if ( creamAd == NULL) {
					myResource->CancelSubmit(this);
					gmState = GM_HOLD;
					break;
				}
				
				rc = gahp->cream_job_register( 
										resourceManagerString,
										delegatedCredentialURI,
										creamAd, leaseId,
										&job_id, &upload_url, &download_url );
				
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				
				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;
				jmProxyExpireTime = jobProxy->expiration_time;
				if ( rc == GLOBUS_SUCCESS ) {
					SetRemoteJobId( job_id );
					free( job_id );
					uploadUrl = upload_url;
					jobAd->Assign( ATTR_CREAM_UPLOAD_URL, uploadUrl );
					downloadUrl = download_url;
					jobAd->Assign( ATTR_CREAM_DOWNLOAD_URL, downloadUrl );
					jobAd->Assign( ATTR_STAGE_IN_START, (int)now );
					gmState = GM_SUBMIT_SAVE;				

					UpdateJobLeaseSent(myResource->GetLeaseExpiration());
					
				} else {
					if ( !resourcePingComplete && IsConnectionError( gahp->getErrorString() ) ) {
						// Don't count this as a submission attempt.
						numSubmitAttempts--;
						connect_failure = true;
						break;
					}
					// unhandled error
					LOG_CREAM_ERROR( "cream_job_register()", rc );
//					dprintf(D_ALWAYS,"(%d.%d)    RSL='%s'\n",
// 						procID.cluster, procID.proc,RSL->Value());
					gahpErrorString = gahp->getErrorString();
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
			// Save the jobmanager's contact for a new cream submission.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CLEANUP;
			} else {
				if ( jobAd->IsAttributeDirty( ATTR_GRID_JOB_ID ) ) {
					requestScheddUpdate( this, true );
					break;
				}
				gmState = GM_STAGE_IN;
			}
			} break;
		case GM_STAGE_IN: {
			if ( condorState == REMOVED || condorState == HELD ) {
				delete m_xfer_request;
				m_xfer_request = NULL;
				gmState = GM_CANCEL;
			} else {
				if ( m_xfer_request == NULL ) {
					m_xfer_request = MakeStageInRequest();
				}
				if ( m_xfer_request->m_status == TransferRequest::TransferDone ) {
					delete m_xfer_request;
					m_xfer_request = NULL;
					jobAd->Assign( ATTR_STAGE_IN_FINISH, (int)now );
					requestScheddUpdate( this, false );
					gmState = GM_SUBMIT_COMMIT;
				} else if ( m_xfer_request->m_status == TransferRequest::TransferFailed ) {
					dprintf( D_ALWAYS, "(%d.%d) Stage-in failed: %s\n",
							 procID.cluster, procID.proc,
							 m_xfer_request->m_errMsg.c_str() );
					gahpErrorString = m_xfer_request->m_errMsg;
					delete m_xfer_request;
					m_xfer_request = NULL;
					gmState = GM_CLEAR_REQUEST;
				}
			}
		} break;
		case GM_SUBMIT_COMMIT: {
			// Now that we've saved the job id, tell cream it can start
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				CHECK_PROXY;
				rc = gahp->cream_job_start( resourceManagerString,
											remoteJobId );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					if ( !resourcePingComplete && IsConnectionError( gahp->getErrorString() ) ) {
						connect_failure = true;
						break;
					}
					// unhandled error
					LOG_CREAM_ERROR( "cream_job_start()", rc );
					gahpErrorString = gahp->getErrorString();
					gmState = GM_CLEAR_REQUEST;
						//gmState = GM_CANCEL;
				} else {
					gmState = GM_SUBMITTED;
				}
			}
			} break;
		case GM_SUBMITTED: {
			// The job has been submitted (or is about to be by the
			// jobmanager). Wait for completion or failure, and probe the
			// jobmanager occassionally to make it's still alive.
			if ( remoteState == CREAM_JOB_STATE_DONE_OK ) {
				gmState = GM_STAGE_OUT;
			} else if ( remoteState == CREAM_JOB_STATE_DONE_FAILED || remoteState == CREAM_JOB_STATE_ABORTED || remoteState == CREAM_JOB_STATE_CANCELLED ) {
				gmState = GM_PURGE;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				if ( probeNow || remoteState == CREAM_JOB_STATE_UNSET ) {
					doActivePoll = true;
					probeNow = false;
				}

				if(doActivePoll) {
					gmState = GM_POLL_JOB_STATE;
					break;
				}
				
			}
			} break;
		case GM_POLL_JOB_STATE: {
			doActivePoll = false;
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				char *status = NULL;
				char *fault = NULL;
				int exit_code = -1;
				CHECK_PROXY;
								
				rc = gahp->cream_job_status( resourceManagerString,
											 remoteJobId, &status,
											 &exit_code, &fault );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					if ( !resourcePingComplete && IsConnectionError( gahp->getErrorString() ) ) {
						connect_failure = true;
						break;
					}
					// unhandled error
					LOG_CREAM_ERROR( "cream_job_status()", rc );
					gahpErrorString = gahp->getErrorString();
					gmState = GM_CANCEL;
					if ( status ) {
						free( status );
					}
					if ( fault ) {
						free (fault);
					}
					break;
				}

				NewCreamState( status, exit_code, fault );
				if ( status ) {
					free( status );
				}
				if ( fault ) {
					free( fault );
				}

				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_STAGE_OUT: {
			if ( condorState == REMOVED || condorState == HELD ) {
				delete m_xfer_request;
				m_xfer_request = NULL;
				gmState = GM_CANCEL;
			} else {
				if ( m_xfer_request == NULL ) {
					m_xfer_request = MakeStageOutRequest();
				}
				// TODO: Add check for job lease renewal
				if ( m_xfer_request->m_status == TransferRequest::TransferDone ) {
					delete m_xfer_request;
					m_xfer_request = NULL;
					gmState = GM_DONE_SAVE;
				} else if ( m_xfer_request->m_status == TransferRequest::TransferFailed ) {
					dprintf( D_ALWAYS, "(%d.%d) Stage-out failed: %s\n",
							 procID.cluster, procID.proc,
							 m_xfer_request->m_errMsg.c_str() );
					gahpErrorString = m_xfer_request->m_errMsg;
					delete m_xfer_request;
					m_xfer_request = NULL;
					gmState = GM_CLEAR_REQUEST;
				}
			}
		} break;
		case GM_DONE_SAVE: {
			// Report job completion to the schedd.
			JobTerminated();
			if ( condorState == COMPLETED ) {
				if ( jobAd->IsAttributeDirty( ATTR_JOB_STATUS ) ) {
					requestScheddUpdate( this, true );
					break;
				}
			}
			gmState = GM_DONE_COMMIT;
			} break;
		case GM_DONE_COMMIT: {
			// Tell the jobmanager it can clean up and exit.
			CHECK_PROXY;
			rc = gahp->cream_job_purge( resourceManagerString, remoteJobId );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				if ( strstr( gahp->getErrorString(), "job does not exist" ) ) {
					// Job already gone, treat as success
				} else if ( !resourcePingComplete && IsConnectionError( gahp->getErrorString() ) ) {
					connect_failure = true;
					break;
				} else {
					// unhandled error
					LOG_CREAM_ERROR( "cream_job_purge()", rc );
					gahpErrorString = gahp->getErrorString();
					gmState = GM_CLEAR_REQUEST;
//					gmState = GM_CANCEL;
					break;
				}
			}
			myResource->CancelSubmit( this );
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				if ( remoteJobId != NULL ) {
					SetRemoteJobId( NULL );
					remoteState = CREAM_JOB_STATE_UNSET;
					SetRemoteJobStatus( NULL );
					requestScheddUpdate( this, false );
				}
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.
			if ( remoteState != CREAM_JOB_STATE_ABORTED &&
				 remoteState != CREAM_JOB_STATE_CANCELLED &&
				 remoteState != CREAM_JOB_STATE_DONE_OK &&
				 remoteState != CREAM_JOB_STATE_DONE_FAILED ) {
				CHECK_PROXY;
				
				rc = gahp->cream_job_cancel( resourceManagerString,
											 remoteJobId );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					if ( !resourcePingComplete && IsConnectionError( gahp->getErrorString() ) ) {
						connect_failure = true;
						break;
					}
						// unhandled error
					LOG_CREAM_ERROR( "cream_job_cancel()", rc );
					gahpErrorString = gahp->getErrorString();
					gmState = GM_CLEAR_REQUEST;
					break;
				}
			}
			gmState = GM_CLEANUP;
			} break;
		case GM_CLEANUP: {
				// Cleanup the job at cream server
				// Need to wait since cream doesn't allow immediate
				// purging of cancelled jobs
			if ( now < enteredCurrentGmState + CLEANUP_DELAY ) {
				daemonCore->Reset_Timer( evaluateStateTid,
										 enteredCurrentGmState + CLEANUP_DELAY - now );
				break;
			}
			CHECK_PROXY;
			rc = gahp->cream_job_purge( resourceManagerString, remoteJobId );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			m_numCleanupAttempts++;
			if ( rc != GLOBUS_SUCCESS ) {
				if ( strstr( gahp->getErrorString(), "job does not exist" ) ) {
					// Job already gone, treat as success
				} else if ( strstr( gahp->getErrorString(),
									"job status does not match" ) && 
							m_numCleanupAttempts < MAX_CLEANUP_ATTEMPTS ) {
					// The server is probably taking a while to process
					// job cancellation. Give it a little more time, then
					// retry the purge request.
					enteredCurrentGmState = now;
					reevaluate_state = true;
					break;
				} else if ( !resourcePingComplete && IsConnectionError( gahp->getErrorString() ) ) {
					connect_failure = true;
					break;
				} else {
					// unhandled error
					LOG_CREAM_ERROR( "cream_job_purge", rc );
					gahpErrorString = gahp->getErrorString();
					gmState = GM_CLEAR_REQUEST;
					break;
				}
			}

			myResource->CancelSubmit( this );
			remoteState = CREAM_JOB_STATE_UNSET;
			SetRemoteJobStatus( NULL );
			requestScheddUpdate( this, false );

			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				SetRemoteJobId( NULL );
				gmState= GM_HOLD;
			}
		} break;
		case GM_PURGE: {
			// The cream server's job state is in a terminal (failed)
			// state. Send a purge command to tell the server it can
			// delete the job from its logs.
			CHECK_PROXY;
			rc = gahp->cream_job_purge( resourceManagerString, remoteJobId );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				if ( !resourcePingComplete && IsConnectionError( gahp->getErrorString() ) ) {
					connect_failure = true;
					break;
				}
					// unhandled error
				LOG_CREAM_ERROR( "cream_job_purge", rc );
				gahpErrorString = gahp->getErrorString();
				gmState = GM_CLEAR_REQUEST;
				break;
			}

			myResource->CancelSubmit( this );
			remoteState = CREAM_JOB_STATE_UNSET;
			SetRemoteJobStatus( NULL );
			requestScheddUpdate( this, false );

			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				SetRemoteJobId( NULL );
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
			// for the schedd to be updated and subsequently this cream job
			// object to be destroyed.  So there is nothing to do.
			if ( wantRematch ) {
				break;
			}
			
			// For now, put problem jobs on hold instead of
			// forgetting about current submission and trying again.
			// TODO: Let our action here be dictated by the user preference
			// expressed in the job ad.
			if ( ( remoteJobId != NULL ||
				   remoteState == CREAM_JOB_STATE_ABORTED ||
				   remoteState == CREAM_JOB_STATE_DONE_FAILED ) 
				     && condorState != REMOVED 
					 && wantResubmit == false 
					 && doResubmit == 0 ) {
				if(remoteJobId == NULL) {
					dprintf(D_FULLDEBUG,
							"(%d.%d) Putting on HOLD: lacks remote job ID\n",
							procID.cluster, procID.proc);
				} else if(remoteState == CREAM_JOB_STATE_ABORTED) {
					dprintf(D_FULLDEBUG,
							"(%d.%d) Putting on HOLD: CREAM_JOB_STATE_ABORTED\n",
							procID.cluster, procID.proc);
				} else if(remoteState == CREAM_JOB_STATE_DONE_FAILED) {
					dprintf(D_FULLDEBUG,
							"(%d.%d) Putting on HOLD: CREAM_JOB_STATE_DONE_FAILED\n",
							procID.cluster, procID.proc);
				} else {
					dprintf(D_FULLDEBUG,
							"(%d.%d) Putting on HOLD: Unknown reason\n",
							procID.cluster, procID.proc);
				}
				gmState = GM_HOLD;
				break;
			}
			// Only allow a rematch *if* we are also going to perform a resubmit
			if ( wantResubmit || doResubmit ) {
				jobAd->LookupBool(ATTR_REMATCH_CHECK,wantRematch);
			}
			if ( wantResubmit ) {
				wantResubmit = false;
				dprintf(D_ALWAYS,
						"(%d.%d) Resubmitting to CREAM because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_GLOBUS_RESUBMIT_CHECK );
			}
			if ( doResubmit ) {
				doResubmit = 0;
				dprintf(D_ALWAYS,
					"(%d.%d) Resubmitting to CREAM (last submit failed)\n",
						procID.cluster, procID.proc );
			}
			remoteState = CREAM_JOB_STATE_UNSET;
			remoteStateFaultString = "";
			SetRemoteJobStatus( NULL );
			gahpErrorString = "";
			errorString = "";
			UpdateJobLeaseSent( -1 );
			myResource->CancelSubmit( this );
			if ( remoteJobId != NULL ) {
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
			delete m_xfer_request;
			m_xfer_request = NULL;
			
			int stage_time;
			if ( jobAd->LookupInteger( ATTR_STAGE_IN_START, stage_time ) ) {
				jobAd->AssignExpr( ATTR_STAGE_IN_START, "Undefined" );
			}
			if ( jobAd->LookupInteger( ATTR_STAGE_IN_FINISH, stage_time ) ) {
				jobAd->AssignExpr( ATTR_STAGE_IN_FINISH, "Undefined" );
			}

			if ( wantRematch ) {
				dprintf(D_ALWAYS,
						"(%d.%d) Requesting schedd to rematch job because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_REMATCH_CHECK );

				// Set ad attributes so the schedd finds a new match.
				bool dummy;
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
			if ( jobAd->dirtyBegin() != jobAd->dirtyEnd() ) {
				requestScheddUpdate( this, true );
				break;
			}
			m_numCleanupAttempts = 0;
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
									 sizeof(holdReason) );
				if ( holdReason[0] == '\0' && errorString != "" ) {
					strncpy( holdReason, errorString.c_str(),
							 sizeof(holdReason) - 1 );
				}
				if ( holdReason[0] == '\0' &&
					 !remoteStateFaultString.empty() ) {

					snprintf( holdReason, 1024, "CREAM error: %s",
							  remoteStateFaultString.c_str() );
				}
				if ( holdReason[0] == '\0' && !gahpErrorString.empty() ) {
					snprintf( holdReason, 1024, "CREAM error: %s",
							  gahpErrorString.c_str() );
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
				RequestPing();
			} else {
				// Do nothing. Our proxy is about to expire.
			}
			} break;
		default:
			EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster,procID.proc,
					gmState );
		}


		if ( gmState != old_gm_state || remoteState != old_remote_state ) {
			reevaluate_state = true;
		}
		if ( remoteState != old_remote_state ) {
/*
			dprintf(D_FULLDEBUG, "(%d.%d) remote state change: %s -> %s\n",
					procID.cluster, procID.proc,
					old_remote_state.c_str(),
					remoteState.c_str());
*/
			enteredCurrentRemoteState = time(NULL);
		}
		if ( gmState != old_gm_state ) {
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
			// If we were waiting for a pending gahp call, we're not
			// anymore so purge it.
			if ( gahp ) {
				gahp->purgePendingRequests();
			}
			// If we were calling a gahp call that creamAd, we're done
			// with it now, so free it.
			if ( creamAd ) {
				free( creamAd );
				creamAd = NULL;
			}
			connectFailureCount = 0;
			resourcePingComplete = false;
		}
	} while ( reevaluate_state );
		//end of evaluateState loop

	if ( connect_failure && !resourceDown ) {
		if ( connectFailureCount < maxConnectFailures ) {
			connectFailureCount++;
			int retry_secs = param_integer(
				"GRIDMANAGER_CONNECT_FAILURE_RETRY_INTERVAL",5);
			dprintf(D_FULLDEBUG,
				"(%d.%d) Connection failure (try #%d), retrying in %d secs\n",
				procID.cluster,procID.proc,connectFailureCount,retry_secs);
			daemonCore->Reset_Timer( evaluateStateTid, retry_secs );
		} else {
			dprintf(D_FULLDEBUG,
				"(%d.%d) Connection failure, requesting a ping of the resource\n",
				procID.cluster,procID.proc);
			RequestPing();
		}
	}
}

BaseResource *CreamJob::GetResource()
{
	return (BaseResource *)myResource;
}

void CreamJob::SetRemoteJobId( const char *job_id )
{
	free( remoteJobId );
	if ( job_id ) {
		remoteJobId = strdup( job_id );
	} else {
		remoteJobId = NULL;
	}

	std::string full_job_id;
	if ( job_id ) {
		full_job_id = CreamJob::getFullJobId(resourceManagerString, job_id);
	}
	BaseJob::SetRemoteJobId( full_job_id.c_str() );
}

void CreamJob::NewCreamState( const char *new_state, int exit_code,
							  const char *failure_reason )
{
	std::string new_state_str = new_state ? new_state : "";

		// TODO verify that the string is a valid state name

	SetRemoteJobStatus( new_state );

	if ( new_state_str != remoteState ) {
		dprintf( D_FULLDEBUG, "(%d.%d) cream state change: %s -> %s\n",
				 procID.cluster, procID.proc, remoteState.c_str(),
				 new_state_str.c_str() );

		if ( ( new_state_str == CREAM_JOB_STATE_RUNNING ||
			   new_state_str == CREAM_JOB_STATE_REALLY_RUNNING ) &&
			 condorState == IDLE ) {
			JobRunning();
		}

		if ( new_state_str == CREAM_JOB_STATE_HELD &&
			 condorState == RUNNING ) {
			JobIdle();
		}

		// TODO When do we consider the submission successful or not:
		//   when Register works, when Start() works, or when the job
		//   state moves to IDLE?
		if ( remoteState == CREAM_JOB_STATE_UNSET &&
			 !submitLogged && !submitFailedLogged ) {
			if ( new_state_str != CREAM_JOB_STATE_ABORTED ) {
					// The request was successfuly submitted. Write it to
					// the user-log
				if ( !submitLogged ) {
					WriteGridSubmitEventToUserLog( jobAd );
					submitLogged = true;
				}
			}
		}

		remoteState = new_state_str;
		enteredCurrentRemoteState = time(NULL);
		SetRemoteJobStatus( remoteState.c_str() );

		// When a job is in DONE-OK state, Cream will often set a
		// failure message of "reason=0", even though there is no
		// failure. If there is a subsequent failure (say in staging
		// output files), having remoteStateFaultString set to a
		// non-empty value will hide the real failure message.
		if ( failure_reason && remoteState != CREAM_JOB_STATE_DONE_OK ) {
			remoteStateFaultString = failure_reason;
		} else {
			remoteStateFaultString = "";
		}

		// TODO handle jobs that exit via a signal
		if ( remoteState == CREAM_JOB_STATE_DONE_OK ) {
			jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );
			jobAd->Assign( ATTR_ON_EXIT_CODE, exit_code );
		}

		requestScheddUpdate( this, false );

		SetEvaluateState();
	}
}


// Build submit classad
char *CreamJob::buildSubmitAd()
{
	const char *ATTR_EXECUTABLE = "Executable";
	const char *ATTR_ARGS = "Arguments";
	const char *ATTR_STD_INPUT = "StdInput";
	const char *ATTR_STD_OUTPUT = "StdOutput";
	const char *ATTR_STD_ERROR = "StdError";
	const char *ATTR_INPUT_SB = "InputSandbox";
	const char *ATTR_OUTPUT_SB = "OutputSandbox";
	const char *ATTR_VIR_ORG = "VirtualOrganization";
	const char *ATTR_BATCH_SYSTEM = "BatchSystem";
	const char *ATTR_QUEUE_NAME = "QueueName";
	
	ClassAd submitAd;

	std::string tmp_str = "";
	std::string tmp_str2 = "";
	std::string buf = "";
	std::string iwd_str = "";
	StringList isb;
	StringList osb;
	bool result;

		// Once we add streaming support, remove this
	if ( streamOutput || streamError ) {
		errorString = "Streaming not supported";
		return NULL;
	}

		//IWD
	jobAd->LookupString(ATTR_JOB_IWD, iwd_str);
	if ( jobAd->LookupString(ATTR_JOB_IWD, iwd_str)) {
		int len = iwd_str.length();
		if ( len > 1 && iwd_str[len - 1] != '/' ) {
			iwd_str += '/';
		}
	} else {
		iwd_str = '/';
	}
	
		//EXECUTABLE can either be PRE-STAGED or TRANSFERED
		//here, JOB_CMD = full path to executable
	result = true;
	jobAd->LookupString(ATTR_JOB_CMD, tmp_str);
	jobAd->LookupBool(ATTR_TRANSFER_EXECUTABLE, result);
	if (result) {
		//TRANSFERED
		tmp_str2 = condor_basename( tmp_str.c_str() );

		isb.insert( tmp_str2.c_str() );

		submitAd.Assign( ATTR_EXECUTABLE, tmp_str2 );
	} else {
		//PRE-STAGED
		submitAd.Assign( ATTR_EXECUTABLE, tmp_str );
	}

		//ARGUMENTS
	ArgList args;
	std::string arg_errors;
	if( !args.AppendArgsFromClassAd( jobAd, arg_errors ) ) {
		dprintf( D_ALWAYS, "(%d.%d) Failed to read job arguments: %s\n",
				 procID.cluster, procID.proc, arg_errors.c_str());
		formatstr( errorString, "Failed to read job arguments: %s\n",
				   arg_errors.c_str() );
		return NULL;
	}
	if(args.Count() != 0) {
		std::string args_str;
		for(int i=0;i<args.Count();i++) {
			if(i) {
				args_str += ' ';
			}
			// TODO Add escaping of special characters
			args_str += args.GetArg(i);
		}
		submitAd.Assign( ATTR_ARGS, args_str );
	}
	
		//STDINPUT can be either be PRE-STAGED or TRANSFERED
	result = true;
	jobAd->LookupBool(ATTR_TRANSFER_INPUT, result);
	jobAd->LookupString(ATTR_JOB_INPUT, tmp_str);
	if ( !tmp_str.empty() ) {
		if (result) {
			//TRANSFERED
			tmp_str2 = condor_basename( tmp_str.c_str() );
			isb.insert(condor_basename( tmp_str2.c_str() ) );
			
			submitAd.Assign( ATTR_STD_INPUT, tmp_str2 );
		} else {
			//PRE-STAGED. Be careful, if stdin is not found in WN, job
			// will not complete successfully.
			if ( tmp_str[0] == '/' ) { //Only add absolute path
				submitAd.Assign( ATTR_STD_INPUT, tmp_str );
			}
		}
	}
		
		//TRANSFER INPUT FILES
	if (jobAd->LookupString(ATTR_TRANSFER_INPUT_FILES, tmp_str)) {
		StringList strlist(tmp_str.c_str());
		strlist.rewind();
		
		for (int i = 0; i < strlist.number(); i++) {
			tmp_str = strlist.next();

			isb.insert( condor_basename( tmp_str.c_str() ) );
		}
	}

		//TRANSFER OUTPUT FILES: handle absolute ?
	if (jobAd->LookupString(ATTR_TRANSFER_OUTPUT_FILES, tmp_str)) {
		char *filename;
		StringList output_files(tmp_str.c_str());
		output_files.rewind();
		while ( (filename = output_files.next()) != NULL ) {

			osb.insert( filename );
		}
	}
	
		//STDOUTPUT TODO: handle absolute ?
	if (jobAd->LookupString(ATTR_JOB_OUTPUT, tmp_str)) {

		result = true;
		jobAd->LookupBool(ATTR_TRANSFER_OUTPUT, result);

		if (result) {
			tmp_str2 = condor_basename( tmp_str.c_str() );
			submitAd.Assign( ATTR_STD_OUTPUT, tmp_str2 );

			osb.insert( condor_basename( tmp_str2.c_str() ) );
		} else {
			submitAd.Assign( ATTR_STD_OUTPUT, tmp_str );
		}
	}

		//STDERROR TODO: handle absolute ?
	if (jobAd->LookupString(ATTR_JOB_ERROR, tmp_str)) {

		result = true;
		jobAd->LookupBool(ATTR_TRANSFER_ERROR, result);

		if (result) {
			tmp_str2 = condor_basename( tmp_str.c_str() );
			submitAd.Assign( ATTR_STD_ERROR, tmp_str2 );

			osb.insert( condor_basename( tmp_str2.c_str() ) );
		} else {
			submitAd.Assign( ATTR_STD_ERROR, tmp_str );
		}
	}

		//VIRTUALORGANISATION. CREAM requires this attribute, but it doesn't
		//need to have a value
		// TODO This needs to be extracted from the VOMS extension in the
		//   job's credential.
	submitAd.Assign(ATTR_VIR_ORG, "ignored");
	
		//BATCHSYSTEM
	submitAd.Assign(ATTR_BATCH_SYSTEM, resourceBatchSystemString);
	
		//QUEUENAME
	submitAd.Assign(ATTR_QUEUE_NAME, resourceQueueString);

	submitAd.Assign("outputsandboxbasedesturi", "gsiftp://localhost");

	std::string ad_string;
	std::string ad_str;

	classad::ClassAdUnParser unparser;
	unparser.Unparse( ad_str, &submitAd );
	ad_string = ad_str;
	// TODO Insert following lists directly into classad and unparse
	//   full ad into std::string and use that.

		// Attributes that use new ClassAd lists have to be manually
		// inserted after unparsing the ad.

		//INPUT SANDBOX
	if (isb.number() > 0) {
		formatstr(buf, "; %s = {", ATTR_INPUT_SB);
		isb.rewind();
		for (int i = 0; i < isb.number(); i++) {
			if (i == 0)
				formatstr_cat(buf, "\"%s\"", isb.next());
			else
				formatstr_cat(buf, ",\"%s\"", isb.next());
		}
		formatstr_cat(buf, "} ]");

		int insert_pos = strrchr( ad_string.c_str(), ']' ) - ad_string.c_str();
		replace_str( ad_string, "]", buf.c_str(), insert_pos );
	}

		//OUTPUT SANDBOX
	if (osb.number() > 0) {
		formatstr(buf, "; %s = {", ATTR_OUTPUT_SB);
		osb.rewind();
		for (int i = 0; i < osb.number(); i++) {
			if (i == 0)
				formatstr_cat(buf, "\"%s\"", osb.next());
			else
				formatstr_cat(buf, ",\"%s\"", osb.next());
		}
		formatstr_cat(buf, "} ]");

		int insert_pos = strrchr( ad_string.c_str(), ']' ) - ad_string.c_str();
		replace_str( ad_string, "]", buf.c_str(), insert_pos );
	}

		//ENVIRONMENT
	Env envobj;
	std::string env_errors;
	if(!envobj.MergeFrom(jobAd, env_errors)) {
		dprintf(D_ALWAYS,"(%d.%d) Failed to read job environment: %s\n",
				procID.cluster, procID.proc, env_errors.c_str());
		formatstr(errorString,"Failed to read job environment: %s\n",
							env_errors.c_str());
		return NULL;
	}
	char **env_vec = envobj.getStringArray();

	if ( env_vec[0] ) {
		formatstr( buf, "; %s = {", ATTR_JOB_ENVIRONMENT2 );

		for ( int i = 0; env_vec[i]; i++ ) {
			if ( i == 0 ) {
				formatstr_cat( buf, "\"%s\"", env_vec[i] );
			} else {
				formatstr_cat( buf, ",\"%s\"", env_vec[i] );
			}
		}
		formatstr_cat( buf, "} ]" );

		int insert_pos = strrchr( ad_string.c_str(), ']' ) - ad_string.c_str();
		replace_str( ad_string, "]", buf.c_str(), insert_pos );
	}
	deleteStringArray(env_vec);

	if ( jobAd->LookupString( ATTR_CREAM_ATTRIBUTES, tmp_str ) ) {
		formatstr( buf, "; %s ]", tmp_str.c_str() );

		int insert_pos = strrchr( ad_string.c_str(), ']' ) - ad_string.c_str();
		replace_str( ad_string, "]", buf.c_str(), insert_pos );
	}

/*
	dprintf(D_FULLDEBUG, "SUBMITAD:\n%s\n",ad_string.Value()); 
*/
	return strdup( ad_string.c_str() );
}

bool CreamJob::IsConnectionError( const char *msg )
{
	if ( strstr( msg, "[Connection timed out]" ) ||
		 strstr( msg, "[Connection refused]" ) ||
		 strstr( msg, "[Unknown host]" ) ||
		 strstr( msg, "EOF detected during communication." ) ) {
		return true;
	} else {
		return false;
	}
}

std::string CreamJob::getFullJobId(const char * resourceManager, const char * job_id) 
{
	ASSERT(resourceManager);
	ASSERT(job_id);
	std::string full_job_id;
	formatstr( full_job_id, "cream %s %s", resourceManager, job_id );
	return full_job_id;
}

TransferRequest *CreamJob::MakeStageInRequest()
{
	std::string tmp_str = "";
	std::string tmp_str2 = "";
	std::string iwd_str = "";
	StringList local_urls;
	StringList remote_urls;
	bool result;

	if ( jobAd->LookupString(ATTR_JOB_IWD, tmp_str) ) {
		int len = tmp_str.length();
		if ( len > 1 && tmp_str[len - 1] != '/' ) {
			tmp_str += '/';
		}
		iwd_str = "file://" + tmp_str;
	} else {
		iwd_str = "file:///";
	}

	result = true;
	jobAd->LookupBool(ATTR_TRANSFER_EXECUTABLE, result);
	if (result) {
		GetJobExecutable( jobAd, tmp_str );
		tmp_str2 = "file://" + tmp_str;
		local_urls.insert(tmp_str2.c_str());

		jobAd->LookupString( ATTR_JOB_CMD, tmp_str );
		formatstr( tmp_str2, "%s/%s", uploadUrl,
				 condor_basename( tmp_str.c_str() ) );
		remote_urls.insert( tmp_str2.c_str() );
	}

	result = true;
	jobAd->LookupBool(ATTR_TRANSFER_INPUT, result);
	if (result) { //TRANSFERED

		if (jobAd->LookupString(ATTR_JOB_INPUT, tmp_str)) {

			if (tmp_str[0] != '/')  //not absolute path
				tmp_str2 = iwd_str + tmp_str;
			else 
				tmp_str2 = "file://" + tmp_str;

			local_urls.insert( tmp_str2.c_str() );

			formatstr( tmp_str2, "%s/%s", uploadUrl,
					 condor_basename( tmp_str.c_str() ) );
			remote_urls.insert( tmp_str2.c_str() );
		}
	}

	if (jobAd->LookupString(ATTR_TRANSFER_INPUT_FILES, tmp_str)) {
		StringList strlist(tmp_str.c_str());
		strlist.rewind();

		for (int i = 0; i < strlist.number(); i++) {
			tmp_str = strlist.next();

			if (tmp_str[0] != '/')  //not absolute path
				tmp_str2 = iwd_str + tmp_str;
			else 
				tmp_str2 = "file://" + tmp_str;

			local_urls.insert( tmp_str2.c_str() );

			formatstr( tmp_str2, "%s/%s", uploadUrl,
					 condor_basename( tmp_str.c_str() ) );
			remote_urls.insert( tmp_str2.c_str() );
		}
	}

	return new TransferRequest( jobProxy, local_urls, remote_urls,
								evaluateStateTid );
}

TransferRequest *CreamJob::MakeStageOutRequest()
{
	StringList remote_urls;
	StringList local_urls;
	std::string tmp_str;
	std::string buf;
	std::string iwd_str = "";
	bool result;

	if ( jobAd->LookupString( ATTR_JOB_IWD, tmp_str ) ) {
		int len = tmp_str.length();
		if ( len > 1 && tmp_str[len - 1] != '/' ) {
			tmp_str += '/';
		}
		iwd_str = "file://" + tmp_str;
	} else {
		iwd_str = "file:///";
	}

	if ( jobAd->LookupString( ATTR_TRANSFER_OUTPUT_FILES, tmp_str ) ) {
		char *filename;
		char *remaps = NULL;
		std::string new_name;
		jobAd->LookupString( ATTR_TRANSFER_OUTPUT_REMAPS, &remaps );

		StringList output_files(tmp_str.c_str());
		output_files.rewind();
		while ( (filename = output_files.next()) != NULL ) {

			formatstr( buf, "%s/%s", downloadUrl, filename );
			remote_urls.insert( buf.c_str() );

			if ( remaps && filename_remap_find( remaps, filename,
												new_name ) ) {
				formatstr( buf, "%s%s",
						 new_name[0] == '/' ? "file://" : iwd_str.c_str(),
						 new_name.c_str() );
			} else {
				formatstr( buf, "%s%s",
						 iwd_str.c_str(),
						 condor_basename( filename ) );
			}
			local_urls.insert( buf.c_str() );
		}

		free( remaps );
	}

	if ( jobAd->LookupString( ATTR_JOB_OUTPUT, tmp_str ) ) {

		result = true;
		jobAd->LookupBool(ATTR_TRANSFER_OUTPUT, result);

		if (result) {
			formatstr( buf, "%s/%s", downloadUrl,
					 condor_basename( tmp_str.c_str() ) );
			remote_urls.insert( buf.c_str() );

			formatstr( buf, "%s%s",
					 tmp_str[0] == '/' ? "file://" : iwd_str.c_str(),
					 tmp_str.c_str());

			local_urls.insert( buf.c_str() );
		}
	}

		//STDERROR TODO: handle absolute ?
	if ( jobAd->LookupString( ATTR_JOB_ERROR, tmp_str ) ) {

		result = true;
		jobAd->LookupBool( ATTR_TRANSFER_ERROR, result );

		if ( result ) {
			formatstr( buf, "%s/%s", downloadUrl,
					 condor_basename( tmp_str.c_str() ) );
			remote_urls.insert( buf.c_str() );

			formatstr( buf, "%s%s",
					 tmp_str[0] == '/' ? "file://" : iwd_str.c_str(),
					 tmp_str.c_str() );

			local_urls.insert( buf.c_str() );
		}
	}

	return new TransferRequest( jobProxy, remote_urls, local_urls,
								evaluateStateTid );
}
