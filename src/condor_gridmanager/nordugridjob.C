/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "environ.h"  // for Environ object
#include "condor_string.h"	// for strnewp and friends
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"
#include "nullfile.h"

#include "globus_utils.h"
#include "gridmanager.h"
#include "nordugridjob.h"
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

static char *GMStateNames[] = {
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
	"GM_START"
};

#define REMOTE_STATE_UNKNOWN		0
#define REMOTE_STATE_ACCEPTED		1
#define REMOTE_STATE_PREPARING		2
#define REMOTE_STATE_SUBMITTING		3
#define REMOTE_STATE_INLRMS			4
#define REMOTE_STATE_CANCELLING		5
#define REMOTE_STATE_FINISHING		6
#define REMOTE_STATE_FINISHED		7
#define REMOTE_STATE_PENDING		8

// Filenames are case insensitive on Win32, but case sensitive on Unix
#ifdef WIN32
#	define file_strcmp _stricmp
#	define file_contains contains_anycase
#else
#	define file_strcmp strcmp
#	define file_contains contains
#endif

#define NORDUGRID_LOG_DIR ".nordugrid_log"

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

static
int remoteStateNameConvert( const char *name )
{
	if ( strcmp( name, "ACCEPTED" ) == 0 ) {
		return REMOTE_STATE_ACCEPTED;
	} else if ( strcmp( name, "PREPARING" ) == 0 ) {
		return REMOTE_STATE_PREPARING;
	} else if ( strcmp( name, "SUBMITTING" ) == 0 ) {
		return REMOTE_STATE_SUBMITTING;
	} else if ( strcmp( name, "INLRMS" ) == 0 ) {
		return REMOTE_STATE_INLRMS;
	} else if ( strcmp( name, "CANCELLING" ) == 0 ) {
		return REMOTE_STATE_CANCELLING;
	} else if ( strcmp( name, "FINISHING" ) == 0 ) {
		return REMOTE_STATE_FINISHING;
	} else if ( strcmp( name, "FINISHED" ) == 0 ) {
		return REMOTE_STATE_FINISHED;
	} else if ( strcmp( name, "PENDING:PREPARING" ) == 0 ) {
		return REMOTE_STATE_PENDING;
	} else {
		return REMOTE_STATE_UNKNOWN;
	}
}

void NordugridJobInit()
{
}

void NordugridJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	NordugridJob::setProbeInterval( tmp_int );
}

bool NordugridJobAdMatch( const ClassAd *job_ad ) {
	int universe;
	MyString resource;
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 strncasecmp( resource.Value(), "nordugrid ", 10 ) == 0 ) {

		return true;
	}
	return false;
}

BaseJob *NordugridJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new NordugridJob( jobad );
}

int NordugridJob::probeInterval = 300;	// default value
int NordugridJob::submitInterval = 300;	// default value
int NordugridJob::gahpCallTimeout = 300;	// default value
int NordugridJob::maxConnectFailures = 3;	// default value

