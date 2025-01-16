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
#include <algorithm>

#include "globus_utils.h"
#include "gridmanager.h"
#include "arcjob.h"
#include "condor_config.h"


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

int ArcJob::m_arcGahpId = 0;

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
							 (CallbackType)&BaseJob::SetEvaluateState, this );

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
	snprintf( buff, sizeof(buff), "ARC/%d/%s#%s",
	          m_arcGahpId,
	          (m_tokenFile.empty() && jobProxy) ? jobProxy->subject->fqan : "",
	          m_tokenFile.c_str() );
	gahp = new GahpClient( buff, gahp_path );
#if defined(CURL_USES_NSS)
	// NSS as used by libcurl has a memory leak. To deal with this, we
	// start a new arc_gahp for new jobs when the previous gahp has
	// handled too many requests. Once the previous jobs leave the system,
	// the old arc_gahp will quietly exit.
	if (gahp->getNextReqId() > param_integer("ARC_GAHP_COMMAND_LIMIT", 1000)) {
		delete gahp;
		m_arcGahpId++;
		snprintf(buff, sizeof(buff), "ARC/%d/%s#%s",
		         m_arcGahpId,
		         (m_tokenFile.empty() && jobProxy) ? jobProxy->subject->fqan : "",
		          m_tokenFile.c_str());
		gahp = new GahpClient(buff, gahp_path);
	}
#endif
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
	                 m_arcGahpId, m_tokenFile.empty() ? jobProxy : nullptr, m_tokenFile );
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
		ReleaseProxy( jobProxy, (CallbackType)&BaseJob::SetEvaluateState, this );
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

