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
#include "condor_string.h"	// for strnewp and friends
#include "condor_daemon_core.h"
#include "condor_config.h"
#include "condor_transfer_request.h"
#include "condor_version.h"

#include "gridmanager.h"
#include "infnbatchjob.h"


// GridManager job states
#define GM_INIT					0
#define GM_UNSUBMITTED			1
#define GM_SUBMIT				2
#define GM_SUBMIT_SAVE			3
#define GM_SUBMITTED			4
#define GM_DONE_SAVE			5
#define GM_DONE_COMMIT			6
#define GM_CANCEL				7
#define GM_DELETE				8
#define GM_CLEAR_REQUEST		9
#define GM_HOLD					10
#define GM_REFRESH_PROXY		11
#define GM_START				12
#define GM_POLL_ACTIVE			13
#define GM_SAVE_SANDBOX_ID		14
#define GM_TRANSFER_INPUT_BEGIN	15
#define GM_TRANSFER_INPUT_FINISH	16
#define GM_TRANSFER_OUTPUT_BEGIN	17
#define GM_TRANSFER_OUTPUT_FINISH	18
#define GM_DELETE_SANDBOX		19

static const char *GMStateNames[] = {
	"GM_INIT",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_DONE_COMMIT",
	"GM_CANCEL",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_REFRESH_PROXY",
	"GM_START",
	"GM_POLL_ACTIVE",
	"GM_SAVE_SANDBOX_ID",
	"GM_TRANSFER_INPUT_BEGIN",
	"GM_TRANSFER_INPUT_FINISH",
	"GM_TRANSFER_OUTPUT_BEGIN",
	"GM_TRANSFER_OUTPUT_FINISH",
	"GM_DELETE_SANDBOX",
};

#define JOB_STATE_UNKNOWN				-1
#define JOB_STATE_UNSUBMITTED			0

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define LOG_GLOBUS_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, remoteState %d: %s returned error %d\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],remoteState, \
        func,error)

class TDMgrMgr : public Service {
public:
	TdAction td_register_callback(TransferDaemon *);
	TdAction td_reaper_callback(long, int, TransferDaemon *);

	TransferDaemon *get_td( const char *name = NULL );

	std::string m_err_msg;
	TDMan td_mgr;
};

/* A callback notification from the TDMan object that we get when the
 * registration of a transferd is complete and the transferd is considered
 * open for business
 */
TdAction
TDMgrMgr::td_register_callback(TransferDaemon *)
{
	dprintf(D_FULLDEBUG, "td_register_callback() called\n");

	// TODO notify job objects waiting for transferd

	return TD_ACTION_CONTINUE;
}

/* A callback notification from the TDMan object we get when the 
 * transferd has died.
 */
TdAction
TDMgrMgr::td_reaper_callback(long, int, TransferDaemon *)
{
	dprintf(D_FULLDEBUG, "td_reaper_callback() called\n");

	// TODO Anything else we should do here?

	return TD_ACTION_TERMINATE;
}

TDMgrMgr td_mgr_mgr;

TransferDaemon*
TDMgrMgr::get_td( const char *name )
{
	TransferDaemon *td = NULL;
	if ( name == NULL ) {
		name = myUserName;
	}
	td = td_mgr.find_td_by_user( name );
	if ( td == NULL ) {
		// No td found at all in any state, so start one.
		MyString rand_id;
		MyString desc;

		// XXX fix this rand_id to be dealt with better, like maybe the tdman
		// object assigns it or something.
		rand_id.randomlyGenerateHex(64);
		td = new TransferDaemon(name, rand_id, TD_PRE_INVOKED);
		ASSERT(td != NULL);

		// set up the default registration callback
		desc = "Transferd Registration callback";
		td->set_reg_callback(desc,
			(TDRegisterCallback)
			 	&TDMgrMgr::td_register_callback, &td_mgr_mgr);

		// set up the default reaper callback
		desc = "Transferd Reaper callback";
		td->set_reaper_callback(desc,
			(TDReaperCallback)
				&TDMgrMgr::td_reaper_callback, &td_mgr_mgr);
	
		// Have the td manager object start this td up.
		// XXX deal with failure here a bit better.
		td_mgr.invoke_a_td(td);

		// TODO Should have a backstop for if we can't get the transferd
		//   running. We'd then notify all jobs.
	}
	if ( td->get_status() == TD_REGISTERED ) {
		return td;
	} else {
		return NULL;
	}
}

void INFNBatchJobInit()
{
		// The TDMan needs to register a cedar command
	td_mgr_mgr.td_mgr.register_handlers();
}

void INFNBatchJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "INFN_JOB_POLL_INTERVAL", 5 * 60 );
	INFNBatchJob::setPollInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	INFNBatchJob::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	INFNBatchJob::setConnectFailureRetry( tmp_int );
}

bool INFNBatchJobAdMatch( const ClassAd *job_ad ) {
	int universe;
	std::string resource;
		// CRUFT: grid-type 'blah' is deprecated. Now, the specific batch
		//   system names should be used (pbs, lsf). Glite are the only
		//   people who care about the old value. This changed happend in
		//   Condor 6.7.12.
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 ( strncasecmp( resource.c_str(), "blah", 4 ) == 0 ||
		   strncasecmp( resource.c_str(), "batch", 5 ) == 0 ||
		   strncasecmp( resource.c_str(), "pbs", 3 ) == 0 ||
		   strncasecmp( resource.c_str(), "lsf", 3 ) == 0 ||
		   strncasecmp( resource.c_str(), "nqs", 3 ) == 0 ||
		   strncasecmp( resource.c_str(), "sge", 3 ) == 0 ||
		   strncasecmp( resource.c_str(), "naregi", 6 ) == 0 ) ) {

		return true;
	}
	return false;
}

