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
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "gridmanager.h"
#include "globusjob.h"
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
#define GM_CHECK_OUTPUT			8
#define GM_DONE_SAVE			9
#define GM_DONE_COMMIT			10
#define GM_STOP_AND_RESTART		11
#define GM_RESTART				12
#define GM_RESTART_SAVE			13
#define GM_RESTART_COMMIT		14
#define GM_CANCEL				15
#define GM_CANCEL_WAIT			16
#define GM_FAILED				17
#define GM_DELETE				18
#define GM_CLEAR_REQUEST		19
#define GM_HOLD					20
#define GM_PROXY_EXPIRED		21
#define GM_CLEAN_JOBMANAGER		22

char *GMStateNames[] = {
	"GM_INIT",
	"GM_REGISTER",
	"GM_STDIO_UPDATE",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_SUBMIT_COMMIT",
	"GM_SUBMITTED",
	"GM_CHECK_OUTPUT",
	"GM_DONE_SAVE",
	"GM_DONE_COMMIT",
	"GM_STOP_AND_RESTART",
	"GM_RESTART",
	"GM_RESTART_SAVE",
	"GM_RESTART_COMMIT",
	"GM_CANCEL",
	"GM_CANCEL_WAIT",
	"GM_FAILED",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROXY_EXPIRED",
	"GM_CLEAN_JOBMANAGER"
};

// TODO: once we can set the jobmanager's proxy timeout, we should either
// let this be set in the config file or set it to
// GRIDMANAGER_MINIMUM_PROXY_TIME + 60
#define JM_MIN_PROXY_TIME	300

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define LOG_GLOBUS_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, globusState %d: %s returned Globus error %d\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],globusState, \
        func,error)

int GlobusJob::probeInterval = 300;		// default value
int GlobusJob::submitInterval = 300;	// default value
int GlobusJob::restartInterval = 60;	// default value
int GlobusJob::gahpCallTimeout = 300;	// default value
int GlobusJob::maxConnectFailures = 3;	// default value

GlobusJob::GlobusJob( GlobusJob& copy )
{
	dprintf(D_ALWAYS, "GlobusJob copy constructor called. This is a No-No!\n");
	ASSERT( 0 );
}

