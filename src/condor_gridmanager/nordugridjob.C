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
	"GM_RECOVER_QUERY"
};

#define TASK_IN_PROGRESS	1
#define TASK_DONE			2
#define TASK_FAILED			3
#define TASK_QUEUED			4

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

#define STAGE_COMPLETE_FILE	".condor_complete"
#define NORDUGRID_LOG_DIR ".nordugrid_log"

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define HASH_TABLE_SIZE			500

HashTable <HashKey, NordugridJob *> JobsByRemoteId( HASH_TABLE_SIZE,
													hashFunction );

void
rehashRemoteJobId( NordugridJob *job, const char *old_id,
				   const char *new_id )
{
	if ( old_id ) {
		JobsByRemoteId.remove(HashKey(old_id));
	}
	if ( new_id ) {
		JobsByRemoteId.insert(HashKey(new_id), job);
	}
}

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

const char *NordugridJobAdConst = "JobUniverse =?= 9 && (JobGridType == \"nordugrid\") =?= True";

bool NordugridJobAdMustExpand( const ClassAd *jobad )
{
	int must_expand = 0;

	jobad->LookupBool(ATTR_JOB_MUST_EXPAND, must_expand);

	return must_expand != 0;
}

BaseJob *NordugridJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new NordugridJob( jobad );
}

int NordugridJob::probeInterval = 300;	// default value
int NordugridJob::submitInterval = 300;	// default value

NordugridJob::NordugridJob( ClassAd *classad )
	: BaseJob( classad )
{
	char buff[4096];
	char *error_string = NULL;

	remoteJobId = NULL;
	remoteJobState = REMOTE_STATE_UNKNOWN;
	gmState = GM_INIT;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	resourceManagerString = NULL;
	ftp_srvr = NULL;
	stage_list = NULL;
	myResource = NULL;

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		UpdateJobAd( ATTR_HOLD_REASON, "UNDEFINED" );
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GLOBUS_RESOURCE, buff );
	if ( buff[0] != '\0' ) {
		resourceManagerString = strdup( buff );
	} else {
		error_string = "GlobusResource is not set in the job ad";
		goto error_exit;
	}

	myResource = NordugridResource::FindOrCreateResource( resourceManagerString );
	myResource->RegisterJob( this );

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GLOBUS_CONTACT_STRING, buff );
	if ( buff[0] != '\0' && strcmp( buff, NULL_JOB_CONTACT ) != 0 ) {
		rehashRemoteJobId( this, remoteJobId, buff );
		remoteJobId = strdup( buff );
	}

	return;

 error_exit:
	gmState = GM_HOLD;
	if ( error_string ) {
		UpdateJobAdString( ATTR_HOLD_REASON, error_string );
	}
	return;
}

