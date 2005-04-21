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
#include "daemon.h"
#include "dc_schedd.h"

#include "gridmanager.h"
#include "condorjob.h"
#include "condor_config.h"


// GridManager job states
#define GM_INIT					0
#define GM_REGISTER				1
#define GM_STDIO_UPDATE			2
#define GM_UNSUBMITTED			3
#define GM_SUBMIT				4
#define GM_SUBMIT_SAVE			5
#define GM_STAGE_IN				6
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
#define GM_START				17
#define GM_HOLD_REMOTE_JOB		18
#define GM_RELEASE_REMOTE_JOB	19
#define GM_POLL_ACTIVE			20
#define GM_STAGE_OUT			21
#define GM_RECOVER_POLL			22

static char *GMStateNames[] = {
	"GM_INIT",
	"GM_REGISTER",
	"GM_STDIO_UPDATE",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_STAGE_IN",
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
	"GM_HOLD_REMOTE_JOB",
	"GM_RELEASE_REMOTE_JOB",
	"GM_POLL_ACTIVE",
	"GM_STAGE_OUT",
	"GM_RECOVER_POLL"
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

template class HashTable<HashKey, CondorJob *>;
template class HashBucket<HashKey, CondorJob *>;

HashTable <HashKey, CondorJob *> CondorJobsById( HASH_TABLE_SIZE,
												 hashFunction );


void CondorJobInit()
{
}

void CondorJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "CONDOR_JOB_POLL_INTERVAL", 5 * 60 );
	CondorResource::setPollInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 8 * 60 * 60 );
	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT_CONDOR", tmp_int );
	CondorJob::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	CondorJob::setConnectFailureRetry( tmp_int );
}

const char *CondorJobAdConst = "JobUniverse =?= 9 && (JobGridType == \"condor\") =?= True";

bool CondorJobAdMustExpand( const ClassAd *jobad )
{
	int must_expand = 0;

	jobad->LookupBool(ATTR_JOB_MUST_EXPAND, must_expand);
	if ( !must_expand ) {
		char resource_name[800];
		jobad->LookupString( ATTR_REMOTE_SCHEDD, resource_name );
		if ( strstr(resource_name,"$$") ) {
			must_expand = 1;
		}
	}

	return must_expand != 0;
}

BaseJob *CondorJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new CondorJob( jobad );
}


int CondorJob::submitInterval = 300;			// default value
int CondorJob::gahpCallTimeout = 8*60*60;		// default value
int CondorJob::maxConnectFailures = 3;			// default value