GlobusJob::GlobusJob( ClassAd *classad, GlobusResource *resource )
{
	int transfer;
	char buff[4096];
	char buff2[_POSIX_PATH_MAX];
	char iwd[_POSIX_PATH_MAX];

	RSL = NULL;
	callbackRegistered = false;
	classad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
	classad->LookupInteger( ATTR_PROC_ID, procID.proc );
	jobContact = NULL;
	localOutput = NULL;
	localError = NULL;
	globusStateErrorCode = 0;
	globusStateBeforeFailure = 0;
	callbackGlobusState = 0;
	callbackGlobusStateErrorCode = 0;
	submitLogged = false;
	executeLogged = false;
	submitFailedLogged = false;
	terminateLogged = false;
	abortLogged = false;
	evictLogged = false;
	holdLogged = false;
	stateChanged = false;
	jmVersion = GRAM_V_UNKNOWN;
	restartingJM = false;
	restartWhen = 0;
	gmState = GM_INIT;
	globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
	jmUnreachable = false;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	enteredCurrentGlobusState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	syncedOutputSize = 0;
	syncedErrorSize = 0;
	submitFailureCode = 0;
	lastRestartReason = 0;
	lastRestartAttempt = 0;
	numRestartAttempts = 0;
	numRestartAttemptsThisSubmit = 0;
	connect_failure_counter = 0;

	evaluateStateTid = daemonCore->Register_Timer( TIMER_NEVER,
								(TimerHandlercpp)&GlobusJob::doEvaluateState,
								"doEvaluateState", (Service*) this );;
	gahp.setNotificationTimerId( evaluateStateTid );
	gahp.setMode( GahpClient::normal );
	gahp.setTimeout( gahpCallTimeout );

	resourceDown = false;
	resourceStateKnown = false;
	myResource = resource;
	// RegisterJob() may call our NotifyResourceUp/Down(), so be careful.
	myResource->RegisterJob( this );

	ad = classad;

	ad->LookupInteger( ATTR_GLOBUS_GRAM_VERSION, jmVersion );

	buff[0] = '\0';
	ad->LookupString( ATTR_GLOBUS_CONTACT_STRING, buff );
	if ( buff[0] != '\0' && strcmp( buff, NULL_JOB_CONTACT ) != 0 ) {
		if ( jmVersion == GRAM_V_UNKNOWN ) {
			dprintf(D_ALWAYS,
					"Non-NULL contact string and unknown gram version!\n");
		}
		rehashJobContact( this, jobContact, buff );
		jobContact = strdup( buff );
	}

	ad->LookupInteger( ATTR_GLOBUS_STATUS, globusState );
	ad->LookupInteger( ATTR_JOB_STATUS, condorState );

	globusError = GLOBUS_SUCCESS;

	ad->LookupInteger( ATTR_JOB_OUTPUT_SIZE, syncedOutputSize );
	ad->LookupInteger( ATTR_JOB_ERROR_SIZE, syncedErrorSize );

	iwd[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_IWD, iwd) && *iwd ) {
		int len = strlen(iwd);
		if ( len > 1 && iwd[len - 1] != '/' ) {
			strcat( iwd, "/" );
		}
	} else {
		strcpy( iwd, "/" );
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_OUTPUT, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {
		if ( !ad->LookupBool( ATTR_TRANSFER_OUTPUT, transfer ) || transfer ) {
			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localOutput = strdup( buff2 );
		}
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_ERROR, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {
		if ( !ad->LookupBool( ATTR_TRANSFER_ERROR, transfer ) || transfer ) {
			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localError = strdup( buff2 );
		}
	}

	wantResubmit = 0;
	ad->EvalBool(ATTR_GLOBUS_RESUBMIT_CHECK,NULL,wantResubmit);

	ad->ClearAllDirtyFlags();
}

GlobusJob::~GlobusJob()
{
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	if ( jobContact ) {
		free( jobContact );
	}
	if ( RSL ) {
		delete RSL;
	}
	if ( localOutput ) {
		free( localOutput );
	}
	if ( localError ) {
		free( localError );
	}
	if (daemonCore) {
		daemonCore->Cancel_Timer( evaluateStateTid );
	}
	if ( ad ) {
		delete ad;
	}
}

void GlobusJob::UpdateJobAd( const char *name, const char *value )
{
	char buff[1024];
	sprintf( buff, "%s = %s", name, value );
	ad->InsertOrUpdate( buff );
}

void GlobusJob::UpdateJobAdInt( const char *name, int value )
{
	char buff[16];
	sprintf( buff, "%d", value );
	UpdateJobAd( name, buff );
}

void GlobusJob::UpdateJobAdFloat( const char *name, float value )
{
	char buff[16];
	sprintf( buff, "%f", value );
	UpdateJobAd( name, buff );
}

void GlobusJob::UpdateJobAdBool( const char *name, int value )
{
	if ( value ) {
		UpdateJobAd( name, "TRUE" );
	} else {
		UpdateJobAd( name, "FALSE" );
	}
}

void GlobusJob::UpdateJobAdString( const char *name, const char *value )
{
	char buff[1024];
	sprintf( buff, "\"%s\"", value );
	UpdateJobAd( name, buff );
}

void GlobusJob::SetEvaluateState()
{
	daemonCore->Reset_Timer( evaluateStateTid, 0 );
}

int GlobusJob::doEvaluateState()
{
	bool connect_failure = false;
	int old_gm_state;
	int old_globus_state;
	bool reevaluate_state = true;
	int schedd_actions = 0;
	time_t now;	// make sure you set this before every use!!!

	bool done;
	int rc;
	int status;
	int error;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, globusState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],globusState);

	if ( jmUnreachable || resourceDown ) {
		gahp.setMode( GahpClient::results_only );
	} else {
		gahp.setMode( GahpClient::normal );
	}

	do {
		reevaluate_state = false;
		old_gm_state = gmState;
		old_globus_state = globusState;

		switch ( gmState ) {
		case GM_INIT: {
			// This is the state all jobs start in when the GlobusJob object
			// is first created. If we think there's a running jobmanager
			// out there, we try to register for callbacks (in GM_REGISTER).
			// The one way jobs can end up back in this state is if we
			// attempt a restart of a jobmanager only to be told that the
			// old jobmanager process is still alive.
			if ( resourceStateKnown == false ) {
				break;
			}
			if ( jobContact == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( wantResubmit ) {
				gmState = GM_CLEAN_JOBMANAGER;
			} else {
				if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
					submitLogged = true;
				}
				if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
					executeLogged = true;
				}

				if ( jmVersion >= GRAM_V_1_0 ) {
					gmState = GM_REGISTER;
				} else {
						// Bad state.
						// NOTE: This means that if you upgrade from v6.4.x 
						// Condor-G to v6.5.x Condor-G, you had better
						// remove all jobs from the queue first!
					dprintf(D_ALWAYS,"Bad GRAM version %d in GM_INIT!\n",
							jmVersion);
					gmState = GM_HOLD;
				}
			}
			} break;
		case GM_REGISTER: {
			// Register for callbacks from an already-running jobmanager.
			rc = gahp.globus_gram_client_job_callback_register( jobContact,
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact, &status,
										&error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				globusError = GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER;
				gmState = GM_RESTART;
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND ) {
				gmState = GM_RESTART;
				break;
			} 
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL ) {
				// the job completed or failed while we were not around -- now
				// the jobmanager is sitting in a state where all it will permit
				// is a status query or a commit to exit.  switch into 
				// GM_SUBMITTED state and do an immediate probe to figure out
				// if the state is done or failed, and move on from there.
				probeNow = true;
				gmState = GM_SUBMITTED;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_callback_register()", rc );
				globusError = rc;
				gmState = GM_STOP_AND_RESTART;
				break;
			}
				// Now handle the case of we got GLOBUS_SUCCESS...
			callbackRegistered = true;
			UpdateGlobusState( status, error );
			if ( jmVersion >= GRAM_V_1_5 ) {
				gmState = GM_STDIO_UPDATE;
			} else {
				if ( localOutput || localError ) {
					gmState = GM_CANCEL;
				} else {
					gmState = GM_SUBMITTED;
				}
			}
			} break;
		case GM_STDIO_UPDATE: {
			// Update an already-running jobmanager to send its I/O to us
			// instead a previous incarnation.
			if ( RSL == NULL ) {
				RSL = buildStdioUpdateRSL();
			}
			rc = gahp.globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_UPDATE,
								RSL->Value(), &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STDIO_UPDATE)", rc );
				globusError = rc;
				gmState = GM_STOP_AND_RESTART;
				break;
			}
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) {
				gmState = GM_SUBMIT_COMMIT;
			} else {
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_UNSUBMITTED: {
			// There are no outstanding gram submissions for this job (if
			// there is one, we've given up on it).
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else if ( condorState == HELD ) {
				addScheddUpdateAction( this, UA_FORGET_JOB, GM_UNSUBMITTED );
				// This object will be deleted when the update occurs
				break;
			} else {
				gmState = GM_SUBMIT;
			}
			} break;
		case GM_SUBMIT: {
			// Start a new gram submission for this job.
			char *job_contact;
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
				UpdateJobAdString( ATTR_HOLD_REASON,
									"Attempts to submit failed" );
				gmState = GM_HOLD;
				break;
			}
			now = time(NULL);
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {
				// Once RequestSubmit() is called at least once, you must
				// CancelRequest() once you're done with the request call
				if ( myResource->RequestSubmit(this) == false ) {
					break;
				}
				if ( RSL == NULL ) {
					RSL = buildSubmitRSL();
				}
				rc = gahp.globus_gram_client_job_request( 
										myResource->ResourceName(),
										RSL->Value(),
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact, &job_contact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				myResource->CancelSubmit(this);
				lastSubmitAttempt = time(NULL);
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED ) {
					gmState = GM_PROXY_EXPIRED;
					break;
				}
				numSubmitAttempts++;
				if ( rc == GLOBUS_SUCCESS ) {
					jmVersion = GRAM_V_1_0;
					UpdateJobAdInt( ATTR_GLOBUS_GRAM_VERSION, GRAM_V_1_0 );
					callbackRegistered = true;
					rehashJobContact( this, jobContact, job_contact );
					jobContact = strdup( job_contact );
					UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
									   job_contact );
					gahp.globus_gram_client_job_contact_free( job_contact );
					gmState = GM_SUBMIT_SAVE;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT ) {
					jmVersion = GRAM_V_1_5;
					UpdateJobAdInt( ATTR_GLOBUS_GRAM_VERSION, GRAM_V_1_5 );
					callbackRegistered = true;
					rehashJobContact( this, jobContact, job_contact );
					jobContact = strdup( job_contact );
					UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
									   job_contact );
					gahp.globus_gram_client_job_contact_free( job_contact );
					gmState = GM_SUBMIT_SAVE;
				} else {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_request()", rc );
					dprintf(D_ALWAYS,"    RSL='%s'\n", RSL->Value());
					submitFailureCode = globusError = rc;
					WriteGlobusSubmitFailedEventToUserLog( ad,
														   submitFailureCode );
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
				done = addScheddUpdateAction( this, UA_UPDATE_JOB_AD,
											GM_SUBMIT_SAVE );
				if ( !done ) {
					break;
				}
				if ( jmVersion >= GRAM_V_1_5 ) {
					gmState = GM_SUBMIT_COMMIT;
				} else {
					gmState = GM_SUBMITTED;
				}
			}
			} break;
		case GM_SUBMIT_COMMIT: {
			// Now that we've saved the jobmanager's contact, commit the
			// gram job submission.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				rc = gahp.globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST,
								NULL, &status, &error );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_REQUEST)", rc );
					globusError = rc;
					WriteGlobusSubmitFailedEventToUserLog( ad, globusError );
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
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
				syncIO();
				if ( jmVersion >= GRAM_V_1_5 ) {
					gmState = GM_CHECK_OUTPUT;
				} else {
					gmState = GM_DONE_SAVE;
				}
			} else if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				gmState = GM_FAILED;
			} else {
				if ( condorState == REMOVED || condorState == HELD ) {
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
						rc = gahp.globus_gram_client_job_status( jobContact,
															&status, &error );
						if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
							 rc == GAHPCLIENT_COMMAND_PENDING ) {
							break;
						}
						if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
							 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
							connect_failure = true;
							break;
						}
						if ( rc != GLOBUS_SUCCESS ) {
							// unhandled error
							LOG_GLOBUS_ERROR( "globus_gram_client_job_status()", rc );
							globusError = rc;
							gmState = GM_STOP_AND_RESTART;
							break;
						}
						UpdateGlobusState( status, error );
						ClearCallbacks();
						lastProbeTime = now;
					} else {
						GetCallbacks();
					}
					unsigned int delay = 0;
					if ( (lastProbeTime + probeInterval) > now ) {
						delay = (lastProbeTime + probeInterval) - now;
					}				
					daemonCore->Reset_Timer( evaluateStateTid, delay );
				}
			}
			} break;
		case GM_CHECK_OUTPUT: {
			// The job has completed. Make sure we got all the output.
			// If we haven't, stop and restart the jobmanager to prompt
			// sending of the rest.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_DONE_COMMIT;
			} else {
				char size_str[128];
				if ( localOutput == NULL && localError == NULL ) {
					gmState = GM_DONE_SAVE;
					break;
				}
				int output_size = localOutput ? syncedOutputSize : -1;
				int error_size = localError ? syncedErrorSize : -1;
				sprintf( size_str, "%d %d", output_size, error_size );
				rc = gahp.globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_SIZE,
									size_str, &status, &error );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc == GLOBUS_SUCCESS ) {
					gmState = GM_DONE_SAVE;
				} else if ( rc ==  GLOBUS_GRAM_PROTOCOL_ERROR_STDIO_SIZE ) {
					globusError = rc;
					gmState = GM_STOP_AND_RESTART;
					dprintf( D_FULLDEBUG, "Requesting jobmanager restart because of GLOBUS_GRAM_PROTOCOL_ERROR_STDIO_SIZE\n" );
					dprintf( D_FULLDEBUG, "output_size = %d, error_size = %d\n", output_size, error_size );
				} else {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STDIO_SIZE)", rc );
					globusError = rc;
					gmState = GM_STOP_AND_RESTART;
					dprintf( D_FULLDEBUG, "Requesting jobmanager restart because of unknown error\n" );
				}
			}
			} break;
		case GM_DONE_SAVE: {
			// Report job completion to the schedd.
			if ( condorState != HELD && condorState != REMOVED ) {
				if ( condorState != COMPLETED ) {
					condorState = COMPLETED;
					UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
				}
				done = addScheddUpdateAction( this, UA_UPDATE_JOB_AD,
											  GM_DONE_SAVE );
				if ( !done ) {
					break;
				}
			}
			gmState = GM_DONE_COMMIT;
			} break;
		case GM_DONE_COMMIT: {
			// Tell the jobmanager it can clean up and exit.
			if ( jmVersion >= GRAM_V_1_5 ) {
				rc = gahp.globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END,
									NULL, &status, &error );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_END)", rc );
					globusError = rc;
					gmState = GM_STOP_AND_RESTART;
					break;
				}
			}
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_STOP_AND_RESTART: {
			// Something has wrong with the jobmanager and we want to stop
			// it and restart it to clear up the problem.
			rc = gahp.globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STOP_MANAGER,
								NULL, &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STOP_MANAGER)", rc );
				globusError = rc;
				gmState = GM_CANCEL;
			} else {
				gmState = GM_RESTART;
			}
			} break;
		case GM_RESTART: {
			// Something has gone wrong with the jobmanager and we need to
			// restart it.
			now = time(NULL);
			if ( jobContact == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( globusError != 0 && globusError == lastRestartReason ) {
				dprintf( D_FULLDEBUG, "Restarting jobmanager for same reason "
						 "two times in a row: %d, aborting request\n",
						 globusError );
				gmState = GM_CLEAR_REQUEST;
			} else if ( now < lastRestartAttempt + restartInterval ) {
				// After a restart, wait at least restartInterval before
				// trying another one.
				daemonCore->Reset_Timer( evaluateStateTid,
								(lastRestartAttempt + restartInterval) - now );
			} else {
				char *job_contact;

				// Once RequestSubmit() is called at least once, you must
				// CancelRequest() once you're done with the request call
				if ( myResource->RequestSubmit(this) == false ) {
					break;
				}
				if ( RSL == NULL ) {
					RSL = buildRestartRSL();
				}
				rc = gahp.globus_gram_client_job_request(
										myResource->ResourceName(),
										RSL->Value(),
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact, &job_contact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				myResource->CancelSubmit(this);
				lastRestartAttempt = time(NULL);
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED ) {
					gmState = GM_PROXY_EXPIRED;
					break;
				}
				// TODO: What should be counted as a restart attempt and
				// what shouldn't?
				numRestartAttempts++;
				numRestartAttemptsThisSubmit++;
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_OLD_JM_ALIVE ) {
					// TODO: need to avoid an endless loop of old jm not
					// responding, start new jm, new jm says old one still
					// running, try to contact old jm again. How likely is
					// this to happen?
					gmState = GM_INIT;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_EXE ) {
					gmState = GM_CLEAR_REQUEST;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT ) {
					rehashJobContact( this, jobContact, job_contact );
					jobContact = strdup( job_contact );
					UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
									   job_contact );
					gahp.globus_gram_client_job_contact_free( job_contact );
					if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
						globusState = globusStateBeforeFailure;
					}
					globusStateErrorCode = 0;
					lastRestartReason = globusError;
					ClearCallbacks();
					gmState = GM_RESTART_SAVE;
				} else {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_request()", rc );
					globusError = rc;
					gmState = GM_CLEAR_REQUEST;
				}
			}
			} break;
		case GM_RESTART_SAVE: {
			// Save the restarted jobmanager's contact string on the schedd.
			done = addScheddUpdateAction( this, UA_UPDATE_JOB_AD,
										GM_RESTART_SAVE );
			if ( !done ) {
				break;
			}
			gmState = GM_RESTART_COMMIT;
			} break;
		case GM_RESTART_COMMIT: {
			// Tell the jobmanager it can proceed with the restart.
			rc = gahp.globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST,
								NULL, &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_REQUEST)", rc );
				globusError = rc;
				gmState = GM_STOP_AND_RESTART;
			}
			gmState = GM_SUBMITTED;
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.
			if ( globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE &&
				 globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				rc = gahp.globus_gram_client_job_cancel( jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
					 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
					connect_failure = true;
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_cancel()", rc );
					globusError = rc;
					gmState = GM_CLEAR_REQUEST;
					break;
				}
			}
			if ( callbackRegistered ) {
				gmState = GM_CANCEL_WAIT;
			} else {
				// This can happen if we're restarting and fail to
				// reattach to a running jobmanager
				if ( condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_CLEAR_REQUEST;
				}
			}
			} break;
		case GM_CANCEL_WAIT: {
			// A cancel has been successfully issued. Wait for the
			// accompanying FAILED callback. Probe the jobmanager
			// occassionally to make sure it hasn't died on us.
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
				gmState = GM_DONE_COMMIT;
			} else if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				gmState = GM_FAILED;
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
					rc = gahp.globus_gram_client_job_status( jobContact,
														&status, &error );
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}
					if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
						 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
						connect_failure = true;
						break;
					}
					if ( rc != GLOBUS_SUCCESS ) {
						// unhandled error
						LOG_GLOBUS_ERROR( "globus_gram_client_job_status()", rc );
						globusError = rc;
						gmState = GM_CLEAR_REQUEST;
					}
					UpdateGlobusState( status, error );
					ClearCallbacks();
					lastProbeTime = now;
				} else {
					GetCallbacks();
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + probeInterval) > now ) {
					delay = (lastProbeTime + probeInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_FAILED: {
			// The jobmanager's job state has moved to FAILED. Send a
			// commit if necessary and take appropriate action.
			if ( globusStateErrorCode == GLOBUS_GRAM_PROTOCOL_ERROR_COMMIT_TIMED_OUT ||
				 globusStateErrorCode == GLOBUS_GRAM_PROTOCOL_ERROR_TTL_EXPIRED ||
				 globusStateErrorCode == GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED ) {
				globusError = 0;
				gmState = GM_RESTART;
			} else if ( globusStateErrorCode == GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED ) {
				gmState = GM_PROXY_EXPIRED;
			} else {
				if ( jmVersion >= GRAM_V_1_5 ) {
					// Sending a COMMIT_END here means we no longer care
					// about this job submission. Either we know the job
					// isn't pending/running or the user has told us to
					// forget lost job submissions.
					rc = gahp.globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END,
									NULL, &status, &error );
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
						 rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}
					if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
						 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
						connect_failure = true;
						break;
					}
					if ( rc != GLOBUS_SUCCESS ) {
						// unhandled error
						LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_END)", rc );
						globusError = rc;
						gmState = GM_STOP_AND_RESTART;
						break;
					}
				}

				// Since we just sent a COMMIT_END, there is no state file
				// to restart from, so there's no reason to keep the job
				// contact around. We clear it here in case this failure
				// triggers a hold on the job, so the contact is not in
				// the held classad. That way, we don't waste our time
				// with a restart doomed to failure when the job is
				// released or removed.
				rehashJobContact( this, jobContact, NULL );
				free( jobContact );
				jobContact = NULL;
				UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
								   NULL_JOB_CONTACT );
				jmVersion = GRAM_V_UNKNOWN;
				UpdateJobAdInt( ATTR_GLOBUS_GRAM_VERSION,
								jmVersion );
				addScheddUpdateAction( this, UA_UPDATE_JOB_AD, 0 );

				// TODO: Evaluate if the failure is permanent or temporary
				//   if it's temporary, try a restart, but put a limit so
				//   that we don't go into an infinite restart loop. If we
				//   do decide to try a restart, we need to stop the
				//   jobmanager instead of sending COMMIT_END.
				// Note: Up through globus 2.0 beta, the FAILED state
				//   following a cancel request has an error code of 0
				//   instead of GLOBUS_GRAM_PROTOCOL_ERROR_USER_CANCELLED
				if ( condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_CLEAR_REQUEST;
				}
			}
			} break;
		case GM_DELETE: {
			// The job has completed or been removed. Delete it from the
			// schedd.
			schedd_actions = UA_DELETE_FROM_SCHEDD | UA_FORGET_JOB;
			if ( condorState == REMOVED ) {
				schedd_actions |= UA_LOG_ABORT_EVENT;
			} else if ( condorState == COMPLETED ) {
				schedd_actions |= UA_LOG_TERMINATE_EVENT;
			}
			addScheddUpdateAction( this, schedd_actions, GM_DELETE );
			// This object will be deleted when the update occurs
			} break;
		case GM_CLEAN_JOBMANAGER: {
			// Tell the jobmanager to cleanup an un-restartable job submission

			// Once RequestSubmit() is called at least once, you must
			// CancelRequest() once you're done with the request call
			char cleanup_rsl[4096];
			char *dummy_job_contact;
			if ( myResource->RequestSubmit(this) == false ) {
				break;
			}
			snprintf( cleanup_rsl, sizeof(cleanup_rsl), "&(cleanup=%s)", jobContact );
			rc = gahp.globus_gram_client_job_request( 
										myResource->ResourceName(), 
										cleanup_rsl,
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact,
										&dummy_job_contact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			myResource->CancelSubmit(this);
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure = true;
				break;
			}
			if ( rc == GLOBUS_SUCCESS ||
				 rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CANCEL_FAILED ) {
				 // All is good... deed is done.
				gmState = GM_CLEAR_REQUEST;
			} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_EXE ) {
				// Jobmanager doesn't support cleanup attribute.
				gmState = GM_CLEAR_REQUEST;
			} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_NO_STATE_FILE ) {
				// State file disappeared.  Could be a normal condition
				// if we never successfully committed a submit (and
				// thus the jobmanager removed the state file when it timed
				// out waiting for the submit commit signal).
				gmState = GM_CLEAR_REQUEST;
			} else {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_request()", rc );
				dprintf(D_ALWAYS,"    RSL='%s'\n", cleanup_rsl);
				globusError = rc;
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CLEAR_REQUEST: {
			// Remove all knowledge of any previous or present job
			// submission, in both the gridmanager and the schedd.

			// For now, put problem jobs on hold instead of
			// forgetting about current submission and trying again.
			// TODO: Let our action here be dictated by the user preference
			// expressed in the job ad.
			if ( (jobContact != NULL || (globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED && globusStateErrorCode != GLOBUS_GRAM_PROTOCOL_ERROR_JOB_UNSUBMITTED)) 
				     && condorState != REMOVED 
					 && wantResubmit == 0 ) {
				gmState = GM_HOLD;
				break;
			}
			if ( wantResubmit ) {
				wantResubmit = 0;
				dprintf(D_ALWAYS,
					"(%d.%d) Resubmitting to Globus because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_GLOBUS_RESUBMIT_CHECK );
			}
			schedd_actions = 0;
			if ( globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) {
				globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
				UpdateJobAdInt( ATTR_GLOBUS_STATUS, globusState );
				schedd_actions |= UA_UPDATE_JOB_AD;
			}
			globusStateErrorCode = 0;
			globusError = 0;
			lastRestartReason = 0;
			numRestartAttemptsThisSubmit = 0;
			jmVersion = GRAM_V_UNKNOWN;
			ClearCallbacks();
			if ( jobContact != NULL ) {
				rehashJobContact( this, jobContact, NULL );
				free( jobContact );
				jobContact = NULL;
				UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
								   NULL_JOB_CONTACT );
				schedd_actions |= UA_UPDATE_JOB_AD;
			}
			if ( condorState == RUNNING ) {
				condorState = IDLE;
				UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
				schedd_actions |= UA_UPDATE_JOB_AD;
			}
			if ( localOutput && syncedOutputSize > 0 ) {
				syncedOutputSize = 0;
				UpdateJobAdInt( ATTR_JOB_OUTPUT_SIZE, syncedOutputSize );
				schedd_actions |= UA_UPDATE_JOB_AD;
			}
			if ( localError && syncedErrorSize > 0 ) {
				syncedErrorSize = 0;
				UpdateJobAdInt( ATTR_JOB_ERROR_SIZE, syncedErrorSize );
				schedd_actions |= UA_UPDATE_JOB_AD;
			}
			if ( submitLogged ) {
				schedd_actions |= UA_LOG_EVICT_EVENT;
			}
			// If there are no updates to be done when we first enter this
			// state, addScheddUpdateAction will return done immediately
			// and not waste time with a needless connection to the
			// schedd. If updates need to be made, they won't show up in
			// schedd_actions after the first pass through this state
			// because we modified our local variables the first time
			// through. However, since we registered update events the
			// first time, addScheddUpdateAction won't return done until
			// they've been committed to the schedd.
			done = addScheddUpdateAction( this, schedd_actions,
										  GM_CLEAR_REQUEST );
			if ( !done ) {
				break;
			}
			if ( localOutput && truncate( localOutput, 0 ) < 0 ) {
				dprintf(D_ALWAYS,
						"(%d.%d) truncate failed for output file %s (errno=%d)\n",
						procID.cluster, procID.proc, localOutput, errno );
			}
			if ( localError && truncate( localError, 0 ) < 0 ) {
				dprintf(D_ALWAYS,
						"(%d.%d) truncate failed for error file %s (errno=%d)\n",
						procID.cluster, procID.proc, localError, errno );
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
			condorState = HELD;
			UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
			schedd_actions = UA_HOLD_JOB | UA_FORGET_JOB | UA_UPDATE_JOB_AD;
			if ( jobContact &&
				 globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN ) {
				globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN;
				UpdateJobAdInt( ATTR_GLOBUS_STATUS, globusState );
				//UpdateGlobusState( GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN, 0 );
			}
			// Set the hold reason as best we can
			// TODO: set the hold reason in a more robust way.
			char holdReason[1024];
			holdReason[0] = '\0';
			holdReason[sizeof(holdReason)-1] = '\0';
			ad->LookupString( ATTR_HOLD_REASON, holdReason,
							  sizeof(holdReason) - 1 );
			if ( holdReason[0] == '\0' && globusStateErrorCode != 0 ) {
				snprintf( holdReason, 1024, "Globus error %d: %s",
						  globusStateErrorCode,
						  gahp.globus_gram_client_error_string( globusStateErrorCode ) );
			}
			if ( holdReason[0] == '\0' && globusError != 0 ) {
				snprintf( holdReason, 1024, "Globus error %d: %s", globusError,
						gahp.globus_gram_client_error_string( globusError ) );
			}
			if ( holdReason[0] == '\0' ) {
				strcpy( holdReason, "Unspecified gridmanager error" );
			}
			UpdateJobAdString( ATTR_HOLD_REASON, holdReason );
			addScheddUpdateAction( this, schedd_actions, GM_HOLD );
			// This object will be deleted when the update occurs
			} break;
		case GM_PROXY_EXPIRED: {
			// The jobmanager has exited because the proxy is about to
			// expire. If we get a new proxy, try to restart the jobmanager.
			// Otherwise, just wait for the imminent death of the
			// gridmanager.
			now = time(NULL);
			if ( Proxy_Expiration_Time > JM_MIN_PROXY_TIME + now ) {
				if ( jobContact ) {
					globusError = 0;
					gmState = GM_RESTART;
				} else {
					gmState = GM_UNSUBMITTED;
				}
			} else {
				// Do nothing. Our proxy is about to expire.
			}
			} break;
		default:
			EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster,procID.proc,
					gmState );
		}

		if ( gmState != old_gm_state || globusState != old_globus_state ) {
			reevaluate_state = true;
		}
		if ( globusState != old_globus_state ) {
//			dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
//					procID.cluster, procID.proc,
//					GlobusJobStatusName(old_globus_state),
//					GlobusJobStatusName(globusState));
			enteredCurrentGlobusState = time(NULL);
		}
		if ( gmState != old_gm_state ) {
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
			// If we were waiting for a pending globus call, we're not
			// anymore so purge it.
			gahp.purgePendingRequests();
			// If we were calling a globus call that used RSL, we're done
			// with it now, so free it.
			if ( RSL ) {
				delete RSL;
				RSL = NULL;
			}
			connect_failure_counter = 0;
		}

	} while ( reevaluate_state );

	if ( connect_failure && !jmUnreachable && !resourceDown ) {
		if ( connect_failure_counter < maxConnectFailures ) {
				// We are seeing a lot of failures to connect
				// with Globus 2.2 libraries, often due to GSI not able 
				// to authenticate.
			connect_failure_counter++;
			int retry_secs = param_integer(
				"GRIDMANAGER_CONNECT_FAILURE_RETRY_INTERVAL",5);
			dprintf(D_FULLDEBUG,
				"(%d.%d) Connection failure (try #%d), retrying in %d secs\n",
				procID.cluster,procID.proc,connect_failure_counter,retry_secs);
			daemonCore->Reset_Timer( evaluateStateTid, retry_secs );
		} else {
			dprintf(D_FULLDEBUG,
				"(%d.%d) Connection failure, requesting a ping of the resource\n",
				procID.cluster,procID.proc);
			jmUnreachable = true;
			myResource->RequestPing( this );
		}
	}

	return TRUE;
}

