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

#include "globus_gram_client.h"
#include "globus_gram_error.h"
#include "globus_duroc_control.h"

#include "gridmanager.h"
#include "globusjob.h"


#define JOB_PROBE_INTERVAL		300

// timer id values that indicates the timer is not registered
#define TIMER_UNSET		-1

// GridManager job states
#define GM_INIT					0
#define GM_REGISTER				1
#define GM_STDIO_UPDATE			2
#define GM_UNSUBMITTED			3
#define GM_SUBMIT_UNCOMITTED	4
#define GM_SUBMIT_SAVED			5
#define GM_SUBMITTED			6
#define GM_DONE_UNCOMMITTED		7
#define GM_DONE_SAVED			8
#define GM_STOP_AND_RESTART		9
#define GM_RESTART				10
#define GM_RESTART_UNCOMMITTED	11
#define GM_RESTART_SAVED		12
#define GM_CANCEL				13
#define GM_CANCEL_WAIT			14
#define GM_DELETE				15
#define GM_CLEAR_REQUEST		16

bool durocControlInited = false;
globus_duroc_control_t durocControl;

GlobusJob::GlobusJob( GlobusJob& copy )
{
	dprintf(D_ALWAYS, "GlobusJob copy constructor called. This is a No-No!\n");
	ASSERT( 0 );
/*
	if ( !durocControlInited ) {
		globus_duroc_control_init( &durocControl );
		durocControlInited = true;
	}

	removedByUser = copy.removedByUser;
	callbackRegistered = copy.callbackRegistered;
	ignore_callbacks = copy.ignore_callbacks;
	procID = copy.procID;
	exitValue = copy.exitValue;
	jobContact = (copy.jobContact == NULL) ? NULL : strdup( copy.jobContact );
	RSL = (copy.RSL == NULL) ? NULL : strdup( copy.RSL );
	localOutput = (copy.localOutput == NULL) ? NULL : strdup( copy.localOutput );
	localError = (copy.localError == NULL) ? NULL : strdup( copy.localError );
	globusError = copy.globusError;
	jmFailureCode = copy.jmFailureCode;
	userLogFile = (copy.userLogFile == NULL) ? NULL : strdup( copy.userLogFile );
	executeLogged = copy.executeLogged;
	exitLogged = copy.exitLogged;
	stateChanged = copy.stateChanged;
	newJM = copy.newJM;
	restartingJM = copy.restartingJM;
	restartWhen = copy.restartWhen;
	durocRequest = copy.durocRequest;
	resourceDown = copy.resourceDown;
	condorState = copy.condorState;
	gmState = copy.gmState;
	globusState = copy.globusState;
	jmUnreachable = false;
	evaluateStateTid = TIMER_UNSET;

	myResource = copy.myResource;
	myResource->RegisterJob( this );
*/
}

GlobusJob::GlobusJob( ClassAd *classad, GlobusResource *resource )
{
	char buf[4096];
	bool full_rsl_given = false;

	if ( !durocControlInited ) {
		globus_duroc_control_init( &durocControl );
		durocControlInited = true;
	}

	removedByUser = false;
	callbackRegistered = false;
	ignore_callbacks = false;
	classad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
	classad->LookupInteger( ATTR_PROC_ID, procID.proc );
	jobContact = NULL;
	RSL = NULL;
	localOutput = NULL;
	localError = NULL;
	userLogFile = NULL;
	globusError = 0;
	jmFailureCode = 0;
	exitValue = 0;
	executeLogged = false;
	exitLogged = false;
	stateChanged = false;
	newJM = false;
	restartingJM = false;
	restartWhen = 0;
	durocRequest = false;
	gmState = GM_INIT;
	globusState = G_UNSUBMITTED;
	jmUnreachable = false;
	evaluateStateTid = TIMER_UNSET;
	lastProbeTime = 0;
	// ad = NULL;

	myResource = resource;
	myResource->RegisterJob( this );
	resourceDown = myResource->IsDown();

	buf[0] = '\0';
	classad->LookupString( ATTR_GLOBUS_RSL, buf );
	if ( buf[0] == '&' ) {
		full_rsl_given = true;
		RSL = strdup( buf );
	} else if ( buf[0] == '+' ) {
		full_rsl_given = true;
		durocRequest = true;
		RSL = strdup( buf );
	}

	// Currently, no contact string is saved on the schedd for duroc jobs
	// since the contact string is meaningless outside of the process that
	// initiates the job. Make sure we don't get one by accident, since we
	// can't resume a duroc job.
	if ( !durocRequest ) {
		buf[0] = '\0';
		classad->LookupString( ATTR_GLOBUS_CONTACT_STRING, buf );
		if ( strcmp( buf, NULL_JOB_CONTACT ) != 0 ) {
			jobContact = strdup( buf );
		}
	}

	classad->LookupInteger( ATTR_GLOBUS_STATUS, globusState );
	classad->LookupInteger( ATTR_JOB_STATUS, condorState );
/*
	if ( condorState == REMOVED ) {
		globusState = G_CANCELED;
		removedByUser = true;
	}
*/

	globusError = GLOBUS_SUCCESS;

	buf[0] = '\0';
	classad->LookupString( ATTR_ULOG_FILE, buf );
	if ( buf[0] != '\0' ) {
		userLogFile = strdup( buf );
	}

	if ( !full_rsl_given ) {
		RSL = buildRSL( classad );
	}
}

