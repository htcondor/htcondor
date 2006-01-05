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
#include "condor_string.h"	// for strnewp and friends
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_ckpt_name.h"
#include "daemon.h"
#include "dc_schedd.h"

#include "gridmanager.h"
#include "mirrorjob.h"
#include "condor_config.h"


// GridManager job states
#define GM_INIT					0
#define GM_REGISTER				1
#define GM_STDIO_UPDATE			2
#define GM_UNSUBMITTED			3
#define GM_SUBMIT				4
#define GM_SUBMIT_SAVE			5
#define GM_STAGE_IN				6
#define GM_SUBMITTED_MIRROR_INACTIVE	7
#define GM_DONE_SAVE			8
#define GM_DONE_COMMIT			9
#define GM_CANCEL_1				10
#define GM_FAILED				11
#define GM_DELETE				12
#define GM_CLEAR_REQUEST		13
#define GM_HOLD					14
#define GM_PROXY_EXPIRED		15
#define GM_REFRESH_PROXY		16
#define GM_START				17
#define GM_MIRROR_ACTIVE_SAVE	18
#define GM_VACATE_SCHEDD		19
#define GM_SUBMITTED_MIRROR_ACTIVE	20
#define GM_HOLD_MIRROR_ACTIVE	21
#define GM_HOLD_REMOTE_JOB		22
#define GM_RELEASE_REMOTE_JOB	23
#define GM_CANCEL_2				24
#define GM_VACATE_WAIT			25
#define GM_POLL_ACTIVE			26

static char *GMStateNames[] = {
	"GM_INIT",
	"GM_REGISTER",
	"GM_STDIO_UPDATE",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_STAGE_IN",
	"GM_SUBMITTED_MIRROR_INACTIVE",
	"GM_DONE_SAVE",
	"GM_DONE_COMMIT",
	"GM_CANCEL_1",
	"GM_FAILED",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROXY_EXPIRED",
	"GM_REFRESH_PROXY",
	"GM_START",
	"GM_MIRROR_ACTIVE_SAVE",
	"GM_VACATE_SCHEDD",
	"GM_SUBMITTED_MIRROR_ACTIVE",
	"GM_HOLD_MIRROR_ACTIVE",
	"GM_HOLD_REMOTE_JOB",
	"GM_RELEASE_REMOTE_JOB",
	"GM_CANCEL_2",
	"GM_VACATE_WAIT",
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


#define HASH_TABLE_SIZE			500

HashTable <HashKey, MirrorJob *> MirrorJobsById( HASH_TABLE_SIZE,
												 hashFunction );


void MirrorJobInit()
{
}

void MirrorJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "MIRROR_JOB_POLL_INTERVAL", 5 * 60 );
	MirrorResource::setPollInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	MirrorJob::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	MirrorJob::setConnectFailureRetry( tmp_int );

	tmp_int = param_integer("MIRROR_JOB_LEASE_INTERVAL",1800);
	MirrorJob::setLeaseInterval( tmp_int );
}

bool MirrorJobAdMatch( const ClassAd *job_ad ) {
	int universe;
	MyString schedd;
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_VANILLA &&
		 job_ad->LookupString( ATTR_MIRROR_SCHEDD, schedd ) ) {

		return true;
	}
	return false;
}

BaseJob *MirrorJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new MirrorJob( jobad );
}


static
ClassAd *ClassAdDiff( ClassAd *old_ad, ClassAd *new_ad )
{
	ClassAd *diff_ad;
	ExprTree *old_expr;
	ExprTree *new_expr;
	const char *next_name;
	StringList new_attr_names;

	if ( old_ad == NULL || new_ad == NULL ) {
		return NULL;
	}

	diff_ad = new ClassAd;

	new_ad->ResetName();
	while ( (next_name = new_ad->NextNameOriginal()) != NULL ) {
		new_attr_names.append( next_name );

		old_expr = old_ad->Lookup( next_name );
		new_expr = new_ad->Lookup( next_name );

		if ( new_expr == NULL ) {
			EXCEPT( "ClassAdDiff: new_expr is NULL" );
		}

		if ( ( old_expr == NULL &&
			   new_expr->RArg()->MyType() != LX_UNDEFINED ) ||
			 (*old_expr == *new_expr) == false ) {

			diff_ad->Insert( new_expr->DeepCopy() );
		}
	}

	old_ad->ResetName();
	while ( (next_name = old_ad->NextNameOriginal()) != NULL ) {
		MyString buff;
		if ( new_attr_names.contains_anycase( next_name ) == false && 
			 old_ad->Lookup( next_name )->RArg()->MyType() != LX_UNDEFINED ) {

			buff.sprintf( "%s = Undefined", next_name );
			diff_ad->Insert( buff.Value() );
		}
	}

	return diff_ad;
}