void GlobusJob::NotifyResourceDown()
{
	resourceStateKnown = true;
	if ( resourceDown == false ) {
		WriteGlobusResourceDownEventToUserLog( ad );
	}
	resourceDown = true;
	jmUnreachable = false;
	// set downtime timestamp?
}

void GlobusJob::NotifyResourceUp()
{
	resourceStateKnown = true;
	if ( resourceDown == true ) {
		WriteGlobusResourceUpEventToUserLog( ad );
	}
	resourceDown = false;
	if ( jmUnreachable ) {
		globusError = GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER;
		gmState = GM_RESTART;
		enteredCurrentGmState = time(NULL);
		if ( RSL ) {
			delete RSL;
			RSL = NULL;
		}
	}
	jmUnreachable = false;
	SetEvaluateState();
}

// We're assuming that any updates that come through UpdateCondorState()
// are coming from the schedd, so we don't need to update it.
void GlobusJob::UpdateCondorState( int new_state )
{
	if ( new_state != condorState ) {
		condorState = new_state;
		UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
		SetEvaluateState();
	}
}

void GlobusJob::UpdateGlobusState( int new_state, int new_error_code )
{
	bool allow_transition = true;

	// Prevent non-transitions or transitions that go backwards in time.
	// The jobmanager shouldn't do this, but notification of events may
	// get re-ordered (callback and probe results arrive backwards).
    if ( new_state == globusState ||
		 new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ||
		 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ||
		 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ||
		 ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING &&
		   globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) ) {
		allow_transition = false;
	}

	if ( allow_transition ) {
		// where to put logging of events: here or in EvaluateState?
		int update_actions = UA_UPDATE_JOB_AD;

		dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
				procID.cluster, procID.proc,
				GlobusJobStatusName(globusState),
				GlobusJobStatusName(new_state));

		if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE &&
			 condorState == IDLE ) {
			condorState = RUNNING;
			UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
		}

		if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED &&
			 condorState == RUNNING ) {
			condorState = IDLE;
			UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
		}

		if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED &&
			 !submitLogged && !submitFailedLogged ) {
			if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
					// TODO: should SUBMIT_FAILED_EVENT be used only on
					//   certain errors (ones we know are submit-related)?
				submitFailureCode = new_error_code;
				update_actions |= UA_LOG_SUBMIT_FAILED_EVENT;
			} else {
					// The request was successfuly submitted. Write it to
					// the user-log and increment the globus submits count.
				int num_globus_submits = 0;
				update_actions |= UA_LOG_SUBMIT_EVENT;
				ad->LookupInteger( ATTR_NUM_GLOBUS_SUBMITS,
								   num_globus_submits );
				num_globus_submits++;
				UpdateJobAdInt( ATTR_NUM_GLOBUS_SUBMITS, num_globus_submits );
			}
		}
		if ( (new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
			  new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ||
			  new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED)
			 && !executeLogged ) {
			update_actions |= UA_LOG_EXECUTE_EVENT;
		}

		if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
			globusStateBeforeFailure = globusState;
		} else {
			UpdateJobAdInt( ATTR_GLOBUS_STATUS, new_state );
		}

		globusState = new_state;
		globusStateErrorCode = new_error_code;
		enteredCurrentGlobusState = time(NULL);

		addScheddUpdateAction( this, update_actions, 0 );
	}
	SetEvaluateState(); // should this be inside the if statement?
}