NordugridJob::NordugridJob( ClassAd *classad )
	: BaseJob( classad )
{
	char buff[4096];
	char *error_string = NULL;
	char *gahp_path = NULL;

	remoteJobId = NULL;
	remoteJobState = REMOTE_STATE_UNKNOWN;
	gmState = GM_INIT;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	resourceManagerString = NULL;
	jobProxy = NULL;
	myResource = NULL;
	gahp = NULL;
	RSL = NULL;
	stageList = NULL;

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start (unless the job is already held).
	if ( condorState != HELD &&
		 jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {

		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_X509_USER_PROXY, buff );
	if ( buff[0] != '\0' ) {
		jobProxy = AcquireProxy( buff, evaluateStateTid );
		if ( jobProxy == NULL ) {
			dprintf( D_ALWAYS, "(%d.%d) error acquiring proxy!\n",
					 procID.cluster, procID.proc );
			error_string = "Failed to acquire proxy";
			goto error_exit;
		}
	} else {
		dprintf( D_ALWAYS, "(%d.%d) %s not set in job ad!\n",
				 procID.cluster, procID.proc, ATTR_X509_USER_PROXY );
		error_string = "x509userproxy is not set in the job ad";
		goto error_exit;
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_RESOURCE, buff );
	if ( buff[0] != '\0' ) {
		const char *token;
		MyString str = buff;

		str.Tokenize();

		token = str.GetNextToken( " ", false );
		if ( !token || stricmp( token, "nordugrid" ) ) {
			error_string = "GridResource not of type nordugrid";
			goto error_exit;
		}

		token = str.GetNextToken( " ", false );
		if ( token && *token ) {
			resourceManagerString = strdup( token );
		} else {
			error_string = "GridResource missing server name";
			goto error_exit;
		}

	} else {
		error_string = "GridResource is not set in the job ad";
		goto error_exit;
	}

	myResource = NordugridResource::FindOrCreateResource( resourceManagerString,
														  jobProxy->subject->subject_name );
	myResource->RegisterJob( this );

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff );
	if ( strrchr( buff, ' ' ) ) {
		SetRemoteJobId( strrchr( buff, ' ' ) + 1 );
	} else {
		SetRemoteJobId( NULL );
	}

	gahp_path = param( "NORDUGRID_GAHP" );
	if ( gahp_path == NULL ) {
		error_string = "NORDUGRID_GAHP not defined";
		goto error_exit;
	}
	snprintf( buff, sizeof(buff), "NORDUGRID/%s",
			  jobProxy->subject->subject_name );
	gahp = new GahpClient( buff, gahp_path );
	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	return;

 error_exit:
	gmState = GM_HOLD;
	if ( error_string ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string );
	}
	return;
}

