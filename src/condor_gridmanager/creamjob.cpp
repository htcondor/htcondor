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
#include "condor_new_classads.h"

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
#define GM_EXTEND_LIFETIME		14
#define GM_POLL_JOB_STATE		15
#define GM_START				16
#define GM_DELEGATE_PROXY		17
#define GM_CLEANUP				18
#define GM_SET_LEASE			19

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
	"GM_EXTEND_LIFETIME",
	"GM_POLL_JOB_STATE",
	"GM_START",
	"GM_DELEGATE_PROXY",
	"GM_CLEANUP",
	"GM_SET_LEASE",
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
const char *ATTR_CREAM_DELEGATION_URI = "CreamDelegationUri";
const char *ATTR_CREAM_LEASE_ID = "CreamLeaseId";

// TODO: once we can set the jobmanager's proxy timeout, we should either
// let this be set in the config file or set it to
// GRIDMANAGER_MINIMUM_PROXY_TIME + 60
#define JM_MIN_PROXY_TIME		(minProxy_time + 60)

#define DEFAULT_LEASE_DURATION	6*60*60 //6 hr

#define CLEANUP_DELAY	5

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define LOG_CREAM_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, remoteState %s: %s %s\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],remoteState.Value(), \
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

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	CreamJob::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_RESOURCE_PROBE_INTERVAL", 5 * 60 );
	CreamResource::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	CreamJob::setGahpCallTimeout( tmp_int );
	CreamResource::setGahpCallTimeout( tmp_int );

	// Tell all the resource objects to deal with their new config values
	CreamResource *next_resource;

	CreamResource::ResourcesByName.startIterations();
	
	while ( CreamResource::ResourcesByName.iterate( next_resource ) != 0 ) {
		next_resource->Reconfig();
	}
}

bool CreamJobAdMatch( const ClassAd *job_ad ) {
	int universe;
	MyString resource;
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 strncasecmp( resource.Value(), "cream ", 6 ) == 0 ) {

		return true;
	}
	return false;
}

BaseJob *CreamJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new CreamJob( jobad );
}

int CreamJob::probeInterval = 300;			// default value
int CreamJob::submitInterval = 300;			// default value
int CreamJob::gahpCallTimeout = 300;		// default value