void GlobusJob::GramCallback( int new_state, int new_error_code )
{
	callbackGlobusState = new_state;
	callbackGlobusStateErrorCode = new_error_code;

	SetEvaluateState();
}

void GlobusJob::GetCallbacks()
{
	if ( callbackGlobusState != 0 ) {
		UpdateGlobusState( callbackGlobusState,
						   callbackGlobusStateErrorCode );
		callbackGlobusState = 0;
		callbackGlobusStateErrorCode = 0;
	}
}

void GlobusJob::ClearCallbacks()
{
	callbackGlobusState = 0;
	callbackGlobusStateErrorCode = 0;
}

GlobusResource *GlobusJob::GetResource()
{
	return myResource;
}

int GlobusJob::syncIO()
{
	int rc;
	int fd;
	struct stat file_status;

	if ( localOutput != NULL ) {
		rc = stat( localOutput, &file_status );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS,
				"(%d.%d) stat failed for output file %s (errno=%d)\n",
				procID.cluster, procID.proc, localOutput, errno );
			file_status.st_size = 0;
		}

		if ( file_status.st_size > syncedOutputSize ) {
			errno = 0;
			fd = open( localOutput, O_WRONLY );
			if ( fd < 0 ) {
				dprintf( D_ALWAYS,
						 "(%d.%d) open failed for output file %s (errno=%d)\n",
						 procID.cluster, procID.proc, localOutput, errno );
			}
			if ( fd >= 0 && fsync( fd ) < 0 ) {
				dprintf( D_ALWAYS,
						 "(%d.%d) fsync failed for output file %s (errno=%d)\n",
						 procID.cluster, procID.proc, localOutput, errno );
			}
			if ( fd >= 0 && close( fd ) < 0 ) {
				dprintf( D_ALWAYS,
						 "(%d.%d) close failed for output file %s (errno=%d)\n",
						 procID.cluster, procID.proc, localOutput, errno );
			}
			if ( errno == 0 ) {
				syncedOutputSize = file_status.st_size;
				UpdateJobAdInt( ATTR_JOB_OUTPUT_SIZE, syncedOutputSize );
				addScheddUpdateAction( this, UA_UPDATE_JOB_AD, 0 );
			}
		}
	}

	if ( localError != NULL ) {
		rc = stat( localError, &file_status );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS,
				"(%d.%d) stat failed for error file %s (errno=%d)\n",
				procID.cluster, procID.proc, localError, errno );
			file_status.st_size = 0;
		}

		if ( file_status.st_size > syncedErrorSize ) {
			errno = 0;
			fd = open( localError, O_WRONLY );
			if ( fd < 0 ) {
				dprintf( D_ALWAYS,
						 "(%d.%d) open failed for error file %s (errno=%d)\n",
						 procID.cluster, procID.proc, localError, errno );
			}
			if ( fd >= 0 && fsync( fd ) < 0 ) {
				dprintf( D_ALWAYS,
						 "(%d.%d) fsync failed for error file %s (errno=%d)\n",
						 procID.cluster, procID.proc, localError, errno );
			}
			if ( fd >= 0 && close( fd ) < 0 ) {
				dprintf( D_ALWAYS,
						 "(%d.%d) close failed for error file %s (errno=%d)\n",
						 procID.cluster, procID.proc, localError, errno );
			}
			if ( errno == 0 ) {
				syncedErrorSize = file_status.st_size;
				UpdateJobAdInt( ATTR_JOB_ERROR_SIZE, syncedErrorSize );
				addScheddUpdateAction( this, UA_UPDATE_JOB_AD, 0 );
			}
		}
	}

	return TRUE;
}