NordugridJob::~NordugridJob()
{
	if ( jobProxy != NULL ) {
		ReleaseProxy( jobProxy, evaluateStateTid );
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
	if ( RSL ) {
		delete RSL;
	}
	if ( stageList ) {
		delete stageList;
	}
}

void NordugridJob::Reconfig()
{
	BaseJob::Reconfig();
}

int NordugridJob::doEvaluateState()
{
	int old_gm_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	bool done;
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
			if ( gahp->Initialize( jobProxy ) == false ) {
				dprintf( D_ALWAYS, "(%d.%d) Error initializing GAHP\n",
						 procID.cluster, procID.proc );

				jobAd->Assign( ATTR_HOLD_REASON,
							   "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			gahp->setDelegProxy( jobProxy );

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

				gmState = GM_RECOVER_QUERY;
			}
			} break;
		case GM_RECOVER_QUERY: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				char *new_status = NULL;
				rc = gahp->nordugrid_status( resourceManagerString,
											 remoteJobId, new_status );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				} else if ( rc != 0 ) {
					// What to do about failure?
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS, "(%d.%d) job probe failed: %s\n",
							 procID.cluster, procID.proc,
							 errorString.Value() );
					gmState = GM_CANCEL;
					break;
				} else {
					remoteJobState = remoteStateNameConvert( new_status );
					//requestScheddUpdate( this );
				}
				if ( new_status ) {
					free( new_status );
				}
				lastProbeTime = now;
				if ( remoteJobState == REMOTE_STATE_ACCEPTED ||
					 remoteJobState == REMOTE_STATE_PREPARING ) {
					gmState = GM_STAGE_IN;
				} else {
					gmState = GM_SUBMITTED;
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
				gmState = GM_SUBMIT;
			}
			} break;
		case GM_SUBMIT: {
			if ( condorState == REMOVED || condorState == HELD ) {
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

				char *job_id = NULL;

				if ( RSL == NULL ) {
					RSL = buildSubmitRSL();
				}
				if ( RSL == NULL ) {
					gmState = GM_HOLD;
					break;
				}
				rc = gahp->nordugrid_submit( 
										resourceManagerString,
										RSL->Value(),
										job_id );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}

				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;

				if ( rc == 0 ) {
					ASSERT( job_id != NULL );
					SetRemoteJobId( job_id );
					free( job_id );
					gmState = GM_SUBMIT_SAVE;
				} else {
					errorString = gahp->getErrorString();
					dprintf(D_ALWAYS,"(%d.%d) job submit failed: %s\n",
							procID.cluster, procID.proc,
							errorString.Value() );
					gmState = GM_UNSUBMITTED;
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
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
				gmState = GM_STAGE_IN;
			}
			} break;
		case GM_STAGE_IN: {
			if ( stageList == NULL ) {
				stageList = buildStageInList();
			}
			rc = gahp->nordugrid_stage_in( resourceManagerString, remoteJobId,
										   *stageList );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			} else if ( rc != 0 ) {
				errorString = gahp->getErrorString();
				dprintf( D_ALWAYS, "(%d.%d) file stage in failed: %s\n",
						 procID.cluster, procID.proc, errorString.Value() );
				gmState = GM_CANCEL;
			} else {
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_SUBMITTED: {
			if ( remoteJobState == REMOTE_STATE_FINISHED ) {
					gmState = GM_EXIT_INFO;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				if ( lastProbeTime < enteredCurrentGmState ) {
					lastProbeTime = enteredCurrentGmState;
				}
				if ( probeNow ) {
					lastProbeTime = 0;
					probeNow = false;
				}
				if ( now >= lastProbeTime + probeInterval ) {
					gmState = GM_PROBE_JOB;
					break;
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + probeInterval) > now ) {
					delay = (lastProbeTime + probeInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_PROBE_JOB: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				char *new_status = NULL;
				rc = gahp->nordugrid_status( resourceManagerString,
											 remoteJobId, new_status );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				} else if ( rc != 0 ) {
					// What to do about failure?
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS, "(%d.%d) job probe failed: %s\n",
							 procID.cluster, procID.proc,
							 errorString.Value() );
				} else {
					remoteJobState = remoteStateNameConvert( new_status );
					//requestScheddUpdate( this );
				}
				if ( new_status ) {
					free( new_status );
				}
				lastProbeTime = now;
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_EXIT_INFO: {
			bool normal_exit;
			int exit_code;
			float wallclock = 0;
			float sys_cpu = 0;
			float user_cpu = 0;
			rc = gahp->nordugrid_exit_info( resourceManagerString, remoteJobId,
											normal_exit, exit_code, wallclock,
											sys_cpu, user_cpu );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			} else if ( rc != 0 ) {
				errorString = gahp->getErrorString();
				dprintf( D_ALWAYS, "(%d.%d) exit info gathering failed: %s\n",
						 procID.cluster, procID.proc, errorString.Value() );
				gmState = GM_CANCEL;
			} else {
				normalExit = normal_exit;
				exitCode = exit_code;
				if ( normalExit ) {
					jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );
					jobAd->Assign( ATTR_ON_EXIT_CODE, exitCode );
				} else {
					jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, true );
					jobAd->Assign( ATTR_ON_EXIT_SIGNAL, exitCode );
				}
				jobAd->Assign( ATTR_JOB_REMOTE_WALL_CLOCK, wallclock );
				jobAd->Assign( ATTR_JOB_REMOTE_SYS_CPU, sys_cpu );
				jobAd->Assign( ATTR_JOB_REMOTE_USER_CPU, user_cpu );
				gmState = GM_STAGE_OUT;
			}
			} break;
		case GM_STAGE_OUT: {
			if ( stageList == NULL ) {
				stageList = buildStageOutList();
			}
			rc = gahp->nordugrid_stage_out( resourceManagerString, remoteJobId,
											*stageList );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			} else if ( rc != 0 ) {
				errorString = gahp->getErrorString();
				dprintf( D_ALWAYS, "(%d.%d) file stage out failed: %s\n",
						 procID.cluster, procID.proc, errorString.Value() );
				gmState = GM_CANCEL;
			} else {
				gmState = GM_DONE_SAVE;
			}
			} break;
		case GM_DONE_SAVE: {
			if ( condorState != HELD && condorState != REMOVED ) {
				JobTerminated();
				if ( condorState == COMPLETED ) {
					done = requestScheddUpdate( this );
					if ( !done ) {
						break;
					}
				}
			}
			gmState = GM_DONE_COMMIT;
			} break;
		case GM_DONE_COMMIT: {
			rc = gahp->nordugrid_cancel( resourceManagerString, remoteJobId );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			} else if ( rc != 0 ) {
				errorString = gahp->getErrorString();
				dprintf( D_ALWAYS, "(%d.%d) job cleanup failed: %s\n",
						 procID.cluster, procID.proc, errorString.Value() );
				gmState = GM_HOLD;
				break;
			}
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
			rc = gahp->nordugrid_cancel( resourceManagerString, remoteJobId );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			} else if ( rc == 0 ) {
				gmState = GM_FAILED;
			} else {
				// What to do about a failed cancel?
				errorString = gahp->getErrorString();
				dprintf( D_ALWAYS, "(%d.%d) job cancel failed: %s\n",
						 procID.cluster, procID.proc, errorString.Value() );
				gmState = GM_FAILED;
			}
			} break;
		case GM_FAILED: {
			SetRemoteJobId( NULL );

			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				gmState = GM_CLEAR_REQUEST;
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
			remoteJobState = REMOTE_STATE_UNKNOWN;
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

		if ( gmState != old_gm_state ) {
			reevaluate_state = true;
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);

			// If we were calling a gahp call that used RSL, we're done
			// with it now, so free it.
			if ( RSL ) {
				delete RSL;
				RSL = NULL;
			}
			if ( stageList ) {
				delete stageList;
				stageList = NULL;
			}
		}

	} while ( reevaluate_state );

	return TRUE;
}