BaseJob *INFNBatchJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new INFNBatchJob( jobad );
}


int INFNBatchJob::pollInterval = 300;			// default value
int INFNBatchJob::submitInterval = 300;			// default value
int INFNBatchJob::gahpCallTimeout = 300;		// default value
int INFNBatchJob::maxConnectFailures = 3;		// default value

INFNBatchJob::INFNBatchJob( ClassAd *classad )
	: BaseJob( classad )
{
	std::string buff;
	std::string error_string = "";
	char *gahp_path;
	ArgList gahp_args;

	gahpAd = NULL;
	gmState = GM_INIT;
	remoteState = JOB_STATE_UNKNOWN;
	enteredCurrentGmState = time(NULL);
	enteredCurrentRemoteState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	remoteSandboxId = NULL;
	remoteJobId = NULL;
	lastPollTime = 0;
	pollNow = false;
	myResource = NULL;
	gahp = NULL;
	m_xfer_gahp = NULL;
	jobProxy = NULL;
	remoteProxyExpireTime = 0;

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	buff = "";
	jobAd->LookupString( ATTR_GRID_RESOURCE, buff );
	if ( buff != "" ) {
		const char *token;
		Tokenize( buff );

		token = GetNextToken( " ", false );
		if ( !strcmp( "batch", token ) ) {
			token = GetNextToken( " ", false );
		}
		batchType = strdup( token );

		while ( (token = GetNextToken( " ", false )) ) {
			gahp_args.AppendArg( token );
		}
	} else {
		sprintf( error_string, "%s is not set in the job ad",
							  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	buff = "";
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff );
	if ( buff != "" ) {
		const char *token;
		Tokenize( buff );

			// TODO Do we want to handle values from older versions
			//   i.e. ones with no sandbox id?

			// This should be 'batch'
		token = GetNextToken( " ", false );
			// This should be the batch system type
		token = GetNextToken( " ", false );
			// This should be the sandbox id
		if ( (token = GetNextToken( " ", false )) ) {
			SetRemoteSandboxId( token );
		}
			// This should be the batch system job id
		if ( (token = GetNextToken( " ", false )) ) {
			SetRemoteJobId( token );
		}
	} else {
		remoteState = JOB_STATE_UNSUBMITTED;
	}

	strupr( batchType );

	if ( gahp_args.Count() > 0 ) {
		gahp_path = param( "REMOTE_GAHP" );
		if ( gahp_path == NULL ) {
			sprintf( error_string, "REMOTE_GAHP not defined" );
			goto error_exit;
		}
	} else {
		sprintf( buff, "%s_GAHP", batchType );
		gahp_path = param(buff.c_str());
		if ( gahp_path == NULL ) {
			gahp_path = param( "BATCH_GAHP" );
			if ( gahp_path == NULL ) {
				sprintf( error_string, "Neither %s nor %s defined", buff.c_str(),
						 "BATCH_GAHP" );
				goto error_exit;
			}
		}
	}

	buff = batchType;
	if ( gahp_args.Count() > 0 ) {
		sprintf_cat( buff, "/%s", gahp_args.GetArg( 0 ) );
	}
	gahp = new GahpClient( buff.c_str(), gahp_path, &gahp_args );
	free( gahp_path );

	if ( gahp_args.Count() > 0 ) {
		gahp_path = param( "TRANSFER_GAHP" );
		if ( gahp_path == NULL ) {
			sprintf( error_string, "TRANSFER_GAHP not defined" );
			goto error_exit;
		}

		sprintf( buff, "xfer/%s/%s", batchType, gahp_args.GetArg( 0 ) );
		m_xfer_gahp = new GahpClient( buff.c_str(), gahp_path, &gahp_args );
		free( gahp_path );
	}

	myResource = INFNBatchResource::FindOrCreateResource( batchType,
														  gahp_args.GetArg(0) );
	myResource->RegisterJob( this );
	if ( remoteJobId ) {
		myResource->AlreadySubmitted( this );
	}

		// Does this have to be lower-case for SetRemoteJobId()?

	strlwr( batchType );

	ASSERT( gahp != NULL );
	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	ASSERT( gahp != NULL );
	m_xfer_gahp->setNotificationTimerId( evaluateStateTid );
	m_xfer_gahp->setMode( GahpClient::normal );
	// TODO: This can't be the normal gahp timeout value. Does it need to
	//   be configurable?
	m_xfer_gahp->setTimeout( 60*60 );

	jobProxy = AcquireProxy( jobAd, error_string,
							 (TimerHandlercpp)&BaseJob::SetEvaluateState, this );
	if ( jobProxy == NULL && error_string != "" ) {
		goto error_exit;
	}

	return;

 error_exit:
		// We must ensure that the code-path from GM_HOLD doesn't depend
		// on any initialization that's been skipped.
	gmState = GM_HOLD;
	if ( !error_string.empty() ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string.c_str() );
	}
	return;
}