MyString *GlobusJob::buildSubmitRSL()
{
	int transfer;
	MyString *rsl = new MyString;
	char buff[11000];
	char buff2[1024];
	char iwd[_POSIX_PATH_MAX];

	buff[0] = '\0';
	ad->LookupString( ATTR_GLOBUS_RSL, buff );
	if ( buff[0] == '&' ) {
		*rsl = buff;
		return rsl;
	}

	iwd[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_IWD, iwd) && *iwd ) {
		int len = strlen(iwd);
		if ( len > 1 && iwd[len - 1] != '/' ) {
			strcat( iwd, "/" );
		}
	} else {
		strcpy( iwd, "/" );
	}

	//We're assuming all job clasads have a command attribute
	buff2[0] = '\0';
	ad->LookupString( ATTR_JOB_CMD, buff );
	*rsl = "&(executable=";
	if ( !ad->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer ) || transfer ) {
		if ( buff[0] != '/' ) {
			strcat( buff2, iwd );
		}
		strcat( buff2, buff );

		sprintf( buff, "%s%s", gassServerUrl, buff2 );
	}
	*rsl += rsl_stringify( buff );

	buff[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_REMOTE_IWD, buff) && *buff ) {
		*rsl += ")(directory=";
		*rsl += rsl_stringify( buff );
	}

	buff[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_ARGUMENTS, buff) && *buff ) {
		*rsl += ")(arguments=";
		*rsl += buff;
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_INPUT, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {
		*rsl += ")(stdin=";
		if ( !ad->LookupBool( ATTR_TRANSFER_INPUT, transfer ) || transfer ) {
			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}
			strcat( buff2, buff );

			sprintf( buff, "%s%s", gassServerUrl, buff2 );
		}
		*rsl += rsl_stringify( buff );
	}

	if ( localOutput != NULL ) {
		*rsl += ")(stdout=";
		sprintf( buff, "%s%s", gassServerUrl, localOutput );
		*rsl += rsl_stringify( buff );
	} else {
		buff[0] = '\0';
		if ( ad->LookupString(ATTR_JOB_OUTPUT, buff) && *buff &&
			 strcmp( buff, NULL_FILE ) ) {
			*rsl += ")(stdout=";
			*rsl += rsl_stringify( buff );
		}
	}

	if ( localError != NULL ) {
		*rsl += ")(stderr=";
		sprintf( buff, "%s%s", gassServerUrl, localError );
		*rsl += rsl_stringify( buff );
	} else {
		buff[0] = '\0';
		if ( ad->LookupString(ATTR_JOB_ERROR, buff) && *buff &&
			 strcmp( buff, NULL_FILE ) ) {
			*rsl += ")(stderr=";
			*rsl += rsl_stringify( buff );
		}
	}

	buff[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_ENVIRONMENT, buff) && *buff ) {
		Environ env_obj;
		env_obj.add_string(buff);
		char **env_vec = env_obj.get_vector();
		char var[5000];
		int i = 0;
		if ( env_vec[0] ) {
			*rsl += ")(environment=";
		}
		while (env_vec[i]) {
			char *equals = strchr(env_vec[i],'=');
			if ( !equals ) {
				// this environment entry has no equals sign!?!?
				continue;
			}
			*equals = '\0';
			sprintf( var, "(%s %s)", env_vec[i], rsl_stringify(equals + 1) );
			*rsl += var;
			i++;
		}
	}

	sprintf( buff, ")(save_state=yes)(two_phase=%d)(remote_io_url=%s)",
			 JM_COMMIT_TIMEOUT, gassServerUrl );
	*rsl += buff;

	if ( ad->LookupString(ATTR_GLOBUS_RSL, buff) && *buff ) {
		*rsl += buff;
	}

	return rsl;
}

