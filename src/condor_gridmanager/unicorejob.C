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
#include "condor_xml_classads.h"

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

static char *GMStateNames[] = {
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

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	UnicoreJob::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	UnicoreJob::setGahpCallTimeout( tmp_int );
}

const char *UnicoreJobAdConst = "JobUniverse =?= 9 && (JobGridType == \"unicore\") =?= True";

bool UnicoreJobAdMustExpand( const ClassAd *jobad )
{
	return false;
}

BaseJob *UnicoreJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new UnicoreJob( jobad );
}


int UnicoreJob::probeInterval = 300;			// default value
int UnicoreJob::submitInterval = 300;			// default value
int UnicoreJob::gahpCallTimeout = 300;			// default value

UnicoreJob::UnicoreJob( ClassAd *classad )
	: BaseJob( classad )
{
	char buff[4096];
	bool job_already_submitted = false;
	char *error_string = NULL;

	jobContact = NULL;
	gmState = GM_INIT;
	unicoreState = condorState;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	enteredCurrentUnicoreState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	submitFailureCode = 0;
	submitAd = NULL;
	gahp = NULL;
	errorString = "";

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( ad->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		UpdateJobAd( ATTR_HOLD_REASON, "UNDEFINED" );
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

	buff[0] = '\0';
	ad->LookupString( "GridJobId", buff );
	if ( buff[0] != '\0' && strrchr( buff, ' ' ) != NULL ) {
		jobContact = strdup( strrchr( buff, ' ' ) + 1 );
		job_already_submitted = true;
	}

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

UnicoreJob::~UnicoreJob()
{
	if ( jobContact ) {
		free( jobContact );
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

int UnicoreJob::doEvaluateState()
{
	int old_gm_state;
	int old_unicore_state;
	bool reevaluate_state = true;
	time_t now;	// make sure you set this before every use!!!

	bool done;
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

		switch ( gmState ) {
		case GM_INIT: {
			// This is the state all jobs start in when the GlobusJob object
			// is first created. Here, we do things that we didn't want to
			// do in the constructor because they could block (the
			// constructor is called while we're connected to the schedd).
			if ( gahp->Startup() == false ) {
				dprintf( D_ALWAYS, "(%d.%d) Error starting up GAHP\n",
						 procID.cluster, procID.proc );
				
				UpdateJobAdString( ATTR_HOLD_REASON, "Failed to start GAHP" );
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
			rc = gahp->unicore_job_recover( submitAd->Value() );
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
				UpdateJobAdString( ATTR_HOLD_REASON,
									"Attempts to submit failed" );
				gmState = GM_HOLD;
				break;
			}
			now = time(NULL);
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
				rc = gahp->unicore_job_create( submitAd->Value(),
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
					jobContact = job_contact;
					char val[80];
					ad->LookupString( "GridResource", val );
					strcat( val, " " );
					strcat( val, jobContact );
					UpdateJobAdString( "GridJobId", val );
					gmState = GM_SUBMIT_SAVE;
				} else {
					// unhandled error
					dprintf(D_ALWAYS,"(%d.%d) unicore_job_create() failed\n",
							procID.cluster, procID.proc);
					dprintf(D_ALWAYS,"(%d.%d)    submitAd='%s'\n",
							procID.cluster, procID.proc,submitAd->Value());
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
				done = requestScheddUpdate( this );
				if ( !done ) {
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
					gmState = GM_PROBE_JOBMANAGER;
					break;
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + probeInterval) > now ) {
					delay = (lastProbeTime + probeInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_PROBE_JOBMANAGER: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				char *status_ad;
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
					gmState = GM_CANCEL;
					break;
				}
				UpdateUnicoreState( status_ad );
				lastProbeTime = now;
				gmState = GM_SUBMITTED;
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
//				if ( jobContact != NULL ) {
//					free( jobContact );
//					jobContact = NULL;
//					UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
//									   NULL_JOB_CONTACT );
//					requestScheddUpdate( this );
//				}
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
			if ( jobContact != NULL ) {
				free( jobContact );
				jobContact = NULL;
				UpdateJobAd( "GridJobId", "Undefined" );
			}
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
			done = requestScheddUpdate( this );
			if ( !done ) {
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

	return TRUE;
}

BaseResource *UnicoreJob::GetResource()
{
	return (BaseResource *)NULL;
}

void UnicoreJob::UpdateUnicoreState( const char *update_ad_string )
{
	ClassAd *update_ad;
	ClassAdXMLParser xml_parser;
	const char *next_attr_name;
	ExprTree *next_expr;

	update_ad = xml_parser.ParseClassAd( update_ad_string );

	update_ad->ResetName();
	while ( ( next_attr_name = update_ad->NextName() ) != NULL ) {
		if ( strcasecmp( next_attr_name, ATTR_MY_TYPE ) == 0 ||
			 strcasecmp( next_attr_name, ATTR_TARGET_TYPE ) == 0 ) {
			continue;
		}
		if ( strcasecmp( next_attr_name, ATTR_JOB_STATUS ) == 0 ) {
			int status = 0;
			update_ad->LookupInteger( ATTR_JOB_STATUS, status );
			unicoreState = status;
			if ( unicoreState == RUNNING ) {
				JobRunning();
			}
			if ( unicoreState == IDLE ) {
				JobIdle();
			}
		}
		next_expr = update_ad->Lookup( next_attr_name );
		this->ad->Insert( next_expr->DeepCopy() );
	}

	delete update_ad;
}

MyString *UnicoreJob::buildSubmitAd()
{
//	ClassAd submit_ad;
	ClassAdXMLUnparser xml_unp;
	MyString *ad_string;

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
		if ( ( expr = ad->Lookup( regular_attrs[i] ) ) != NULL ) {
			submit_ad.Insert( expr->DeepCopy() );
		}

	}
*/

	xml_unp.SetUseCompactSpacing( true );
	xml_unp.SetOutputType( false );
	xml_unp.SetOutputTargetType( false );
	ad_string = new MyString;
//	xml_unp.Unparse( &submit_ad, *ad_string );
xml_unp.Unparse( ad, *ad_string );

	return ad_string;
}