INFNBatchJob::~INFNBatchJob()
{
	if ( jobProxy != NULL ) {
		ReleaseProxy( jobProxy, (TimerHandlercpp)&BaseJob::SetEvaluateState, this );
	}
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	if ( batchType != NULL ) {
		free( batchType );
	}
	free( remoteSandboxId );
	if ( remoteJobId != NULL ) {
		free( remoteJobId );
	}
	if ( gahpAd ) {
		delete gahpAd;
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
	delete m_xfer_gahp;
}

void INFNBatchJob::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

void INFNBatchJob::doEvaluateState()
{
	int old_gm_state;
	int old_remote_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	bool attr_exists;
	bool attr_dirty;
	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, remoteState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],remoteState);

	if ( gahp ) {
		gahp->setMode( GahpClient::normal );
	}

	do {
		reevaluate_state = false;
		old_gm_state = gmState;
		old_remote_state = remoteState;

		ASSERT ( gahp != NULL || gmState == GM_HOLD || gmState == GM_DELETE );

		switch ( gmState ) {
		case GM_INIT: {
			// This is the state all jobs start in when the GlobusJob object
			// is first created. Here, we do things that we didn't want to
			// do in the constructor because they could block (the
			// constructor is called while we're connected to the schedd).
			if ( gahp->Startup() == false ) {
				dprintf( D_ALWAYS, "(%d.%d) Error starting GAHP\n",
						 procID.cluster, procID.proc );

				jobAd->Assign( ATTR_HOLD_REASON, "Failed to start GAHP" );
				gmState = GM_HOLD;
				break;
			}
			if ( myResource->GahpIsRemote() ) {
				// This job requires a transfer gahp
				ASSERT( m_xfer_gahp );
				if ( m_xfer_gahp->Startup() == false ) {
					dprintf( D_ALWAYS, "(%d.%d) Error starting transfer GAHP\n",
							 procID.cluster, procID.proc );

					jobAd->Assign( ATTR_HOLD_REASON, "Failed to start transfer GAHP" );
					gmState = GM_HOLD;
					break;
				}
				// This job requires a transferd
				TransferDaemon *td = td_mgr_mgr.get_td();
				if ( td == NULL ) {
					if ( !td_mgr_mgr.m_err_msg.empty() ) {
						std::string err;
						sprintf( err, "Error starting transferd: %s",
								 td_mgr_mgr.m_err_msg.c_str() );
						dprintf( D_ALWAYS, "(%d.%d) %s\n", procID.cluster,
								 procID.proc, err.c_str() );
						jobAd->Assign( ATTR_HOLD_REASON, err );
						gmState = GM_HOLD;
						break;
					}
					// Transferd isn't ready yet, but should be soon.
					break;
				}
			}
			if ( jobProxy ) {
				if ( gahp->Initialize( jobProxy ) == false ) {
					dprintf( D_ALWAYS, "(%d.%d) Error initializing GAHP\n",
							 procID.cluster, procID.proc );

					jobAd->Assign( ATTR_HOLD_REASON,
								   "Failed to initialize GAHP" );
					gmState = GM_HOLD;
					break;
				}

				gahp->setDelegProxy( jobProxy );
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
			if ( condorState == COMPLETED ) {
				gmState = GM_DONE_COMMIT;
			} else if ( remoteJobId == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else {
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_UNSUBMITTED: {
			// There are no outstanding remote submissions for this job (if
			// there is one, we've given up on it).
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else if ( condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			} else if ( condorState == COMPLETED ) {
				gmState = GM_DELETE;
			} else {
				gmState = GM_SAVE_SANDBOX_ID;
			}
			} break;
		case GM_SAVE_SANDBOX_ID: {
			// Create and save sandbox id for bosco
			if ( remoteSandboxId == NULL ) {
				CreateSandboxId();
			}
			jobAd->GetDirtyFlag( ATTR_GRID_JOB_ID, &attr_exists, &attr_dirty );
			if ( attr_exists && attr_dirty ) {
				requestScheddUpdate( this, true );
				break;
			}
			gmState = GM_TRANSFER_INPUT_BEGIN;
		} break;
		case GM_TRANSFER_INPUT_BEGIN: {
			// Transfer input sandbox
			if ( !myResource->GahpIsRemote() ) {
				gmState = GM_SUBMIT;
				break;
			}
			// First, send request to the local transferd
			if ( !StartTransferRequest( true ) ) {
				dprintf( D_ALWAYS, "(%d.%d) Failed to send transfer request to transferd: %s\n",
						 procID.cluster, procID.proc, errorString.c_str() );
				gmState = GM_HOLD;
				break;
			}
			gmState = GM_TRANSFER_INPUT_FINISH;
		} break;
		case GM_TRANSFER_INPUT_FINISH: {
			// Transfer input sandbox
			// Now, send request to the remote transfer gahp
			if ( gahpAd == NULL ) {
				gahpAd = buildTransferAd();
			}
			if ( gahpAd == NULL ) {
				gmState = GM_HOLD;
				break;
			}

			rc = m_xfer_gahp->blah_download_sandbox( gahpAd );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != 0 ) {
				// Failure!
				dprintf( D_ALWAYS,
						 "(%d.%d) blah_download_sandbox() failed: %s\n",
						 procID.cluster, procID.proc,
						 gahp->getErrorString() );
				errorString = gahp->getErrorString();
				gmState = GM_CLEAR_REQUEST;
			} else {
				gmState = GM_SUBMIT;
			}
		} break;
		case GM_SUBMIT: {
			// Start a new remote submission for this job.

			// Can't break in the middle of a submit, because the job will
			// end up running if the submit succeeds
			/*
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_UNSUBMITTED;
				break;
			}
			*/
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
				jobAd->Assign( ATTR_HOLD_REASON,
							   "Attempts to submit failed" );
				gmState = GM_HOLD;
				break;
			}
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {
				char *job_id_string = NULL;

				// Once RequestSubmit() is called at least once, you must
				// CancelRequest() once you're done with the request call
				if ( myResource->RequestSubmit( this ) == false ) {
					break;
				}

				if ( gahpAd == NULL ) {
					gahpAd = buildSubmitAd();
				}
				if ( gahpAd == NULL ) {
					gmState = GM_HOLD;
					break;
				}

				rc = gahp->blah_job_submit( gahpAd, &job_id_string );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;
				if ( rc == GLOBUS_SUCCESS ) {
					SetRemoteJobId( job_id_string );
					if(jobProxy) {
						remoteProxyExpireTime = jobProxy->expiration_time;
					}
					WriteGridSubmitEventToUserLog( jobAd );
					gmState = GM_SUBMIT_SAVE;
				} else {
					// unhandled error
					dprintf( D_ALWAYS,
							 "(%d.%d) blah_job_submit() failed: %s\n",
							 procID.cluster, procID.proc,
							 gahp->getErrorString() );
					errorString = gahp->getErrorString();
					myResource->CancelSubmit( this );
					gmState = GM_DELETE_SANDBOX;
					reevaluate_state = true;
				}
				if ( job_id_string != NULL ) {
					free( job_id_string );
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
			// Save the job id for a new remote submission.
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
			// The job has been submitted. Wait for completion or failure,
			// and poll the remote schedd occassionally to let it know
			// we're still alive.
			// The resetting of the evaluateStateTid timer should be the
			// last thing we do, after checking everything of interest.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else if ( remoteState == COMPLETED ) {
				gmState = GM_TRANSFER_OUTPUT_BEGIN;
			} else if ( remoteState == REMOVED ) {
				errorString = "Job removed from batch queue manually";
				SetRemoteJobId( NULL );
				gmState = GM_HOLD;
			} else if ( jobProxy && remoteProxyExpireTime < jobProxy->expiration_time ) {
					gmState = GM_REFRESH_PROXY;
			} else {
				if ( lastPollTime < enteredCurrentGmState ) {
					lastPollTime = enteredCurrentGmState;
				}
				if ( pollNow ) {
					lastPollTime = 0;
					pollNow = false;
				}
				if ( now >= lastPollTime + pollInterval ) {
					gmState = GM_POLL_ACTIVE;
					break;
				}
				unsigned int delay = 0;
				if ( (lastPollTime + pollInterval) > now ) {
					delay = (lastPollTime + pollInterval) - now;
				}
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_POLL_ACTIVE: {
			ClassAd *status_ad = NULL;
			rc = gahp->blah_job_status( remoteJobId, &status_ad );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				dprintf( D_ALWAYS,
						 "(%d.%d) blah_job_status() failed: %s\n",
						 procID.cluster, procID.proc, gahp->getErrorString() );
				errorString = gahp->getErrorString();
				gmState = GM_CANCEL;
				break;
			}
			ProcessRemoteAd( status_ad );
			delete status_ad;
			lastPollTime = time(NULL);
			gmState = GM_SUBMITTED;
			} break;
		case GM_REFRESH_PROXY: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_SUBMITTED;
			} else {
				// We should never end up here if we don't have a
				// proxy already!
				if( ! jobProxy ) {
					EXCEPT( "(%d.%d) Requested to refresh proxy, but no proxy present. ", procID.cluster,procID.proc);
				}

				rc = gahp->blah_job_refresh_proxy( remoteJobId,
												   jobProxy->proxy_filename );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					dprintf( D_ALWAYS,
							 "(%d.%d) blah_job_refresh_proxy() failed: %s\n",
							 procID.cluster, procID.proc, gahp->getErrorString() );
					errorString = gahp->getErrorString();
					gmState = GM_CANCEL;
					break;
				}
				remoteProxyExpireTime = jobProxy->expiration_time;
				gmState = GM_SUBMITTED;
			}
		} break;
		case GM_TRANSFER_OUTPUT_BEGIN: {
			if ( !myResource->GahpIsRemote() ) {
				gmState = GM_DONE_SAVE;
				break;
			}
			// Transfer output sandbox
			// First, send request to the local transferd
			if ( !StartTransferRequest( false ) ) {
				dprintf( D_ALWAYS, "(%d.%d) Failed to send transfer request to transferd: %s\n",
						 procID.cluster, procID.proc, errorString.c_str() );
				gmState = GM_HOLD;
				break;
			}
			gmState = GM_TRANSFER_OUTPUT_FINISH;
		} break;
		case GM_TRANSFER_OUTPUT_FINISH: {
			// Transfer output sandbox
			if ( gahpAd == NULL ) {
				gahpAd = buildTransferAd();
			}
			if ( gahpAd == NULL ) {
				gmState = GM_HOLD;
				break;
			}

			rc = m_xfer_gahp->blah_upload_sandbox( gahpAd );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != 0 ) {
				// Failure!
				dprintf( D_ALWAYS,
						 "(%d.%d) blah_upload_sandbox() failed: %s\n",
						 procID.cluster, procID.proc,
						 gahp->getErrorString() );
				errorString = gahp->getErrorString();
				gmState = GM_DELETE_SANDBOX;
			} else {
				gmState = GM_DONE_SAVE;
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
			// Tell the remote schedd it can remove the job from the queue.

			// Nothing to do for this job type
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				// noop
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				SetRemoteIds( NULL, NULL );
				requestScheddUpdate( this, false );
				myResource->CancelSubmit( this );
			}
			gmState = GM_DELETE_SANDBOX;
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.

			// Should this if-stmt be here? Even if the job is completed,
			// we may still want to remove it (say if we have trouble
			// fetching the output files).
			if ( remoteState != COMPLETED ) {
				rc = gahp->blah_job_cancel( remoteJobId );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					dprintf( D_ALWAYS,
							 "(%d.%d) blah_job_cancel() failed: %s\n",
							 procID.cluster, procID.proc, gahp->getErrorString() );
					errorString = gahp->getErrorString();
					gmState = GM_CLEAR_REQUEST;
					break;
				}
				SetRemoteJobId( NULL );
				myResource->CancelSubmit( this );
			}
			gmState = GM_DELETE_SANDBOX;
			} break;
		case GM_DELETE_SANDBOX: {
			// Delete the remote sandbox
			// TODO implement
			//   send delete command to remote side
			//   wait for completion (possibly in another state)
			SetRemoteSandboxId( NULL );
			if ( condorState == COMPLETED || condorState == REMOVED ) {
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

			// For now, put problem jobs on hold instead of
			// forgetting about current submission and trying again.
			// TODO: Let our action here be dictated by the user preference
			// expressed in the job ad.
			if ( remoteJobId != NULL && condorState != REMOVED ) {
				gmState = GM_HOLD;
				break;
			}
			errorString = "";
			SetRemoteIds( NULL, NULL );
			myResource->CancelSubmit( this );
			JobIdle();
			if ( submitLogged ) {
				JobEvicted();
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
			remoteProxyExpireTime = 0;
			submitLogged = false;
			executeLogged = false;
			submitFailedLogged = false;
			terminateLogged = false;
			abortLogged = false;
			evictLogged = false;
			gmState = GM_UNSUBMITTED;
			remoteState = JOB_STATE_UNSUBMITTED;
			SetRemoteJobStatus( NULL );
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
					strncpy( holdReason, errorString.c_str(),
							 sizeof(holdReason) - 1 );
				}
				if ( holdReason[0] == '\0' ) {
					strncpy( holdReason, "Unspecified gridmanager error",
							 sizeof(holdReason) - 1 );
				}

				JobHeld( holdReason );
			}
			gmState = GM_DELETE;
			} break;
		default:
			EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster,procID.proc,
					gmState );
		}

		if ( gmState != old_gm_state || remoteState != old_remote_state ) {
			reevaluate_state = true;
		}
		if ( remoteState != old_remote_state ) {
//			dprintf(D_FULLDEBUG, "(%d.%d) remote state change: %s -> %s\n",
//					procID.cluster, procID.proc,
//					JobStatusNames(old_remote_state),
//					JobStatusNames(remoteState));
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
			// If we were calling a gahp func that used gahpAd, we're done
			// with it now, so free it.
			if ( gahpAd ) {
				delete gahpAd;
				gahpAd = NULL;
			}
		}

	} while ( reevaluate_state );
}