GlobusJob::~GlobusJob()
{
	if ( evaluateStateTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( evaluateStateTid );
	}
	if ( myResource != NULL ) {
		myResource->UnregisterJob( this );
	}
	if ( jobContact ) {
		free( jobContact );
	}
	if ( RSL ) {
		free( RSL );
	}
	if ( localOutput ) {
		free( localOutput );
	}
	if ( localError ) {
		free( localError );
	}
	if ( userLogFile ) {
		free( userLogFile );
	}
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
//	if ( ad ) {
//		delete ad;
//	}
}

void GlobusJob::SetEvaluateState()
{
	if ( evaluateStateTid == TIMER_UNSET ) {
		evaluateStateTid = daemonCore->Register_Timer( 0,
					(TimerHandlercpp)&GlobusJob::doEvaluateState,
					"doEvaluateState", (Service*) this );
	} else {
		daemonCore->Reset_Timer( evaluateStateTid, 0 );
	}
}

int GlobusJob::doEvaluateState()
{
	bool new_call = true;
	bool connect_failure = false;
	int old_gm_state;
	int old_globus_state;
	bool reevaluate_state = true;

	int rc;
	int status;
	int error;

	evaluateStateTid = TIMER_UNSET;

	if ( jmUnreachable || resourceDown ) {
		new_call = false;
	}

	do {
		reevaluate_state = false;
		old_gm_state = gmState;
		old_globus_state = globusState;

		switch ( gmState ) {
		case GM_INIT:
			if ( jobContact == NULL ) {
				gmState = GM_UNSUBMITTED;
			} else {
				rc = globus_gram_client_job_signal( jobContact,
										(globus_gram_client_job_signal_t)0,
										NULL, &status, &error, new_call );
				if ( rc == WOULD_CALL || rc == PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_UNKNOWN_SIGNAL_TYPE ) {
					newJM = true;
					gmState = GM_REGISTER;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOB_QUERY ||
							rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL ) {
					newJM = false;
					if ( localOutput || localError ) {
						gmState = GM_CANCEL;
					} else {
						gmState = GM_REGISTER;
					}
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
					gmState = GM_RESTART;
				} else {
					// unhandled error
				}
			}
			break;
		case GM_REGISTER:
			rc = globus_gram_client_job_callback_register( jobContact,
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact, &status,
										&error, new_call );
			if ( rc == WOULD_CALL || rc == PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
				connect_failure = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
			}
			globusStatus = status;
			globusError = error;
			addScheddUpdateAction( this, UA_UPDATE_GLOBUS_STATE );
			if ( newJM ) {
				gmState = GM_STDIO_UPDATE;
			} else {
				gmState = GM_SUBMITTED;
			}
			break;
		case GM_STDIO_UPDATE:
			char rsl[4096];
			char buffer[1024];
			struct stat file_status;

			sprintf( rsl, "&(remote_io_url=%s)", gassServerUrl );
			if ( localOutput ) {
				rc = stat( localOutput, &file_status );
				if ( rc < 0 ) {
					file_status.st_size = 0;
				}
				sprintf( buffer, "(stdout=%s%s)(stdout_position=%d)",
						 gassServerUrl, localOutput, file_status.st_size );
				strcat( rsl, buffer );
			}
			if ( localError ) {
				rc = stat( localError, &file_status );
				if ( rc < 0 ) {
					file_status.st_size = 0;
				}
				sprintf( buffer, "(stderr=%s%s)(stderr_position=%d)",
						 gassServerUrl, localError, file_status.st_size );
				strcat( rsl, buffer );
			}
			rc = globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_UPDATE,
								rsl, &status, &error, new_cal );
			if ( rc == WOULD_CALL || rc == PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
				connect_failure = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
			}
			if ( globusState == UNSUBMITTED ) {
				gmState = GM_SUBMIT_SAVED;
			} else {
				gmState = GM_SUBMITTED;
			}
			break;
		case GM_UNSUBMITTED:
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else if ( condorState == HELD ) {
				DeleteJob( this );
			} else {
				char *job_contact;
				rc = globus_gram_client_job_request( rmContact, rsl,
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact, &job_contact,
										new_call );
				if ( rc == WOULD_CALL || rc == PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ) {
					connect_failure = true;
					break;
				}
				if ( rc == GLOBUS_SUCCESS ) {
					newJM = false;
					jobContact = strdup( job_contact );
					globus_gram_client_job_contact_free( job_contact );
					gmState = GM_SUBMIT_UNCOMMITTED;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT ) {
					newJM = true;
					jobContact = strdup( job_contact );
					globus_gram_client_job_contact_free( job_contact );
					gmState = GM_SUBMIT_UNCOMMITTED;
				} else {
					// unhandled error
				}
			}
			break;
		case GM_SUBMIT_UNCOMITTED:
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				rc = addScheddUpdateAction( this, UA_UPDATE_CONTACT_STRING,
											GM_SUBMIT_UNCOMMITTED );
				if ( rc == PENDING ) {
					break;
				}
				if ( newJM ) {
					gmState = GM_SUBMIT_SAVED;
				} else {
					gmState = GM_SUBMITTED;
				}
			}
			break;
		case GM_SUBMIT_SAVED:
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				rc = globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST,
								NULL, &status, &error, new_call );
				if ( rc == WOULD_CALL || rc == PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
					connect_failure = true;
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
				}
				gmState = GM_SUBMITTED;
			}
			break;
		case GM_SUBMITTED:
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				if ( globusState == DONE ) {
					if ( newJM ) {
						if ( localOutput == NULL && localError == NULL ) {
							gmState = GM_DONE_UNCOMMITTED;
							break;
						}
						int output_size = -1;
						int error_size = -1;
						struct stat file_status;
						char size_str[50];
						if ( localOutput ) {
							rc = stat( localOutput, &file_status );
							if ( rc < 0 ) {
								output_size = 0;
							} else {
								output_size = file_status.st_size;
							}
						}
						if ( localError ) {
							rc = stat( localError, &file_status );
							if ( rc < 0 ) {
								error_size = 0;
							} else {
								error_size = file_status.st_size;
							}
						}
						sprintf( size_str, "%d %d", output_size, error_size );
						rc = globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_SIZE,
									size_str, &status, &error, new_call );
						if ( rc == WOULD_CALL || rc == PENDING ) {
							break;
						}
						if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
							connect_failure = true;
							break;
						}
						if ( rc == GLOBUS_SUCCESS ) {
							gmState = GM_DONE_UNCOMMITTED;
						} else if ( rc ==  GLOBUS_GRAM_PROTOCOL_ERROR_STDIO_SIZE ) {
							gmState = GM_STOP_AND_RESTART;
						} else {
							// unhandled error
						}
					} else {
						condorState = COMPLETED;
						addScheddUpdateAction( this, UA_UPDATE_CONDOR_STATE );
						gmState = GM_DELETE;
					}
				} else if ( globusState == FAILED ) {
					if ( globusError == GLOBUS_GRAM_PROTOCOL_ERROR_COMMIT_TIMED_OUT ||
						 globusError == GLOBUS_GRAM_PROTOCOL_ERROR_TTL_EXPIRED ||
						 globusError == GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED ||
						 globusError == GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED ) {
						gmState = GM_RESTART;
					} else {
						if ( newJM ) {
							rc = globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END,
									NULL, &status, &error, new_call );
							if ( rc == WOULD_CALL || rc == PENDING ) {
								break;
							}
							if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
								connect_failure = true;
								break;
							}
							if ( rc != GLOBUS_SUCCESS ) {
								// unhandled error
							}
						}
						gmState = GM_CLEAR_REQUEST;
					}
				} else {
					time_t now = time(NULL);
					if ( now >= lastProbeTime + JOB_PROBE_INTERVAL ) {
						rc = globus_gram_client_job_status( jobContact,
															&status, &error );
						if ( rc == WOULD_CALL || rc == PENDING ) {
							break;
						}
						if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
							connect_failure = true;
							break;
						}
						if ( rc != GLOBUS_SUCCESS ) {
							// unhandled error
						}
						globusState = state;
						globusError = error;
						lastProbeTime = now;
					}
					evaluateStateTid = daemonCore->Register_Timer(
								lastProbeTime + JOB_PROBE_INTERVAL,
								(TimerHandlercpp)&GlobusJob::doEvaluateState,
								"doEvaluateState", (Service*) this );
				}
			}
			break;
		case GM_DONE_UNCOMMITTED:
			condorState = COMPLETED;
			rc = addScheddUpdateAction( this, UA_UPDATE_CONDOR_STATE,
										GM_DONE_UNCOMMITTED );
			if ( rc == PENDING ) {
				break;
			}
			gmState = GM_DONE_SAVED;
			break;
		case GM_DONE_SAVED:
			rc = globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END,
									NULL, &status, &error, new_call );
			if ( rc == WOULD_CALL || rc == PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
				connect_failure = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
			}	
			gmState = GM_DELETE;
			break;
		case GM_STOP_AND_RESTART:
			rc = globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STOP_MANAGER,
								NULL, &status, &error, new_call );
			if ( rc == WOULD_CALL || rc == PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
				connect_failure = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
			}	
			gmState = GM_RESTART;
			break;
		case GM_RESTART:
			if ( jobContact == NULL ) {
				gmState = GM_UNSUBMITTED;
			} else {
				char *job_contact;
				char rsl[4096];
				char buffer[1024];
				struct stat file_status;

				sprintf( rsl, "&(restart=%s)(remote_io_url=%s)", jobContact,
						 gassServerUrl );
				if ( localOutput ) {
					rc = stat( localOutput, &file_status );
					if ( rc < 0 ) {
						file_status.st_size = 0;
					}
					sprintf( buffer, "(stdout=%s%s)(stdout_position=%d)",
							 gassServerUrl, localOutput, file_status.st_size );
					strcat( rsl, buffer );
				}
				if ( localError ) {
					rc = stat( localError, &file_status );
					if ( rc < 0 ) {
						file_status.st_size = 0;
					}
					sprintf( buffer, "(stderr=%s%s)(stderr_position=%d)",
							 gassServerUrl, localError, file_status.st_size );
					strcat( rsl, buffer );
				}
				rc = globus_gram_client_job_request( rmContact, rsl,
										GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
										gramCallbackContact, &job_contact,
										new_call );
				if ( rc == WOULD_CALL || rc == PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED ) {
					connect_failure = true;
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_EXE ) {
					newJM = false;
					gmState = GM_CLEAR_REQUEST;
				} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT ) {
					newJM = true;
					jobContact = strdup( job_contact );
					globus_gram_client_job_contact_free( job_contact );
					gmState = GM_SUBMIT_UNCOMMITTED;
				} else {
					gmState = GM_CLEAR_REQUEST;
				}
			}
			break;
		case GM_RESTART_UNCOMMITTED:
			rc = addScheddUpdateAction( this, UA_UPDATE_CONTACT_STRING,
										GM_RESTART_UNCOMMITTED );
			if ( rc == PENDING ) {
				break;
			}
			gmState = GM_RESTART_SAVED;
			break;
		case GM_RESTART_SAVED:
			rc = globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST,
								NULL, &status, &error, new_call );
			if ( rc == WOULD_CALL || rc == PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
				connect_failure = true;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
			}
			gmState = GM_SUBMITTED;
			break;
		case GM_CANCEL:
			if ( globusState != DONE && globusState != FAILED ) {
				rc = globus_gram_client_job_cancel( jobContact );
				if ( rc == WOULD_CALL || rc == PENDING ) {
					break;
				}
				if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
					connect_failure = true;
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
				}
			}
			gmState = GM_CANCEL_WAIT;
			break;
		case GM_CANCEL_WAIT:
			if ( globusState == DONE ) {
				if ( newJM ) {
					rc = globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END,
									NULL, &status, &error, new_call );
					if ( rc == WOULD_CALL || rc == PENDING ) {
						break;
					}
					if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
						connect_failure = true;
						break;
					}
					if ( rc != GLOBUS_SUCCESS ) {
						// unhandled error
					}
				}
				if ( condorState == REMOVED ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_CLEAR_REQUEST;
				}
			} else if ( globusState == FAILED ) {
				if ( globusError == GLOBUS_GRAM_PROTOCOL_ERROR_COMMIT_TIMED_OUT ||
					 globusError == GLOBUS_GRAM_PROTOCOL_ERROR_TTL_EXPIRED ||
					 globusError == GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED ||
					 globusError == GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED ) {
					gmState = GM_RESTART;
				} else {
					if ( newJM ) {
						rc = globus_gram_client_job_signal( jobContact,
									GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END,
									NULL, &status, &error, new_call );
						if ( rc == WOULD_CALL || rc == PENDING ) {
							break;
						}
						if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
							connect_failure = true;
							break;
						}
						if ( rc != GLOBUS_SUCCESS ) {
							// unhandled error
						}
					}
					if ( condorState == REMOVED ) {
						gmState = GM_DELETE;
					} else {
						gmState = GM_CLEAR_REQUEST;
					}
				}
			} else {
				time_t now = time(NULL);
				if ( now >= lastProbeTime + JOB_PROBE_INTERVAL ) {
					rc = globus_gram_client_job_status( jobContact, &status,
														&error );
					if ( rc == WOULD_CALL || rc == PENDING ) {
						break;
					}
					if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
						connect_failure = true;
						break;
					}
					if ( rc != GLOBUS_SUCCESS ) {
						// unhandled error
					}
					globusState = state;
					globusError = error;
					lastProbeTime = now;
				}
				evaluateStateTid = daemonCore->Register_Timer(
								lastProbeTime + JOB_PROBE_INTERVAL,
								(TimerHandlercpp)&GlobusJob::doEvaluateState,
								"doEvaluateState", (Service*) this );
			}
			break;
		case GM_DELETE:
			rc = addScheddUpdateAction( this, UA_DELETE_FROM_SCHEDD,
										GM_DELETE );
			if ( rc == PENDING ) {
				break;
			}
			DeleteJob( this );
			break;
		case GM_CLEAR_REQUEST:
			globusState = UNSUBMITTED;
			free( jobContact );
			jobContact = NULL;
			rc = addScheddUpdateAction( this, UA_UPDATE_CONTACT_STRING,
										GM_CLEAR_REQUEST );
			if ( rc == PENDING ) {
				break;
			}
			gmState = GM_UNSUBMITTED;
			break;
		default:
			// Complain loudly!!
		}

		if ( gmState != old_gm_state || globusState != old_globus_state ) {
			reevaluate_state = true;
		}
		if ( gmState != old_gm_state ) {
			// reset lastProbeTime whenever we enter a new state
			lastProbeTime = time(NULL);
		}

	} while ( reevaluate_state );

	if ( connect_failure && !jmUnreachable && !resourceDown ) {
		jmUnreachable = true;
		myResource->RequestPing( this );
	}

	return TRUE;
}

