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
#include "condor_daemon_core.h"
#include "basename.h"
#include "spooled_job_files.h"

#include "gridmanager.h"
#include "unicorejob.h"
#include "condor_config.h"


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
#define GM_RECOVER				19

static const char *GMStateNames[] = {
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
	"GM_START",
	"GM_RECOVER"
};

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1


void UnicoreJobInit()
{
}

void UnicoreJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL_UNICORE", -1 );
	if ( tmp_int <= 0 ) {
		tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	}
	UnicoreJob::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	UnicoreJob::setGahpCallTimeout( tmp_int );
}

bool UnicoreJobAdMatch( const ClassAd *job_ad ) {
	int universe;
	std::string resource;
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 strncasecmp( resource.c_str(), "unicore ", 4 ) == 0 ) {

		return true;
	}
	return false;
}

BaseJob *UnicoreJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new UnicoreJob( jobad );
}


void
UnicoreJob::UnicoreGahpCallbackHandler( const char *update_ad_string )
{
	ClassAd *update_ad = new ClassAd();
	std::string job_id;
	classad::ClassAdXMLParser xml_parser;
	UnicoreJob *job = NULL;

	dprintf( D_FULLDEBUG, "UnicoreGahpCallbackHandler: got job callback: %s\n",
			 update_ad_string );

	if ( !xml_parser.ParseClassAd( update_ad_string, *update_ad ) ) {
		delete update_ad;
		dprintf( D_ALWAYS, "UnicoreGahpCallbackHandler: unparsable ad\n" );
		return;
	}

	if ( update_ad->LookupString( "UnicoreJobId", job_id ) == 0 ) {
		dprintf( D_ALWAYS, "UnicoreGahpCallbackHandler: no UnicoreJobId in "
				 "status ad\n" );
		delete update_ad;
		return;
	}
	
	if ( JobsByUnicoreId.lookup( job_id, job ) != 0 ||
		 job == NULL ) {
		dprintf( D_FULLDEBUG, "UnicoreGahpCallbackHandler: status ad for "
				 "unknown job, ignoring\n" );
		delete update_ad;
		return;
	}

	if ( job->newRemoteStatusAd == NULL ) {
		job->newRemoteStatusAd = update_ad;
		job->SetEvaluateState();
		return;
	}

		// If we already have an unprocessed update ad, merge the two,
		// with the new one overwriting duplicate attributes.
	job->newRemoteStatusAd->Update( *update_ad );

	job->SetEvaluateState();

	delete update_ad;
}

int UnicoreJob::probeInterval = 300;			// default value
int UnicoreJob::submitInterval = 300;			// default value
int UnicoreJob::gahpCallTimeout = 300;			// default value

HashTable<std::string, UnicoreJob *>
    UnicoreJob::JobsByUnicoreId( hashFunction );