BaseResource *NordugridJob::GetResource()
{
	return (BaseResource *)myResource;
}

void NordugridJob::SetRemoteJobId( const char *job_id )
{
	free( remoteJobId );
	if ( job_id ) {
		remoteJobId = strdup( job_id );
	} else {
		remoteJobId = NULL;
	}

	MyString full_job_id;
	if ( job_id ) {
		full_job_id.sprintf( "nordugrid %s %s", resourceManagerString,
							 job_id );
	}
	BaseJob::SetRemoteJobId( full_job_id.Value() );
}

MyString *NordugridJob::buildSubmitRSL()
{
	int transfer_exec = TRUE;
	MyString *rsl = new MyString;
	MyString buff;
	StringList *stage_list = NULL;
	char *attr_value = NULL;
	char *rsl_suffix = NULL;
	char *iwd = NULL;
	char *executable = NULL;

	if ( jobAd->LookupString( ATTR_NORDUGRID_RSL, &rsl_suffix ) &&
						   rsl_suffix[0] == '&' ) {
		*rsl = rsl_suffix;
		free( rsl_suffix );
		return rsl;
	}

	if ( jobAd->LookupString( ATTR_JOB_IWD, &iwd ) != 1 ) {
		errorString = "ATTR_JOB_IWD not defined";
		if ( rsl_suffix != NULL ) {
			free( rsl_suffix );
		}
		return NULL;
	}

	//Start off the RSL
	attr_value = param( "FULL_HOSTNAME" );
	rsl->sprintf( "&(savestate=yes)(action=request)(lrmstype=pbs)(hostname=%s)", attr_value );
	free( attr_value );
	attr_value = NULL;

	//We're assuming all job clasads have a command attribute
	jobAd->LookupString( ATTR_JOB_CMD, &executable );
	jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer_exec );

	*rsl += "(arguments=";
	// If we're transferring the executable, strip off the path for the
	// remote machine, since it refers to the submit machine.
	if ( transfer_exec ) {
		*rsl += basename( executable );
	} else {
		*rsl += executable;
	}

	if ( jobAd->LookupString(ATTR_JOB_ARGUMENTS, &attr_value) && *attr_value ) {
		*rsl += " ";
		*rsl += attr_value;
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	// If we're transferring the executable, tell Nordugrid to set the
	// execute bit on the transferred executable.
	if ( transfer_exec ) {
		*rsl += ")(excutables=";
		*rsl += basename( executable );
	}

	if ( jobAd->LookupString( ATTR_JOB_INPUT, &attr_value ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value) ) {
			*rsl += ")(stdin=";
			*rsl += basename(attr_value);
		}
		free( attr_value );
		attr_value = NULL;
	}

	stage_list = buildStageInList();

	if ( stage_list->isEmpty() == false ) {
		char *file;
		stage_list->rewind();

		*rsl += ")(inputfiles=";

		while ( (file = stage_list->next()) != NULL ) {
			*rsl += "(";
			*rsl += condor_basename(file);
			*rsl += " \"\")";
		}
	}

	delete stage_list;
	stage_list = NULL;

	if ( jobAd->LookupString( ATTR_JOB_OUTPUT, &attr_value ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value) ) {
			*rsl += ")(stdout=";
			*rsl += basename(attr_value);
		}
		free( attr_value );
		attr_value = NULL;
	}

	if ( jobAd->LookupString( ATTR_JOB_ERROR, &attr_value ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value) ) {
			*rsl += ")(stderr=";
			*rsl += basename(attr_value);
		}
		free( attr_value );
	}

	stage_list = buildStageOutList();

	if ( stage_list->isEmpty() == false ) {
		char *file;
		stage_list->rewind();

		*rsl += ")(outputfiles=";

		while ( (file = stage_list->next()) != NULL ) {
			*rsl += "(";
			*rsl += condor_basename(file);
			*rsl += " \"\")";
		}
	}

	delete stage_list;
	stage_list = NULL;

	*rsl += ')';

	if ( rsl_suffix != NULL ) {
		*rsl += rsl_suffix;
		free( rsl_suffix );
	}

	free( executable );
	free( iwd );