NordugridJob::~NordugridJob()
{
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	if ( remoteJobId ) {
		rehashRemoteJobId( this, remoteJobId, NULL );
		free( remoteJobId );
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
	time_t now;	// make sure you set this before every use!!!

	bool done;
	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, condorState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],condorState);

	do {
		reevaluate_state = false;
		old_gm_state = gmState;

		switch ( gmState ) {
		case GM_INIT: {
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
			bool need_stage_in;

			rc = doStageInQuery( need_stage_in );
			if ( rc == TASK_QUEUED || rc == TASK_IN_PROGRESS ) {
				break;
			} else if ( rc == TASK_FAILED ) {
				dprintf( D_ALWAYS, "(%d.%d) file stage in query failed: %s\n",
						 procID.cluster, procID.proc, errorString.Value() );
				gmState = GM_CANCEL;
			} else {
				if ( need_stage_in ) {
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
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
//				UpdateJobAdString( ATTR_HOLD_REASON,
//									"Attempts to submit failed" );
				gmState = GM_HOLD;
				break;
			}
			now = time(NULL);
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {

				char *job_id = NULL;

				rc = doSubmit( job_id );

				if ( rc == TASK_QUEUED || rc == TASK_IN_PROGRESS ) {
					break;
				}

				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;

				if ( rc == TASK_DONE ) {
					ASSERT( job_id != NULL );
					rehashRemoteJobId( this, remoteJobId, job_id );
					remoteJobId = job_id;
					UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
									   job_id );
					gmState = GM_SUBMIT_SAVE;
				} else {
					dprintf(D_ALWAYS,"(%d.%d) job submit failed: %s\n",
							procID.cluster, procID.proc, errorString.Value());
					myResource->ReleaseConnection( this );
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
			rc = doStageIn();
			if ( rc == TASK_QUEUED || rc == TASK_IN_PROGRESS ) {
				break;
			} else if ( rc == TASK_FAILED ) {
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
				now = time(NULL);
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
				myResource->ReleaseConnection( this );
				gmState = GM_CANCEL;
			} else {
				int new_remote_state;
				rc = doStatus( new_remote_state );
				if ( rc == TASK_QUEUED ) {
					break;
				} else if ( rc == TASK_FAILED ) {
					// What to do about failure?
					dprintf( D_ALWAYS, "(%d.%d) job probe failed: %s\n",
							 procID.cluster, procID.proc,
							 errorString.Value() );
				} else {
					remoteJobState = new_remote_state;
					//requestScheddUpdate( this );
				}
				lastProbeTime = now;
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_EXIT_INFO: {
			rc = doExitInfo();
			if ( rc == TASK_QUEUED || rc == TASK_IN_PROGRESS ) {
				break;
			} else if ( rc == TASK_FAILED ) {
				dprintf( D_ALWAYS, "(%d.%d) exit info gathering failed: %s\n",
						 procID.cluster, procID.proc, errorString.Value() );
				gmState = GM_CANCEL;
			} else {
				gmState = GM_STAGE_OUT;
			}
			} break;
		case GM_STAGE_OUT: {
			rc = doStageOut();
			if ( rc == TASK_QUEUED || rc == TASK_IN_PROGRESS ) {
				break;
			} else if ( rc == TASK_FAILED ) {
				dprintf( D_ALWAYS, "(%d.%d) file stage out failed: %s\n",
						 procID.cluster, procID.proc, errorString.Value() );
				gmState = GM_CANCEL;
			} else {
				gmState = GM_DONE_SAVE;
			}
			} break;
		case GM_DONE_SAVE: {
			if ( condorState != HELD && condorState != REMOVED ) {
				if ( normalExit ) {
					UpdateJobAdBool( ATTR_ON_EXIT_BY_SIGNAL, 0 );
					UpdateJobAdInt( ATTR_ON_EXIT_CODE, exitCode );
				} else {
					UpdateJobAdBool( ATTR_ON_EXIT_BY_SIGNAL, 1 );
					UpdateJobAdInt( ATTR_ON_EXIT_SIGNAL, exitCode );
				}
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
			rc = doRemove();
			if ( rc == TASK_QUEUED ) {
				break;
			} else if ( rc == TASK_FAILED ) {
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
					rehashRemoteJobId( this, remoteJobId, NULL );
					free( remoteJobId );
					remoteJobId = NULL;
					UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
									   NULL_JOB_CONTACT );
					requestScheddUpdate( this );
				}
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			rc = doRemove();
			if ( rc == TASK_QUEUED ) {
				break;
			} else if ( rc == TASK_DONE ) {
				gmState = GM_FAILED;
			} else {
				// What to do about a failed cancel?
				dprintf( D_ALWAYS, "(%d.%d) job cancel failed: %s\n",
						 procID.cluster, procID.proc, errorString.Value() );
				gmState = GM_FAILED;
			}
			} break;
		case GM_FAILED: {
			rehashRemoteJobId( this, remoteJobId, NULL );
			free( remoteJobId );
			remoteJobId = NULL;
			UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
							   NULL_JOB_CONTACT );
			requestScheddUpdate( this );

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
				rehashRemoteJobId( this, remoteJobId, NULL );
				free( remoteJobId );
				remoteJobId = NULL;
				UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
								   NULL_JOB_CONTACT );
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
					UpdateJobAdBool( ATTR_JOB_MATCHED, 0 );
					UpdateJobAdInt( ATTR_CURRENT_HOSTS, 0 );
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
		}
		if ( gmState != old_gm_state ) {
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
		}

	} while ( reevaluate_state );

	return TRUE;
}

BaseResource *NordugridJob::GetResource()
{
	return (BaseResource *)myResource;
}

int NordugridJob::doSubmit( char *&job_id )
{
	FILE *ftp_put_fp = NULL;
	MyString *rsl = NULL;
	size_t rsl_len;
	char *job_dir = NULL;
	char *tmp_job_id = NULL;
	MyString buff;
	int rc;

	rc = myResource->AcquireConnection( this, ftp_srvr );
	if ( rc == ACQUIRE_QUEUED ) {
		return TASK_QUEUED;
	} else if ( rc == ACQUIRE_FAILED ) {
		return TASK_FAILED;
	}

	rsl = buildSubmitRSL();
	rsl_len = strlen( rsl->Value() );

	if ( ftp_lite_change_dir( ftp_srvr, "/jobs/new" ) == 0 ) {
		errorString.sprintf( "ftp_lite_change_dir(/jobs/new) failed, errno=%d",
							 errno );
		goto doSubmit_error_exit;
	}

	if ( ftp_lite_print_dir( ftp_srvr, &job_dir ) == 0 ) {
		errorString.sprintf( "ftp_lite_print_dir() failed, errno=%d", errno );
		goto doSubmit_error_exit;
	}

	tmp_job_id = strrchr( job_dir, '/' );
	if ( tmp_job_id == NULL ) {
		errorString = "strrchr() failed";
		goto doSubmit_error_exit;
	}
	tmp_job_id++;

	ftp_put_fp = ftp_lite_put( ftp_srvr, "/jobs/new/job", 0,
							   FTP_LITE_WHOLE_FILE );
	if ( ftp_put_fp == NULL ) {
		errorString.sprintf( "ftp_lite_put() failed, errno=%d", errno );
		goto doSubmit_error_exit;
	}

	if ( fwrite( rsl->Value(), 1, rsl_len, ftp_put_fp ) != rsl_len ) {
		errorString = "fwrite() failed";
		goto doSubmit_error_exit;
	}

	fclose( ftp_put_fp );
	ftp_put_fp = NULL;

	if ( ftp_lite_done( ftp_srvr ) == 0 ) {
		errorString.sprintf( "ftp_lite_done() failed, errno=%d", errno );
		goto doSubmit_error_exit;
	}

	if ( ftp_lite_change_dir( ftp_srvr, "/jobs" ) == 0 ) {
		errorString.sprintf( "ftp_lite_change_dir(/jobs) failed, errno=%d",
							 errno );
		goto doSubmit_error_exit;
	}

	job_id = strdup( tmp_job_id );
	free( job_dir );
	delete rsl;
	myResource->ReleaseConnection( this );
	return TASK_DONE;

 doSubmit_error_exit:
	if ( ftp_put_fp != NULL ) {
		fclose( ftp_put_fp );
	}
	if ( job_dir != NULL ) {
		free( job_dir );
	}
	if ( rsl != NULL ) {
		delete rsl;
	}
	myResource->ReleaseConnection( this );

	return TASK_FAILED;
}

int NordugridJob::doStatus( int &new_remote_state )
{
	MyString filename;
	char *status_buff = NULL;
	int status_len = 0;
	FILE *status_fp = NULL;
	MyString buff;
	int rc;

	rc = myResource->AcquireConnection( this, ftp_srvr );
	if ( rc == ACQUIRE_QUEUED ) {
		return TASK_QUEUED;
	} else if ( rc == ACQUIRE_FAILED ) {
		return TASK_FAILED;
	}

	filename.sprintf( "/jobs/%s/%s/status", remoteJobId, NORDUGRID_LOG_DIR );

	status_fp = ftp_lite_get( ftp_srvr, filename.Value(), 0 );
	if ( status_fp == NULL ) {
		errorString.sprintf( "ftp_lite_get() failed, errno=%d", errno );
		goto doStatus_error_exit;
	}

	status_len = ftp_lite_stream_to_buffer( status_fp, &status_buff );
	if ( status_len == -1 ) {
		errorString.sprintf( "ftp_lite_stream_to_buffer() failed, errno=%d",
							 errno );
		goto doStatus_error_exit;
	}

	fclose( status_fp );
	status_fp = NULL;

	if ( ftp_lite_done( ftp_srvr ) == 0 ) {
		errorString.sprintf( "ftp_lite_done() failed, errno=%d", errno );
		goto doStatus_error_exit;
	}

	myResource->ReleaseConnection( this );

	status_buff[status_len-1] = '\0';
	new_remote_state = remoteStateNameConvert( status_buff );
	if ( new_remote_state == REMOTE_STATE_UNKNOWN ) {
		errorString.sprintf( "invalid job status of '%s'", status_buff );
		goto doStatus_error_exit;
	}

	return TASK_DONE;

 doStatus_error_exit:
	if ( status_buff != NULL ) {
		free( status_buff );
	}
	if ( status_fp != NULL ) {
		fclose( status_fp );
	}
	myResource->ReleaseConnection( this );

	return TASK_FAILED;
}

int NordugridJob::doRemove()
{
	MyString dir;
	MyString buff;
	int rc;

	rc = myResource->AcquireConnection( this, ftp_srvr );
	if ( rc == ACQUIRE_QUEUED ) {
		return TASK_QUEUED;
	} else if ( rc == ACQUIRE_FAILED ) {
		return TASK_FAILED;
	}

	dir.sprintf( "/jobs/%s", remoteJobId );
	if ( ftp_lite_delete_dir( ftp_srvr, dir.Value() ) == 0 ) {
		errorString.sprintf( "ftp_lite_delete_dir() failed, errno=%d", errno );
		goto doRemove_error_exit;
	}

	myResource->ReleaseConnection( this );

	return TASK_DONE;

 doRemove_error_exit:
	myResource->ReleaseConnection( this );

	return TASK_FAILED;
}

int NordugridJob::doStageIn()
{
	FILE *curr_file_fp = NULL;
	FILE *curr_ftp_fp = NULL;
	char *curr_filename = NULL;
	int rc;

	if ( stage_list == NULL ) {
		char *buf = NULL;
		int transfer_exec = TRUE;

		jobAd->LookupString( ATTR_TRANSFER_INPUT_FILES, &buf );
		stage_list = new StringList( buf, "," );
		if ( buf != NULL ) {
			free( buf );
		}

		jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer_exec );
		if ( transfer_exec ) {
			jobAd->LookupString( ATTR_JOB_CMD, &buf );
			if ( !stage_list->file_contains( buf ) ) {
				stage_list->append( buf );
			}
			free( buf );
		}

		if ( jobAd->LookupString( ATTR_JOB_INPUT, &buf ) == 1) {
			// only add to list if not NULL_FILE (i.e. /dev/null)
			if ( ! nullFile(buf) ) {
				if ( !stage_list->file_contains( buf ) ) {
					stage_list->append( buf );			
				}
			}
			free( buf );
		}

		stage_list->append( STAGE_COMPLETE_FILE );

		stage_list->rewind();
	}


	rc = myResource->AcquireConnection( this, ftp_srvr );
	if ( rc == ACQUIRE_QUEUED ) {
		return TASK_QUEUED;
	} else if ( rc == ACQUIRE_FAILED ) {
		return TASK_FAILED;
	}

	curr_filename = stage_list->next();

	if ( curr_filename != NULL ) {

		MyString full_filename;
		if ( strcmp( curr_filename, STAGE_COMPLETE_FILE ) == 0 ) {
			full_filename = "/dev/null";
		} else {
			if ( curr_filename[0] != DIR_DELIM_CHAR ) {
				char *iwd = NULL;
				jobAd->LookupString( ATTR_JOB_IWD, &iwd );
				full_filename.sprintf( "%s%c%s", iwd, DIR_DELIM_CHAR,
									   curr_filename );
				free( iwd );
			} else {
				full_filename = curr_filename;
			}
		}
		curr_file_fp = fopen( full_filename.Value(), "r" );
		if ( curr_file_fp == NULL ) {
			errorString = "fopen failed";
			goto doStageIn_error_exit;
		}

full_filename.sprintf("/jobs/%s",remoteJobId);
if ( ftp_lite_change_dir( ftp_srvr, full_filename.Value() ) == 0 ) {
errorString.sprintf( "ftp_lite_change_dir() failed, errno=%d", errno );
goto doStageIn_error_exit;
}

		full_filename.sprintf( "/jobs/%s/%s", remoteJobId,
							   basename(curr_filename) );
		curr_ftp_fp = ftp_lite_put( ftp_srvr, full_filename.Value(), 0,
									FTP_LITE_WHOLE_FILE );
		if ( curr_ftp_fp == NULL ) {
			errorString.sprintf( "ftp_lite_put() failed, errno=%d", errno );
			goto doStageIn_error_exit;
		}

		if ( ftp_lite_stream_to_stream( curr_file_fp, curr_ftp_fp ) == -1 ) {
			errorString.sprintf( "ftp_lite_stream_to_stream failed, errno=%d",
								 errno );
			goto doStageIn_error_exit;
		}

		fclose( curr_file_fp );
		curr_file_fp = NULL;

		fclose( curr_ftp_fp );
		curr_ftp_fp = NULL;

		if ( ftp_lite_done( ftp_srvr ) == 0 ) {
			errorString.sprintf( "ftp_lite_done() failed, errno=%d", errno );
			goto doStageIn_error_exit;
		}

		SetEvaluateState();
		return TASK_IN_PROGRESS;
	}

	delete stage_list;
	stage_list = NULL;
	myResource->ReleaseConnection( this );

	return TASK_DONE;

 doStageIn_error_exit:
	if ( curr_file_fp != NULL ) {
		fclose( curr_file_fp );
	}
	if ( curr_ftp_fp != NULL ) {
		fclose( curr_ftp_fp );
	}
	if ( stage_list != NULL ) {
		delete stage_list;
		stage_list = NULL;
	}
	myResource->ReleaseConnection( this );
	return TASK_FAILED;
}

int NordugridJob::doStageOut()
{
	FILE *curr_file_fp = NULL;
	FILE *curr_ftp_fp = NULL;
	char *curr_filename = NULL;
	int rc;

	if ( stage_list == NULL ) {
		char *buf = NULL;

		jobAd->LookupString( ATTR_TRANSFER_OUTPUT_FILES, &buf );
		stage_list = new StringList( buf, "," );
		if ( buf != NULL ) {
			free( buf );
		}

		if ( jobAd->LookupString( ATTR_JOB_OUTPUT, &buf ) == 1) {
			// only add to list if not NULL_FILE (i.e. /dev/null)
			if ( ! nullFile(buf) ) {
				if ( !stage_list->file_contains( buf ) ) {
					stage_list->append( buf );			
				}
			}
			free( buf );
		}

		if ( jobAd->LookupString( ATTR_JOB_ERROR, &buf ) == 1) {
			// only add to list if not NULL_FILE (i.e. /dev/null)
			if ( ! nullFile(buf) ) {
				if ( !stage_list->file_contains( buf ) ) {
					stage_list->append( buf );			
				}
			}
			free( buf );
		}

		stage_list->rewind();
	}

	rc = myResource->AcquireConnection( this, ftp_srvr );
	if ( rc == ACQUIRE_QUEUED ) {
		return TASK_QUEUED;
	} else if ( rc == ACQUIRE_FAILED ) {
		return TASK_FAILED;
	}

	curr_filename = stage_list->next();

	if ( curr_filename != NULL ) {

		MyString full_filename;
		if ( curr_filename[0] != DIR_DELIM_CHAR ) {
			char *iwd = NULL;
			jobAd->LookupString( ATTR_JOB_IWD, &iwd );
			full_filename.sprintf( "%s%c%s", iwd, DIR_DELIM_CHAR,
								   curr_filename );
			free( iwd );
		} else {
			full_filename = curr_filename;
		}
		curr_file_fp = fopen( full_filename.Value(), "w" );
		if ( curr_file_fp == NULL ) {
			errorString = "fopen failed";
			goto doStageOut_error_exit;
		}

		full_filename.sprintf( "/jobs/%s/%s", remoteJobId,
							   basename(curr_filename) );
		curr_ftp_fp = ftp_lite_get( ftp_srvr, full_filename.Value(), 0 );
		if ( curr_ftp_fp == NULL ) {
			errorString.sprintf( "ftp_lite_get() failed, errno=%d", errno );
			goto doStageOut_error_exit;
		}

		if ( ftp_lite_stream_to_stream( curr_ftp_fp, curr_file_fp ) == -1 ) {
			errorString.sprintf( "ftp_lite_stream_to_stream failed, errno=%d",
								 errno );
			goto doStageOut_error_exit;
		}

		fclose( curr_file_fp );
		curr_file_fp = NULL;

		fclose( curr_ftp_fp );
		curr_ftp_fp = NULL;

		if ( ftp_lite_done( ftp_srvr ) == 0 ) {
			errorString.sprintf( "ftp_lite_done() failed, errno=%d", errno );
			goto doStageOut_error_exit;
		}

		SetEvaluateState();
		return TASK_IN_PROGRESS;
	}

	delete stage_list;
	stage_list = NULL;
	myResource->ReleaseConnection( this );

	return TASK_DONE;

 doStageOut_error_exit:
	if ( curr_file_fp != NULL ) {
		fclose( curr_file_fp );
	}
	if ( curr_ftp_fp != NULL ) {
		fclose( curr_ftp_fp );
	}
	myResource->ReleaseConnection( this );
	if ( stage_list != NULL ) {
		delete stage_list;
		stage_list = NULL;
	}
	return TASK_FAILED;
}

int NordugridJob::doExitInfo()
{
	MyString diag_filename;
	char diag_buff[256];
	FILE *diag_fp = NULL;
	int rc;

	rc = myResource->AcquireConnection( this, ftp_srvr );
	if ( rc == ACQUIRE_QUEUED ) {
		return TASK_QUEUED;
	} else if ( rc == ACQUIRE_FAILED ) {
		return TASK_FAILED;
	}

	diag_filename.sprintf( "/jobs/%s/%s/diag", remoteJobId,
						   NORDUGRID_LOG_DIR );

	diag_fp = ftp_lite_get( ftp_srvr, diag_filename.Value(), 0 );
	if ( diag_fp == NULL ) {
		errorString.sprintf( "ftp_lite_get() failed, errno=%d", errno );
		goto doExitInfo_error_exit;
	}

		// line "runtimeenvironments="
	if ( fgets( diag_buff, sizeof(diag_buff), diag_fp ) == NULL ) {
		errorString = "fgets() on diag failed";
		goto doExitInfo_error_exit;
	}
		// line "nodename=<hostname>"
	if ( fgets( diag_buff, sizeof(diag_buff), diag_fp ) == NULL ) {
		errorString = "fgets() on diag failed";
		goto doExitInfo_error_exit;
	}
		// not sure what this line will be yet...
	if ( fgets( diag_buff, sizeof(diag_buff), diag_fp ) == NULL ) {
		errorString = "fgets() on diag failed";
		goto doExitInfo_error_exit;
	}
	if ( sscanf( diag_buff, "Command exited with non-zero status %d",
				 &exitCode ) == 1 ) {
		normalExit = true;

		if ( fgets( diag_buff, sizeof(diag_buff), diag_fp ) == NULL ) {
			errorString = "fgets() on diag failed";
			goto doExitInfo_error_exit;
		}
	} else if ( sscanf( diag_buff, "Command terminated by signal %d",
						&exitCode ) == 1 ) {
		normalExit = false;

		if ( fgets( diag_buff, sizeof(diag_buff), diag_fp ) == NULL ) {
			errorString = "fgets() on diag failed";
			goto doExitInfo_error_exit;
		}
	} else {
		normalExit = true;
		exitCode = 0;
	}
		// now we've just read "WallTime=<float>s"
		// we can read more if we want, but not for now

	fclose( diag_fp );
	diag_fp = NULL;

	if ( ftp_lite_done( ftp_srvr ) == 0 ) {
		errorString.sprintf( "ftp_lite_done() failed, errno=%d", errno );
		goto doExitInfo_error_exit;
	}

	myResource->ReleaseConnection( this );

	return TASK_DONE;

 doExitInfo_error_exit:
	if ( diag_fp != NULL ) {
		fclose( diag_fp );
	}
	myResource->ReleaseConnection( this );

	return TASK_FAILED;
}

int NordugridJob::doStageInQuery( bool &need_stage_in )
{
	int rc;
	MyString dir_name;
	StringList *dir_list = NULL;

	dir_name.sprintf( "/jobs/%s", remoteJobId );

	rc = doList( dir_name.Value(), dir_list );
	if ( rc != TASK_DONE ) {
		return rc;
	}

	if ( dir_list->contains( STAGE_COMPLETE_FILE ) == true ) {
		need_stage_in = false;
	} else {
		need_stage_in = true;
	}

	delete dir_list;

	return TASK_DONE;
}

int NordugridJob::doList( const char *dir_name, StringList *&dir_list )
{
	char list_buff[256];
	FILE *list_fp = NULL;
	StringList *my_dir_list = NULL;
	int rc;

	rc = myResource->AcquireConnection( this, ftp_srvr );
	if ( rc == ACQUIRE_QUEUED ) {
		return TASK_QUEUED;
	} else if ( rc == ACQUIRE_FAILED ) {
		return TASK_FAILED;
	}

	list_fp = ftp_lite_list( ftp_srvr, dir_name );
	if ( list_fp == NULL ) {
		errorString.sprintf( "ftp_lite_get() failed, errno=%d", errno );
		goto doList_error_exit;
	}

	my_dir_list = new StringList;

	while ( fgets( list_buff, sizeof(list_buff), list_fp ) != NULL ) {
		int len = strlen( list_buff );
		if ( list_buff[len-2] == '\015' && list_buff[len-1] == '\012' ) {
			list_buff[len-2] = '\0';
		} else if ( list_buff[len-1] == '\n' ) {
			list_buff[len-1] = '\0';
		}
		my_dir_list->append( list_buff );
	}

	fclose( list_fp );
	list_fp = NULL;

	if ( ftp_lite_done( ftp_srvr ) == 0 ) {
		errorString.sprintf( "ftp_lite_done() failed, errno=%d", errno );
		goto doList_error_exit;
	}

	myResource->ReleaseConnection( this );

	dir_list = my_dir_list;

	return TASK_DONE;

 doList_error_exit:
	if ( my_dir_list != NULL ) {
		delete my_dir_list;
	}
	if ( list_fp != NULL ) {
		fclose( list_fp );
	}
	myResource->ReleaseConnection( this );

	return TASK_FAILED;
}

MyString *NordugridJob::buildSubmitRSL()
{
	int transfer_exec = TRUE;
	MyString *rsl = new MyString;
	MyString buff;
	StringList stage_in_list( NULL, "," );
	StringList stage_out_list( NULL, "," );
	char *attr_value = NULL;
	char *rsl_suffix = NULL;
	char *iwd = NULL;
	char *executable = NULL;

	if ( jobAd->LookupString( ATTR_GLOBUS_RSL, &rsl_suffix ) &&
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
	rsl->sprintf( "&(savestate=yes)(action=request)(lrmstype=pbs)(hostname=nostos.cs.wisc.edu)(gmlog=%s)", NORDUGRID_LOG_DIR );

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

	jobAd->LookupString( ATTR_TRANSFER_INPUT_FILES, &attr_value );
	if ( attr_value != NULL ) {
		stage_in_list.initializeFromString( attr_value );
		free( attr_value );
		attr_value = NULL;
	}

	if ( jobAd->LookupString( ATTR_JOB_INPUT, &attr_value ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value) ) {
			*rsl += ")(stdin=";
			*rsl += basename(attr_value);
			if ( !stage_in_list.file_contains( attr_value ) ) {
				stage_in_list.append( attr_value );
			}
		}
		free( attr_value );
		attr_value = NULL;
	}

	if ( transfer_exec ) {
		if ( !stage_in_list.file_contains( executable ) ) {
			stage_in_list.append( executable );
		}
	}

	if ( stage_in_list.isEmpty() == false ) {
		char *file;
		stage_in_list.rewind();

		*rsl += ")(inputfiles=";

		while ( (file = stage_in_list.next()) != NULL ) {
			*rsl += "(";
			*rsl += basename(file);
			*rsl += " \"\")";
		}
	}

	jobAd->LookupString( ATTR_TRANSFER_OUTPUT_FILES, &attr_value );
	if ( attr_value != NULL ) {
		stage_out_list.initializeFromString( attr_value );
		free( attr_value );
		attr_value = NULL;
	}

	if ( jobAd->LookupString( ATTR_JOB_OUTPUT, &attr_value ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value) ) {
			*rsl += ")(stdout=";
			*rsl += basename(attr_value);
			if ( !stage_out_list.file_contains( attr_value ) ) {
				stage_out_list.append( attr_value );
			}
		}
		free( attr_value );
		attr_value = NULL;
	}

	if ( jobAd->LookupString( ATTR_JOB_ERROR, &attr_value ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value) ) {
			*rsl += ")(stderr=";
			*rsl += basename(attr_value);
			if ( !stage_out_list.file_contains( attr_value ) ) {
				stage_out_list.append( attr_value );
			}
		}
		free( attr_value );
	}

	if ( stage_out_list.isEmpty() == false ) {
		char *file;
		stage_out_list.rewind();

		*rsl += ")(outputfiles=";

		while ( (file = stage_out_list.next()) != NULL ) {
			*rsl += "(";
			*rsl += basename(file);
			*rsl += " \"\")";
		}
	}

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