UnicoreJob::UnicoreJob( ClassAd *classad )
	: BaseJob( classad )
{
	std::string buff;
	std::string error_string = "";

	resourceName = NULL;
	jobContact = NULL;
	gmState = GM_INIT;
	unicoreState = condorState;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	enteredCurrentUnicoreState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	submitAd = NULL;
	newRemoteStatusAd = NULL;
	gahp = NULL;
	errorString = "";

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "UNDEFINED" );
	}

	char *gahp_path = param("UNICORE_GAHP");
	if ( gahp_path == NULL ) {
		error_string = "UNICORE_GAHP not defined";
		goto error_exit;
	}
	gahp = new GahpClient( "UNICORE", gahp_path );
	free( gahp_path );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	jobAd->LookupString( ATTR_GRID_RESOURCE, &resourceName );

	jobAd->LookupString( ATTR_GRID_JOB_ID, buff );
	if ( !buff.empty() ) {
		const char *token;

		Tokenize( buff );

		token = GetNextToken( " ", false );
		if ( !token || strcasecmp( token, "unicore" ) ) {
			formatstr( error_string, "%s not of type unicore", ATTR_GRID_JOB_ID );
			goto error_exit;
		}

		GetNextToken( " ", false );
		GetNextToken( " ", false );
		token = GetNextToken( " ", false );
		if ( !token ) {
			formatstr( error_string, "%s missing job ID",
								  ATTR_GRID_JOB_ID );
			goto error_exit;
		}
		SetRemoteJobId( token );
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

UnicoreJob::~UnicoreJob()
{
	if ( jobContact ) {
		JobsByUnicoreId.remove(jobContact);
		free( jobContact );
	}
	if ( resourceName ) {
		free( resourceName );
	}
	if ( newRemoteStatusAd ) {
		delete newRemoteStatusAd;
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
}

void UnicoreJob::Reconfig()
{
	BaseJob::Reconfig();
	if ( gahp ) {
		gahp->setTimeout( gahpCallTimeout );
	}
}

void UnicoreJob::doEvaluateState()
{
	int old_gm_state;
	int old_unicore_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, unicoreState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],unicoreState);

	if ( gahp ) {
		gahp->setMode( GahpClient::normal );
	}

	do {
		reevaluate_state = false;
		old_gm_state = gmState;
		old_unicore_state = unicoreState;
		ASSERT ( gahp != NULL || gmState == GM_HOLD || gmState == GM_DELETE );

		switch ( gmState ) {
		case GM_INIT: {
			// This is the state all jobs start in when the GlobusJob object
			// is first created. Here, we do things that we didn't want to
			// do in the constructor because they could block (the
			// constructor is called while we're connected to the schedd).
			if ( gahp->Startup() == false ) {
				dprintf( D_ALWAYS, "(%d.%d) Error starting up GAHP\n",
						 procID.cluster, procID.proc );
				
				jobAd->Assign( ATTR_HOLD_REASON, "Failed to start GAHP" );
				gmState = GM_HOLD;
				break;
			}

			GahpClient::mode saved_mode = gahp->getMode();
			gahp->setMode( GahpClient::blocking );

			rc = gahp->unicore_job_callback( UnicoreGahpCallbackHandler );
			if ( rc != GLOBUS_SUCCESS ) {
				dprintf( D_ALWAYS,
						 "(%d.%d) Error enabling unicore callback, err=%d\n", 
						 procID.cluster, procID.proc, rc );
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
			// The one way jobs can end up back in this state is if we
			// attempt a restart of a jobmanager only to be told that the
			// old jobmanager process is still alive.
			errorString = "";
			if ( jobContact == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( wantResubmit || doResubmit ) {
				gmState = GM_CLEAR_REQUEST;
			} else {
				if ( condorState == RUNNING ) {
					executeLogged = true;
				}

				gmState = GM_RECOVER;
			}
			} break;
		case GM_RECOVER: {
			// We're recovering from a crash after the job was submitted.
			// Allow the gahp server to recover its internal state about
			// the job.
			if ( submitAd == NULL ) {
				submitAd = buildSubmitAd();
			}
			if ( submitAd == NULL ) {
				gmState = GM_HOLD;
				break;
			}
			rc = gahp->unicore_job_recover( submitAd->c_str() );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				dprintf(D_ALWAYS,"(%d.%d) unicore_job_recover() failed\n",
						procID.cluster, procID.proc);
				gmState = GM_CANCEL;
				break;
			}
			gmState = GM_SUBMITTED;
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
				gmState = GM_UNSUBMITTED;
				break;
			}
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
				jobAd->Assign( ATTR_HOLD_REASON,
							   "Attempts to submit failed" );
				gmState = GM_HOLD;
				break;
			}
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {
				if ( submitAd == NULL ) {
					submitAd = buildSubmitAd();
				}
				if ( submitAd == NULL ) {
					gmState = GM_HOLD;
					break;
				}
				rc = gahp->unicore_job_create( submitAd->c_str(),
											   &job_contact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;
				if ( rc == GLOBUS_SUCCESS ) {
						// job_contact was strdup()ed for us. Now we take
						// responsibility for free()ing it.
					SetRemoteJobId( job_contact );
					free( job_contact );
					WriteGridSubmitEventToUserLog( jobAd );
					gmState = GM_SUBMIT_SAVE;
				} else {
					// unhandled error
					dprintf(D_ALWAYS,"(%d.%d) unicore_job_create() failed\n",
							procID.cluster, procID.proc);
					dprintf(D_ALWAYS,"(%d.%d)    submitAd='%s'\n",
							procID.cluster, procID.proc,submitAd->c_str());
					if ( job_contact ) {
						free( job_contact );
					}
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
				if ( jobAd->IsAttributeDirty( ATTR_GRID_JOB_ID ) ) {
					requestScheddUpdate( this, true );
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
				rc = gahp->unicore_job_start( jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					dprintf(D_ALWAYS,"(%d.%d) unicore_job_start() failed\n",
							procID.cluster, procID.proc);
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
			if ( unicoreState == COMPLETED ) {
				gmState = GM_DONE_SAVE;
//			} else if ( unicoreState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
//				gmState = GM_CANCEL;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else if ( newRemoteStatusAd ) {
dprintf(D_FULLDEBUG,"(%d.%d) *** Processing callback ad\n",procID.cluster, procID.proc );
				lastProbeTime = now;
				UpdateUnicoreState( newRemoteStatusAd );
				delete newRemoteStatusAd;
				newRemoteStatusAd = NULL;
				reevaluate_state = true;
/* Now that the gahp tells us when a job status changes, we don't need to
 * do active probes.
			} else {
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
*/
			}
			} break;
		case GM_PROBE_JOBMANAGER: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				char *status_ad = NULL;
				rc = gahp->unicore_job_status( jobContact,
											   &status_ad );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					dprintf(D_ALWAYS,"(%d.%d) unicore_job_status() failed\n",
							procID.cluster, procID.proc);
					if ( status_ad ) {
						free( status_ad );
					}
					gmState = GM_CANCEL;
					break;
				}
				UpdateUnicoreState( status_ad );
				if ( status_ad ) {
					free( status_ad );
				}
				if ( newRemoteStatusAd ) {
					delete newRemoteStatusAd;
					newRemoteStatusAd = NULL;
				}
				lastProbeTime = now;
				gmState = GM_SUBMITTED;
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
			rc = gahp->unicore_job_destroy( jobContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				dprintf(D_ALWAYS,"(%d.%d) unicore_job_destroy() failed\n",
						procID.cluster, procID.proc);
				gmState = GM_CANCEL;
				break;
			}
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
//				SetRemoteJobId( NULL );
//				gmState = GM_CLEAR_REQUEST;
				gmState = GM_HOLD;
			}
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.
//			if ( unicoreState != COMPLETED &&
//				 unicoreState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
if ( unicoreState != COMPLETED ) {
				rc = gahp->unicore_job_destroy( jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					dprintf(D_ALWAYS,"(%d.%d) unicore_job_destroy() failed\n",
							procID.cluster, procID.proc);
					gmState = GM_HOLD;
					break;
				}
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

			errorString = "";
			SetRemoteJobId( NULL );
			SetRemoteJobStatus( NULL );
			JobIdle();

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
			executeLogged = false;
			terminateLogged = false;
			abortLogged = false;
			evictLogged = false;
			gmState = GM_UNSUBMITTED;
			} break;
		case GM_HOLD: {
			// Put the job on hold in the schedd.
			// TODO: what happens if we learn here that the job is removed?
//			if ( jobContact &&
//				 unicoreState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN ) {
//				unicoreState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN;
//			}
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

		if ( gmState != old_gm_state || unicoreState != old_unicore_state ) {
			reevaluate_state = true;
		}
		if ( unicoreState != old_unicore_state ) {
//			dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
//					procID.cluster, procID.proc,
//					GlobusJobStatusName(old_globus_state),
//					GlobusJobStatusName(globusState));
			enteredCurrentUnicoreState = time(NULL);
		}
		if ( gmState != old_gm_state ) {
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
			// If we were waiting for a pending unicore call, we're not
			// anymore so purge it.
			if ( gahp ) {
				gahp->purgePendingRequests();
			}
			// If we were calling unicore_job_create and using submitAd,
			// we're done with it now, so free it.
			if ( submitAd ) {
				delete submitAd;
				submitAd = NULL;
			}
		}

	} while ( reevaluate_state );
}

BaseResource *UnicoreJob::GetResource()
{
	return (BaseResource *)NULL;
}

void UnicoreJob::UpdateUnicoreState( const char *update_ad_string )
{
	ClassAd *update_ad;
	classad::ClassAdXMLParser xml_parser;

	if ( update_ad_string == NULL ) {
		dprintf( D_ALWAYS, "(%d.%d) Received NULL unicore status ad string\n",
				 procID.cluster, procID.proc );
		return;
	}

	update_ad = new ClassAd();
	if ( !xml_parser.ParseClassAd( update_ad_string, *update_ad ) ) {
		delete update_ad;
		update_ad = NULL;
	}

	UpdateUnicoreState( update_ad );

	delete update_ad;
}

void UnicoreJob::UpdateUnicoreState( ClassAd *update_ad )
{
	if ( update_ad == NULL ) {
		dprintf( D_ALWAYS, "(%d.%d) Received NULL unicore status ad\n",
				 procID.cluster, procID.proc );
		return;
	}

	for ( auto itr = update_ad->begin(); itr != update_ad->end(); itr++ ) {
		if ( strcasecmp( itr->first.c_str(), ATTR_MY_TYPE ) == 0 ||
		     strcasecmp( itr->first.c_str(), ATTR_TARGET_TYPE ) == 0 ||
		     strcasecmp( itr->first.c_str(), "UnicoreJobId" ) == 0 ) {
			continue;
		}
		if ( strcasecmp( itr->first.c_str(), ATTR_JOB_STATUS ) == 0 ) {
			int status = 0;
			update_ad->LookupInteger( ATTR_JOB_STATUS, status );
			unicoreState = status;
			SetRemoteJobStatus( getJobStatusString( unicoreState ) );
			if ( unicoreState == RUNNING ) {
				JobRunning();
			}
			if ( unicoreState == IDLE ) {
				JobIdle();
			}
		}
		ExprTree * pTree = itr->second->Copy();
		jobAd->Insert( itr->first, pTree );
	}
}

void UnicoreJob::SetRemoteJobId( const char *job_id )
{
		// We need to maintain a hashtable based on the job id returned
		// but the unicore gahp. This is because the job status
		// notifications we receive don't include the unicore resource.
	if ( jobContact ) {
		JobsByUnicoreId.remove(jobContact);
	}
	if ( job_id ) {
		ASSERT( JobsByUnicoreId.insert(job_id, this) == 0);
	}

	free( jobContact );
	if ( job_id ) {
		jobContact = strdup( job_id );
	} else {
		jobContact = NULL;
	}

	std::string full_job_id;
	if ( job_id ) {
		formatstr( full_job_id, "%s %s", resourceName, job_id );
	}
	BaseJob::SetRemoteJobId( full_job_id.c_str() );
}

std::string *UnicoreJob::buildSubmitAd()
{
//	ClassAd submit_ad;
	classad::ClassAdXMLUnParser xml_unp;
	std::string *ad_string;

/*
	ExprTree *expr;

	static const char *regular_attrs[] = {
		ATTR_JOB_IWD,
		ATTR_JOB_CMD,
		ATTR_JOB_INPUT,
		ATTR_JOB_OUTPUT,
		ATTR_JOB_ERROR,
		ATTR_JOB_ARGUMENTS,
		ATTR_JOB_ENVIRONMENT,
		ATTR_TRANSFER_EXECUTABLE,
		ATTR_TRANSFER_INPUT,
		ATTR_TRANSFER_OUTPUT,
		ATTR_TRANSFER_ERROR,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_TRANSFER_OUTPUT_FILES,
		ATTR_SHOULD_TRANSFER_FILES,
		ATTR_WHEN_TO_TRANSFER_OUTPUT,
		"UnicoreUSite",
		"UnicoreVSite",
		"UnicoreKeystoreFile",
		"UnicorePassphraseFile",
		NULL
	};

	for ( int i = 0; regular_attrs[i] != NULL; i++ ) {

		ExprTree *expr;
		if ( ( expr = jobAd->Lookup( regular_attrs[i] ) ) != NULL ) {
			submit_ad.Insert( expr->DeepCopy() );
		}

	}
*/

	xml_unp.SetCompactSpacing( true );
	ad_string = new std::string;
//	xml_unp.Unparse( *ad_string, &submit_ad );
	xml_unp.Unparse( *ad_string, jobAd );

	return ad_string;
}
