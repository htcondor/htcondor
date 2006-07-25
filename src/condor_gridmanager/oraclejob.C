/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_string.h"	// for strnewp and friends
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"
#include "globus_utils.h"

#include "gridmanager.h"
#include "oraclejob.h"
#include "condor_config.h"


// GridManager job states
#define GM_INIT					0
#define GM_UNSUBMITTED			1
#define GM_SUBMIT_1				2
#define GM_SUBMIT_1_SAVE		3
#define GM_SUBMIT_2				4
#define GM_SUBMIT_2_SAVE		5
#define GM_SUBMIT_3				6
#define GM_SUBMITTED			7
#define GM_DONE_SAVE			8
#define GM_DONE_COMMIT			9
#define GM_CANCEL				10
#define GM_FAILED				11
#define GM_DELETE				12
#define GM_CLEAR_REQUEST		13
#define GM_HOLD					14
#define GM_RECOVER_QUERY		15

static char *GMStateNames[] = {
	"GM_INIT",
	"GM_UNSUBMITTED",
	"GM_SUBMIT_1",
	"GM_SUBMIT_1_SAVE",
	"GM_SUBMIT_2",
	"GM_SUBMIT_2_SAVE",
	"GM_SUBMIT_3",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_DONE_COMMIT",
	"GM_CANCEL",
	"GM_FAILED",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_RECOVER_QUERY"
};

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define ATTR_ORACLE_JOB_RUN_PHASE "OracleJobRunPhase"

void OracleJobInit()
{
}

void OracleJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	OracleJob::setProbeInterval( tmp_int );

	// Tell all the resource objects to deal with their new config values
//	OracleResource *next_resource;

//	ResourcesByName.startIterations();

//	while ( ResourcesByName.iterate( next_resource ) != 0 ) {
//		next_resource->Reconfig();
//	}
}

bool OracleJobAdMatch( const ClassAd *job_ad ) {
	int universe;
	MyString resource;
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 strncasecmp( resource.Value(), "oracle ", 7 ) == 0 ) {

		return true;
	}
	return false;
}

BaseJob *OracleJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new OracleJob( jobad );
}

int OracleJob::probeInterval = 300;		// default value
int OracleJob::submitInterval = 300;	// default value

OCIEnv *GlobalOciEnvHndl = NULL;
OCIError *GlobalOciErrHndl = NULL;
bool GlobalOciInitDone = false;

int InitGlobalOci()
{
	int rc;
	char *param_value = NULL;
	MyString buff;

	if ( GlobalOciInitDone ) {
		return OCI_SUCCESS;
	}

	param_value = param("ORACLE_HOME");
	if ( param_value == NULL ) {
		dprintf( D_ALWAYS, "ORACLE_HOME undefined!\n" );
		return -1;
	}
	buff.sprintf( "ORACLE_HOME=%s", param_value );
	putenv( strdup( buff.Value() ) );
	free(param_value);

	if ( GlobalOciEnvHndl == NULL ) {
		rc = OCIEnvCreate( &GlobalOciEnvHndl, OCI_DEFAULT, NULL, NULL, NULL,
						   NULL, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
			return rc;
		}
	}

	if ( GlobalOciErrHndl == NULL ) {
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&GlobalOciErrHndl,
							 OCI_HTYPE_ERROR, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
			return rc;
		}
	}

	GlobalOciInitDone = true;

	return OCI_SUCCESS;
}

void print_error( sword status, OCIError *error_handle )
{
	switch ( status ) {
	case OCI_SUCCESS:
		dprintf( D_ALWAYS, "   OCI_SUCCESS\n" );
		break;
	case OCI_SUCCESS_WITH_INFO:
		dprintf( D_ALWAYS, "   OCI_SUCCESS_WITH_INFO\n" );
		break;
	case OCI_NEED_DATA:
		dprintf( D_ALWAYS, "   OCI_NEED_DATA\n" );
		break;
	case OCI_NO_DATA:
		dprintf( D_ALWAYS, "   OCI_NO_DATA\n" );
		break;
	case OCI_ERROR:
		dprintf( D_ALWAYS, "   OCI_ERROR\n" );
		if ( error_handle == NULL ) {
			dprintf( D_ALWAYS, "   No error handle!\n" );
		} else {
			text errbuf[512];
			sb4 errcode;
			OCIErrorGet( error_handle, 1, NULL, &errcode, errbuf,
						 sizeof(errbuf), OCI_HTYPE_ERROR);
			dprintf( D_ALWAYS, "   errcode = %d, error text follows\n", errcode);
			dprintf( D_ALWAYS, "   %s\n", errbuf );
		}
		break;
	case OCI_INVALID_HANDLE:
		dprintf( D_ALWAYS, "   OCI_INVALID_HANDLE\n" );
		break;
	case OCI_STILL_EXECUTING:
		dprintf( D_ALWAYS, "   OCI_STILL_EXECUTING\n" );
		break;
	default:
		dprintf( D_ALWAYS, "   Invalid status code!!!\n" );
		break;
	}
}