MyString *GlobusJob::buildRestartRSL()
{
	int rc;
	MyString *rsl = new MyString;
	char buff[1024];
	struct stat file_status;

	rsl->sprintf( "&(restart=%s)(remote_io_url=%s)", jobContact,
				  gassServerUrl );
	if ( localOutput ) {
		rc = stat( localOutput, &file_status );
		if ( rc < 0 ) {
			file_status.st_size = 0;
		}
		sprintf( buff, "%s%s", gassServerUrl, localOutput );
		sprintf( buff, "(stdout=%s)(stdout_position=%lu)",
				 rsl_stringify( buff ), file_status.st_size );
		*rsl += buff;
	}
	if ( localError ) {
		rc = stat( localError, &file_status );
		if ( rc < 0 ) {
			file_status.st_size = 0;
		}
		sprintf( buff, "%s%s", gassServerUrl, localError );
		sprintf( buff, "(stderr=%s)(stderr_position=%lu)",
				 rsl_stringify( buff ), file_status.st_size );
		*rsl += buff;
	}

	return rsl;
}

MyString *GlobusJob::buildStdioUpdateRSL()
{
	int rc;
	MyString *rsl;
	char buff[1024];
	struct stat file_status;

	rsl->sprintf( "&(remote_io_url=%s)", gassServerUrl );
	if ( localOutput ) {
		rc = stat( localOutput, &file_status );
		if ( rc < 0 ) {
			file_status.st_size = 0;
		}
		sprintf( buff, "%s%s", gassServerUrl, localOutput );
		sprintf( buff, "(stdout=%s)(stdout_position=%lu)",
				 rsl_stringify( buff ), file_status.st_size );
		*rsl += buff;
	}
	if ( localError ) {
		rc = stat( localError, &file_status );
		if ( rc < 0 ) {
			file_status.st_size = 0;
		}
		sprintf( buff, "%s%s", gassServerUrl, localError );
		sprintf( buff, "(stderr=%s)(stderr_position=%lu)",
				 rsl_stringify( buff ), file_status.st_size );
		*rsl += buff;
	}

	return rsl;
}