static
void ClassAdPatch( ClassAd *orig_ad, ClassAd *diff_ad )
{
	ExprTree *diff_expr;

	if ( orig_ad == NULL || diff_ad == NULL ) {
		return;
	}

	diff_ad->ResetExpr();
	while ( (diff_expr = diff_ad->NextExpr()) != NULL ) {
		orig_ad->Insert( diff_expr->DeepCopy() );
	}
}

int MirrorJob::submitInterval = 300;			// default value
int MirrorJob::gahpCallTimeout = 300;			// default value
int MirrorJob::maxConnectFailures = 3;			// default value
int MirrorJob::leaseInterval = 1800;			// default value

MirrorJob::MirrorJob( ClassAd *classad )
	: BaseJob( classad )
{
	int tmp;
	char buff[4096];
	MyString error_string = "";
	char *gahp_path;
	ArgList args;

	mirrorJobId.cluster = 0;
	gahpAd = NULL;
	gmState = GM_INIT;
	remoteState = JOB_STATE_UNKNOWN;
	enteredCurrentGmState = time(NULL);
	enteredCurrentRemoteState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	submitFailureCode = 0;
	mirrorScheddName = NULL;
	remoteJobIdString = NULL;
	mirrorActive = false;
	mirrorReleased = false;
	submitterId = NULL;
	myResource = NULL;
	newRemoteStatusAd = NULL;
	newRemoteStatusServerTime = 0;

	lastRemoteStatusServerTime = 0;

	gahp = NULL;

	// This is a BaseJob variable. At no time do we want BaseJob mucking
	// around with job runtime attributes, so set it to false and leave it
	// that way.
	calcRuntimeStats = false;

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_MIRROR_SCHEDD, buff );
	if ( buff[0] != '\0' ) {
		mirrorScheddName = strdup( buff );
	} else {
		error_string.sprintf( "%s is not set in the job ad",
							  ATTR_MIRROR_SCHEDD );
		goto error_exit;
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_MIRROR_JOB_ID, buff );
	if ( buff[0] != '\0' ) {
		SetRemoteJobId( buff );
	} else {
		remoteState = JOB_STATE_UNSUBMITTED;
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GLOBAL_JOB_ID, buff );
	if ( buff[0] != '\0' ) {
		char *ptr = strchr( buff, '#' );
		if ( ptr != NULL ) {
			*ptr = '\0';
		}
		submitterId = strdup( buff );
	} else {
		error_string.sprintf( "%s is not set in the job ad",
							  ATTR_GLOBAL_JOB_ID );
		goto error_exit;
	}

	myResource = MirrorResource::FindOrCreateResource( mirrorScheddName );
	myResource->RegisterJob( this, submitterId );

	gahp_path = param("MIRROR_GAHP");
	if ( gahp_path == NULL ) {
		gahp_path = param( "CONDOR_GAHP" );
		if ( gahp_path == NULL ) {
			error_string = "CONDOR_GAHP not defined";
			goto error_exit;
		}
	}
		// TODO remove mirrorScheddName from the gahp server key if/when
		//   a gahp server can handle multiple schedds
	sprintf( buff, "MIRROR/%s", mirrorScheddName );

	args.AppendArg("-f");
	args.AppendArg("-s");
	args.AppendArg(mirrorScheddName);

	gahp = new GahpClient( buff, gahp_path, &args );
	free( gahp_path );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	tmp = 0;
	jobAd->LookupBool( ATTR_MIRROR_ACTIVE, tmp );
	if ( tmp != 0 ) {
		mirrorActive = true;
	}

	tmp = 0;
	jobAd->LookupBool( ATTR_MIRROR_RELEASED, tmp );
	if ( tmp != 0 ) {
		mirrorReleased = true;
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

MirrorJob::~MirrorJob()
{
	if ( submitterId != NULL ) {
		free( submitterId );
	}
	if ( newRemoteStatusAd != NULL ) {
		delete newRemoteStatusAd;
	}
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	if ( remoteJobIdString != NULL ) {
		MirrorJobsById.remove( HashKey( remoteJobIdString ) );
		free( remoteJobIdString );
	}
	if ( mirrorScheddName ) {
		free( mirrorScheddName );
	}
	if ( gahpAd ) {
		delete gahpAd;
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
}

void MirrorJob::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

int MirrorJob::doEvaluateState()
{
	int old_gm_state;
	int old_remote_state;
	bool reevaluate_state = true;
	time_t now;	// make sure you set this before every use!!!

	bool done;
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
			if ( mirrorJobId.cluster == 0 ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( mirrorReleased == false ) {
				gmState = GM_SUBMITTED_MIRROR_INACTIVE;
			} else if ( mirrorActive == false ) {
				gmState = GM_VACATE_SCHEDD;
			} else {
				gmState = GM_SUBMITTED_MIRROR_ACTIVE;
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
			now = time(NULL);
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
				rc = gahp->condor_job_submit( mirrorScheddName,
											  gahpAd,
											  &job_id_string );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;
				if ( rc == GLOBUS_SUCCESS ) {
					SetRemoteJobId( job_id_string );
					gmState = GM_SUBMIT_SAVE;
				} else {
					// unhandled error
					dprintf( D_ALWAYS,
							 "(%d.%d) condor_job_submit() failed: %s\n",
							 procID.cluster, procID.proc,
							 gahp->getErrorString() );
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
				gmState = GM_CANCEL_1;
			} else {
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
				gmState = GM_STAGE_IN;
			}
			} break;
		case GM_STAGE_IN: {
			// Now stage files to the remote schedd
#if 0
			if ( gahpAd == NULL ) {
				gahpAd = buildStageInAd();
			}
			if ( gahpAd == NULL ) {
				gmState = GM_HOLD;
				break;
			}
			rc = gahp->condor_job_stage_in( mirrorScheddName, gahpAd );
#else
			if ( gahpAd == NULL ) {
				gahpAd = new ClassAd;
				gahpAd->Assign( ATTR_STAGE_IN_START, (int)time(NULL) );
				gahpAd->Assign( ATTR_STAGE_IN_FINISH, (int)time(NULL) );
			}
			rc = gahp->condor_job_update( mirrorScheddName, mirrorJobId,
										  gahpAd );
#endif
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == 0 ) {
				gmState = GM_SUBMITTED_MIRROR_INACTIVE;
			} else {
				// unhandled error
				dprintf( D_ALWAYS,
						 "(%d.%d) condor_job_stage_in() failed: %s\n",
						 procID.cluster, procID.proc, gahp->getErrorString() );
				gmState = GM_CANCEL_1;
			}
			} break;
		case GM_SUBMITTED_MIRROR_INACTIVE: {
			// The job has been submitted. Wait for completion or failure,
			// and poll the remote schedd occassionally to let it know
			// we're still alive.
			if ( remoteState == COMPLETED ) {
				gmState = GM_DONE_SAVE;
			} else if ( condorState == REMOVED || condorState == HELD ||
						condorState == COMPLETED ) {
				gmState = GM_CANCEL_1;
			} else if ( mirrorReleased ) {
				gmState = GM_MIRROR_ACTIVE_SAVE;
			} else if ( newRemoteStatusAd != NULL ) {
				if ( newRemoteStatusServerTime <= lastRemoteStatusServerTime ) {
dprintf(D_FULLDEBUG,"(%d.%d) newRemoteStatusAd too long!\n",procID.cluster,procID.proc);
					delete newRemoteStatusAd;
					newRemoteStatusAd = NULL;
				} else {
					ProcessRemoteAdInactive( newRemoteStatusAd );
					if ( mirrorReleased ) {
						// Leave the new remote status ad looking like an
						// unprocessed one so that GM_SUBMITTED_MIRROR_ACTIVE
						// look at it as well
						gmState = GM_MIRROR_ACTIVE_SAVE;
					} else {
						lastRemoteStatusServerTime = newRemoteStatusServerTime;
						delete newRemoteStatusAd;
						newRemoteStatusAd = NULL;
					}
					reevaluate_state = true;
				}
			}
			} break;
		case GM_MIRROR_ACTIVE_SAVE: {
			// The mirror has just become active. Make sure that state has
			// been saved in the schedd before we send the vacate command.
			done = requestScheddUpdate( this );
			if ( !done ) {
				break;
			}

			gmState = GM_VACATE_SCHEDD;
			} break;
		case GM_VACATE_SCHEDD: {
			// The mirror has just become active. Send a vacate command to
			// the schedd to stop any local execution of the job.

			action_result_t result;
			done = requestScheddVacate( this, result );
			if ( done == false ) {
				break;
			} else if ( result != AR_SUCCESS && result != AR_BAD_STATUS ) {
				dprintf( D_FULLDEBUG, "(%d.%d) vacateJobs failed, result=%d\n",
						 procID.cluster, procID.proc, result );
			} else {
				dprintf( D_FULLDEBUG, "(%d.%d) vacateJobs succeeded, result=%d\n",
						 procID.cluster, procID.proc, result );
			}

			gmState = GM_VACATE_WAIT;
			} break;
		case GM_VACATE_WAIT: {

			int job_status;
			done = requestJobStatus( this, job_status );
			if ( done == false ) {
				break;
			} else if ( job_status == RUNNING ) {
				// The shadow is still running, wait and poll again
				reevaluate_state = true;
				break;
			} else if ( job_status == COMPLETED ) {
				// Cancel the mirror job and let the local job complete
				gmState = GM_CANCEL_1;
			} else {
				mirrorActive = true;
				jobAd->Assign( ATTR_MIRROR_ACTIVE, true );
				requestScheddUpdate( this );
				gmState = GM_SUBMITTED_MIRROR_ACTIVE;
			}
			} break;
		case GM_SUBMITTED_MIRROR_ACTIVE: {
			// The job has been submitted. Wait for completion or failure,
			// and poll the remote schedd occassionally to let it know
			// we're still alive.
			writeUserLog = true;
			if ( condorState == REMOVED ) {
				gmState = GM_CANCEL_1;
			} else if ( newRemoteStatusAd != NULL ) {
				if ( newRemoteStatusServerTime <= lastRemoteStatusServerTime ) {
dprintf(D_FULLDEBUG,"(%d.%d) newRemoteStatusAd too long!\n",procID.cluster,procID.proc);
					delete newRemoteStatusAd;
					newRemoteStatusAd = NULL;
				} else {
					ProcessRemoteAdActive( newRemoteStatusAd );
					lastRemoteStatusServerTime = newRemoteStatusServerTime;
					delete newRemoteStatusAd;
					newRemoteStatusAd = NULL;
					reevaluate_state = true;
				}
			} else if ( remoteState == COMPLETED ) {
				gmState = GM_DONE_SAVE;
			} else if ( condorState == HELD ) {
				if ( remoteState == HELD ) {
					// The job is on hold both locally and remotely. We're
					// done, delete this job object from the gridmanager.
					gmState = GM_DELETE;
				} else {
					gmState = GM_HOLD_REMOTE_JOB;
				}
			} else if ( remoteState == HELD ) {
				// The job is on hold remotely but not locally. This means
				// the remote job needs to be released.
				gmState = GM_RELEASE_REMOTE_JOB;
			}
			} break;
		case GM_HOLD_REMOTE_JOB: {
			rc = gahp->condor_job_hold( mirrorScheddName, mirrorJobId,
										"by gridmanager" );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				dprintf( D_ALWAYS,
						 "(%d.%d) condor_job_hold() failed: %s\n",
						 procID.cluster, procID.proc, gahp->getErrorString() );
				gmState = GM_CANCEL_1;
				break;
			}
			gmState = GM_POLL_ACTIVE;
			} break;
		case GM_RELEASE_REMOTE_JOB: {
			rc = gahp->condor_job_release( mirrorScheddName, mirrorJobId,
										   "by gridmanager" );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				dprintf( D_ALWAYS,
						 "(%d.%d) condor_job_release() failed: %s\n",
						 procID.cluster, procID.proc, gahp->getErrorString() );
				gmState = GM_CANCEL_1;
				break;
			}
			gmState = GM_POLL_ACTIVE;
			} break;
		case GM_POLL_ACTIVE: {
			int num_ads;
			ClassAd **status_ads = NULL;
			MyString constraint;
			constraint.sprintf( "%s==%d&&%s==%d", ATTR_CLUSTER_ID,
								mirrorJobId.cluster, ATTR_PROC_ID,
								mirrorJobId.proc );
			rc = gahp->condor_job_status_constrained( mirrorScheddName,
													  constraint.Value(),
													  &num_ads, &status_ads );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				dprintf( D_ALWAYS,
						 "(%d.%d) condor_job_status_constrained() failed: %s\n",
						 procID.cluster, procID.proc, gahp->getErrorString() );
				gmState = GM_CANCEL_1;
				break;
			}
			if ( num_ads != 1 ) {
				dprintf( D_ALWAYS,
						 "(%d.%d) condor_job_status_constrained() returned %d ads\n",
						 procID.cluster, procID.proc, num_ads );
				gmState = GM_CANCEL_1;
				break;
			}
			ProcessRemoteAdActive( status_ads[0] );
			int server_time;
			if ( status_ads[0]->LookupInteger( ATTR_SERVER_TIME,
											   server_time ) == 0 ) {
				dprintf( D_ALWAYS, "(%d.%d) Ad from remote schedd has no %s, "
						 "faking with current local time\n",
						 procID.cluster, procID.proc, ATTR_SERVER_TIME );
				server_time = time(NULL);
			}
			lastRemoteStatusServerTime = server_time;
			delete status_ads[0];
			free( status_ads );
			gmState = GM_SUBMITTED_MIRROR_ACTIVE;
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
			// Tell the remote schedd it can remove the job from the queue.
			if ( gahpAd == NULL ) {
				MyString expr;
				gahpAd = new ClassAd;
				expr.sprintf( "%s = False", ATTR_JOB_LEAVE_IN_QUEUE );
				gahpAd->Insert( expr.Value() );
			}
			rc = gahp->condor_job_update( mirrorScheddName, mirrorJobId,
										  gahpAd );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				dprintf( D_ALWAYS,
						 "(%d.%d) condor_job_update() failed: %s\n",
						 procID.cluster, procID.proc, gahp->getErrorString() );
				gmState = GM_CANCEL_1;
				break;
			}
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				SetRemoteJobId( NULL );
				requestScheddUpdate( this );
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL_1: {
			if ( gahpAd == NULL ) {
				MyString buff;
				gahpAd = new ClassAd;
				buff.sprintf( "%s = False", ATTR_JOB_LEAVE_IN_QUEUE );
				gahpAd->Insert( buff.Value() );
			}
			rc = gahp->condor_job_update( mirrorScheddName, mirrorJobId,
										  gahpAd );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				dprintf( D_FULLDEBUG,
					"(%d.%d) GM_CANCEL_1: condor_job_update() failed: %s\n",
					procID.cluster, procID.proc, gahp->getErrorString() );
			}
			gmState = GM_CANCEL_2;
			} break;
		case GM_CANCEL_2: {
			// We need to cancel the job submission.

			// Should this if-stmt be here? Even if the job is completed,
			// we may still want to remove it (say if we have trouble
			// fetching the output files).
			if ( remoteState != COMPLETED ) {
				rc = gahp->condor_job_remove( mirrorScheddName, mirrorJobId,
											  "by gridmanager" );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					dprintf( D_ALWAYS,
							 "(%d.%d) condor_job_remove() failed: %s\n",
							 procID.cluster, procID.proc, gahp->getErrorString() );
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
			bool tmp_bool;

			// For now, put problem jobs on hold instead of
			// forgetting about current submission and trying again.
			// TODO: Let our action here be dictated by the user preference
			// expressed in the job ad.
			if ( mirrorJobId.cluster != 0 && condorState != REMOVED ) {
				gmState = GM_HOLD;
				break;
			}
			errorString = "";
			SetRemoteJobId( NULL );
			if ( newRemoteStatusAd != NULL ) {
				delete newRemoteStatusAd;
				newRemoteStatusAd = NULL;
			}
			// If we haven't failed over to the mirror job, don't change
			// any attributes that may affect the local job execution.
			if ( mirrorActive == true ) {
				JobIdle();
				if ( submitLogged ) {
					JobEvicted();
				}
			}
			if ( mirrorActive == true ||
				 jobAd->LookupBool( ATTR_MIRROR_ACTIVE, tmp_bool ) == false ) {
				jobAd->Assign( ATTR_MIRROR_ACTIVE, false );
				mirrorActive = false;
			}
			if ( mirrorReleased == true ||
				 jobAd->LookupBool( ATTR_MIRROR_RELEASED, tmp_bool ) == false ) {
				jobAd->Assign( ATTR_MIRROR_RELEASED, false );
				mirrorReleased = false;
			}
			writeUserLog = false;

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
			submitLogged = false;
			executeLogged = false;
			submitFailedLogged = false;
			terminateLogged = false;
			abortLogged = false;
			evictLogged = false;
			gmState = GM_UNSUBMITTED;
			remoteState = JOB_STATE_UNSUBMITTED;
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

void MirrorJob::SetRemoteJobId( const char *job_id )
{
	if ( remoteJobIdString != NULL && job_id != NULL &&
		 strcmp( remoteJobIdString, job_id ) == 0 ) {
		return;
	}
	if ( remoteJobIdString != NULL ) {
		MirrorJobsById.remove( HashKey( remoteJobIdString ) );
		free( remoteJobIdString );
		remoteJobIdString = NULL;
		mirrorJobId.cluster = 0;
		jobAd->AssignExpr( ATTR_MIRROR_JOB_ID, "Undefined" );
	}
	if ( job_id != NULL ) {
		MyString id_string;
		sscanf( job_id, "%d.%d", &mirrorJobId.cluster, &mirrorJobId.proc );
		id_string.sprintf( "%s/%d.%d", mirrorScheddName, mirrorJobId.cluster,
						   mirrorJobId.proc );
		remoteJobIdString = strdup( id_string.Value() );
		MirrorJobsById.insert( HashKey( remoteJobIdString ), this );
		jobAd->Assign( ATTR_MIRROR_JOB_ID, job_id );
	}
	requestScheddUpdate( this );
}

void MirrorJob::NotifyNewRemoteStatus( ClassAd *update_ad )
{
	int tmp_int;
	dprintf( D_FULLDEBUG, "(%d.%d) ***got classad from MirrorResource\n",
			 procID.cluster, procID.proc );
	if ( update_ad->LookupInteger( ATTR_SERVER_TIME, tmp_int ) == 0 ) {
		dprintf( D_ALWAYS, "(%d.%d) Ad from remote schedd has no %s\n",
				 procID.cluster, procID.proc, ATTR_SERVER_TIME );
		delete update_ad;
		return;
	}
	if ( newRemoteStatusAd != NULL && tmp_int <= newRemoteStatusServerTime ) {
		dprintf( D_ALWAYS, "(%d.%d) Ad from remote schedd is stale\n",
				 procID.cluster, procID.proc );
		delete update_ad;
		return;
	}
	if ( newRemoteStatusAd != NULL ) {
		delete newRemoteStatusAd;
	}
	newRemoteStatusAd = update_ad;
	newRemoteStatusServerTime = tmp_int;
	SetEvaluateState();
}

void MirrorJob::ProcessRemoteAdInactive( ClassAd *remote_ad )
{
	int rc;
	int tmp_int;

	if ( remote_ad == NULL ) {
		return;
	}

	dprintf( D_FULLDEBUG, "(%d.%d) ***ProcessRemoteAdInactive\n",
			 procID.cluster, procID.proc );

	remote_ad->LookupInteger( ATTR_JOB_STATUS, tmp_int );

	if ( tmp_int != HELD ) {
		jobAd->Assign( ATTR_MIRROR_RELEASED, true );
		mirrorReleased = true;
	}
	remoteState = tmp_int;

	rc = remote_ad->LookupInteger( ATTR_MIRROR_LEASE_TIME,
								   tmp_int );
	if ( rc ) {
		jobAd->Assign( ATTR_MIRROR_REMOTE_LEASE_TIME, tmp_int );
	} else {
		jobAd->AssignExpr( ATTR_MIRROR_REMOTE_LEASE_TIME, "Undefined" );
	}

	requestScheddUpdate( this );

	return;
}

void MirrorJob::ProcessRemoteAdActive( ClassAd *remote_ad )
{
	int rc;
	int tmp_int;
	int new_remote_state;
	ClassAd *diff_ad;
	MyString buff;
	const char *next_name;

	if ( remote_ad == NULL ) {
		return;
	}

	dprintf( D_FULLDEBUG, "(%d.%d) ***ProcessRemoteAdActive\n",
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


	diff_ad = ClassAdDiff( jobAd, remote_ad );

	rc = diff_ad->LookupInteger( ATTR_MIRROR_LEASE_TIME, tmp_int );
	if ( rc ) {
		diff_ad->Assign( ATTR_MIRROR_REMOTE_LEASE_TIME, tmp_int );
	} else {
		buff.sprintf( "%s = Undefined", ATTR_MIRROR_REMOTE_LEASE_TIME );
		diff_ad->Insert( buff.Value() );
	}

	diff_ad->Delete( ATTR_CLUSTER_ID );
	diff_ad->Delete( ATTR_PROC_ID );
	diff_ad->Delete( ATTR_MIRROR_ACTIVE );
	diff_ad->Delete( ATTR_MIRROR_RELEASED );
	diff_ad->Delete( ATTR_MIRROR_SCHEDD );
	diff_ad->Delete( ATTR_MIRROR_JOB_ID );
	diff_ad->Delete( ATTR_MIRROR_LEASE_TIME );
	diff_ad->Delete( ATTR_ULOG_FILE );
	diff_ad->Delete( ATTR_Q_DATE );
	diff_ad->Delete( ATTR_ENTERED_CURRENT_STATUS );
	diff_ad->Delete( ATTR_JOB_LEAVE_IN_QUEUE );
	diff_ad->Delete( ATTR_HOLD_REASON );
	diff_ad->Delete( ATTR_HOLD_REASON_CODE );
	diff_ad->Delete( ATTR_HOLD_REASON_SUBCODE );
	diff_ad->Delete( ATTR_LAST_HOLD_REASON );
	diff_ad->Delete( ATTR_LAST_HOLD_REASON_CODE );
	diff_ad->Delete( ATTR_LAST_HOLD_REASON_SUBCODE );
	diff_ad->Delete( ATTR_RELEASE_REASON );
	diff_ad->Delete( ATTR_LAST_RELEASE_REASON );
	diff_ad->Delete( ATTR_REMOVE_REASON );
	diff_ad->Delete( ATTR_JOB_STATUS_ON_RELEASE );
	diff_ad->Delete( ATTR_JOB_STATUS );
	diff_ad->Delete( ATTR_JOB_MANAGED );
	diff_ad->Delete( ATTR_PERIODIC_HOLD_CHECK );
	diff_ad->Delete( ATTR_PERIODIC_RELEASE_CHECK );
	diff_ad->Delete( ATTR_PERIODIC_REMOVE_CHECK );
	diff_ad->Delete( ATTR_ON_EXIT_HOLD_CHECK );
	diff_ad->Delete( ATTR_ON_EXIT_REMOVE_CHECK );
	diff_ad->Delete( ATTR_SERVER_TIME );
	diff_ad->Delete( ATTR_WANT_MATCHING );
	diff_ad->Delete( ATTR_GLOBAL_JOB_ID );
	diff_ad->Delete( ATTR_JOB_NOTIFICATION );
	diff_ad->Delete( ATTR_MIRROR_SUBMITTER_ID );
	diff_ad->Delete( ATTR_STAGE_IN_START );
	diff_ad->Delete( ATTR_STAGE_IN_FINISH );

	// Remove attributes that were renamed by the remote schedd because
	// of moving the job's sandbox. These can be identified by looking
	// for pairs of attributes named <attr name> and SUBMIT_<attr name>.
	diff_ad->ResetName();
	while ( (next_name = diff_ad->NextNameOriginal()) != NULL ) {
		if ( strncmp( next_name, "SUBMIT_", 7 ) == 0 &&
			 remote_ad->Lookup( &next_name[7] ) != NULL ) {
			diff_ad->Delete( next_name );
			diff_ad->Delete( &next_name[7] );
		}
	}

	ClassAdPatch( jobAd, diff_ad );

	requestScheddUpdate( this );

	delete diff_ad;

	return;
}

BaseResource *MirrorJob::GetResource()
{
	return (BaseResource *)NULL;
}

ClassAd *MirrorJob::buildSubmitAd()
{
	int now = time(NULL);
	MyString expr;
	ClassAd *submit_ad;

		// Base the submit ad on our own job ad
	submit_ad = new ClassAd( *jobAd );

	submit_ad->Delete( ATTR_CLUSTER_ID );
	submit_ad->Delete( ATTR_PROC_ID );
	submit_ad->Delete( ATTR_MIRROR_REMOTE_LEASE_TIME );
	submit_ad->Delete( ATTR_MIRROR_ACTIVE );
	submit_ad->Delete( ATTR_MIRROR_RELEASED );
	submit_ad->Delete( ATTR_MIRROR_SCHEDD );
	submit_ad->Delete( ATTR_MIRROR_JOB_ID );
	submit_ad->Delete( ATTR_MIRROR_LEASE_TIME );
	submit_ad->Delete( ATTR_WANT_MATCHING );
	submit_ad->Delete( ATTR_PERIODIC_HOLD_CHECK );
	submit_ad->Delete( ATTR_PERIODIC_RELEASE_CHECK );
	submit_ad->Delete( ATTR_PERIODIC_REMOVE_CHECK );
	submit_ad->Delete( ATTR_ON_EXIT_HOLD_CHECK );
	submit_ad->Delete( ATTR_ON_EXIT_REMOVE_CHECK );
	submit_ad->Delete( ATTR_HOLD_REASON );
	submit_ad->Delete( ATTR_HOLD_REASON_CODE );
	submit_ad->Delete( ATTR_HOLD_REASON_SUBCODE );
	submit_ad->Delete( ATTR_LAST_HOLD_REASON );
	submit_ad->Delete( ATTR_LAST_HOLD_REASON_CODE );
	submit_ad->Delete( ATTR_LAST_HOLD_REASON_SUBCODE );
	submit_ad->Delete( ATTR_RELEASE_REASON );
	submit_ad->Delete( ATTR_LAST_RELEASE_REASON );
	submit_ad->Delete( ATTR_JOB_STATUS_ON_RELEASE );
	submit_ad->Delete( ATTR_DAG_NODE_NAME );
	submit_ad->Delete( ATTR_DAGMAN_JOB_ID );
	submit_ad->Delete( ATTR_ULOG_FILE );
	submit_ad->Delete( ATTR_SERVER_TIME );
	submit_ad->Delete( ATTR_JOB_MANAGED );
	submit_ad->Delete( ATTR_GLOBAL_JOB_ID );
	submit_ad->Delete( ATTR_STAGE_IN_START );
	submit_ad->Delete( ATTR_STAGE_IN_FINISH );

	submit_ad->Assign( ATTR_JOB_STATUS, HELD );
	submit_ad->Assign( ATTR_HOLD_REASON, "submitted on hold at user's request" );
	submit_ad->Assign( ATTR_Q_DATE, now );
	submit_ad->Assign( ATTR_COMPLETION_DATE, 0 );
	submit_ad->Assign( ATTR_JOB_REMOTE_WALL_CLOCK, (float)0.0 );
	submit_ad->Assign( ATTR_JOB_LOCAL_USER_CPU, (float)0.0 );
	submit_ad->Assign( ATTR_JOB_LOCAL_SYS_CPU, (float)0.0 );
	submit_ad->Assign( ATTR_JOB_REMOTE_USER_CPU, (float)0.0 );
	submit_ad->Assign( ATTR_JOB_REMOTE_SYS_CPU, (float)0.0 );
	submit_ad->Assign( ATTR_JOB_EXIT_STATUS, 0 );
	submit_ad->Assign( ATTR_NUM_CKPTS, 0 );
	submit_ad->Assign( ATTR_NUM_RESTARTS, 0 );
	submit_ad->Assign( ATTR_NUM_SYSTEM_HOLDS, 0 );
	submit_ad->Assign( ATTR_JOB_COMMITTED_TIME, 0 );
	submit_ad->Assign( ATTR_TOTAL_SUSPENSIONS, 0 );
	submit_ad->Assign( ATTR_LAST_SUSPENSION_TIME, 0 );
	submit_ad->Assign( ATTR_CUMULATIVE_SUSPENSION_TIME, 0 );
	submit_ad->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );
	submit_ad->Assign( ATTR_CURRENT_HOSTS, 0 );
	submit_ad->Assign( ATTR_ENTERED_CURRENT_STATUS, now );
	submit_ad->Assign( ATTR_JOB_NOTIFICATION, NOTIFY_NEVER );
	submit_ad->Assign( ATTR_JOB_LEAVE_IN_QUEUE, true );

	expr.sprintf( "%s = (%s >= %s) =!= True && CurrentTime > %s + %d",
				  ATTR_PERIODIC_REMOVE_CHECK, ATTR_STAGE_IN_FINISH,
				  ATTR_STAGE_IN_START, ATTR_Q_DATE, 1800 );
	submit_ad->Insert( expr.Value() );

	// TODO In the current expression below, if the remote schedd crashes
	//   and restarts, it'll wait 15 minutes for an updated lease before
	//   releasing the mirror job. Should this timeout be based on the
	//   lease update interval (15 minutes is 3 times the current default).
	expr.sprintf( "%s = %s == %s && (%s >= %s) =?= True && (CurrentTime > %s) =?= True && (CurrentTime > %s + %d) =!= False",
				  ATTR_PERIODIC_RELEASE_CHECK, ATTR_ENTERED_CURRENT_STATUS,
				  ATTR_Q_DATE, ATTR_STAGE_IN_FINISH, ATTR_STAGE_IN_START,
				  ATTR_MIRROR_LEASE_TIME, ATTR_SCHEDD_BIRTHDATE, 15*60 );
	submit_ad->Insert( expr.Value() );

	submit_ad->Assign( ATTR_MIRROR_SUBMITTER_ID, submitterId );

		// worry about ATTR_JOB_[OUTPUT|ERROR]_ORIG

	return submit_ad;
}

ClassAd *MirrorJob::buildStageInAd()
{
	MyString expr;
	ClassAd *stage_in_ad;

		// Base the stage in ad on our own job ad
	stage_in_ad = new ClassAd( *jobAd );

	stage_in_ad->Assign( ATTR_CLUSTER_ID, mirrorJobId.cluster );
	stage_in_ad->Assign( ATTR_PROC_ID, mirrorJobId.proc );

	stage_in_ad->Delete( ATTR_ULOG_FILE );

	return stage_in_ad;
}