OracleJob::OracleJob( ClassAd *classad )
	: BaseJob( classad )
{
	int bool_val;
	char buff[4096];
	MyString error_string = "";

	remoteJobId = NULL;
	gmState = GM_INIT;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	resourceManagerString = NULL;
	dbName = NULL;
	dbUsername = NULL;
	dbPassword = NULL;
	remoteJobState = 0;
	newRemoteStateUpdate = false;
	newRemoteState = 0;

	// In GM_HOLD, we assme HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

		// Ensure that OCI has been initialized successfully
	if ( InitGlobalOci() != OCI_SUCCESS ) {
		error_string = "OCI initialization failed";
		goto error_exit;
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_RESOURCE, buff );
	if ( buff[0] != '\0' ) {
		const char *token;
		MyString str = buff;

		str.Tokenize();

		token = str.GetNextToken( " ", false );
		if ( !token || stricmp( token, "oracle" ) ) {
			error_string.sprintf( "%s not of type oracle",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

		token = str.GetNextToken( " ", false );
		if ( token && *token ) {
			resourceManagerString = strdup( token );
		} else {
			error_string.sprintf( "%s missing server name",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

	} else {
		error_string.sprintf( "%s is not set in the job ad",
							  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	bool_val = FALSE;
	jobAd->LookupBool( ATTR_ORACLE_JOB_RUN_PHASE, bool_val );
	jobRunPhase = bool_val ? true : false;

	{
		StringList list( resourceManagerString );
		list.rewind();
		list.next();
		list.next();
		dbName = strdup ( list.next() );
		dbUsername = strdup( list.next() );
		dbPassword = strdup( list.next() );
	}

	ociSession = GetOciSession( dbName, dbUsername, dbPassword );
	if ( ociSession == NULL ) {
		error_string = "OCI initialization failed";
		goto error_exit;
	}
	ociSession->RegisterJob( this );

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff );
	if ( buff[0] != '\0' ) {
		SetRemoteJobId( strchr( buff, ' ' ) );
	}

	if ( OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociErrorHndl,
						 OCI_HTYPE_ERROR, 0, NULL ) != OCI_SUCCESS ) {
		error_string = "OCI initialization failed";
		goto error_exit;
	}

	return;

 error_exit:
	gmState = GM_HOLD;
	if ( !error_string.IsEmpty() ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string.Value() );
	}
	return;
}

OracleJob::~OracleJob()
{
	if ( ociSession ) {
		ociSession->UnregisterJob( this );
	}
	if ( remoteJobId ) {
		free( remoteJobId );
	}
	if ( ociErrorHndl ) {
		OCIHandleFree( ociErrorHndl, OCI_HTYPE_ERROR );
	}
}

void OracleJob::Reconfig()
{
	BaseJob::Reconfig();
}

int OracleJob::doEvaluateState()
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
			if ( newRemoteStateUpdate ) {
				if ( jobRunPhase == false ) {
					if ( newRemoteState == ORACLE_JOB_SUBMIT ) {
						gmState = GM_SUBMIT_2_SAVE;
					} else {
						gmState = GM_CLEAR_REQUEST;
					}
				} else {
					if ( newRemoteState == ORACLE_JOB_SUBMIT ) {
						gmState = GM_SUBMIT_3;
					} else {
						gmState = GM_SUBMITTED;
					}
				}
			} else {
				ociSession->RequestProbeJobs();
			}
			} break;
		case GM_UNSUBMITTED: {
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else if ( condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			} else {
				gmState = GM_SUBMIT_1;
			}
			} break;
		case GM_SUBMIT_1: {
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
				jobAd->Assign( ATTR_HOLD_REASON,
							   "Attempts to submit failed" );
				gmState = GM_HOLD;
				break;
			}
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {

				char *job_id = NULL;

				job_id = doSubmit1();

				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;

				if ( job_id != NULL ) {
					SetRemoteJobId( job_id );
					gmState = GM_SUBMIT_1_SAVE;
				} else {
					dprintf(D_ALWAYS,"(%d.%d) job submit 1 failed!\n",
							procID.cluster, procID.proc);
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
		case GM_SUBMIT_1_SAVE: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
				gmState = GM_SUBMIT_2;
			}
			} break;
		case GM_SUBMIT_2: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				rc = doSubmit2();
				if ( rc >= 0 ) {
					remoteJobState = ORACLE_JOB_SUBMIT;
					gmState = GM_SUBMIT_2_SAVE;
				} else {
					dprintf(D_ALWAYS,"(%d.%d) job submit 2 failed!\n",
							procID.cluster, procID.proc);
					gmState = GM_CANCEL;
				}
			}
			} break;
		case GM_SUBMIT_2_SAVE: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				if ( jobRunPhase == false ) {
					jobRunPhase = true;
					jobAd->Assign( ATTR_ORACLE_JOB_RUN_PHASE, true );
				}
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
				gmState = GM_SUBMIT_3;
			}
			} break;
		case GM_SUBMIT_3: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				rc = doSubmit3();
				if ( rc >= 0 ) {
					newRemoteStateUpdate = false;
					gmState = GM_SUBMITTED;
				} else {
					dprintf(D_ALWAYS,"(%d.%d) job submit 3 failed!\n",
							procID.cluster, procID.proc);
					gmState = GM_CANCEL;
				}
			}
			} break;
		case GM_SUBMITTED: {
			if ( condorState == COMPLETED ) {
					gmState = GM_DONE_SAVE;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				if ( newRemoteStateUpdate ) {
					switch ( newRemoteState ) {
					case ORACLE_JOB_UNQUEUED:
						remoteJobState = newRemoteState;
						JobTerminated( true, 0 );
						reevaluate_state = true;
							// set gmState to GM_DONE_SAVE??
						break;
					case ORACLE_JOB_SUBMIT:
						remoteJobState = newRemoteState;
							// what to do??
						break;
					case ORACLE_JOB_BROKEN:
						remoteJobState = newRemoteState;
							// what to do??
						break;
					case ORACLE_JOB_IDLE:
						remoteJobState = newRemoteState;
						JobIdle();
						break;
					case ORACLE_JOB_ACTIVE:
						remoteJobState = newRemoteState;
						JobRunning();
						break;
					default:
						dprintf( D_ALWAYS,
								 "(%d.%d) Unknown remote job state %d!\n",
								 procID.cluster, procID.proc, remoteJobState );
					}
					newRemoteStateUpdate = false;
				}
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
			// For now, there's nothing to clean up
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				SetRemoteJobId( NULL );
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			rc = doRemove();
			if ( rc >= 0 ) {
				gmState = GM_FAILED;
			} else {
				// What to do about a failed cancel?
				dprintf( D_ALWAYS, "(%d.%d) job cancel failed!\n",
						 procID.cluster, procID.proc );
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
			SetRemoteJobId( NULL );
			if ( jobRunPhase == true ) {
				jobRunPhase = false;
				jobAd->Assign( ATTR_ORACLE_JOB_RUN_PHASE, false );
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
			remoteJobState = ORACLE_JOB_UNQUEUED;
			newRemoteStateUpdate = false;
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

BaseResource *OracleJob::GetResource()
{
//	return (BaseResource *)myResource;
	return NULL;
}

void OracleJob::SetRemoteJobId( const char *job_id )
{
	free( remoteJobId );
	if ( job_id ) {
		remoteJobId = strdup( job_id );
	} else {
		remoteJobId = NULL;
	}

	MyString full_job_id;
	if ( job_id ) {
		full_job_id.sprintf( "oracle %s", job_id );
	}
	BaseJob::SetRemoteJobId( full_job_id.Value() );
}

void OracleJob::UpdateRemoteState( int new_state )
{
dprintf(D_FULLDEBUG,"(%d.%d) UpdateRemoteState: %d\n", procID.cluster, procID.proc, new_state );
	if ( new_state != remoteJobState ) {
		newRemoteStateUpdate = true;
		newRemoteState = new_state;
		SetEvaluateState();
	}
}

char *OracleJob::doSubmit1()
{
	OCIError *ret_err_hndl = NULL;
	OCISvcCtx *srvc_hndl = NULL;
	OCIStmt *stmt_hndl = NULL;
	OCIBind *bind1_hndl = NULL;
	OCIBind *bind2_hndl = NULL;
	int rc;
	MyString buff;
	int job_id;
	bool trans_open = false;
	const char *stmt = "begin DBMS_JOB.SUBMIT(:jobid,:jobtext,SYSDATE+(10/1440)); end;";
	const char *jobtext = "null;";

	rc = ociSession->AcquireSession( this, srvc_hndl, ret_err_hndl );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "AcquireSession failed\n" );
		print_error( rc, ret_err_hndl );
		goto doSubmit1_error_exit;
	}

	rc = OCITransStart( srvc_hndl, ociErrorHndl, 60, OCI_TRANS_NEW );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCITransStart failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit1_error_exit;
	}
	trans_open = true;

	rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&stmt_hndl, OCI_HTYPE_STMT,
						 0, NULL );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIHandleAlloc failed\n" );
		print_error( rc, NULL );
		goto doSubmit1_error_exit;
	}

	rc = OCIStmtPrepare( stmt_hndl, ociErrorHndl, (const OraText *)stmt,
						 strlen(stmt), OCI_NTV_SYNTAX, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIStmtPrepare failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit1_error_exit;
	}

	rc = OCIBindByName( stmt_hndl, &bind1_hndl, ociErrorHndl,
						(const OraText *)":jobid",
						strlen(":jobid"), &job_id, sizeof(job_id), SQLT_INT,
						NULL, NULL, NULL, 0, NULL, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIBindByName failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit1_error_exit;
	}

	rc = OCIBindByName( stmt_hndl, &bind2_hndl, ociErrorHndl,
						(const OraText *)":jobtext",
						strlen( ":jobtext" ), (void *)jobtext, strlen(jobtext) + 1,
						SQLT_STR, NULL, NULL, NULL, 0, NULL, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIBindByNamefailed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit1_error_exit;
	}

	rc = OCIStmtExecute( srvc_hndl, stmt_hndl, ociErrorHndl, 1, 0, NULL, NULL,
						 OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIStmtExecute failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit1_error_exit;
	}

	rc = OCITransCommit( srvc_hndl, ociErrorHndl, 0 );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCITransCommit failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit1_error_exit;
	}
	trans_open = false;

	rc = OCIHandleFree( stmt_hndl, OCI_HTYPE_STMT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIHandleFree failed, ignoring\n" );
		print_error( rc, NULL );
	}
	stmt_hndl = NULL;

	ociSession->ReleaseSession( this );
	srvc_hndl = NULL;

	buff.sprintf( "%d", job_id );

	return strdup( buff.Value() );

 doSubmit1_error_exit:
	if ( stmt_hndl != NULL ) {
		OCIHandleFree( stmt_hndl, OCI_HTYPE_STMT );
	}
	if ( trans_open ) {
		OCITransRollback( srvc_hndl, ociErrorHndl, OCI_DEFAULT );
	}
	if ( srvc_hndl != NULL ) {
		ociSession->ReleaseSession( this );
	}
	return NULL;
}

