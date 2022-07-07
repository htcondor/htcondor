/***************************************************************
 *
 * Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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
#include "nullfile.h"
#include "filename_tools.h"
#include "condor_url.h"

#include "globus_utils.h"
#include "gridmanager.h"
#include "arcjob.h"
#include "condor_config.h"
#include "nordugridjob.h" // for rsl_stringify()


// GridManager job states
#define GM_INIT					0
#define GM_UNSUBMITTED			1
#define GM_SUBMIT				2
#define GM_SUBMIT_SAVE			3
#define GM_SUBMITTED			4
#define GM_DONE_SAVE			5
#define GM_DONE_COMMIT			6
#define GM_CANCEL				7
#define GM_FAILED				8
#define GM_DELETE				9
#define GM_CLEAR_REQUEST		10
#define GM_HOLD					11
#define GM_PROBE_JOB			12
#define GM_STAGE_IN				13
#define GM_STAGE_OUT			14
#define GM_EXIT_INFO			15
#define GM_RECOVER_QUERY		16
#define GM_START				17
#define GM_CANCEL_CLEAN			18
#define GM_DELEGATE_PROXY		19

static const char *GMStateNames[] = {
	"GM_INIT",
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
	"GM_PROBE_JOB",
	"GM_STAGE_IN",
	"GM_STAGE_OUT",
	"GM_EXIT_INFO",
	"GM_RECOVER_QUERY",
	"GM_START",
	"GM_CANCEL_CLEAN",
	"GM_DELEGATE_PROXY",
};

#define REMOTE_STATE_ACCEPTING		"ACCEPTING"
#define REMOTE_STATE_ACCEPTED		"ACCEPTED"
#define REMOTE_STATE_PREPARING		"PREPARING"
#define REMOTE_STATE_PREPARED		"PREPARED"
#define REMOTE_STATE_SUBMITTING		"SUBMITTING"
#define REMOTE_STATE_RUNNING		"RUNNING"
#define REMOTE_STATE_QUEUING		"QUEUING"
#define REMOTE_STATE_HELD			"HELD"
#define REMOTE_STATE_EXITINGLRMS	"EXITINGLRMS"
#define REMOTE_STATE_OTHER			"OTHER"
#define REMOTE_STATE_KILLING		"KILLING"
#define REMOTE_STATE_EXECUTED		"EXECUTED"
#define REMOTE_STATE_FINISHING		"FINISHING"
#define REMOTE_STATE_FINISHED		"FINISHED"
#define REMOTE_STATE_FAILED			"FAILED"
#define REMOTE_STATE_KILLED			"KILLED"
#define REMOTE_STATE_WIPED			"WIPED"

#define HTTP_200_OK			200
#define HTTP_201_CREATED	201
#define HTTP_202_ACCEPTED	202
#define HTTP_404_NOT_FOUND	404

#define REMOTE_STDOUT_NAME	"_arc_stdout"
#define REMOTE_STDERR_NAME	"_arc_stderr"

// Filenames are case insensitive on Win32, but case sensitive on Unix
#ifdef WIN32
#	define file_contains contains_anycase
#else
#	define file_contains contains
#endif

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

static
const std::string& escapeXML(const std::string& in)
{
	static std::string out;
	out.clear();
	for ( char c : in ) {
		switch( c ) {
		case '<':
			out += "&lt;";
			break;
		case '>':
			out += "&gt;";
			break;
		case '&':
			out += "&amp;";
			break;
		default:
			out += c;
		}
	}
	return out;
}

void ArcJobInit()
{
}

void ArcJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	ArcJob::setGahpCallTimeout( tmp_int );

	// Tell all the resource objects to deal with their new config values
	for (auto &elem : ArcResource::ResourcesByName) {
		elem.second->Reconfig();
	}
}

bool ArcJobAdMatch( const ClassAd *job_ad ) {
	int universe;
	std::string resource;
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 strncasecmp( resource.c_str(), "arc ", 4 ) == 0 ) {

		return true;
	}
	return false;
}

BaseJob *ArcJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new ArcJob( jobad );
}

int ArcJob::submitInterval = 300;	// default value
int ArcJob::gahpCallTimeout = 300;	// default value
int ArcJob::maxConnectFailures = 3;	// default value
// If ARC_JOB_INFO retruns stale data, how long to wait before retrying
int ArcJob::jobInfoInterval = 60;

ArcJob::ArcJob( ClassAd *classad )
	: BaseJob( classad )
{
	char buff[4096];
	std::string error_string = "";
	char *gahp_path = NULL;

	remoteJobId = NULL;
	remoteJobState = "";
	gmState = GM_INIT;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	lastJobInfoTime = 0;
	resourceManagerString = NULL;
	jobProxy = NULL;
	myResource = NULL;
	gahp = NULL;
	stageList = NULL;
	stageLocalList = NULL;

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start (unless the job is already held).
	if ( condorState != HELD &&
		 jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {

		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	jobProxy = AcquireProxy( jobAd, error_string,
							 (TimerHandlercpp)&BaseJob::SetEvaluateState, this );

	jobAd->LookupString( ATTR_SCITOKENS_FILE, m_tokenFile );

	if (jobProxy == nullptr && m_tokenFile.empty()) {
		error_string = "ARC job has no credentials";
		goto error_exit;
	}

	gahp_path = param( "ARC_GAHP" );
	if ( gahp_path == NULL ) {
		error_string = "ARC_GAHP not defined";
		goto error_exit;
	}
	snprintf( buff, sizeof(buff), "ARC/%s#%s",
	          (m_tokenFile.empty() && jobProxy) ? jobProxy->subject->fqan : "",
	          m_tokenFile.c_str() );
	gahp = new GahpClient( buff, gahp_path );
	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	free( gahp_path );
	gahp_path = NULL;

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_RESOURCE, buff, sizeof(buff) );
	if ( buff[0] != '\0' ) {
		const char *token;

		Tokenize( buff );

		token = GetNextToken( " ", false );
		if ( !token || strcasecmp( token, "arc" ) ) {
			formatstr( error_string, "%s not of type arc",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = GetNextToken( " ", false );
		if ( token && *token ) {
			resourceManagerString = strdup( token );
		} else {
			formatstr( error_string, "%s missing server name",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

	} else {
		formatstr( error_string, "%s is not set in the job ad",
							  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	myResource = ArcResource::FindOrCreateResource( resourceManagerString,
	                 m_tokenFile.empty() ? jobProxy : nullptr, m_tokenFile );
	myResource->RegisterJob( this );

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff, sizeof(buff) );
	if ( strrchr( buff, ' ' ) ) {
		SetRemoteJobId( strrchr( buff, ' ' ) + 1 );
		myResource->AlreadySubmitted( this );
	} else {
		SetRemoteJobId( NULL );
	}

	jobAd->LookupString( ATTR_GRID_JOB_STATUS, remoteJobState );

	return;

 error_exit:
	gmState = GM_HOLD;
	if ( !error_string.empty() ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string );
	}
	return;
}

ArcJob::~ArcJob()
{
	if ( jobProxy != NULL ) {
		ReleaseProxy( jobProxy, (TimerHandlercpp)&BaseJob::SetEvaluateState, this );
	}
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	if ( remoteJobId ) {
		free( remoteJobId );
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( stageList ) {
		delete stageList;
	}
	if ( stageLocalList ) {
		delete stageLocalList;
	}
}

void ArcJob::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

void ArcJob::doEvaluateState()
{
	int old_gm_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, condorState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],condorState);

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
			if ( m_tokenFile.empty() && jobProxy && gahp->Initialize( jobProxy ) == false ) {
				dprintf( D_ALWAYS, "(%d.%d) Error initializing GAHP\n",
						 procID.cluster, procID.proc );

				jobAd->Assign( ATTR_HOLD_REASON,
							   "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}
			// TODO Ensure gahp command isn't issued for every new job
			if ( !m_tokenFile.empty() && !gahp->UpdateToken( m_tokenFile ) ) {
				dprintf( D_ALWAYS, "(%d.%d) Error initializing GAHP\n",
				         procID.cluster, procID.proc );

				jobAd->InsertAttr( ATTR_HOLD_REASON,
				                   "Failed to provide GAHP with token" );
				gmState = GM_HOLD;
				break;
			}

			if (m_tokenFile.empty() && jobProxy) {
				gahp->setDelegProxy( jobProxy );
			}

			gmState = GM_START;
			} break;
		case GM_START: {
			errorString = "";
			if ( remoteJobId == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else {
				submitLogged = true;
				if ( condorState == RUNNING ||
					 condorState == COMPLETED ) {
					executeLogged = true;
				}

				if ( remoteJobState == "" ||
					 remoteJobState == REMOTE_STATE_ACCEPTING ||
					 remoteJobState == REMOTE_STATE_ACCEPTED ||
					 remoteJobState == REMOTE_STATE_PREPARING ) {
					gmState = GM_RECOVER_QUERY;
				} else {
					gmState = GM_SUBMITTED;
				}
			}
			} break;
		case GM_RECOVER_QUERY: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				if ( m_lastRemoteStatusUpdate > enteredCurrentGmState ) {
					if ( remoteJobState == REMOTE_STATE_ACCEPTING ||
						 remoteJobState == REMOTE_STATE_ACCEPTED ||
						 remoteJobState == REMOTE_STATE_PREPARING ) {
						gmState = GM_STAGE_IN;
					} else {
						gmState = GM_SUBMITTED;
					}
				} else if ( m_currentStatusUnknown ) {
					gmState = GM_CANCEL;
				}
			}
			} break;
		case GM_UNSUBMITTED: {
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
			if ( condorState == REMOVED || condorState == HELD ) {
				myResource->CancelSubmit( this );
				gmState = GM_UNSUBMITTED;
				break;
			}
			if ( ! jobProxy || ! delegationId.empty() ) {
				gmState = GM_SUBMIT;
				break;
			}

			std::string deleg_id;

			rc = gahp->arc_delegation_new( resourceManagerString,
			                               jobProxy->proxy_filename, deleg_id );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}

			if ( rc == HTTP_200_OK ) {
				ASSERT( !deleg_id.empty() );
				delegationId = deleg_id;
				gmState = GM_SUBMIT;
			} else {
				errorString = gahp->getErrorString();
				dprintf(D_ALWAYS,"(%d.%d) proxy delegation failed: %s\n",
				        procID.cluster, procID.proc,
				        errorString.c_str() );
				gmState = GM_HOLD;
			}

			} break;
		case GM_SUBMIT: {
			if ( condorState == REMOVED || condorState == HELD ) {
				myResource->CancelSubmit( this );
				gmState = GM_UNSUBMITTED;
				break;
			}
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
//				jobAd->Assign( ATTR_HOLD_REASON,
//							   "Attempts to submit failed" );
				gmState = GM_HOLD;
				break;
			}
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {

				std::string job_id;
				std::string job_status;

				// Once RequestSubmit() is called at least once, you must
				// CancelRequest() once you're done with the request call
				if ( myResource->RequestSubmit( this ) == false ) {
					break;
				}

				if ( RSL.empty() ) {
//					if ( ! buildSubmitRSL() ) {
					if ( ! buildJobADL() ) {
						gmState = GM_HOLD;
						break;
					}
				}
				rc = gahp->arc_job_new(
									resourceManagerString,
									RSL,
									job_id,
									job_status );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}

				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;

				if ( rc == HTTP_201_CREATED ) {
					ASSERT( !job_id.empty() );
					SetRemoteJobId( job_id.c_str() );
					remoteJobState = job_status;
					SetRemoteJobStatus( job_status.c_str() );
					WriteGridSubmitEventToUserLog( jobAd );
					gmState = GM_SUBMIT_SAVE;
				} else {
					errorString = gahp->getErrorString();
					dprintf(D_ALWAYS,"(%d.%d) job submit failed: %s\n",
							procID.cluster, procID.proc,
							errorString.c_str() );
					myResource->CancelSubmit( this );
					gmState = GM_HOLD;
				}

			} else {
				unsigned int delay = 0;
				if ( (lastSubmitAttempt + submitInterval) > now ) {
					delay = (lastSubmitAttempt + submitInterval) - now;
				}
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_SUBMIT_SAVE: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				if ( jobAd->IsAttributeDirty( ATTR_GRID_JOB_ID ) ) {
					requestScheddUpdate( this, true );
					break;
				}
				gmState = GM_STAGE_IN;
			}
			} break;
		case GM_STAGE_IN: {
			if ( stageList == NULL ) {
				const char *file;
				stageList = buildStageInList();
				stageList->rewind();
				while ( (file = stageList->next()) ) {
					if ( IsUrl( file ) ) {
						stageList->deleteCurrent();
					}
				}
			}
			if ( stageList->isEmpty() ) {
				rc = HTTP_200_OK;
			} else {
				rc = gahp->arc_job_stage_in( resourceManagerString, remoteJobId,
									   *stageList );
			}
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			} else if ( rc != HTTP_200_OK && rc != HTTP_201_CREATED ) {
				formatstr(errorString, "File stage-in failed: %s",
				          gahp->getErrorString());
				dprintf( D_ALWAYS, "(%d.%d) %s\n",
				         procID.cluster, procID.proc, errorString.c_str() );
				gmState = GM_CANCEL;
			} else {
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_SUBMITTED: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else if ( remoteJobState == REMOTE_STATE_FINISHED ||
				 remoteJobState == REMOTE_STATE_FAILED ||
				 remoteJobState == REMOTE_STATE_KILLED ||
				 remoteJobState == REMOTE_STATE_WIPED ) {
					gmState = GM_EXIT_INFO;
			} else {
				if ( lastProbeTime < enteredCurrentGmState ) {
					lastProbeTime = enteredCurrentGmState;
				}
				if ( probeNow ) {
					lastProbeTime = 0;
					probeNow = false;
				}
#if 0
				int probe_interval = myResource->GetJobPollInterval();
				if ( now >= lastProbeTime + probe_interval ) {
					gmState = GM_PROBE_JOB;
					break;
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + probe_interval) > now ) {
					delay = (lastProbeTime + probe_interval) - now;
				}
				daemonCore->Reset_Timer( evaluateStateTid, delay );
#endif
			}
			} break;
		case GM_PROBE_JOB: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				std::string new_status;
				rc = gahp->arc_job_status( resourceManagerString,
										 remoteJobId, new_status );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				} else if ( rc != HTTP_200_OK ) {
					// What to do about failure?
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS, "(%d.%d) job probe failed: %s\n",
							 procID.cluster, procID.proc,
							 errorString.c_str() );
				} else {
					remoteJobState = new_status;
					SetRemoteJobStatus( new_status.c_str() );
				}
				lastProbeTime = now;
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_EXIT_INFO: {
			std::string reply;

			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL_CLEAN;
				break;
			}
			// The job info is not always immediately available, especially
			// for a failed job.
			if ( now < lastJobInfoTime + jobInfoInterval ) {
				daemonCore->Reset_Timer( evaluateStateTid, (lastJobInfoTime + jobInfoInterval) - now );
				break;
			}
			rc = gahp->arc_job_info( resourceManagerString, remoteJobId, reply );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			lastJobInfoTime = time(NULL);
			if ( rc != HTTP_200_OK ) {
				errorString = gahp->getErrorString();
				dprintf( D_ALWAYS, "(%d.%d) exit info gathering failed: %s\n",
						 procID.cluster, procID.proc, errorString.c_str() );
				gmState = GM_CANCEL;
			}

			if ( reply.empty() ) {
				errorString = "Job exit information missing";
				dprintf( D_ALWAYS, "(%d.%d) exit info missing\n",
				         procID.cluster, procID.proc );
				gmState = GM_CANCEL;
				break;
			}
			std::string val;
			ClassAd info_ad;
			classad::ClassAdJsonParser parser;
			if ( ! parser.ParseClassAd(reply, info_ad, true) ) {
				errorString = "Job exit information invalid format";
				dprintf( D_ALWAYS, "(%d.%d) Job exit info invalid: %s\n",
				         procID.cluster, procID.proc, reply.c_str() );
				gmState = GM_CANCEL_CLEAN;
				break;
			}

			if ( info_ad.Lookup("EndTime") == NULL ) {
				dprintf(D_FULLDEBUG, "(%d.%d) Job info is stale, will retry in %ds.\n", procID.cluster, procID.proc, jobInfoInterval);
				daemonCore->Reset_Timer( evaluateStateTid, (lastJobInfoTime + jobInfoInterval) - now );
				break;
			}

			if ( remoteJobState == REMOTE_STATE_FINISHED ) {
				int exit_code = 0;
				int wallclock = 0;
				int cpu = 0;
				if ( info_ad.LookupString("ExitCode", val ) ) {
					exit_code = atoi(val.c_str());
				}
				if ( info_ad.LookupString("UsedTotalWallTime", val ) ) {
					wallclock = atoi(val.c_str());
				}
				if ( info_ad.LookupString("UsedTotalCPUTime", val ) ) {
					cpu = atoi(val.c_str());
				}

				// We can't distinguish between normal job exit and
				// exit-by-signal.
				// Assume it's always a normal exit.
				jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );
				jobAd->Assign( ATTR_ON_EXIT_CODE, exit_code );
				jobAd->Assign( ATTR_JOB_REMOTE_WALL_CLOCK, wallclock * 60.0 );
				jobAd->Assign( ATTR_JOB_REMOTE_USER_CPU, cpu * 60.0 );
				gmState = GM_STAGE_OUT;
			} else {
				if ( info_ad.LookupString("Error", val) ) {
					errorString = "ARC job failed: " + val;
				} else {
					errorString = "ARC job failed for unknown reason";
				}
				gmState = GM_STAGE_OUT;
			}
			} break;
		case GM_STAGE_OUT: {
			if ( stageList == NULL ) {
				stageList = buildStageOutList();
			}
			if ( stageLocalList == NULL ) {
				const char *file;
				stageLocalList = buildStageOutLocalList( stageList );
				stageList->rewind();
				stageLocalList->rewind();
				while ( (file = stageLocalList->next()) ) {
					ASSERT( stageList->next() );
					if ( IsUrl( file ) ) {
						stageList->deleteCurrent();
						stageLocalList->deleteCurrent();
					}
				}
			}
			if ( stageList->isEmpty() ) {
				rc = HTTP_200_OK;
			} else {
				rc = gahp->arc_job_stage_out( resourceManagerString,
										 remoteJobId,
										 *stageList, *stageLocalList );
			}
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			} else if ( rc != HTTP_200_OK ) {
				// If the ARC job failed, we want to keep that error message
				if ( remoteJobState == REMOTE_STATE_FINISHED || errorString.empty() ) {
					formatstr(errorString, "File stage-out failed: %s",
				              gahp->getErrorString());
				}
				dprintf( D_ALWAYS, "(%d.%d) File stage-out failed: %s\n",
				         procID.cluster, procID.proc, gahp->getErrorString() );
				gmState = GM_HOLD;
			} else if ( remoteJobState != REMOTE_STATE_FINISHED ) {
				gmState = GM_CANCEL_CLEAN;
			} else {
				gmState = GM_DONE_SAVE;
			}
			} break;
		case GM_DONE_SAVE: {
			if ( condorState != HELD && condorState != REMOVED ) {
				JobTerminated();
				if ( condorState == COMPLETED ) {
					if ( jobAd->IsAttributeDirty( ATTR_JOB_STATUS ) ) {
						requestScheddUpdate( this, true );
						break;
					}
				}
			}
			gmState = GM_DONE_COMMIT;
			} break;
		case GM_DONE_COMMIT: {
			rc = gahp->arc_job_clean( resourceManagerString, remoteJobId );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			} else if ( rc != HTTP_202_ACCEPTED ) {
				errorString = gahp->getErrorString();
				dprintf( D_ALWAYS, "(%d.%d) job cleanup failed: %s\n",
						 procID.cluster, procID.proc, errorString.c_str() );
				gmState = GM_HOLD;
				break;
			}
			myResource->CancelSubmit( this );
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				if ( remoteJobId != NULL ) {
					SetRemoteJobId( NULL );
				}
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			rc = gahp->arc_job_kill( resourceManagerString, remoteJobId );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			} else if ( rc == HTTP_202_ACCEPTED ) {
				gmState = GM_CANCEL_CLEAN;
			} else {
				// What to do about a failed cancel?
				errorString = gahp->getErrorString();
				dprintf( D_ALWAYS, "(%d.%d) job cancel failed: %s\n",
						 procID.cluster, procID.proc, errorString.c_str() );
				gmState = GM_FAILED;
			}
			} break;
		case GM_CANCEL_CLEAN: {
			rc = gahp->arc_job_clean( resourceManagerString, remoteJobId );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			} else if ( rc == HTTP_202_ACCEPTED ) {
				gmState = GM_FAILED;
			} else {
				// What to do about a failed clean?
				errorString = gahp->getErrorString();
				dprintf( D_ALWAYS, "(%d.%d) job cancel cleanup failed: %s\n",
						 procID.cluster, procID.proc, errorString.c_str() );
				gmState = GM_FAILED;
			}
			} break;
		case GM_FAILED: {
			myResource->CancelSubmit( this );

			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				SetRemoteJobId( NULL );
				gmState = GM_HOLD;
			}
			} break;
		case GM_DELETE: {
			// The job has completed or been removed. Delete it from the
			// schedd.
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
			if ( remoteJobId != NULL
				     && condorState != REMOVED
					 && wantResubmit == false
					 && doResubmit == 0 ) {
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
						"(%d.%d) Resubmitting to Globus because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_GLOBUS_RESUBMIT_CHECK );
			}
			if ( doResubmit ) {
				doResubmit = 0;
				dprintf(D_ALWAYS,
					"(%d.%d) Resubmitting to Globus (last submit failed)\n",
						procID.cluster, procID.proc );
			}
			errorString = "";
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
			myResource->CancelSubmit( this );

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
			if ( remoteJobState != "" ) {
				remoteJobState = "";
				SetRemoteJobStatus( NULL );
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
				std::string holdReason;
				jobAd->LookupString( ATTR_HOLD_REASON, holdReason );
				if ( holdReason.empty() && errorString != "" ) {
					holdReason = errorString;
				}
				if ( holdReason.empty() ) {
					holdReason = "Unspecified gridmanager error";

				}

				JobHeld( holdReason.c_str() );
			}
			gmState = GM_DELETE;
			} break;
		default:
			EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster,procID.proc,
					gmState );
		}

		if ( gmState != old_gm_state ) {
			reevaluate_state = true;
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);

			// If we were calling a gahp call that used RSL, we're done
			// with it now, so clear it.
			RSL.clear();
			if ( stageList ) {
				delete stageList;
				stageList = NULL;
			}
			if ( stageLocalList ) {
				delete stageLocalList;
				stageLocalList = NULL;
			}
		}

	} while ( reevaluate_state );
}

BaseResource *ArcJob::GetResource()
{
	return (BaseResource *)myResource;
}

void ArcJob::SetRemoteJobId( const char *job_id )
{
	free( remoteJobId );
	if ( job_id ) {
		remoteJobId = strdup( job_id );
	} else {
		remoteJobId = NULL;
	}

	std::string full_job_id;
	if ( job_id ) {
		formatstr( full_job_id, "arc %s %s", resourceManagerString,
							 job_id );
	}
	BaseJob::SetRemoteJobId( full_job_id.c_str() );
}

static
bool AppendEnvVar(void* pv, const std::string & var, const std::string & val)
{
	std::string &rsl = *(std::string*)pv;
	rsl += "<Environment><Name>";
	rsl += escapeXML(var);
	rsl += "</Name><Value>";
	rsl += escapeXML(val);
	rsl += "</Value></Environment>";
	return true;
}

bool ArcJob::buildJobADL()
{
	bool transfer_exec = true;
	StringList *stage_list = NULL;
	std::string attr_value;
	std::string iwd;
	std::string executable;
	std::string resources;
	std::string slot_req;
	long int_value;
	char *file;

	RSL.clear();

	if ( jobAd->LookupString( ATTR_JOB_IWD, iwd ) != 1 ) {
		formatstr(errorString, "%s not defined", ATTR_JOB_IWD);
		RSL.clear();
		return false;
	}

	// The ADL description of a job consists of three primary elements
	// (under the top-level <ActivityDescription> element):
	// <Application>
	//   Describes how to run the job (executable, arguments, stdin/out/err).
	// <Resources>
	//   Describes the environment the job needs to run in (physical
	//   resources, batch system parameters).
	//   Includes special RunTimeEnvironment labels that trigger arbitrary
	//   additional configuration of the execution envrionment.
	// <DataStaging>
	//   Describes input/output files be to staged for the job.
	//   Includes delegated proxies and whether client or server will
	//   transfer files.

	//Start off the ADL and start the <Application> element
	RSL = "<?xml version=\"1.0\"?>";
	RSL += "<ActivityDescription xmlns=\"http://www.eu-emi.eu/es/2010/12/adl\" xmlns:ng-adl=\"http://www.nordugrid.org/es/2011/12/nordugrid-adl\">";
	RSL += "<Application>";

	//We're assuming all job clasads have a command attribute
	GetJobExecutable( jobAd, executable );
	jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer_exec );

	RSL += "<Executable><Path>";
	// If we're transferring the executable, strip off the path for the
	// remote machine, since it refers to the submit machine.
	if ( transfer_exec ) {
		RSL += escapeXML(condor_basename( executable.c_str() ));
	} else {
		RSL += escapeXML(executable);
	}
	RSL += "</Path>";

	ArgList args;
	MyString arg_errors;
	if(!args.AppendArgsFromClassAd(jobAd,&arg_errors)) {
		dprintf(D_ALWAYS,"(%d.%d) Failed to read job arguments: %s\n",
				procID.cluster, procID.proc, arg_errors.Value());
		formatstr(errorString,"Failed to read job arguments: %s\n",
				arg_errors.Value());
		RSL.clear();
		return false;
	}
	for(int i=0;i<args.Count();i++) {
		RSL += "<Argument>";
		RSL += escapeXML(args.GetArg(i));
		RSL += "</Argument>";
	}

	RSL += "</Executable>";

	if ( jobAd->LookupString( ATTR_JOB_INPUT, attr_value ) ) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value.c_str()) ) {
			RSL += "<Input>";
			RSL += escapeXML(condor_basename(attr_value.c_str()));
			RSL += "</Input>";
		}
	}

	if ( jobAd->LookupString( ATTR_JOB_OUTPUT, attr_value ) ) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value.c_str()) ) {
			RSL += "<Output>";
			RSL += escapeXML(REMOTE_STDOUT_NAME);
			RSL += "</Output>";
		}
	}

	if ( jobAd->LookupString( ATTR_JOB_ERROR, attr_value ) ) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value.c_str()) ) {
			RSL += "<Error>";
			RSL += escapeXML(REMOTE_STDERR_NAME);
			RSL += "</Error>";
		}
	}

	Env envobj;
	std::string env_errors;
	if (!envobj.MergeFrom(jobAd, env_errors)) {
		dprintf(D_ALWAYS,"(%d.%d) Failed to read job environment: %s\n",
		        procID.cluster, procID.proc, env_errors.c_str());
		formatstr(errorString, "Failed to read job environment: %s\n",
		          arg_errors.c_str());
		RSL.clear();
		return false;
	}
	envobj.Walk(&AppendEnvVar, &RSL);

	// Add additional Application elements from the ArcApplication
	// job attribute.
	if ( jobAd->LookupString( ATTR_ARC_APPLICATION, attr_value ) ) {
		RSL += attr_value;
	}

	RSL += "</Application>";

	// Now handle the <Resources> element.
	// This needs to merge ADL elements derived from standard job ad
	// attributes and arbitrary ADL elements from the ArcResources job
	// attribute.
	jobAd->LookupString( ATTR_ARC_RESOURCES, resources );

	// <NumberOfSlots> goes under <SlotRequirement>.
	// If ArcResources contains <SlotRequiment> with other elements under it,
	// ensure we end up with only one <SlotRequirment>.
	if ( jobAd->LookupInteger(ATTR_REQUEST_CPUS, int_value) && int_value > 1 ) {
		slot_req += "<NumberOfSlots>";
		slot_req += std::to_string(int_value);
		slot_req += "</NumberOfSlots>";
		slot_req += "<SlotsPerHost>";
		slot_req += std::to_string(int_value);
		slot_req += "</SlotsPerHost>";
	}
	if ( ! slot_req.empty() ) {
		size_t insert_pos = resources.find("</SlotRequirement>");
		if ( insert_pos == std::string::npos ) {
			resources += "<SlotRequirement>";
			resources += slot_req;
			resources += "</SlotRequirement>";
		} else {
			resources.insert(insert_pos, slot_req);
		}
	}

	if ( jobAd->LookupInteger(ATTR_REQUEST_MEMORY, int_value) &&
	     resources.find("<IndividualPhysicalMemory>") == std::string::npos ) {
		resources += "<IndividualPhysicalMemory>";
		resources += std::to_string(int_value*1024*1024);
		resources += "</IndividualPhysicalMemory>";
	}

	if ( jobAd->LookupString(ATTR_BATCH_QUEUE, attr_value) ) {
		resources += "<QueueName>";
		resources += attr_value;
		resources += "</QueueName>";
	}

	if ( jobAd->LookupString(ATTR_ARC_RTE, attr_value) ) {
		const char *next_rte;
		StringList rte_list(attr_value, ",");
		rte_list.rewind();
		while ( (next_rte = rte_list.next()) ) {
			const char *next_opt;
			StringList rte_opts(next_rte, " ");
			rte_opts.rewind();
			next_opt = rte_opts.next();
			if ( next_opt == nullptr ) {
				// This shouldn't happen, but let's be safe
				continue;
			}
			resources += "<RunTimeEnvironment>";
			resources += "<Name>";
			resources += next_opt;
			resources += "</Name>";
			while ( (next_opt = rte_opts.next()) ) {
				resources += "<Option>";
				resources += next_opt;
				resources += "</Option>";
			}
			resources += "</RunTimeEnvironment>";
		}
	}

	RSL += "<Resources>";
//	RSL += "<RuntimeEnvironment>";
//	RSL += "<Name>ENV/PROXY</Name>";
//	RSL += "<Option>USE_DELEGATION_DB</Option>";
//	RSL += "</RuntimeEnvironment>";
	RSL += resources;
	RSL += "</Resources>";

	// Now handle the <DataStaging> element.
	RSL += "<DataStaging>";
	if ( ! delegationId.empty() ) {
		RSL += "<ng-adl:DelegationID>";
		RSL += escapeXML(delegationId);
		RSL += "</ng-adl:DelegationID>";
	}

	stage_list = buildStageInList();

	stage_list->rewind();
	while ( (file = stage_list->next()) != NULL ) {
		RSL += "<InputFile><Name>";
		RSL += escapeXML(condor_basename(file));
		RSL += "</Name>";
		if ( transfer_exec && ! strcmp(executable.c_str(), file) ) {
			RSL += "<IsExecutable>true</IsExecutable>";
		}
		RSL += "</InputFile>";
	}

	delete stage_list;
	stage_list = NULL;

	stage_list = buildStageOutList();

	stage_list->rewind();
	while ( (file = stage_list->next()) != NULL ) {
		RSL += "<OutputFile><Name>";
		RSL += escapeXML(condor_basename(file));
		RSL += "</Name></OutputFile>";
	}

	delete stage_list;
	stage_list = NULL;

	RSL += "</DataStaging>";
	RSL += "</ActivityDescription>";

dprintf(D_FULLDEBUG,"*** ADL='%s'\n",RSL.c_str());
	return true;
}

bool ArcJob::buildSubmitRSL()
{
	bool transfer_exec = true;
	StringList *stage_list = NULL;
	StringList *stage_local_list = NULL;
	char *attr_value = NULL;
	std::string rsl_suffix;
	std::string iwd;
	std::string executable;

	RSL.clear();

	if ( jobAd->LookupString( ATTR_ARC_RSL, rsl_suffix ) &&
						   rsl_suffix[0] == '&' ) {
		RSL = rsl_suffix;
		return true;
	}

	if ( jobAd->LookupString( ATTR_JOB_IWD, iwd ) != 1 ) {
		errorString = "ATTR_JOB_IWD not defined";
		RSL.clear();
		return false;
	}

	//Start off the RSL
	attr_value = param( "FULL_HOSTNAME" );
//	formatstr( RSL, "&(savestate=yes)(action=request)(hostname=%s)(DelegationID=%s)", attr_value, delegationId.c_str() );
	formatstr( RSL, "&(savestate=yes)(action=request)(hostname=%s)", attr_value );
	free( attr_value );
	attr_value = NULL;

	//We're assuming all job clasads have a command attribute
	GetJobExecutable( jobAd, executable );
	jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer_exec );

	RSL += "(executable=";
	// If we're transferring the executable, strip off the path for the
	// remote machine, since it refers to the submit machine.
	if ( transfer_exec ) {
		RSL += condor_basename( executable.c_str() );
	} else {
		RSL += executable;
	}

	{
		ArgList args;
		MyString arg_errors;
		MyString rsl_args;
		if(!args.AppendArgsFromClassAd(jobAd,&arg_errors)) {
			dprintf(D_ALWAYS,"(%d.%d) Failed to read job arguments: %s\n",
					procID.cluster, procID.proc, arg_errors.Value());
			formatstr(errorString,"Failed to read job arguments: %s\n",
					arg_errors.Value());
			RSL.clear();
			return false;
		}
		if(args.Count() != 0) {
			if(args.InputWasV1()) {
					// In V1 syntax, the user's input _is_ RSL
				if(!args.GetArgsStringV1Raw(&rsl_args,&arg_errors)) {
					dprintf(D_ALWAYS,
							"(%d.%d) Failed to get job arguments: %s\n",
							procID.cluster,procID.proc,arg_errors.Value());
					formatstr(errorString,"Failed to get job arguments: %s\n",
							arg_errors.Value());
					RSL.clear();
					return false;
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
			RSL += ")(arguments=";
			RSL += rsl_args;
		}
	}

	// If we're transferring the executable, tell ARC to set the
	// execute bit on the transferred executable.
	if ( transfer_exec ) {
		RSL += ")(executables=";
		RSL += condor_basename( executable.c_str() );
	}

	if ( jobAd->LookupString( ATTR_JOB_INPUT, &attr_value ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value) ) {
			RSL += ")(stdin=";
			RSL += condor_basename(attr_value);
		}
		free( attr_value );
		attr_value = NULL;
	}

	stage_list = buildStageInList();

	if ( stage_list->isEmpty() == false ) {
		char *file;
		stage_list->rewind();

		RSL += ")(inputfiles=";

		while ( (file = stage_list->next()) != NULL ) {
			RSL += "(";
			RSL += condor_basename(file);
			if ( IsUrl( file ) ) {
				formatstr_cat( RSL, " \"%s\")", file );
			} else {
				RSL += " \"\")";
			}
		}
	}

	delete stage_list;
	stage_list = NULL;

	if ( jobAd->LookupString( ATTR_JOB_OUTPUT, &attr_value ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value) ) {
			RSL += ")(stdout=";
			RSL += REMOTE_STDOUT_NAME;
		}
		free( attr_value );
		attr_value = NULL;
	}

	if ( jobAd->LookupString( ATTR_JOB_ERROR, &attr_value ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value) ) {
			RSL += ")(stderr=";
			RSL += REMOTE_STDERR_NAME;
		}
		free( attr_value );
	}

	stage_list = buildStageOutList();
	stage_local_list = buildStageOutLocalList( stage_list );

	if ( stage_list->isEmpty() == false ) {
		char *file;
		char *local_file;
		stage_list->rewind();
		stage_local_list->rewind();

		RSL += ")(outputfiles=";

		while ( (file = stage_list->next()) != NULL ) {
			local_file = stage_local_list->next();
			RSL += "(";
			RSL += condor_basename(file);
			if ( IsUrl( local_file ) ) {
				formatstr_cat( RSL, " \"%s\")", local_file );
			} else {
				RSL += " \"\")";
			}
		}
	}

	delete stage_list;
	stage_list = NULL;
	delete stage_local_list;
	stage_local_list = NULL;

	RSL += ')';

	if ( !rsl_suffix.empty() ) {
		RSL += rsl_suffix;
	}

dprintf(D_FULLDEBUG,"*** RSL='%s'\n",RSL.c_str());
	return true;
}

StringList *ArcJob::buildStageInList()
{
	StringList *tmp_list = NULL;
	StringList *stage_list = NULL;
	char *filename = NULL;
	std::string buf;
	std::string iwd;
	bool transfer = true;

	if ( jobAd->LookupString( ATTR_JOB_IWD, iwd ) ) {
		if ( iwd.length() > 1 && iwd[iwd.length() - 1] != '/' ) {
			iwd += '/';
		}
	}

	jobAd->LookupString( ATTR_TRANSFER_INPUT_FILES, buf );
	tmp_list = new StringList( buf.c_str(), "," );

	jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer );
	if ( transfer ) {
		GetJobExecutable( jobAd, buf );
		if ( !tmp_list->file_contains( buf.c_str() ) ) {
			tmp_list->append( buf.c_str() );
		}
	}

	transfer = true;
	jobAd->LookupBool( ATTR_TRANSFER_INPUT, transfer );
	if ( transfer && jobAd->LookupString( ATTR_JOB_INPUT, buf ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(buf.c_str()) ) {
			if ( !tmp_list->file_contains( buf.c_str() ) ) {
				tmp_list->append( buf.c_str() );
			}
		}
	}

	stage_list = new StringList;

	tmp_list->rewind();
	while ( ( filename = tmp_list->next() ) ) {
		if ( filename[0] == '/' || IsUrl( filename ) ) {
			formatstr( buf, "%s", filename );
		} else {
			formatstr( buf, "%s%s", iwd.c_str(), filename );
		}
		stage_list->append( buf.c_str() );
	}

	delete tmp_list;

	return stage_list;
}

StringList *ArcJob::buildStageOutList()
{
	StringList *stage_list = NULL;
	std::string buf;
	bool transfer = true;

	jobAd->LookupString( ATTR_TRANSFER_OUTPUT_FILES, buf );
	stage_list = new StringList( buf.c_str(), "," );

	jobAd->LookupBool( ATTR_TRANSFER_OUTPUT, transfer );
	if ( transfer && jobAd->LookupString( ATTR_JOB_OUTPUT, buf ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(buf.c_str()) ) {
			if ( !stage_list->file_contains( REMOTE_STDOUT_NAME ) ) {
				stage_list->append( REMOTE_STDOUT_NAME );
			}
		}
	}

	transfer = true;
	jobAd->LookupBool( ATTR_TRANSFER_ERROR, transfer );
	if ( transfer && jobAd->LookupString( ATTR_JOB_ERROR, buf ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(buf.c_str()) ) {
			if ( !stage_list->file_contains( REMOTE_STDERR_NAME ) ) {
				stage_list->append( REMOTE_STDERR_NAME );
			}
		}
	}

	return stage_list;
}

StringList *ArcJob::buildStageOutLocalList( StringList *stage_list )
{
	StringList *stage_local_list;
	char *remaps = NULL;
	MyString local_name;
	char *remote_name;
	std::string stdout_name = "";
	std::string stderr_name = "";
	std::string buff;
	std::string iwd = "/";

	if ( jobAd->LookupString( ATTR_JOB_IWD, iwd ) ) {
		if ( iwd.length() > 1 && iwd[iwd.length() - 1] != '/' ) {
			iwd += '/';
		}
	}

	jobAd->LookupString( ATTR_JOB_OUTPUT, stdout_name );
	jobAd->LookupString( ATTR_JOB_ERROR, stderr_name );

	stage_local_list = new StringList;

	jobAd->LookupString( ATTR_TRANSFER_OUTPUT_REMAPS, &remaps );

	stage_list->rewind();
	while ( (remote_name = stage_list->next()) ) {
			// stdout and stderr don't get remapped, and their paths
			// are evaluated locally
		if ( strcmp( REMOTE_STDOUT_NAME, remote_name ) == 0 ) {
			local_name = stdout_name.c_str();
		} else if ( strcmp( REMOTE_STDERR_NAME, remote_name ) == 0 ) {
			local_name = stderr_name.c_str();
		} else if( remaps && filename_remap_find( remaps, remote_name,
												  local_name ) ) {
				// file is remapped
		} else {
			local_name = condor_basename( remote_name );
		}

		if ( (local_name.Length() && local_name[0] == '/')
			 || IsUrl( local_name.Value() ) ) {
			buff = local_name;
		} else {
			formatstr( buff, "%s%s", iwd.c_str(), local_name.Value() );
		}
		stage_local_list->append( buff.c_str() );
	}

	if ( remaps ) {
		free( remaps );
	}

	return stage_local_list;
}

void ArcJob::NotifyNewRemoteStatus( const char *status )
{
	if ( SetRemoteJobStatus( status ) ) {
		remoteJobState = status;
		SetEvaluateState();

		if ( condorState == IDLE &&
			 ( remoteJobState == REMOTE_STATE_RUNNING ||
			   remoteJobState == REMOTE_STATE_EXITINGLRMS ||
			   remoteJobState == REMOTE_STATE_EXECUTED ||
			   remoteJobState == REMOTE_STATE_FINISHING ||
			   remoteJobState == REMOTE_STATE_FINISHED ||
			   remoteJobState == REMOTE_STATE_FAILED ) ) {
			JobRunning();
		} else if ( condorState == RUNNING &&
					( remoteJobState == REMOTE_STATE_QUEUING ||
					  remoteJobState == REMOTE_STATE_HELD ) ) {
			JobIdle();
		}
	}
	if ( gmState == GM_RECOVER_QUERY ) {
		SetEvaluateState();
	}
}