dprintf(D_FULLDEBUG,"*** RSL='%s'\n",rsl->Value());
	return rsl;
}

StringList *NordugridJob::buildStageInList()
{
	StringList *tmp_list = NULL;
	StringList *stage_list = NULL;
	char *filename = NULL;
	MyString buf;
	MyString iwd;
	int transfer = TRUE;

	if ( jobAd->LookupString( ATTR_JOB_IWD, iwd ) ) {
		if ( iwd.Length() > 1 && iwd[iwd.Length() - 1] != '/' ) {
			iwd += '/';
		}
	}

	jobAd->LookupString( ATTR_TRANSFER_INPUT_FILES, buf );
	tmp_list = new StringList( buf.Value(), "," );

	jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer );
	if ( transfer ) {
		jobAd->LookupString( ATTR_JOB_CMD, buf );
		if ( !tmp_list->file_contains( buf.Value() ) ) {
			tmp_list->append( buf.Value() );
		}
	}

	transfer = TRUE;
	jobAd->LookupBool( ATTR_TRANSFER_INPUT, transfer );
	if ( transfer && jobAd->LookupString( ATTR_JOB_INPUT, buf ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(buf.Value()) ) {
			if ( !tmp_list->file_contains( buf.Value() ) ) {
				tmp_list->append( buf.Value() );
			}
		}
	}

	stage_list = new StringList;

	tmp_list->rewind();
	while ( ( filename = tmp_list->next() ) ) {
		if ( filename[0] == '/' ) {
			buf.sprintf( "%s", filename );
		} else {
			buf.sprintf( "%s%s", iwd.Value(), filename );
		}
		stage_list->append( buf.Value() );
	}

	delete tmp_list;

	return stage_list;
}

StringList *NordugridJob::buildStageOutList()
{
	StringList *tmp_list = NULL;
	StringList *stage_list = NULL;
	char *filename = NULL;
	MyString buf;
	MyString iwd = "/";
	bool transfer = TRUE;

	if ( jobAd->LookupString( ATTR_JOB_IWD, iwd ) ) {
		if ( iwd.Length() > 1 && iwd[iwd.Length() - 1] != '/' ) {
			iwd += '/';
		}
	}

	jobAd->LookupString( ATTR_TRANSFER_OUTPUT_FILES, buf );
	tmp_list = new StringList( buf.Value(), "," );

	jobAd->LookupBool( ATTR_TRANSFER_OUTPUT, transfer );
	if ( transfer && jobAd->LookupString( ATTR_JOB_OUTPUT, buf ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(buf.Value()) ) {
			if ( !tmp_list->file_contains( buf.Value() ) ) {
				tmp_list->append( buf.Value() );
			}
		}
	}

	transfer = TRUE;
	jobAd->LookupBool( ATTR_TRANSFER_ERROR, transfer );
	if ( transfer && jobAd->LookupString( ATTR_JOB_ERROR, buf ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(buf.Value()) ) {
			if ( !tmp_list->file_contains( buf.Value() ) ) {
				tmp_list->append( buf.Value() );
			}
		}
	}

	stage_list = new StringList;

	tmp_list->rewind();
	while ( ( filename = tmp_list->next() ) ) {
		if ( filename[0] == '/' ) {
			buf.sprintf( "%s", filename );
		} else {
			buf.sprintf( "%s%s", iwd.Value(), filename );
		}
		stage_list->append( buf.Value() );
	}

	delete tmp_list;

	return stage_list;
}