int OracleJob::doSubmit2()
{
	OCIError *ret_err_hndl = NULL;
	OCISvcCtx *srvc_hndl = NULL;
	OCIStmt *stmt_hndl = NULL;
	OCIBind *bind_hndl = NULL;
	int rc;
	bool trans_open = false;
	MyString stmt;
	MyString jobtext_str = "";
	char *jobtext = NULL;
	char *exec_file = NULL;
	char buff[1024];
	FILE *exec_fp;

	rc = ociSession->AcquireSession( this, srvc_hndl, ret_err_hndl );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "AcquireSession failed\n" );
		print_error( rc, ret_err_hndl );
		goto doSubmit2_error_exit;
	}

	jobAd->LookupString( ATTR_JOB_CMD, &exec_file );
	if ( exec_file == NULL || *exec_file == '\0' ) {
		dprintf( D_ALWAYS, "No executable defined!\n" );
		goto doSubmit2_error_exit;
	}

	exec_fp = fopen( exec_file, "r" );
	if ( exec_fp == NULL ) {
		dprintf( D_ALWAYS, "fopen of exec file failed\n" );
		goto doSubmit2_error_exit;
	}

	while ( fgets( buff, sizeof(buff), exec_fp ) ) {
		jobtext_str += buff;
	}

	fclose( exec_fp );
	exec_fp = NULL;
	free( exec_file );
	exec_file = NULL;

	jobtext = strdup( jobtext_str.Value() );

	rc = OCITransStart( srvc_hndl, ociErrorHndl, 60, OCI_TRANS_NEW );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCITransStart failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit2_error_exit;
	}
	trans_open = true;

	rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&stmt_hndl, OCI_HTYPE_STMT,
						 0, NULL );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIHandleAlloc failed\n" );
		print_error( rc, NULL );
		goto doSubmit2_error_exit;
	}

	stmt.sprintf( "begin DBMS_JOB.CHANGE(%s,:jobtext,SYSDATE,NULL); DBMS_JOB.BROKEN(%s,TRUE); end;",
				  remoteJobId, remoteJobId );

	rc = OCIStmtPrepare( stmt_hndl, ociErrorHndl, (const OraText *)stmt.Value(),
						 strlen(stmt.Value()), OCI_NTV_SYNTAX, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIStmtPrepare failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit2_error_exit;
	}

	rc = OCIBindByName( stmt_hndl, &bind_hndl, ociErrorHndl, (const OraText *)":jobtext",
						strlen( ":jobtext" ), jobtext, strlen(jobtext) + 1,
						SQLT_STR, NULL, NULL, NULL, 0, NULL, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIBindByNamefailed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit2_error_exit;
	}

	rc = OCIStmtExecute( srvc_hndl, stmt_hndl, ociErrorHndl, 1, 0, NULL, NULL,
						 OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIStmtExecute failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit2_error_exit;
	}

	rc = OCITransCommit( srvc_hndl, ociErrorHndl, 0 );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCITransCommit failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit2_error_exit;
	}
	trans_open = false;

	rc = OCIHandleFree( stmt_hndl, OCI_HTYPE_STMT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIHandleFree failed, ignoring\n" );
		print_error( rc, NULL );
	}
	stmt_hndl = NULL;

	free( jobtext );
	jobtext = NULL;

	ociSession->ReleaseSession( this );
	srvc_hndl = NULL;

	return 0;

 doSubmit2_error_exit:
	if ( exec_file != NULL ) {
		free( exec_file );
	}
	if ( exec_fp != NULL ) {
		fclose( exec_fp );
	}
	if ( stmt_hndl != NULL ) {
		OCIHandleFree( stmt_hndl, OCI_HTYPE_STMT );
	}
	if ( trans_open ) {
		OCITransRollback( srvc_hndl, ociErrorHndl, OCI_DEFAULT );
	}
	if ( srvc_hndl != NULL ) {
		ociSession->ReleaseSession( this );
	}
	if ( jobtext != NULL ) {
		free( jobtext );
	}
	return -1;
}