void INFNBatchJob::SetRemoteSandboxId( const char *sandbox_id )
{
	SetRemoteIds( sandbox_id, remoteJobId );
}

void INFNBatchJob::SetRemoteJobId( const char *job_id )
{
	SetRemoteIds( remoteSandboxId, job_id );
}

void INFNBatchJob::SetRemoteIds( const char *sandbox_id, const char *job_id )
{
	if ( sandbox_id != remoteSandboxId ) {
		free( remoteSandboxId );
		if ( sandbox_id ) {
			remoteSandboxId = strdup( sandbox_id );
		} else {
			remoteSandboxId = NULL;
		}
	}

	if ( job_id != remoteJobId ) {
		free( remoteJobId );
		if ( job_id ) {
			ASSERT( remoteSandboxId );
			remoteJobId = strdup( job_id );
		} else {
			remoteJobId = NULL;
		}
	}

	std::string full_job_id;
	if ( remoteSandboxId ) {
		sprintf( full_job_id, "batch %s %s", batchType, remoteSandboxId );
	}
	if ( remoteJobId ) {
		sprintf_cat( full_job_id, " %s", remoteJobId );
	}
	BaseJob::SetRemoteJobId( full_job_id.c_str() );
}

void INFNBatchJob::ProcessRemoteAd( ClassAd *remote_ad )
{
	int new_remote_state;
	ExprTree *new_expr, *old_expr;

	int index;
	const char *attrs_to_copy[] = {
		ATTR_BYTES_SENT,
		ATTR_BYTES_RECVD,
		ATTR_COMPLETION_DATE,
		ATTR_JOB_RUN_COUNT,
		ATTR_JOB_START_DATE,
		ATTR_ON_EXIT_BY_SIGNAL,
		ATTR_ON_EXIT_SIGNAL,
		ATTR_ON_EXIT_CODE,
		ATTR_EXIT_REASON,
		ATTR_JOB_CURRENT_START_DATE,
		ATTR_JOB_LOCAL_SYS_CPU,
		ATTR_JOB_LOCAL_USER_CPU,
		ATTR_JOB_REMOTE_SYS_CPU,
		ATTR_JOB_REMOTE_USER_CPU,
		ATTR_NUM_CKPTS,
		ATTR_NUM_GLOBUS_SUBMITS,
		ATTR_NUM_MATCHES,
		ATTR_NUM_RESTARTS,
		ATTR_JOB_REMOTE_WALL_CLOCK,
		ATTR_JOB_CORE_DUMPED,
		ATTR_EXECUTABLE_SIZE,
		ATTR_IMAGE_SIZE,
		NULL };		// list must end with a NULL

	if ( remote_ad == NULL ) {
		return;
	}

	dprintf( D_FULLDEBUG, "(%d.%d) ***ProcessRemoteAd\n",
			 procID.cluster, procID.proc );

	remote_ad->LookupInteger( ATTR_JOB_STATUS, new_remote_state );

	if ( new_remote_state == IDLE ) {
		JobIdle();
	}
	if ( new_remote_state == RUNNING ) {
		JobRunning();
	}
	// If the job has been removed locally, don't propagate a hold from
	// the remote schedd. 
	// If HELD is the first job status we get from the remote schedd,
	// assume that it's an old hold that was also reflected in the local
	// schedd and has since been released locally (and should be released
	// remotely as well). This won't always be true, but releasing the
	// remote job anyway shouldn't cause any major trouble.
	if ( new_remote_state == HELD && condorState != REMOVED &&
		 remoteState != JOB_STATE_UNKNOWN ) {
		char *reason = NULL;
		int code = 0;
		int subcode = 0;
		if ( remote_ad->LookupString( ATTR_HOLD_REASON, &reason ) ) {
			remote_ad->LookupInteger( ATTR_HOLD_REASON_CODE, code );
			remote_ad->LookupInteger( ATTR_HOLD_REASON_SUBCODE, subcode );
			JobHeld( reason, code, subcode );
			free( reason );
		} else {
			JobHeld( "held remotely with no hold reason" );
		}
	}
	remoteState = new_remote_state;
	SetRemoteJobStatus( getJobStatusString( remoteState ) );


	index = -1;
	while ( attrs_to_copy[++index] != NULL ) {
		old_expr = jobAd->LookupExpr( attrs_to_copy[index] );
		new_expr = remote_ad->LookupExpr( attrs_to_copy[index] );

		if ( new_expr != NULL && ( old_expr == NULL || !(*old_expr == *new_expr) ) ) {
			jobAd->Insert( attrs_to_copy[index], new_expr->Copy() );
		}
	}

	requestScheddUpdate( this, false );

	return;
}