void GlobusJob::NotifyResourceDown()
{
	if ( resourceDown == false ) {
		WriteGlobusResourceDownEventToUserLog( this );
	}
	resourceDown = true;
	jmUnreachable = false;
	// set downtime timestamp?
}

void GlobusJob::NotifyResourceUp()
{
	if ( resourceDown == true ) {
		WriteGlobusResourceUpEventToUserLog( this );
	}
	resourceDown = false;
	if ( jmUnreachable ) {
		gmState = GM_RESTART;
	}
	jmUnreachable = false;
	SetEvaluateState();
}

void GlobusJob::UpdateCondorState( int new_state )
{
	condorState = new_state;
	SetEvaluateState();
}

void GlobusJob::UpdateGlobusState( int new_state )
{
	globusState = new_state;
	// where to put logging of events: here or in EvaluateState?
	addScheddUpdateAction( this, UA_UPDATE_GLOBUS_STATE, 0 );
	SetEvaluateState();
}

GlobusResrouce *GlobusJob::GetResource()
{
	return myResource;
}

const char *GlobusJob::errorString()
{
	if ( durocRequest ) {
		if ( globus_duroc_error_is_gram_client_error( globusError ) ) {
			return globus_gram_client_error_string(
					globus_duroc_error_get_gram_client_error( globusError ) );
		} else {
			return globus_duroc_error_string( globusError );
		}
	} else {
		return globus_gram_client_error_string( globusError );
	}
}