int OracleJob::doSubmit3()
{
	OCIError *ret_err_hndl = NULL;
	OCISvcCtx *srvc_hndl = NULL;
	OCIStmt *stmt_hndl = NULL;
	int rc;
	bool trans_open = false;
	MyString stmt;

	rc = ociSession->AcquireSession( this, srvc_hndl, ret_err_hndl );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "AcquireSession failed\n" );
		print_error( rc, ret_err_hndl );
		goto doSubmit3_error_exit;
	}

	rc = OCITransStart( srvc_hndl, ociErrorHndl, 60, OCI_TRANS_NEW );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCITransStart failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit3_error_exit;
	}
	trans_open = true;

	rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&stmt_hndl, OCI_HTYPE_STMT,
						 0, NULL );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIHandleAlloc failed\n" );
		print_error( rc, NULL );
		goto doSubmit3_error_exit;
	}

	stmt.sprintf( "begin DBMS_JOB.BROKEN(%s,FALSE); end;",
				  remoteJobId );

	rc = OCIStmtPrepare( stmt_hndl, ociErrorHndl, (const OraText *)stmt.Value(),
						 strlen(stmt.Value()), OCI_NTV_SYNTAX, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIStmtPrepare failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit3_error_exit;
	}

	rc = OCIStmtExecute( srvc_hndl, stmt_hndl, ociErrorHndl, 1, 0, NULL, NULL,
						 OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIStmtExecute failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit3_error_exit;
	}

	rc = OCITransCommit( srvc_hndl, ociErrorHndl, 0 );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCITransCommit failed\n" );
		print_error( rc, ociErrorHndl );
		goto doSubmit3_error_exit;
	}
	trans_open = false;

	rc = OCIHandleFree( stmt_hndl, OCI_HTYPE_STMT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIHandleFree failed, ignoring\n" );
		print_error( rc, NULL );
	}
	stmt_hndl = NULL;

	ociSession->ReleaseSession( this );
	srvc_hndl = NULL;

	return 0;

 doSubmit3_error_exit:
	if ( stmt_hndl != NULL ) {
		OCIHandleFree( stmt_hndl, OCI_HTYPE_STMT );
	}
	if ( trans_open ) {
		OCITransRollback( srvc_hndl, ociErrorHndl, OCI_DEFAULT );
	}
	if ( srvc_hndl != NULL ) {
		ociSession->ReleaseSession( this );
	}
	return -1;
}