CondorJob::CondorJob( ClassAd *classad )
	: BaseJob( classad )
{
	char buff[4096];
	char buff2[4096];
	char *error_string = NULL;
	char *gahp_path;

	remoteJobId.cluster = 0;
	gahpAd = NULL;
	gmState = GM_INIT;
	remoteState = JOB_STATE_UNKNOWN;
	enteredCurrentGmState = time(NULL);
	enteredCurrentRemoteState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	submitFailureCode = 0;
	remoteScheddName = NULL;
	remotePoolName = NULL;
	remoteJobIdString = NULL;
	submitterId = NULL;
	jobProxy = NULL;
	remoteProxyExpireTime = 0;
	lastProxyRefreshAttempt = 0;
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
	if ( ad->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		UpdateJobAd( ATTR_HOLD_REASON, "UNDEFINED" );
	}

	buff[0] = '\0';
	ad->LookupString( ATTR_X509_USER_PROXY, buff );
	if ( buff[0] != '\0' ) {
		jobProxy = AcquireProxy( buff, evaluateStateTid );
		if ( jobProxy == NULL ) {
			dprintf( D_ALWAYS, "(%d.%d) error acquiring proxy!\n",
					 procID.cluster, procID.proc );
		}
	}

	buff[0] = '\0';
	ad->LookupString( ATTR_REMOTE_SCHEDD, buff );
	if ( buff[0] != '\0' ) {
		remoteScheddName = strdup( buff );
	} else {
		error_string = "RemoteSchedd is not set in the job ad";
		goto error_exit;
	}

	buff[0] = '\0';
	ad->LookupString( ATTR_REMOTE_POOL, buff );
	if ( buff[0] != '\0' ) {
		remotePoolName = strdup( buff );
	}

	buff[0] = '\0';
	ad->LookupString( ATTR_REMOTE_JOB_ID, buff );
	if ( buff[0] != '\0' ) {
		SetRemoteJobId( buff );
	} else {
		remoteState = JOB_STATE_UNSUBMITTED;
	}

	buff[0] = '\0';
	ad->LookupString( ATTR_GLOBAL_JOB_ID, buff );
	if ( buff[0] != '\0' ) {
		char *ptr = strchr( buff, '#' );
		if ( ptr != NULL ) {
			*ptr = '\0';
		}
		submitterId = strdup( buff );
	} else {
		error_string = "GlobalJobId is not set in the job ad";
		goto error_exit;
	}

	myResource = CondorResource::FindOrCreateResource( remoteScheddName,
													   remotePoolName,
													   jobProxy ? jobProxy->subject->subject_name : NULL );
	myResource->RegisterJob( this, submitterId );

	gahp_path = param("CONDOR_GAHP");
	if ( gahp_path == NULL ) {
		error_string = "CONDOR_GAHP not defined";
		goto error_exit;
	}
		// TODO remove remoteScheddName from the gahp server key if/when
		//   a gahp server can handle multiple schedds
	sprintf( buff, "CONDOR/%s/%s/%s", remotePoolName ? remotePoolName : "NULL",
			 remoteScheddName,
			 jobProxy != NULL ? jobProxy->subject->subject_name : "NULL" );
	sprintf( buff2, "-f -s %s", remoteScheddName );
	if ( remotePoolName ) {
		strcat( buff2, " -P " );
		strcat( buff2, remotePoolName );
	}
	gahp = new GahpClient( buff, gahp_path, buff2 );
	free( gahp_path );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	return;

 error_exit:
		// We must ensure that the code-path from GM_HOLD doesn't depend
		// on any initialization that's been skipped.
	gmState = GM_HOLD;
	if ( error_string ) {
		UpdateJobAdString( ATTR_HOLD_REASON, error_string );
	}
	return;
}

CondorJob::~CondorJob()
{
	if ( jobProxy != NULL ) {
		ReleaseProxy( jobProxy, evaluateStateTid );
	}
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
		CondorJobsById.remove( HashKey( remoteJobIdString ) );
		free( remoteJobIdString );
	}
	if ( remoteScheddName ) {
		free( remoteScheddName );
	}
	if ( remotePoolName ) {
		free( remotePoolName );
	}
	if ( gahpAd ) {
		delete gahpAd;
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
}

void CondorJob::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