CreamJob::CreamJob( ClassAd *classad )
	: BaseJob( classad )
{

	int bool_value;
	char buff[4096];
	MyString buff2;
	MyString iwd;
	MyString job_output;
	MyString job_error;
	MyString grid_resource;
	bool job_already_submitted = false;
	MyString error_string = "";
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
	remoteStateFaultString = 0;
	gmState = GM_INIT;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	enteredCurrentRemoteState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	jmProxyExpireTime = 0;
	jmLifetime = 0;
	resourceManagerString = NULL;
	myResource = NULL;
	jobProxy = NULL;
	uploadUrl = NULL;
	gahp = NULL;
	delegatedCredentialURI = NULL;
	gridftpServer = NULL;
	leaseId = NULL;

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	// TODO This is bad. The job may already be on hold with a valid hold
	//   reason, and here we'll clear it out (and propogate to the schedd).
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}
	
	jobProxy = AcquireProxy( jobAd, error_string,
							 (Eventcpp)&BaseJob::SetEvaluateState, this );
	if ( jobProxy == NULL ) {
		if ( error_string == "" ) {
			error_string.sprintf( "%s is not set in the job ad",
								  ATTR_X509_USER_PROXY );
		}
		dprintf(D_ALWAYS, "errorstring %s\n", error_string.Value());
		goto error_exit;
	}

	gahp_path = param("CREAM_GAHP");
	if ( gahp_path == NULL ) {
		error_string = "CREAM_GAHP not defined";
		goto error_exit;
	}
	snprintf( buff, sizeof(buff), "CREAM/%s",
			  jobProxy->subject->subject_name );

	gahp = new GahpClient( buff, gahp_path );
	free( gahp_path );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );
	
	jobAd->LookupString( ATTR_GRID_RESOURCE, grid_resource );

	if ( grid_resource.Length() ) {
		const char *token;

		grid_resource.Tokenize();

		token = grid_resource.GetNextToken( " ", false );
		if ( !token || stricmp( token, "cream" ) ) {
			error_string.sprintf( "%s not of type cream", ATTR_GRID_RESOURCE );
			goto error_exit;
		}

			/* TODO Make port and '/ce-cream/services/CREAM' optional */
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
			error_string.sprintf( "%s missing CREAM Service URL",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = grid_resource.GetNextToken( " ", false );
		if ( token && *token ) {
			resourceBatchSystemString = strdup( token );
		} else {
			error_string.sprintf( "%s missing batch system (LRMS) type.",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = grid_resource.GetNextToken( " ", false );
		if ( token && *token ) {
			resourceQueueString = strdup( token );
		} else {
			error_string.sprintf( "%s missing LRMS queue name.",
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
			//since GridJobId = <cream> <ResourceManager> <jobid>
		SetRemoteJobId(strchr((strchr(buff, ' ') + 1), ' ') + 1);
		job_already_submitted = true;
	}
	
		// Find/create an appropriate CreamResource for this job
	myResource = CreamResource::FindOrCreateResource( resourceManagerString,
													  jobProxy->subject->subject_name);
	if ( myResource == NULL ) {
		error_string = "Failed to initialize CreamResource object";
		goto error_exit;
	}

	// RegisterJob() may call our NotifyResourceUp/Down(), so be careful.
	myResource->RegisterJob( this );
	if ( job_already_submitted ) {
		myResource->AlreadySubmitted( this );
	}

	buff[0] = '\0';
	if ( job_already_submitted ) {
		jobAd->LookupString( ATTR_GRIDFTP_URL_BASE, buff );
	}

	gridftpServer = GridftpServer::FindOrCreateServer( jobProxy );

		// TODO It would be nice to register only after going through
		//   GM_CLEAR_REQUEST, so that a ATTR_GRIDFTP_URL_BASE from a
		//   previous submission isn't requested here.
	gridftpServer->RegisterClient( evaluateStateTid, buff[0] ? buff : NULL );

	if ( job_already_submitted &&
		 jobAd->LookupString( ATTR_CREAM_DELEGATION_URI, buff ) ) {

		delegatedCredentialURI = strdup( buff );
		myResource->registerDelegationURI( delegatedCredentialURI, jobProxy );
	}

	if ( job_already_submitted ) {
		jobAd->LookupString( ATTR_CREAM_UPLOAD_URL, &uploadUrl );
		jobAd->LookupString( ATTR_CREAM_LEASE_ID, &leaseId );
	}

	gahpErrorString = "";

	if ( jobAd->LookupString(ATTR_JOB_IWD, iwd) && iwd.Length() ) {
		int len = iwd.Length();
		if ( len > 1 && iwd[len - 1] != '/' ) {
			iwd += "/";
		}
	} else {
		iwd = "/";
	}

	buff2 = "";
	if ( jobAd->LookupString(ATTR_JOB_OUTPUT, job_output) && job_output.Length() &&
		 strcmp( job_output.Value(), NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_OUTPUT, bool_value ) ||
			 bool_value ) {

			if ( job_output[0] != '/' ) {
				buff2 = iwd;
			}

			buff2 += job_output;
			localOutput = strdup( buff2.Value() );

			bool_value = 0;
			jobAd->LookupBool( ATTR_STREAM_OUTPUT, bool_value );
			streamOutput = (bool_value != 0);
			stageOutput = !streamOutput;
		}
	}

	buff2 = "";
	if ( jobAd->LookupString(ATTR_JOB_ERROR, job_error) && job_error.Length() &&
		 strcmp( job_error.Value(), NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_ERROR, bool_value ) ||
			 bool_value ) {

			if ( job_error[0] != '/' ) {
				buff2 = iwd;
			}

			buff2 += job_error;
			localError = strdup( buff2.Value() );

			bool_value = 0;
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

CreamJob::~CreamJob()
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
		ReleaseProxy( jobProxy, (Eventcpp)&BaseJob::SetEvaluateState, this );
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
	free( leaseId );
}

void CreamJob::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

int CreamJob::doEvaluateState()
{
	int old_gm_state;
	MyString old_remote_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	bool attr_exists;
	bool attr_dirty;
	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );
	dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, creamState %s\n",
			procID.cluster,procID.proc,GMStateNames[gmState],
			remoteState.Value());

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
				
				probeNow = true;

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
			// Create a lease id on the cream server for this job
			if ( condorState == REMOVED || condorState == HELD ) {
				myResource->CancelSubmit(this);
				gmState = GM_UNSUBMITTED;
				break;
			}
			if ( leaseId == NULL ) {
				// Create an ID unique to this job

				// get condor pool name
				// In case there are multiple collectors, strip out the spaces
				// If there's no collector, insert a dummy name
				char* pool_name = param( "COLLECTOR_HOST" );
				if ( pool_name ) {
					StringList collectors( pool_name );
					free( pool_name );
					pool_name = collectors.print_to_string();
				} else {
					pool_name = strdup( "NoPool" );
				}

				// use "ATTR_GLOBAL_JOB_ID" to get unique global job id
				MyString job_id;
				jobAd->LookupString( ATTR_GLOBAL_JOB_ID, job_id );

				MyString buf;
				buf.sprintf( "Condor_%s_%s", pool_name, job_id.Value() );
				leaseId = strdup( buf.Value() );

				jobAd->Assign( ATTR_CREAM_LEASE_ID, leaseId );

				free( pool_name );
			}

			if ( jmLifetime == 0 ) {
				int new_lease;
				if ( CalculateJobLease( jobAd, new_lease,
										DEFAULT_LEASE_DURATION ) == false ) {
					dprintf( D_ALWAYS, "(%d.%d) No lease for cream job!?\n",
							 procID.cluster, procID.proc );
					jmLifetime = now + DEFAULT_LEASE_DURATION;
				} else {
					jmLifetime = new_lease;
				}
			}

			time_t server_lease = jmLifetime;
			rc = gahp->cream_set_lease( resourceManagerString,
										leaseId, server_lease );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}

			if ( rc != GLOBUS_SUCCESS ) {
				// Unhandled error
				LOG_CREAM_ERROR( "cream_set_lease()", rc );
				gahpErrorString = gahp->getErrorString();
				myResource->CancelSubmit( this );
				gmState = GM_HOLD;
				break;
			}

			if ( server_lease != jmLifetime ) {
				dprintf( D_ALWAYS, "(%d.%d) Server changed lease time from %d to %d\n",
						 procID.cluster, procID.proc, jmLifetime,
						 server_lease );
				jmLifetime = server_lease;
			}

			gmState = GM_SUBMIT;
		} break;
		case GM_SUBMIT: {
			// Start a new cream submission for this job.
			char *job_id = NULL;
			char *upload_url = NULL;
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
										myResource->getDelegationService(),
										delegatedCredentialURI,
										creamAd, leaseId,
										&job_id, &upload_url );
				
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
					gmState = GM_SUBMIT_SAVE;				
					
					UpdateJobLeaseSent(jmLifetime);
					
				} else {
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
				gmState = GM_CANCEL;
			} else {
				jobAd->GetDirtyFlag( ATTR_GRID_JOB_ID, &attr_exists, &attr_dirty );
				if ( attr_exists && attr_dirty ) {
					requestScheddUpdate( this, true );
					break;
				}
				gmState = GM_SUBMIT_COMMIT;
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
					// unhandled error
					LOG_CREAM_ERROR( "cream_job_start()", rc );
					gahpErrorString = gahp->getErrorString();
					gmState = GM_CLEAR_REQUEST;
						//gmState = GM_CANCEL;
				} else {
						// We don't want an old or zeroed lastProbeTime
						// make us do a probe immediately after submitting
						// the job, so set it to now
					lastProbeTime = time(NULL);
					gmState = GM_SUBMITTED;
				}
			}
			} break;
		case GM_SUBMITTED: {
			// The job has been submitted (or is about to be by the
			// jobmanager). Wait for completion or failure, and probe the
			// jobmanager occassionally to make it's still alive.
			if ( remoteState == CREAM_JOB_STATE_DONE_OK ) {
				gmState = GM_DONE_SAVE;
			} else if ( remoteState == CREAM_JOB_STATE_DONE_FAILED || remoteState == CREAM_JOB_STATE_ABORTED ) {
				gmState = GM_PURGE;
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

				int new_lease;	// CalculateJobLease needs an int
				if ( CalculateJobLease( jobAd, new_lease,
										DEFAULT_LEASE_DURATION ) ) {
					jmLifetime = new_lease;
					gmState = GM_EXTEND_LIFETIME;
					break;
				}

				if ( probeNow || remoteState == CREAM_JOB_STATE_UNSET ) {
					lastProbeTime = 0;
					probeNow = false;
				}

				if ( now >= lastProbeTime + probeInterval ) {
					gmState = GM_POLL_JOB_STATE;
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
				time_t server_lease = jmLifetime;

				rc = gahp->cream_set_lease (resourceManagerString, leaseId, server_lease );

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_CREAM_ERROR("cream_set_lease()",rc);
					gahpErrorString = gahp->getErrorString();
					gmState = GM_CANCEL;
					break;
				}
				jmLifetime = server_lease;

				UpdateJobLeaseSent( jmLifetime );
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_POLL_JOB_STATE: {
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
				lastProbeTime = time(NULL);

				if ( remoteState != CREAM_JOB_STATE_DONE_OK && 
					 remoteState != CREAM_JOB_STATE_DONE_FAILED && 
					 remoteState != CREAM_JOB_STATE_ABORTED) {
				   
					int new_lease;	// CalculateJobLease needs an int
					if ( CalculateJobLease( jobAd, new_lease,
											DEFAULT_LEASE_DURATION ) ) {
						jmLifetime = new_lease;
						gmState = GM_EXTEND_LIFETIME;
						break;
					}
				}
				
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
			rc = gahp->cream_job_purge( resourceManagerString, remoteJobId );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_CREAM_ERROR( "cream_job_purge()", rc );
				gahpErrorString = gahp->getErrorString();
				gmState = GM_CLEAR_REQUEST;
//				gmState = GM_CANCEL;
				break;
			}
			myResource->CancelSubmit( this );
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				SetRemoteJobId( NULL );
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
			if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
				LOG_CREAM_ERROR( "cream_job_purge", rc );
				gahpErrorString = gahp->getErrorString();
				gmState = GM_CLEAR_REQUEST;
				break;
			}

			SetRemoteJobId( NULL );
			myResource->CancelSubmit( this );
			remoteState = CREAM_JOB_STATE_UNSET;
			SetRemoteJobStatus( NULL );
			requestScheddUpdate( this, false );

			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
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
					// unhandled error
				LOG_CREAM_ERROR( "cream_job_purge", rc );
				gahpErrorString = gahp->getErrorString();
				gmState = GM_CLEAR_REQUEST;
				break;
			}

			SetRemoteJobId( NULL );
			myResource->CancelSubmit( this );
			remoteState = CREAM_JOB_STATE_UNSET;
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
			jmLifetime = 0;
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
			MyString val;
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
			jobAd->ResetExpr();
			if ( jobAd->NextDirtyExpr() ) {
				requestScheddUpdate( this, true );
				break;
			}
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
					 !remoteStateFaultString.IsEmpty() ) {

					snprintf( holdReason, 1024, "CREAM error: %s",
							  remoteStateFaultString.Value() );
				}
				if ( holdReason[0] == '\0' && !gahpErrorString.IsEmpty() ) {
					snprintf( holdReason, 1024, "CREAM error: %s",
							  gahpErrorString.Value() );
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
					old_remote_state.Value(),
					remoteState.Value());
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
		}
	} while ( reevaluate_state );

		//end of evaluateState loop
		
	return TRUE;
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

	MyString full_job_id;
	if ( job_id ) {
		full_job_id.sprintf( "cream %s %s", resourceManagerString, job_id );
	}
	BaseJob::SetRemoteJobId( full_job_id.Value() );
}