char *GlobusJob::buildRSL( ClassAd *classad )
{
	int transfer;
	char rsl[15000];
	char buff[11000];
	char buff2[1024]; // used to build localOutput and localError
	char *iwd;

	buff[0] = '\0';
	if ( classad->LookupString(ATTR_JOB_IWD, buff) && *buff ) {
		int len = strlen(buff);
		if ( len > 1 && buff[len - 1] != '/' ) {
			strcat( buff, "/" );
		}
		iwd = strdup( buff );
	} else {
		iwd = strdup( "/" );
	}

	//We're assuming all job clasads have a command attribute
	classad->LookupString( ATTR_JOB_CMD, buff );
	strcpy( rsl, "&(executable=" );
	if ( !classad->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer ) || transfer ) {
		strcat( rsl, gassServerUrl );
		if ( buff[0] != '/' ) {
			strcat( rsl, iwd );
		}
	}
	strcat( rsl, buff );

	buff[0] = '\0';
	if ( classad->LookupString(ATTR_JOB_REMOTE_IWD, buff) && *buff ) {
		strcat( rsl, ")(directory=" );
		strcat( rsl, buff );
	}

	buff[0] = '\0';
	if ( classad->LookupString(ATTR_JOB_ARGUMENTS, buff) && *buff ) {
		strcat( rsl, ")(arguments=" );
		strcat( rsl, buff );
	}

	buff[0] = '\0';
	if ( classad->LookupString(ATTR_JOB_INPUT, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {
		strcat( rsl, ")(stdin=" );
		if ( !classad->LookupBool( ATTR_TRANSFER_INPUT, transfer ) || transfer ) {
			strcat( rsl, gassServerUrl );
			if ( buff[0] != '/' ) {
				strcat( rsl, iwd );
			}
		}
		strcat( rsl, buff );
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( classad->LookupString(ATTR_JOB_OUTPUT, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {
		strcat( rsl, ")(stdout=" );
		if ( !classad->LookupBool( ATTR_TRANSFER_OUTPUT, transfer ) || transfer ) {
			strcat( rsl, gassServerUrl );
			if ( buff[0] != '/' ) {
				strcat( rsl, iwd );
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localOutput = strdup( buff2 );
		}
		strcat( rsl, buff );
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( classad->LookupString(ATTR_JOB_ERROR, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {
		strcat( rsl, ")(stderr=" );
		if ( !classad->LookupBool( ATTR_TRANSFER_ERROR, transfer ) || transfer ) {
			strcat( rsl, gassServerUrl );
			if ( buff[0] != '/' ) {
				strcat( rsl, iwd );
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localError = strdup( buff2 );
		}
		strcat( rsl, buff );
	}

	buff[0] = '\0';
	if ( classad->LookupString(ATTR_JOB_ENVIRONMENT, buff) && *buff ) {
		Environ env_obj;
		env_obj.add_string(buff);
		char **env_vec = env_obj.get_vector();
		char var[5000];
		int i = 0;
		if ( env_vec[0] ) {
			strcat( rsl, ")(environment=" );
		}
		while (env_vec[i]) {
			char *equals = strchr(env_vec[i],'=');
			if ( !equals ) {
				// this environment entry has no equals sign!?!?
				continue;
			}
			*equals = '\0';
			sprintf( var, "(%s %s)", env_vec[i], rsl_stringify(equals + 1) );
			strcat(rsl,var);
			i++;
		}
	}

	sprintf( buff, ")(save_state=yes)(two_phase=%d)(remote_io_url=%s)",
			 JM_COMMIT_TIMEOUT, gassServerUrl );
	strcat( rsl, buff );

	if ( classad->LookupString(ATTR_GLOBUS_RSL, buff) && *buff ) {
		strcat( rsl, buff );
	}

	if (iwd) {
		free(iwd);
	}

	return strdup( rsl );
}