int OracleJob::doRemove()
{
	OCIError *ret_err_hndl = NULL;
	OCISvcCtx *srvc_hndl = NULL;
	OCIStmt *stmt_hndl = NULL;
	int rc;
	bool trans_open = false;
	MyString stmt;

	rc = ociSession->AcquireSession( this, srvc_hndl, ret_err_hndl );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "AcquireSession failed\n" );
		print_error( rc, ret_err_hndl );
		goto doRemove_error_exit;
	}

	rc = OCITransStart( srvc_hndl, ociErrorHndl, 60, OCI_TRANS_NEW );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCITransStart failed\n" );
		print_error( rc, ociErrorHndl );
		goto doRemove_error_exit;
	}
	trans_open = true;

	rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&stmt_hndl, OCI_HTYPE_STMT,
						 0, NULL );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIHandleAlloc failed\n" );
		print_error( rc, NULL );
		goto doRemove_error_exit;
	}

	stmt.sprintf( "begin DBMS_JOB.REMOVE(%s); end;",
				  remoteJobId );

	rc = OCIStmtPrepare( stmt_hndl, ociErrorHndl, (const OraText *)stmt.Value(),
						 strlen(stmt.Value()), OCI_NTV_SYNTAX, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIStmtPrepare failed\n" );
		print_error( rc, ociErrorHndl );
		goto doRemove_error_exit;
	}

	rc = OCIStmtExecute( srvc_hndl, stmt_hndl, ociErrorHndl, 1, 0, NULL, NULL,
						 OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIStmtExecute failed\n" );
		print_error( rc, ociErrorHndl );
		goto doRemove_error_exit;
	}

	rc = OCITransCommit( srvc_hndl, ociErrorHndl, 0 );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCITransCommit failed\n" );
		print_error( rc, ociErrorHndl );
		goto doRemove_error_exit;
	}
	trans_open = false;

	rc = OCIHandleFree( stmt_hndl, OCI_HTYPE_STMT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIHandleFree failed, ignoring\n" );
		print_error( rc, NULL );
	}
	stmt_hndl = NULL;

	ociSession->ReleaseSession( this );
	srvc_hndl = NULL;

	return 0;

 doRemove_error_exit:
	if ( stmt_hndl != NULL ) {
		OCIHandleFree( stmt_hndl, OCI_HTYPE_STMT );
	}
	if ( trans_open ) {
		OCITransRollback( srvc_hndl, ociErrorHndl, OCI_DEFAULT );
	}
	if ( srvc_hndl != NULL ) {
		ociSession->ReleaseSession( this );
	}
	return -1;
}