void ArcJob::doEvaluateState( int /* timerID */ )
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
					if ( ! buildJobADL() ) {
						gmState = GM_HOLD;
						break;
					}
				}
				rc = gahp->arc_job_new(
									resourceManagerString,
									RSL,
									!delegationId.empty(),
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
				stageList = buildStageInList(false);
			}
			if ( stageList->empty() ) {
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
				break;
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

			// Element State is a list of strings, one of which is of
			// the form "arcrest:XXX", where XXX is the job's status
			// as reported in the rest of the REST interface.
			// If that status isn't one of the terminal states, then
			// we received stale data, and we should re-query after a
			// short delay.
			classad::ExprTree *expr = info_ad.Lookup("State");
			if (expr == nullptr || expr->GetKind() != classad::ExprTree::EXPR_LIST_NODE) {
				errorString = "Job exit information is missing State element";
				dprintf(D_ALWAYS, "(%d.%d) Job exit information is missing State element\n",
				        procID.cluster, procID.proc);
				gmState = GM_CANCEL_CLEAN;
				break;
			}
			std::string info_status;
			std::vector<classad::ExprTree*> state_list;
			((classad::ExprList*)expr)->GetComponents(state_list);
			for (auto item : state_list) {
				if (dynamic_cast<classad::StringLiteral *>(item) != nullptr) {
					classad::StringLiteral *sl = (classad::StringLiteral *)item;
					std::string str = sl->getCString();
					if (str.compare(0, 8, "arcrest:") == 0) {
						info_status = str.substr(8);
						break;
					}
				}
			}
			if (info_status.empty()) {
				errorString = "Job exit information is missing arcrest element";
				dprintf(D_ALWAYS, "(%d.%d) Job exit information is missing arcrest element\n",
				        procID.cluster, procID.proc);
				gmState = GM_CANCEL_CLEAN;
				break;
			}
			if (info_status != REMOTE_STATE_FINISHED &&
			    info_status != REMOTE_STATE_FAILED &&
			    info_status != REMOTE_STATE_KILLED &&
			    info_status != REMOTE_STATE_WIPED)
			{
				dprintf(D_FULLDEBUG, "(%d.%d) Job info is stale, will retry in %ds.\n", procID.cluster, procID.proc, jobInfoInterval);
				daemonCore->Reset_Timer( evaluateStateTid, (lastJobInfoTime + jobInfoInterval) - now );
				break;
			}
			if (info_status != remoteJobState) {
				remoteJobState = info_status;
				SetRemoteJobStatus(info_status.c_str());
			}

			// With some LRMS, the job is FAILED if it has a non-zero exit
			// code. So use the presence of an ExitCode attribute to
			// determine whether the job ran to completion.
			if (info_ad.LookupString("ExitCode", val)) {
				int exit_code = atoi(val.c_str());
				int wallclock = 0;
				int cpu = 0;

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
				jobAd->Assign( ATTR_JOB_REMOTE_WALL_CLOCK, wallclock );
				jobAd->Assign( ATTR_JOB_REMOTE_USER_CPU, cpu );
				gmState = GM_STAGE_OUT;
			} else {
				if ( info_ad.LookupString("Error", val) ) {
					errorString = "ARC job failed: " + val;
				} else {
					errorString = "ARC job failed for unknown reason";
				}
				gmState = GM_CANCEL_CLEAN;
			}
			} break;
		case GM_STAGE_OUT: {
			if ( stageList == NULL ) {
				stageList = buildStageOutList();
			}
			if ( stageLocalList == NULL ) {
				stageLocalList = buildStageOutLocalList( stageList );
			}
			if ( stageList->empty() ) {
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

			// For now, put problem jobs on hold instead of
			// forgetting about current submission and trying again.
			// TODO: Let our action here be dictated by the user preference
			// expressed in the job ad.
			if (remoteJobId != NULL && condorState != REMOVED) {
				gmState = GM_HOLD;
				break;
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
	std::vector<std::string> *stage_list = NULL;
	std::string attr_value;
	std::string iwd;
	std::string executable;
	std::string resources;
	std::string slot_req;
	long int_value;

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
	//   Includes special RuntimeEnvironment labels that trigger arbitrary
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
	std::string arg_errors;
	if(!args.AppendArgsFromClassAd(jobAd,arg_errors)) {
		dprintf(D_ALWAYS,"(%d.%d) Failed to read job arguments: %s\n",
				procID.cluster, procID.proc, arg_errors.c_str());
		formatstr(errorString,"Failed to read job arguments: %s\n",
				arg_errors.c_str());
		RSL.clear();
		return false;
	}
	for(size_t i=0;i<args.Count();i++) {
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
		for (const auto& next_rte : StringTokenIterator(attr_value, ",")) {
			bool first_time = true;
			resources += "<RuntimeEnvironment>";
			for (const auto& next_opt : StringTokenIterator(next_rte, " ")) {
				if (first_time) {
					first_time = false;
					resources += "<Name>";
					resources += next_opt;
					resources += "</Name>";
				} else {
					resources += "<Option>";
					resources += next_opt;
					resources += "</Option>";
				}
			}
			resources += "</RuntimeEnvironment>";
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

	stage_list = buildStageInList(true);

	for (const auto& file : *stage_list) {
		RSL += "<InputFile><Name>";
		RSL += escapeXML(condor_basename(file.c_str()));
		RSL += "</Name>";
		if ( transfer_exec && ! strcmp(executable.c_str(), file.c_str()) ) {
			RSL += "<IsExecutable>true</IsExecutable>";
		}
		RSL += "</InputFile>";
	}

	delete stage_list;
	stage_list = NULL;

	stage_list = buildStageOutList();

	for (const auto& file : *stage_list) {
		RSL += "<OutputFile><Name>";
		RSL += escapeXML(condor_basename(file.c_str()));
		RSL += "</Name></OutputFile>";
	}

	delete stage_list;
	stage_list = NULL;

	// Add additional DataStaging elements from the ArcDataStaging
	// job attribute.
	if ( jobAd->LookupString( ATTR_ARC_DATA_STAGING, attr_value ) ) {
		RSL += attr_value;
	}

	RSL += "</DataStaging>";
	RSL += "</ActivityDescription>";

dprintf(D_FULLDEBUG,"*** ADL='%s'\n",RSL.c_str());
	return true;
}

std::vector<std::string> *ArcJob::buildStageInList(bool with_urls)
{
	std::vector<std::string> *stage_list = NULL;
	std::string buf;
	std::string iwd;
	bool transfer = true;

	if ( jobAd->LookupString( ATTR_JOB_IWD, iwd ) ) {
		if ( iwd.length() > 1 && iwd[iwd.length() - 1] != '/' ) {
			iwd += '/';
		}
	}

	jobAd->LookupString( ATTR_TRANSFER_INPUT_FILES, buf );
	stage_list = new std::vector<std::string>;
	*stage_list = split(buf);

	jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer );
	if ( transfer ) {
		GetJobExecutable( jobAd, buf );
		if ( !file_contains(*stage_list, buf.c_str()) ) {
			stage_list->emplace_back(buf);
		}
	}

	transfer = true;
	jobAd->LookupBool( ATTR_TRANSFER_INPUT, transfer );
	if ( transfer && jobAd->LookupString( ATTR_JOB_INPUT, buf ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(buf.c_str()) ) {
			if ( !file_contains(*stage_list, buf.c_str()) ) {
				stage_list->emplace_back(buf);
			}
		}
	}

	if (!with_urls) {
		auto no_urls = [](std::string& file) { return IsUrl(file.c_str()) != nullptr; };
		stage_list->erase(std::remove_if(stage_list->begin(),
		                                 stage_list->end(), no_urls),
		                  stage_list->end());
	}

	for (auto& filename : *stage_list) {
		if ( filename[0] != '/' && !IsUrl(filename.c_str()) ) {
			filename.insert(0, iwd);
		}
	}

	return stage_list;
}

std::vector<std::string> *ArcJob::buildStageOutList()
{
	std::vector<std::string> *stage_list = NULL;
	std::string buf;
	bool transfer = true;

	jobAd->LookupString( ATTR_TRANSFER_OUTPUT_FILES, buf );
	stage_list = new std::vector<std::string>;
	*stage_list = split(buf);

	jobAd->LookupBool( ATTR_TRANSFER_OUTPUT, transfer );
	if ( transfer && jobAd->LookupString( ATTR_JOB_OUTPUT, buf ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(buf.c_str()) ) {
			if ( !file_contains(*stage_list, REMOTE_STDOUT_NAME) ) {
				stage_list->emplace_back( REMOTE_STDOUT_NAME );
			}
		}
	}

	transfer = true;
	jobAd->LookupBool( ATTR_TRANSFER_ERROR, transfer );
	if ( transfer && jobAd->LookupString( ATTR_JOB_ERROR, buf ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(buf.c_str()) ) {
			if ( !file_contains(*stage_list, REMOTE_STDERR_NAME) ) {
				stage_list->emplace_back( REMOTE_STDERR_NAME );
			}
		}
	}

	return stage_list;
}

std::vector<std::string> *ArcJob::buildStageOutLocalList( std::vector<std::string> *stage_list )
{
	std::vector<std::string> *stage_local_list;
	char *remaps = NULL;
	std::string local_name;
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

	stage_local_list = new std::vector<std::string>;

	jobAd->LookupString( ATTR_TRANSFER_OUTPUT_REMAPS, &remaps );

	for (const auto& remote_name : *stage_list) {
			// stdout and stderr don't get remapped, and their paths
			// are evaluated locally
		if ( strcmp( REMOTE_STDOUT_NAME, remote_name.c_str() ) == 0 ) {
			local_name = stdout_name;
		} else if ( strcmp( REMOTE_STDERR_NAME, remote_name.c_str() ) == 0 ) {
			local_name = stderr_name;
		} else if( remaps && filename_remap_find( remaps, remote_name.c_str(),
												  local_name ) ) {
				// file is remapped
		} else {
			local_name = condor_basename( remote_name.c_str() );
		}

		if ( (local_name.length() && local_name[0] == '/')
			 || IsUrl( local_name.c_str() ) ) {
			buff = local_name;
		} else {
			formatstr( buff, "%s%s", iwd.c_str(), local_name.c_str() );
		}
		stage_local_list->emplace_back(buff);
	}

	if ( remaps ) {
		free( remaps );
	}

	// Remove items from both lists where the destination file is a URL
	size_t i = 0;
	while (i < stage_list->size()) {
		if (IsUrl((*stage_local_list)[i].c_str())) {
			stage_list->erase(stage_list->begin()+i);
			stage_local_list->erase(stage_local_list->begin()+i);
		} else {
			i++;
		}
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

