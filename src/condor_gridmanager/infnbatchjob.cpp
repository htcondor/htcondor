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
#include "condor_ckpt_name.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "setenv.h"

#include "gridmanager.h"
#include "infnbatchjob.h"
#include "condor_config.h"


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
#define GM_REFRESH_PROXY		15
#define GM_START				16
#define GM_POLL_ACTIVE			17

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
	"GM_REFRESH_PROXY",
	"GM_START",
	"GM_POLL_ACTIVE"
};

#define JOB_STATE_UNKNOWN				-1
#define JOB_STATE_UNSUBMITTED			UNEXPANDED

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define LOG_GLOBUS_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, remoteState %d: %s returned error %d\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],remoteState, \
        func,error)


void INFNBatchJobInit()
{
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
	MyString resource;
		// CRUFT: grid-type 'blah' is deprecated. Now, the specific batch
		//   system names should be used (pbs, lsf). Glite are the only
		//   people who care about the old value. This changed happend in
		//   Condor 6.7.12.
		// TODO: Why are we doing a substring match? These are the exact
		//   string we expect to see.
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 ( strncasecmp( resource.Value(), "blah", 4 ) == 0 ||
		   strncasecmp( resource.Value(), "pbs", 4 ) == 0 ||
		   strncasecmp( resource.Value(), "lsf", 4 ) == 0 ||
		   strncasecmp( resource.Value(), "nqs", 4 ) == 0 ||
		   strncasecmp( resource.Value(), "naregi", 6 ) == 0 ) ) {

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
	char buff[4096];
	MyString error_string = "";
	char *gahp_path;

	gahpAd = NULL;
	gmState = GM_INIT;
	remoteState = JOB_STATE_UNKNOWN;
	enteredCurrentGmState = time(NULL);
	enteredCurrentRemoteState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	remoteJobId = NULL;
	lastPollTime = 0;
	pollNow = false;
	gahp = NULL;
	jobProxy = NULL;
	remoteProxyExpireTime = 0;

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_RESOURCE, buff );
	if ( buff[0] != '\0' ) {
		batchType = strdup( buff );
	} else {
		error_string.sprintf( "%s is not set in the job ad",
							  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff );
	if ( buff[0] != '\0' ) {
		SetRemoteJobId( strchr( buff, ' ' ) + 1 );
	} else {
		remoteState = JOB_STATE_UNSUBMITTED;
	}

	strupr( batchType );

	sprintf( buff, "%s_GAHP", batchType );
	gahp_path = param(buff);
	if ( gahp_path == NULL ) {
		error_string.sprintf( "%s not defined", buff );
		goto error_exit;
	}
	gahp = new GahpClient( batchType, gahp_path );
	free( gahp_path );

		// Does this have to be lower-case for SetRemoteJobId()?
	strlwr( batchType );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	jobProxy = AcquireProxy( jobAd, error_string,
							 (Eventcpp)&BaseJob::SetEvaluateState, this );
	if ( jobProxy == NULL && error_string != "" ) {
		goto error_exit;
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

INFNBatchJob::~INFNBatchJob()
{
	if ( jobProxy != NULL ) {
		ReleaseProxy( jobProxy, (Eventcpp)&BaseJob::SetEvaluateState, this );
	}
	if ( batchType != NULL ) {
		free( batchType );
	}
	if ( remoteJobId != NULL ) {
		free( remoteJobId );
	}
	if ( gahpAd ) {
		delete gahpAd;
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
}

void INFNBatchJob::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

int INFNBatchJob::doEvaluateState()
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
					gmState = GM_UNSUBMITTED;
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
				gmState = GM_DONE_SAVE;
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
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				SetRemoteJobId( NULL );
				requestScheddUpdate( this, false );
				gmState = GM_CLEAR_REQUEST;
			}
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
			}
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

			// For now, put problem jobs on hold instead of
			// forgetting about current submission and trying again.
			// TODO: Let our action here be dictated by the user preference
			// expressed in the job ad.
			if ( remoteJobId != NULL && condorState != REMOVED ) {
				gmState = GM_HOLD;
				break;
			}
			errorString = "";
			SetRemoteJobId( NULL );
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
			jobAd->ResetExpr();
			if ( jobAd->NextDirtyExpr() ) {
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
					strncpy( holdReason, errorString.Value(),
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

	return TRUE;
}

void INFNBatchJob::SetRemoteJobId( const char *job_id )
{
	free( remoteJobId );
	if ( job_id ) {
		remoteJobId = strdup( job_id );
	} else {
		remoteJobId = NULL;
	}

	MyString full_job_id;
	if ( job_id ) {
		full_job_id.sprintf( "%s %s", batchType, job_id );
	}
	BaseJob::SetRemoteJobId( full_job_id.Value() );
}

void INFNBatchJob::ProcessRemoteAd( ClassAd *remote_ad )
{
	int new_remote_state;
	MyString buff;
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
		old_expr = jobAd->Lookup( attrs_to_copy[index] );
		new_expr = remote_ad->Lookup( attrs_to_copy[index] );

		if ( new_expr != NULL && ( old_expr == NULL || !(*old_expr == *new_expr) ) ) {
			jobAd->Insert( new_expr->DeepCopy() );
		}
	}

	requestScheddUpdate( this, false );

	return;
}

BaseResource *INFNBatchJob::GetResource()
{
	return (BaseResource *)NULL;
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
		ATTR_JOB_CMD,
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
		if ( ( next_expr = jobAd->Lookup( attrs_to_copy[index] ) ) != NULL ) {
			submit_ad->Insert( next_expr->DeepCopy() );
		}
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
			errorString = error_str;
			delete submit_ad;
			return NULL;
		}
		submit_ad->Assign( ATTR_JOB_ARGUMENTS1, value_str.Value() );

		value_str = "";
		if ( !env.MergeFrom( jobAd, &error_str ) ||
			 !env.getDelimitedStringV1Raw( &value_str, &error_str ) ) {
			errorString = error_str;
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
//	submit_ad->Assign( ATTR_TOTAL_SUSPENSIONS, 0 );
//	submit_ad->Assign( ATTR_LAST_SUSPENSION_TIME, 0 );
//	submit_ad->Assign( ATTR_CUMULATIVE_SUSPENSION_TIME, 0 );
//	submit_ad->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );
//	submit_ad->Assign( ATTR_CURRENT_HOSTS, 0 );
//	submit_ad->Assign( ATTR_ENTERED_CURRENT_STATUS, now  );
//	submit_ad->Assign( ATTR_JOB_NOTIFICATION, NOTIFY_NEVER );
//	submit_ad->Assign( ATTR_JOB_LEAVE_IN_QUEUE, true );
//	submit_ad->Assign( ATTR_SHOULD_TRANSFER_FILES, false );

//	expr.sprintf( "%s = (%s >= %s) =!= True && CurrentTime > %s + %d",
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
	while ( (next_expr = jobAd->NextExpr()) != NULL ) {
		if ( strncasecmp( ((Variable*)next_expr->LArg())->Name(),
						  "REMOTE_", 7 ) == 0 &&
			 strlen( ((Variable*)next_expr->LArg())->Name() ) > 7 ) {

			char *attr_value;
			char const *attr_name = &((Variable*)next_expr->LArg())->Name()[7];

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

			next_expr->RArg()->PrintToNewStr(&attr_value);
			submit_ad->AssignExpr( attr_name, attr_value );
			free(attr_value);
		}
	}

		// worry about ATTR_JOB_[OUTPUT|ERROR]_ORIG

	return submit_ad;
}