BaseResource *INFNBatchJob::GetResource()
{
	return (BaseResource *)myResource;
}

ClassAd *INFNBatchJob::buildSubmitAd()
{
	MyString expr;
	ClassAd *submit_ad;
	ExprTree *next_expr;
	bool use_new_args_env = false;

		// Older versions of the blahp don't know about the new
		// Arguments and Environment job attributes. Newer version
		// put "new_esc_format" in their version string to indicate
		// that they understand these new attributes. If we're
		// talking to an old blahp, convert to the old Args and Env
		// attributes.
	if ( strstr( gahp->getVersion(), "new_esc_format" ) ) {
		use_new_args_env = true;
	} else {
		use_new_args_env = false;
	}

	int index;
	const char *attrs_to_copy[] = {
//		ATTR_JOB_CMD,
		ATTR_JOB_ARGUMENTS1,
		ATTR_JOB_ARGUMENTS2,
		ATTR_JOB_ENVIRONMENT1,
		ATTR_JOB_ENVIRONMENT1_DELIM,
		ATTR_JOB_ENVIRONMENT2,
		ATTR_JOB_INPUT,
		ATTR_JOB_OUTPUT,
		ATTR_JOB_ERROR,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_TRANSFER_OUTPUT_FILES,
		ATTR_TRANSFER_OUTPUT_REMAPS,
//		ATTR_REQUIREMENTS,
//		ATTR_RANK,
//		ATTR_OWNER,
//		ATTR_DISK_USAGE,
//		ATTR_IMAGE_SIZE,
//		ATTR_EXECUTABLE_SIZE,
//		ATTR_MAX_HOSTS,
//		ATTR_MIN_HOSTS,
//		ATTR_JOB_PRIO,
		ATTR_JOB_IWD,
		ATTR_X509_USER_PROXY,
		ATTR_GRID_RESOURCE,
		NULL };		// list must end with a NULL

	submit_ad = new ClassAd;

	index = -1;
	while ( attrs_to_copy[++index] != NULL ) {
		if ( ( next_expr = jobAd->LookupExpr( attrs_to_copy[index] ) ) != NULL ) {
			submit_ad->Insert( attrs_to_copy[index], next_expr->Copy() );
		}
	}

	expr = "";
	jobAd->LookupString( ATTR_JOB_REMOTE_IWD, expr );
	if ( !expr.IsEmpty() ) {
		submit_ad->Assign( ATTR_JOB_IWD, expr );
	}

	expr = "";
	jobAd->LookupString( ATTR_BATCH_QUEUE, expr );
	if ( !expr.IsEmpty() ) {
		submit_ad->Assign( "Queue", expr );
	}

	// The blahp expects the Cmd attribute to contain the full pathname
	// of the job executable.
	jobAd->LookupString( ATTR_JOB_CMD, expr );
	if ( expr.FindChar( '/' ) < 0 ) {
		std::string fullpath;
		submit_ad->LookupString( ATTR_JOB_IWD, fullpath );
		sprintf_cat( fullpath, "/%s", expr.Value() );
		submit_ad->Assign( ATTR_JOB_CMD, fullpath );
	} else {
		submit_ad->Assign( ATTR_JOB_CMD, expr );
	}

		// CRUFT: In the current glite code, jobs have a grid-type of 'blah'
		//   and the blahp looks at the attribute "gridtype" for 'pbs' or
		//   'lsf'. Until the blahp changes to look at GridResource, set
		//   'gridtype' for non-glite users.
	if ( strcmp( batchType, "blah" ) != 0 ) {
		submit_ad->Assign( "gridtype", batchType );
	}

	if ( !use_new_args_env ) {
		MyString error_str;
		MyString value_str;
		ArgList args;
		Env env;

		submit_ad->Delete( ATTR_JOB_ARGUMENTS2 );
		submit_ad->Delete( ATTR_JOB_ENVIRONMENT1_DELIM );
		submit_ad->Delete( ATTR_JOB_ENVIRONMENT2 );

		if ( !args.AppendArgsFromClassAd( jobAd, &error_str ) ||
			 !args.GetArgsStringV1Raw( &value_str, &error_str ) ) {
			errorString = error_str.Value();
			delete submit_ad;
			return NULL;
		}
		submit_ad->Assign( ATTR_JOB_ARGUMENTS1, value_str.Value() );

		value_str = "";
		if ( !env.MergeFrom( jobAd, &error_str ) ||
			 !env.getDelimitedStringV1Raw( &value_str, &error_str ) ) {
			errorString = error_str.Value();
			delete submit_ad;
			return NULL;
		}
		submit_ad->Assign( ATTR_JOB_ENVIRONMENT1, value_str.Value() );
	}

//	submit_ad->Assign( ATTR_JOB_STATUS, IDLE );
//submit_ad->Assign( ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_VANILLA );

//	submit_ad->Assign( ATTR_Q_DATE, now );
//	submit_ad->Assign( ATTR_CURRENT_HOSTS, 0 );
//	submit_ad->Assign( ATTR_COMPLETION_DATE, 0 );
//	submit_ad->Assign( ATTR_JOB_REMOTE_WALL_CLOCK, (float)0.0 );
//	submit_ad->Assign( ATTR_JOB_LOCAL_USER_CPU, (float)0.0 );
//	submit_ad->Assign( ATTR_JOB_LOCAL_SYS_CPU, (float)0.0 );
//	submit_ad->Assign( ATTR_JOB_REMOTE_USER_CPU, (float)0.0 );
//	submit_ad->Assign( ATTR_JOB_REMOTE_SYS_CPU, (float)0.0 );
//	submit_ad->Assign( ATTR_JOB_EXIT_STATUS, 0 );
//	submit_ad->Assign( ATTR_NUM_CKPTS, 0 );
//	submit_ad->Assign( ATTR_NUM_RESTARTS, 0 );
//	submit_ad->Assign( ATTR_NUM_SYSTEM_HOLDS, 0 );
//	submit_ad->Assign( ATTR_JOB_COMMITTED_TIME, 0 );
//	submit_ad->Assign( ATTR_COMMITTED_SLOT_TIME, 0 );
//	submit_ad->Assign( ATTR_CUMULATIVE_SLOT_TIME, 0 );
//	submit_ad->Assign( ATTR_TOTAL_SUSPENSIONS, 0 );
//	submit_ad->Assign( ATTR_LAST_SUSPENSION_TIME, 0 );
//	submit_ad->Assign( ATTR_CUMULATIVE_SUSPENSION_TIME, 0 );
//	submit_ad->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );
//	submit_ad->Assign( ATTR_CURRENT_HOSTS, 0 );
//	submit_ad->Assign( ATTR_ENTERED_CURRENT_STATUS, now  );
//	submit_ad->Assign( ATTR_JOB_NOTIFICATION, NOTIFY_NEVER );
//	submit_ad->Assign( ATTR_JOB_LEAVE_IN_QUEUE, true );
//	submit_ad->Assign( ATTR_SHOULD_TRANSFER_FILES, false );

//	expr.sprintf( "%s = (%s >= %s) =!= True && time() > %s + %d",
//				  ATTR_PERIODIC_REMOVE_CHECK, ATTR_STAGE_IN_FINISH,
//				  ATTR_STAGE_IN_START, ATTR_Q_DATE, 1800 );
//	submit_ad->Insert( expr.Value() );

	bool cleared_environment = false;
	bool cleared_arguments = false;

		// Remove all remote_* attributes from the new ad before
		// translating remote_* attributes from the original ad.
		// See gittrac #376 for why we have two loops here.
	const char *next_name;
	submit_ad->ResetName();
	while ( (next_name = submit_ad->NextNameOriginal()) != NULL ) {
		if ( strncasecmp( next_name, "REMOTE_", 7 ) == 0 &&
			 strlen( next_name ) > 7 ) {

			submit_ad->Delete( next_name );
		}
	}

	jobAd->ResetExpr();
	while ( jobAd->NextExpr(next_name, next_expr) ) {
		if ( strncasecmp( next_name, "REMOTE_", 7 ) == 0 &&
			 strlen( next_name ) > 7 ) {

			char const *attr_name = &(next_name[7]);

			if(strcasecmp(attr_name,ATTR_JOB_ENVIRONMENT1) == 0 ||
			   strcasecmp(attr_name,ATTR_JOB_ENVIRONMENT1_DELIM) == 0 ||
			   strcasecmp(attr_name,ATTR_JOB_ENVIRONMENT2) == 0)
			{
				//Any remote environment settings indicate that we
				//should clear whatever environment was already copied
				//over from the non-remote settings, so the non-remote
				//settings can never trump the remote settings.
				if(!cleared_environment) {
					cleared_environment = true;
					submit_ad->Delete(ATTR_JOB_ENVIRONMENT1);
					submit_ad->Delete(ATTR_JOB_ENVIRONMENT1_DELIM);
					submit_ad->Delete(ATTR_JOB_ENVIRONMENT2);
				}
			}

			if(strcasecmp(attr_name,ATTR_JOB_ARGUMENTS1) == 0 ||
			   strcasecmp(attr_name,ATTR_JOB_ARGUMENTS2) == 0)
			{
				//Any remote arguments settings indicate that we
				//should clear whatever arguments was already copied
				//over from the non-remote settings, so the non-remote
				//settings can never trump the remote settings.
				if(!cleared_arguments) {
					cleared_arguments = true;
					submit_ad->Delete(ATTR_JOB_ARGUMENTS1);
					submit_ad->Delete(ATTR_JOB_ARGUMENTS2);
				}
			}

			submit_ad->Insert( attr_name, next_expr->Copy() );
		}
	}

		// worry about ATTR_JOB_[OUTPUT|ERROR]_ORIG

	return submit_ad;
}