void CreamJob::NewCreamState( const char *new_state, int exit_code,
							  const char *failure_reason )
{
	MyString new_state_str = new_state;

		// TODO verify that the string is a valid state name

	SetRemoteJobStatus( new_state );

	if ( new_state_str != remoteState ) {
		dprintf( D_FULLDEBUG, "(%d.%d) cream state change: %s -> %s\n",
				 procID.cluster, procID.proc, remoteState.Value(),
				 new_state_str.Value() );

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
		SetRemoteJobStatus( remoteState.Value() );

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
	const char *ATTR_OUTPUT_SB_DEST_URI = "OutputSandboxDestURI";
	const char *ATTR_VIR_ORG = "VirtualOrganization";
	const char *ATTR_BATCH_SYSTEM = "BatchSystem";
	const char *ATTR_QUEUE_NAME = "QueueName";
	
	ClassAd submitAd;

	MyString tmp_str = "";
	MyString tmp_str2 = "";
	MyString buf = "";
	MyString iwd_str = "";
	MyString gridftp_url = "";
	StringList isb;
	StringList osb;
	StringList osb_url;
	bool result;

		// Once we add streaming support, remove this
	if ( streamOutput || streamError ) {
		errorString = "Streaming not supported";
		return NULL;
	}

		//IWD
	jobAd->LookupString(ATTR_JOB_IWD, iwd_str);
	if ( jobAd->LookupString(ATTR_JOB_IWD, iwd_str)) {
		int len = iwd_str.Length();
		if ( len > 1 && iwd_str[len - 1] != '/' ) {
			iwd_str += '/';
		}
	} else {
		iwd_str = '/';
	}
	
		//Gridftp server to use with CREAM
	if(!jobAd->LookupString(ATTR_GRIDFTP_URL_BASE, gridftp_url)){
		errorString.sprintf( "%s not defined", ATTR_GRIDFTP_URL_BASE );
		return NULL;
	}
	
		//EXECUTABLE can either be STAGED or TRANSFERED
	result = true;
	jobAd->LookupBool(ATTR_TRANSFER_EXECUTABLE, result);
	if (result) { //TRANSFERED
		
			//here, JOB_CMD = full path to executable
		jobAd->LookupString(ATTR_JOB_CMD, tmp_str);
		tmp_str = gridftp_url + tmp_str;
		isb.insert(tmp_str.Value());

			//CREAM only accepts absolute path | simple filename only
		if (tmp_str[0] != '/') { //not absolute path

				//get simple filename
			StringList strlist(tmp_str.Value(), "/");
			strlist.rewind();
			for(int i = 0; i < strlist.number(); i++) 
				tmp_str = strlist.next();
		}

		buf.sprintf("%s = \"%s\"", ATTR_EXECUTABLE, tmp_str.Value());
		submitAd.Insert(buf.Value());
	}
	else { //STAGED
		jobAd->LookupString(ATTR_JOB_CMD, tmp_str);
		buf.sprintf("%s = \"%s\"", ATTR_EXECUTABLE, tmp_str.Value());
		submitAd.Insert(buf.Value());
	}

		//ARGUMENTS
	if (jobAd->LookupString(ATTR_JOB_ARGUMENTS1, tmp_str)) {
		buf.sprintf("%s = \"%s\"", ATTR_ARGS, tmp_str.Value());
		submitAd.Insert(buf.Value());
	}
	
		//STDINPUT can be either be STAGED or TRANSFERED
	result = true;
	jobAd->LookupBool(ATTR_TRANSFER_INPUT, result);
	if (result) { //TRANSFERED
		
		if (jobAd->LookupString(ATTR_JOB_INPUT, tmp_str)) {

			if (tmp_str[0] != '/')  //not absolute path
				tmp_str2 = gridftp_url + iwd_str + tmp_str;
			else 
				tmp_str2 = gridftp_url + tmp_str;
			
			isb.insert(tmp_str2.Value());
			
				//get simple filename
			StringList strlist(tmp_str.Value(), "/");
			strlist.rewind();
			for(int i = 0; i < strlist.number(); i++) 
				tmp_str = strlist.next();
		}
			buf.sprintf("%s = \"%s\"", ATTR_STD_INPUT, tmp_str.Value());
			submitAd.Insert(buf.Value());
	}
	else { //STAGED. Be careful, if stdin is not found in WN, job will not
			//complete successfully.
		if (jobAd->LookupString(ATTR_JOB_INPUT, tmp_str)) {
			if (tmp_str[0] == '/') { //Only add absolute path
				buf.sprintf("%s = \"%s\"", ATTR_STD_INPUT, tmp_str.Value());
				submitAd.Insert(buf.Value());
			}
		}
	}
		
		//TRANSFER INPUT FILES
	if (jobAd->LookupString(ATTR_TRANSFER_INPUT_FILES, tmp_str)) {
		StringList strlist(tmp_str.Value());
		strlist.rewind();
		
		for (int i = 0; i < strlist.number(); i++) {
			tmp_str = strlist.next();

			if (tmp_str[0] != '/')  //not absolute path
				tmp_str2 = gridftp_url + iwd_str + tmp_str;
			else 
				tmp_str2 = gridftp_url + tmp_str;

			isb.insert(tmp_str2.Value());
		}
	}

		//TRANSFER OUTPUT FILES: handle absolute ?
	if (jobAd->LookupString(ATTR_TRANSFER_OUTPUT_FILES, tmp_str)) {
		char *filename;
		char *remaps = NULL;
		MyString new_name;
		jobAd->LookupString( ATTR_TRANSFER_OUTPUT_REMAPS, &remaps );

		StringList output_files(tmp_str.Value());
		output_files.rewind();
		while ( (filename = output_files.next()) != NULL ) {

			osb.insert( filename );

			if ( remaps && filename_remap_find( remaps, filename,
												new_name ) ) {
				buf.sprintf( "%s%s%s", gridftp_url.Value(),
							 new_name[0] == '/' ? "" : iwd_str.Value(),
							 new_name.Value() );
			} else {
				buf.sprintf( "%s%s%s", gridftp_url.Value(),
							 iwd_str.Value(),
							 condor_basename( filename ) );
			}
			osb_url.insert( buf.Value() );
		}

		free( remaps );
	}
	
		//STDOUTPUT TODO: handle absolute ?
	if (jobAd->LookupString(ATTR_JOB_OUTPUT, tmp_str)) {

		result = true;
		jobAd->LookupBool(ATTR_TRANSFER_OUTPUT, result);

		if (result) {
			buf.sprintf("%s = \"%s\"", ATTR_STD_OUTPUT,
						condor_basename(tmp_str.Value()));
			submitAd.Insert(buf.Value());

			osb.insert(condor_basename(tmp_str.Value()));

			buf.sprintf("%s%s%s", gridftp_url.Value(),
						tmp_str[0] == '/' ? "" : iwd_str.Value(),
						tmp_str.Value());

			osb_url.insert(buf.Value());
		} else {
			buf.sprintf("%s = \"%s\"", ATTR_STD_OUTPUT, tmp_str.Value());
			submitAd.Insert(buf.Value());
		}
	}

		//STDERROR TODO: handle absolute ?
	if (jobAd->LookupString(ATTR_JOB_ERROR, tmp_str)) {

		result = true;
		jobAd->LookupBool(ATTR_TRANSFER_ERROR, result);

		if (result) {
			buf.sprintf("%s = \"%s\"", ATTR_STD_ERROR,
						condor_basename(tmp_str.Value()));
			submitAd.Insert(buf.Value());

			osb.insert(condor_basename(tmp_str.Value()));

			buf.sprintf("%s%s%s", gridftp_url.Value(),
						tmp_str[0] == '/' ? "" : iwd_str.Value(),
						tmp_str.Value());

			osb_url.insert(buf.Value());
		} else {
			buf.sprintf("%s = \"%s\"", ATTR_STD_ERROR, tmp_str.Value());
			submitAd.Insert(buf.Value());
		}
	}

		//VIRTUALORGANISATION. CREAM requires this attribute, but it doesn't
		//need to have a value
		// TODO This needs to be extracted from the VOMS extension in the
		//   job's credential.
//	buf.sprintf("%s = \"%s\"", ATTR_VIR_ORG, "");
	buf.sprintf("%s = \"%s\"", ATTR_VIR_ORG, "ignored");
	submitAd.Insert(buf.Value());
	
		//BATCHSYSTEM
	buf.sprintf("%s = \"%s\"", ATTR_BATCH_SYSTEM, resourceBatchSystemString);
	submitAd.Insert(buf.Value());
	
		//QUEUENAME
	buf.sprintf("%s = \"%s\"", ATTR_QUEUE_NAME, resourceQueueString);
	submitAd.Insert(buf.Value());
	

	MyString ad_string;

	NewClassAdUnparser unparser;
	unparser.SetUseCompactSpacing( true );
	unparser.SetOutputType( false );
	unparser.SetOutputTargetType( false );
	unparser.Unparse( &submitAd, ad_string );

		// Attributes that use new ClassAd lists have to be manually
		// inserted after unparsing the ad.

		//INPUT SANDBOX
	if (isb.number() > 0) {
		buf.sprintf("%s = {", ATTR_INPUT_SB);
		isb.rewind();
		for (int i = 0; i < isb.number(); i++) {
			if (i == 0)
				buf.sprintf_cat("\"%s\"", isb.next());
			else
				buf.sprintf_cat(",\"%s\"", isb.next());
		}
		buf.sprintf_cat("}; ]");

		int insert_pos = strrchr( ad_string.Value(), ']' ) - ad_string.Value();
		ad_string.replaceString( "]", buf.Value(), insert_pos );
	}

		//OUTPUT SANDBOX
	if (osb.number() > 0) {
		buf.sprintf("%s = {", ATTR_OUTPUT_SB);
		osb.rewind();
		for (int i = 0; i < osb.number(); i++) {
			if (i == 0)
				buf.sprintf_cat("\"%s\"", osb.next());
			else
				buf.sprintf_cat(",\"%s\"", osb.next());
		}
		buf.sprintf_cat("}; ]");

		int insert_pos = strrchr( ad_string.Value(), ']' ) - ad_string.Value();
		ad_string.replaceString( "]", buf.Value(), insert_pos );
	}

		//OUTPUT SANDBOX DEST URI
	if (osb_url.number() > 0) {
		buf.sprintf("%s = {", ATTR_OUTPUT_SB_DEST_URI);
		osb_url.rewind();
		for (int i = 0; i < osb_url.number(); i++) {
			if (i == 0)
				buf.sprintf_cat("\"%s\"", osb_url.next());
			else
				buf.sprintf_cat(",\"%s\"", osb_url.next());
		}
		buf.sprintf_cat("}; ]");

		int insert_pos = strrchr( ad_string.Value(), ']' ) - ad_string.Value();
		ad_string.replaceString( "]", buf.Value(), insert_pos );
	}

		//ENVIRONMENT
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

	if ( env_vec[0] ) {
		buf.sprintf( "%s = {", ATTR_JOB_ENVIRONMENT2 );

		for ( int i = 0; env_vec[i]; i++ ) {
			if ( i == 0 ) {
				buf.sprintf_cat( "\"%s\"", env_vec[i] );
			} else {
				buf.sprintf_cat( ",\"%s\"", env_vec[i] );
			}
		}
		buf.sprintf_cat( "}; ]" );

		int insert_pos = strrchr( ad_string.Value(), ']' ) - ad_string.Value();
		ad_string.replaceString( "]", buf.Value(), insert_pos );
	}
	deleteStringArray(env_vec);

/*
	dprintf(D_FULLDEBUG, "SUBMITAD:\n%s\n",ad_string.Value()); 
*/
	return strdup( ad_string.Value() );
}