int CondorJob::doEvaluateState()
{
	int old_gm_state;
	int old_remote_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

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

				UpdateJobAdString( ATTR_HOLD_REASON, "Failed to start GAHP" );
				gmState = GM_HOLD;
				break;
			}
			if ( jobProxy && gahp->Initialize( jobProxy ) == false ) {
				dprintf( D_ALWAYS, "(%d.%d) Error initializing GAHP\n",
						 procID.cluster, procID.proc );

				UpdateJobAdString( ATTR_HOLD_REASON,
								   "Failed to initialize GAHP" );
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
			if ( remoteJobId.cluster == 0 ) {
				if( condorState == COMPLETED ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_CLEAR_REQUEST;
				}
			} else if ( condorState == COMPLETED ) {
				gmState = GM_DONE_COMMIT;
			} else {
				gmState = GM_RECOVER_POLL;
			}
			} break;
		case GM_RECOVER_POLL: {
			int num_ads = 0;
			int tmp_int = 0;
			ClassAd **status_ads = NULL;
			MyString constraint;
			constraint.sprintf( "%s==%d&&%s==%d", ATTR_CLUSTER_ID,
								remoteJobId.cluster, ATTR_PROC_ID,
								remoteJobId.proc );
			rc = gahp->condor_job_status_constrained( remoteScheddName,
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
				errorString = gahp->getErrorString();
				gmState = GM_CANCEL;
				break;
			}
			if ( num_ads != 1 ) {
					// Didn't get the number of ads we expected. Abort!
				dprintf( D_ALWAYS,
						 "(%d.%d) condor_job_status_constrained() returned %d ads\n",
						 procID.cluster, procID.proc, num_ads );
				errorString = "Remote schedd returned multiple ads";
				gmState = GM_CANCEL;
			} else if ( status_ads[0]->LookupInteger( ATTR_STAGE_IN_FINISH,
													  tmp_int ) == 0 ||
						tmp_int <= 0 ) {
					// We haven't finished staging in files yet
				gmState = GM_STAGE_IN;
			} else {

					// Copy any attributes we care about from the remote
					// ad to our local one before doing anything else
				ProcessRemoteAd( status_ads[0] );
				int server_time;
				if ( status_ads[0]->LookupInteger( ATTR_SERVER_TIME,
												   server_time ) == 0 ) {
					dprintf( D_ALWAYS, "(%d.%d) Ad from remote schedd has "
							 "no %s, faking with current local time\n",
							 procID.cluster, procID.proc, ATTR_SERVER_TIME );
					server_time = time(NULL);
				}
				lastRemoteStatusServerTime = server_time;

				if ( remoteState == COMPLETED &&
					 status_ads[0]->LookupInteger( ATTR_STAGE_OUT_FINISH,
												   tmp_int ) != 0 &&
					 tmp_int > 0 ) {

						// We already staged out all the files
					gmState = GM_DONE_SAVE;
				} else {
						// All other cases can be handled by GM_SUBMITTED
					gmState = GM_SUBMITTED;
				}
			}
			for ( int i = 0; i < num_ads; i++ ) {
				delete status_ads[i];
			}
			free( status_ads );
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
				errorString = "Repeated submit attempts (GAHP reports:";
				errorString += gahp->getErrorString();
				errorString += ")";
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
					errorString = "Internal Error: Failed to build submit ad.";
					gmState = GM_HOLD;
					break;
				}
				rc = gahp->condor_job_submit( remoteScheddName,
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
					dprintf( D_ALWAYS,
							 "(%d.%d) condor_job_submit() failed: %s\n",
							 procID.cluster, procID.proc,
							 gahp->getErrorString() );
					int jcluster = -1;
					int jproc = -1;
					if(job_id_string) {
							// job_id_string is null in many failure cases.
						sscanf( job_id_string, "%d.%d", &jcluster, &jproc );
					}
					// if the job failed to submit, the cluster number
					// will hold the error code for the call to 
					// NewCluster(), and the proc number will hold
					// error code for the call to NewProc().
					// now check if either call failed w/ -2, which
					// signifies MAX_JOBS_SUBMITTED was exceeded.
					if ( jcluster==-2 || jproc==-2 ) {
						// MAX_JOBS_SUBMITTED error.
						// For now, we will always put this job back
						// to idle and tell the schedd to find us
						// another match.
						// TODO: this hard-coded logic should be
						// replaced w/ a WANT_REMATCH expression, like
						// is currently done in the Globus gridtype.
						dprintf(D_ALWAYS,"(%d.%d) Requesting schedd to "
							"rematch job because of MAX_JOBS_SUBMITTED\n",
							procID.cluster, procID.proc);
						// Set ad attributes so the schedd finds a new match.
						int dummy;
						if ( ad->LookupBool( ATTR_JOB_MATCHED, dummy ) != 0 ) {
							UpdateJobAdBool( ATTR_JOB_MATCHED, 0 );
							UpdateJobAdInt( ATTR_CURRENT_HOSTS, 0 );
						}
						// We are rematching,  so forget about this job 
						// cuz we wanna pull a fresh new job ad, with 
						// a fresh new match, from the all-singing schedd.
						gmState = GM_DELETE;
					} else {
						// unhandled error
						gmState = GM_UNSUBMITTED;
						reevaluate_state = true;
					}
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
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
				gmState = GM_STAGE_IN;
			}
			} break;
		case GM_STAGE_IN: {
			// Now stage files to the remote schedd
#if 1
			if ( gahpAd == NULL ) {
				gahpAd = buildStageInAd();
			}
			if ( gahpAd == NULL ) {
				errorString = "Internal Error: Failed to build stage in ad.";
				gmState = GM_HOLD;
				break;
			}
			rc = gahp->condor_job_stage_in( remoteScheddName, gahpAd );
#else
			if ( gahpAd == NULL ) {
				gahpAd = new ClassAd;
				gahpAd->Assign( ATTR_STAGE_IN_START, (int)time(NULL) );
				gahpAd->Assign( ATTR_STAGE_IN_FINISH, (int)time(NULL) );
			}
			rc = gahp->condor_job_update( remoteScheddName, remoteJobId,
										  gahpAd );
#endif
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == 0 ) {
				// We go to GM_POLL_ACTIVE here because if we get a status
				// ad from our CondorResource that's from before the stage
				// in completed, we'll get confused by the jobStatus of
				// HELD. By doing an active probe, we'll automatically
				// ignore any update ads from before the probe.
				if(jobProxy) {
					remoteProxyExpireTime = jobProxy->expiration_time;
				}
				gmState = GM_POLL_ACTIVE;
			} else {
				// unhandled error
				dprintf( D_ALWAYS,
						 "(%d.%d) condor_job_stage_in() failed: %s\n",
						 procID.cluster, procID.proc, gahp->getErrorString() );
				errorString = gahp->getErrorString();
				gmState = GM_CANCEL;
			}
			} break;
		case GM_SUBMITTED: {
			// The job has been submitted. Wait for completion or failure,
			// and poll the remote schedd occassionally to let it know
			// we're still alive.
			if ( condorState == REMOVED ) {
				gmState = GM_CANCEL;
			} else if ( newRemoteStatusAd != NULL ) {
				if ( newRemoteStatusServerTime <= lastRemoteStatusServerTime ) {
					dprintf( D_FULLDEBUG, "(%d.%d) Discarding old job status ad from collective poll\n",
							 procID.cluster, procID.proc );
				} else {
					ProcessRemoteAd( newRemoteStatusAd );
					lastRemoteStatusServerTime = newRemoteStatusServerTime;
				}
				delete newRemoteStatusAd;
				newRemoteStatusAd = NULL;
				reevaluate_state = true;
			} else if ( remoteState == COMPLETED ) {
				gmState = GM_STAGE_OUT;
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
			} else if ( jobProxy && remoteProxyExpireTime < jobProxy->expiration_time ) {
				int interval = param_integer( "GRIDMANAGER_PROXY_REFRESH_INTERVAL", 10*60 );
				if ( now >= lastProxyRefreshAttempt + interval ) {
					gmState = GM_REFRESH_PROXY;
				} else {
					dprintf( D_ALWAYS, "(%d.%d) Delaying refresh of proxy\n",
							 procID.cluster, procID.proc );
					unsigned int delay = 0;
					delay = (lastProxyRefreshAttempt + interval) - now;
					daemonCore->Reset_Timer( evaluateStateTid, delay );
				}
			}
			} break;
		case GM_REFRESH_PROXY: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_SUBMITTED;
			} else {
				rc = gahp->condor_job_refresh_proxy( remoteScheddName,
													 remoteJobId,
													 jobProxy->proxy_filename );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					dprintf( D_ALWAYS,
							 "(%d.%d) condor_job_refresh_proxy() failed: %s\n",
							 procID.cluster, procID.proc, gahp->getErrorString() );
					errorString = gahp->getErrorString();

					if ( ( remoteProxyExpireTime != 0 &&
						   remoteProxyExpireTime < now + 60 ) ||
						 ( remoteProxyExpireTime == 0 &&
						   jobProxy->near_expired ) ) {

						dprintf( D_ALWAYS,
								 "(%d.%d) Proxy almosted expired, cancelling job\n",
								 procID.cluster, procID.proc );
						gmState = GM_CANCEL;
						break;
					}
				} else {
					remoteProxyExpireTime = jobProxy->expiration_time;
				}
				lastProxyRefreshAttempt = time(NULL);
				gmState = GM_SUBMITTED;
			}
		} break;
		case GM_HOLD_REMOTE_JOB: {
			rc = gahp->condor_job_hold( remoteScheddName, remoteJobId,
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
				errorString = gahp->getErrorString();
				gmState = GM_CANCEL;
				break;
			}
			gmState = GM_POLL_ACTIVE;
			} break;
		case GM_RELEASE_REMOTE_JOB: {
			rc = gahp->condor_job_release( remoteScheddName, remoteJobId,
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
				errorString = gahp->getErrorString();
				gmState = GM_CANCEL;
				break;
			}
			gmState = GM_POLL_ACTIVE;
			} break;
		case GM_POLL_ACTIVE: {
			int num_ads;
			ClassAd **status_ads = NULL;
			MyString constraint;
			constraint.sprintf( "%s==%d&&%s==%d", ATTR_CLUSTER_ID,
								remoteJobId.cluster, ATTR_PROC_ID,
								remoteJobId.proc );
			rc = gahp->condor_job_status_constrained( remoteScheddName,
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
				errorString = gahp->getErrorString();
				gmState = GM_CANCEL;
				break;
			}
			if ( num_ads != 1 ) {
				dprintf( D_ALWAYS,
						 "(%d.%d) condor_job_status_constrained() returned %d ads\n",
						 procID.cluster, procID.proc, num_ads );
				errorString = "Remote schedd returned multiple ads";
				gmState = GM_CANCEL;
				break;
			}
			ProcessRemoteAd( status_ads[0] );
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
			gmState = GM_SUBMITTED;
			} break;
		case GM_STAGE_OUT: {
			// Now stage files back from the remote schedd
			rc = gahp->condor_job_stage_out( remoteScheddName, remoteJobId );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == 0 ) {
				gmState = GM_DONE_SAVE;
			} else {
				// unhandled error
				dprintf( D_ALWAYS,
						 "(%d.%d) condor_job_stage_out() failed: %s\n",
						 procID.cluster, procID.proc, gahp->getErrorString() );
				errorString = gahp->getErrorString();
				gmState = GM_CANCEL;
			}
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
			rc = gahp->condor_job_update( remoteScheddName, remoteJobId,
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
				errorString = gahp->getErrorString();
				gmState = GM_CANCEL;
				break;
			}
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
				SetRemoteJobId( NULL );
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				SetRemoteJobId( NULL );
				requestScheddUpdate( this );
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.

			// Should this if-stmt be here? Even if the job is completed,
			// we may still want to remove it (say if we have trouble
			// fetching the output files).
			if ( remoteState != COMPLETED ) {
				rc = gahp->condor_job_remove( remoteScheddName, remoteJobId,
											  "by gridmanager" );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
					// TODO what about error "Already done"? We should
					//   recognize it and act accordingly
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					// Should we retry the remove instead of instantly
					// giving up?
					dprintf( D_ALWAYS,
							 "(%d.%d) condor_job_remove() failed: %s\n",
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
			if( remoteJobId.cluster != 0 ) {
				errorString.sprintf( "Internal error: Attempting to clear "
									 "request, but remoteJobId.cluster(%d) "
									 "!= 0, condorState is %s (%d)",
									 remoteJobId.cluster,
									 getJobStatusString(condorState), 
									 condorState );
				gmState = GM_HOLD;
				break;
			}
			errorString = "";
			SetRemoteJobId( NULL );
			if ( newRemoteStatusAd != NULL ) {
				delete newRemoteStatusAd;
				newRemoteStatusAd = NULL;
			}
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
			done = requestScheddUpdate( this );
			if ( !done ) {
				break;
			}
			remoteProxyExpireTime = 0;
			lastProxyRefreshAttempt = 0;
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
				ad->LookupString( ATTR_HOLD_REASON, holdReason,
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

void CondorJob::SetRemoteJobId( const char *job_id )
{
	if ( remoteJobIdString != NULL && job_id != NULL &&
		 strcmp( remoteJobIdString, job_id ) == 0 ) {
		return;
	}
	if ( remoteJobIdString != NULL ) {
		CondorJobsById.remove( HashKey( remoteJobIdString ) );
		free( remoteJobIdString );
		remoteJobIdString = NULL;
		remoteJobId.cluster = 0;
		UpdateJobAd( ATTR_REMOTE_JOB_ID, "UNDEFINED" );
	}
	if ( job_id != NULL ) {
		MyString id_string;
		sscanf( job_id, "%d.%d", &remoteJobId.cluster, &remoteJobId.proc );
		id_string.sprintf( "%s/%d.%d", remoteScheddName, remoteJobId.cluster,
						   remoteJobId.proc );
		remoteJobIdString = strdup( id_string.Value() );
		CondorJobsById.insert( HashKey( remoteJobIdString ), this );
		UpdateJobAdString( ATTR_REMOTE_JOB_ID, job_id );
	}
	requestScheddUpdate( this );
}

void CondorJob::NotifyNewRemoteStatus( ClassAd *update_ad )
{
	int tmp_int;
	dprintf( D_FULLDEBUG, "(%d.%d) Got classad from CondorResource\n",
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

void CondorJob::ProcessRemoteAd( ClassAd *remote_ad )
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

	dprintf( D_FULLDEBUG, "(%d.%d) Processing remote job status ad\n",
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


	index = -1;
	while ( attrs_to_copy[++index] != NULL ) {
		old_expr = ad->Lookup( attrs_to_copy[index] );
		new_expr = remote_ad->Lookup( attrs_to_copy[index] );

		if ( new_expr != NULL && ( old_expr == NULL || !(*old_expr == *new_expr) ) ) {
			ad->Insert( new_expr->DeepCopy() );
		}
	}

	requestScheddUpdate( this );

	return;
}

BaseResource *CondorJob::GetResource()
{
	return (BaseResource *)NULL;
}

#if 0
// Old white-list version
ClassAd *CondorJob::buildSubmitAd()
{
	int now = time(NULL);
	MyString expr;
	ClassAd *submit_ad;
	ExprTree *next_expr;

	int index;
	const char *attrs_to_copy[] = {
		ATTR_JOB_CMD,
		ATTR_JOB_ARGUMENTS,
		ATTR_JOB_ENVIRONMENT,
		ATTR_JOB_INPUT,
		ATTR_JOB_OUTPUT,
		ATTR_JOB_ERROR,
		ATTR_REQUIREMENTS,
		ATTR_RANK,
		ATTR_OWNER,
		ATTR_DISK_USAGE,
		ATTR_IMAGE_SIZE,
		ATTR_EXECUTABLE_SIZE,
		ATTR_MAX_HOSTS,
		ATTR_MIN_HOSTS,
		ATTR_JOB_PRIO,
		ATTR_JOB_IWD,
		ATTR_SHOULD_TRANSFER_FILES,
		ATTR_WHEN_TO_TRANSFER_OUTPUT,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_TRANSFER_OUTPUT_FILES,
		ATTR_NICE_USER,
		NULL };		// list must end with a NULL

	submit_ad = new ClassAd;

	index = -1;
	while ( attrs_to_copy[++index] != NULL ) {
		if ( ( next_expr = ad->Lookup( attrs_to_copy[index] ) ) != NULL ) {
			submit_ad->Insert( next_expr->DeepCopy() );
		}
	}

	submit_ad->Assign( ATTR_JOB_STATUS, HELD );
	submit_ad->Assign( ATTR_HOLD_REASON, "Spooling input data files" );
	submit_ad->Assign( ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_VANILLA );

	submit_ad->Assign( ATTR_Q_DATE, now );
	submit_ad->Assign( ATTR_CURRENT_HOSTS, 0 );
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
	submit_ad->Assign( ATTR_ENTERED_CURRENT_STATUS, now  );
	submit_ad->Assign( ATTR_JOB_NOTIFICATION, NOTIFY_NEVER );

	expr.sprintf( "%s = (%s > 0) =!= True && CurrentTime > %s + %d",
				  ATTR_PERIODIC_REMOVE_CHECK, ATTR_STAGE_IN_FINISH,
				  ATTR_Q_DATE, 1800 );
	submit_ad->Insert( expr.Value() );

	expr.sprintf( "%s = %s == %d", ATTR_JOB_LEAVE_IN_QUEUE, ATTR_JOB_STATUS,
				  COMPLETED );
	submit_ad->Insert( expr.Value() );

	submit_ad->Assign( ATTR_SUBMITTER_ID, submitterId );

	ad->ResetExpr();
	while ( (next_expr = ad->NextExpr()) != NULL ) {
		if ( strncasecmp( ((Variable*)next_expr->LArg())->Name(), "REMOTE_", 7 ) == 0 ) {
			char *attr_value;
			MyString buf;
			next_expr->RArg()->PrintToNewStr(&attr_value);
			buf.sprintf( "%s = %s", &((Variable*)next_expr->LArg())->Name()[7],
						 attr_value );
			submit_ad->Insert( buf.Value() );
			free(attr_value);
		}
	}


		// worry about ATTR_JOB_[OUTPUT|ERROR]_ORIG

	return submit_ad;
}
#else
// New black-list version
ClassAd *CondorJob::buildSubmitAd()
{
	int now = time(NULL);
	MyString expr;
	ClassAd *submit_ad;
	ExprTree *next_expr;

		// Base the submit ad on our own job ad
	submit_ad = new ClassAd( *ad );

	submit_ad->Delete( ATTR_CLUSTER_ID );
	submit_ad->Delete( ATTR_PROC_ID );
	submit_ad->Delete( ATTR_REMOTE_SCHEDD );
	submit_ad->Delete( ATTR_REMOTE_POOL );
	submit_ad->Delete( ATTR_JOB_MATCHED );
	submit_ad->Delete( ATTR_JOB_MANAGED );
	submit_ad->Delete( ATTR_MIRROR_ACTIVE );
	submit_ad->Delete( ATTR_MIRROR_JOB_ID );
	submit_ad->Delete( ATTR_MIRROR_LEASE_TIME );
	submit_ad->Delete( ATTR_MIRROR_RELEASED );
	submit_ad->Delete( ATTR_MIRROR_REMOTE_LEASE_TIME );
	submit_ad->Delete( ATTR_MIRROR_SCHEDD );
	submit_ad->Delete( ATTR_STAGE_IN_FINISH );
	submit_ad->Delete( ATTR_STAGE_IN_START );
	submit_ad->Delete( ATTR_SCHEDD_BIRTHDATE );
	submit_ad->Delete( ATTR_FILE_SYSTEM_DOMAIN );
	submit_ad->Delete( ATTR_ULOG_FILE );
	submit_ad->Delete( ATTR_ULOG_USE_XML );
	submit_ad->Delete( ATTR_NOTIFY_USER );
	submit_ad->Delete( ATTR_ON_EXIT_HOLD_CHECK );
	submit_ad->Delete( ATTR_ON_EXIT_REMOVE_CHECK );
	submit_ad->Delete( ATTR_PERIODIC_HOLD_CHECK );
	submit_ad->Delete( ATTR_PERIODIC_RELEASE_CHECK );
	submit_ad->Delete( ATTR_PERIODIC_REMOVE_CHECK );
	submit_ad->Delete( ATTR_SERVER_TIME );
	submit_ad->Delete( ATTR_JOB_MANAGED );
	submit_ad->Delete( ATTR_GLOBAL_JOB_ID );
	submit_ad->Delete( "CondorPlatform" );
	submit_ad->Delete( "CondorVersion" );
	submit_ad->Delete( ATTR_JOB_GRID_TYPE );
	submit_ad->Delete( ATTR_WANT_CLAIMING );
	submit_ad->Delete( ATTR_WANT_MATCHING );
	submit_ad->Delete( ATTR_HOLD_REASON );
	submit_ad->Delete( ATTR_HOLD_REASON_CODE );
	submit_ad->Delete( ATTR_HOLD_REASON_SUBCODE );
	submit_ad->Delete( ATTR_LAST_HOLD_REASON );
	submit_ad->Delete( ATTR_LAST_HOLD_REASON_CODE );
	submit_ad->Delete( ATTR_LAST_HOLD_REASON_SUBCODE );
	submit_ad->Delete( ATTR_RELEASE_REASON );
	submit_ad->Delete( ATTR_LAST_RELEASE_REASON );
	submit_ad->Delete( ATTR_JOB_STATUS_ON_RELEASE );

	submit_ad->Assign( ATTR_JOB_STATUS, HELD );
	submit_ad->Assign( ATTR_HOLD_REASON, "Spooling input data files" );
	submit_ad->Assign( ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_VANILLA );

	submit_ad->Assign( ATTR_Q_DATE, now );
	submit_ad->Assign( ATTR_CURRENT_HOSTS, 0 );
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
	submit_ad->Assign( ATTR_ENTERED_CURRENT_STATUS, now  );
	submit_ad->Assign( ATTR_JOB_NOTIFICATION, NOTIFY_NEVER );

	expr.sprintf( "%s = (%s > 0) =!= True && CurrentTime > %s + %d",
				  ATTR_PERIODIC_REMOVE_CHECK, ATTR_STAGE_IN_FINISH,
				  ATTR_Q_DATE, 1800 );
	submit_ad->Insert( expr.Value() );

	expr.sprintf( "%s = %s == %d", ATTR_JOB_LEAVE_IN_QUEUE, ATTR_JOB_STATUS,
				  COMPLETED );
	submit_ad->Insert( expr.Value() );

	submit_ad->Assign( ATTR_SUBMITTER_ID, submitterId );

	ad->ResetExpr();
	while ( (next_expr = ad->NextExpr()) != NULL ) {
		if ( strncasecmp( ((Variable*)next_expr->LArg())->Name(), "REMOTE_", 7 ) == 0 ) {
			char *attr_value;
			MyString buf;
			submit_ad->Delete( ((Variable*)next_expr->LArg())->Name() );
			next_expr->RArg()->PrintToNewStr(&attr_value);
			buf.sprintf( "%s = %s", &((Variable*)next_expr->LArg())->Name()[7],
						 attr_value );
			submit_ad->Insert( buf.Value() );
			free(attr_value);
		}
	}

		// worry about ATTR_JOB_[OUTPUT|ERROR]_ORIG

	return submit_ad;
}
#endif

ClassAd *CondorJob::buildStageInAd()
{
	MyString expr;
	ClassAd *stage_in_ad;

		// Base the stage in ad on our own job ad
//	stage_in_ad = new ClassAd( *ad );
	stage_in_ad = buildSubmitAd();

	stage_in_ad->Assign( ATTR_CLUSTER_ID, remoteJobId.cluster );
	stage_in_ad->Assign( ATTR_PROC_ID, remoteJobId.proc );

	stage_in_ad->Delete( ATTR_ULOG_FILE );

	return stage_in_ad;
}