ClassAd *INFNBatchJob::buildTransferAd()
{
	// TODO This will probably require some additional attributes
	//   to be set.
	return buildSubmitAd();
}

void INFNBatchJob::CreateSandboxId()
{
	// Build a name for the sandbox that will be unique to this job.
	// Our pattern is <collector name>_<GlobalJobId>

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
	std::string job_id;
	jobAd->LookupString( ATTR_GLOBAL_JOB_ID, job_id );

	std::string unique_id;
	sprintf( unique_id, "%s_%s", pool_name, job_id.c_str() );

	free( pool_name );

	SetRemoteSandboxId( unique_id.c_str() );
}

bool INFNBatchJob::StartTransferRequest( bool is_input )
{
	int direction = is_input ? FTPD_DOWNLOAD : FTPD_UPLOAD;
	MyString desc;
	TransferDaemon *td = td_mgr_mgr.get_td();
	ASSERT( td != NULL );

	TransferRequest *treq = new TransferRequest();

	// Set up the header which will get serialized to the transferd.
	treq->set_direction( direction );
	treq->set_used_constraint( false );
	//  The transfer gahp's version should match our own
	treq->set_peer_version( CondorVersion() );
	treq->set_xfer_protocol( FTP_CFTP );
	// TODO This next line may need to change...
	treq->set_transfer_service("Passive");
	treq->set_num_transfers( 1 );
	treq->set_protocol_version( 0 ); // for the treq structure, not xfer protocol

	// Set the callback handlers to work on this treq as it progresses through
	// the process of going & comming back from the transferd.

	// called just before the request is sent to the td itself.
	// used to modify the jobads for starting of transfer time
	desc = "Treq Upload Pre Push Callback Handler";
	treq->set_pre_push_callback(desc,
		(TreqPrePushCallback)&INFNBatchJob::TreqPrePushCb, this);

	// called after the push and response from the schedd, gives schedd
	// access to the treq capability string, the client was already
	// notified.
	desc = "Treq Upload Post Push Callback Handler";
	treq->set_post_push_callback(desc,
		(TreqPostPushCallback)&INFNBatchJob::TreqPostPushCb, this);

	// called with an update status from the td about this request.
	// (completed, not completed, etc, etc, etc)
	// job ads are processed, job taken off hold, etc if a successful 
	// completion had happend
	desc = "Treq Upload update Callback Handler";
	treq->set_update_callback(desc,
		(TreqUpdateCallback)&INFNBatchJob::TreqUpdateCb, this);

	// called when the td dies, if the td handles and updates and 
	// everything correctly, this is not normally called.
	desc = "Treq Upload Reaper Callback Handler";
	treq->set_reaper_callback(desc,
		(TreqReaperCallback)&INFNBatchJob::TreqReaperCb, this);

	// The TransferRequest docs say it takes over management of the
	// ClassAd pointer, but that appears to be lies.
	treq->append_task( jobAd );

	// queue the transfer request to the waiting td who will own the 
	// transfer request memory which owns the socket.
	td->add_transfer_request( treq );

	// Push the request to the td itself, where the callbacks from the 
	// transfer request will contact the client as needed.
	td->push_transfer_requests();

	return true;
}

TreqAction INFNBatchJob::TreqPrePushCb( TransferRequest*, TransferDaemon* )
{
	dprintf( D_FULLDEBUG, "(%d.%d) TreqPrePushCallback() called\n",
			 procID.cluster, procID.proc );

	return TREQ_ACTION_CONTINUE;
}

TreqAction INFNBatchJob::TreqPostPushCb( TransferRequest*, TransferDaemon* )
{
	dprintf( D_FULLDEBUG, "(%d.%d) TreqPostPushCallback() called\n",
			 procID.cluster, procID.proc );

	return TREQ_ACTION_CONTINUE;
}

TreqAction INFNBatchJob::TreqUpdateCb( TransferRequest*, TransferDaemon*,
								   ClassAd * )
{
	dprintf( D_FULLDEBUG, "(%d.%d) TreqUpdateCallback() called\n",
			 procID.cluster, procID.proc );

	return TREQ_ACTION_TERMINATE;
}

TreqAction INFNBatchJob::TreqReaperCb( TransferRequest* )
{
	dprintf( D_FULLDEBUG, "(%d.%d) TreqReaperCallback() called\n",
			 procID.cluster, procID.proc );

	return TREQ_ACTION_TERMINATE;
}
