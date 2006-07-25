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
#include "environ.h"  // for Environ object

#include "globus_gram_client.h"
#include "globus_duroc_control.h"

#include "gm_common.h"
#include "globusjob.h"

bool durocControlInited = false;
globus_duroc_control_t durocControl;

GlobusJob::GlobusJob( GlobusJob& copy )
{
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
	jobState = copy.jobState;
	RSL = (copy.RSL == NULL) ? NULL : strdup( copy.RSL );
	rmContact = (copy.rmContact == NULL) ? NULL : strdup( copy.rmContact );
	localOutput = (copy.localOutput == NULL) ? NULL : strdup( copy.localOutput );
	localError = (copy.localError == NULL) ? NULL : strdup( copy.localError );
	errorCode = copy.errorCode;
	jmFailureCode = copy.jmFailureCode;
	userLogFile = (copy.userLogFile == NULL) ? NULL : strdup( copy.userLogFile );
	executeLogged = copy.executeLogged;
	exitLogged = copy.exitLogged;
	stateChanged = copy.stateChanged;
	newJM = copy.newJM;
	restartingJM = copy.restartingJM;
	restartWhen = copy.restartWhen;
	durocRequest = copy.durocRequest;
	shadow_birthday = copy.shadow_birthday;
}

GlobusJob::GlobusJob( ClassAd *classad )
{
	char buf[4096];
	int job_status;
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
	rmContact = NULL;
	localOutput = NULL;
	localError = NULL;
	userLogFile = NULL;
	jobState = 0;
	errorCode = 0;
	jmFailureCode = 0;
	exitValue = 0;
	executeLogged = false;
	exitLogged = false;
	stateChanged = false;
	newJM = true;
	restartingJM = false;
	restartWhen = 0;
	durocRequest = false;
	shadow_birthday = 0;
	// classad->LookupInteger(ATTR_SHADOW_BIRTHDATE,shadow_birthday);
	// ad = NULL;

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

	classad->LookupInteger( ATTR_GLOBUS_STATUS, jobState );
	classad->LookupInteger( ATTR_JOB_STATUS, job_status );
	if ( job_status == REMOVED ) {
		jobState = G_CANCELED;
		removedByUser = true;
	}

	buf[0] = '\0';
	classad->LookupString( ATTR_GLOBUS_RESOURCE, buf );
	if ( buf[0] != '\0' ) {
		rmContact = strdup( buf );
	} else {
		rmContact = strdup( "<unknown>" );
	}

	errorCode = GLOBUS_SUCCESS;

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
	if ( jobContact ) {
		free( jobContact );
	}
	if ( RSL ) {
		free( RSL );
	}
	if ( rmContact ) {
		free( rmContact );
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
//	if ( ad ) {
//		delete ad;
//	}
}

bool GlobusJob::start()
{
	int rc;
	char *job_contact = NULL;

	// Truncate stdout/err so that we don't have duplicated output when a
	// job gets resubmitted.
	if ( localOutput != NULL && truncate( localOutput, 0 ) < 0 ) {
		dprintf(D_ALWAYS,
				"truncate failed for job %d.%d's output file %s (errno=%d)\n",
				procID.cluster, procID.proc, localOutput, errno );
	}

	if ( localError != NULL && truncate( localError, 0 ) < 0 ) {
		dprintf(D_ALWAYS,
				"truncate failed for job %d.%d's error file %s (errno=%d)\n",
				procID.cluster, procID.proc, localError, errno );
	}

	if ( durocRequest ) {

		int results_count;
		int *results;

		rc = globus_duroc_control_job_request( &durocControl, RSL, 0, NULL,
											   &job_contact, &results_count,
											   (volatile int **)&results );

		if ( rc == GLOBUS_DUROC_SUCCESS ) {

			// Should we make sure all subjobs didn't fail?

			globus_free( results );

			rc = globus_duroc_control_barrier_release( &durocControl,
													   job_contact,
													   GLOBUS_TRUE );

			if ( rc != GLOBUS_DUROC_SUCCESS ) {
				// Not sure if this should always be fatal
				globus_duroc_control_job_cancel( &durocControl, job_contact );
				errorCode = rc;
				globus_free( job_contact );
				return false;
			}

			// do we need to update the schedd on anything?

			jobContact = strdup( job_contact );
			globus_free( job_contact );

			jobState = G_ACTIVE;
			stateChanged = true;
			addJobUpdateEvent( this, JOB_UE_RUNNING );

			return true;

		} else {
			errorCode = jmFailureCode = rc;
			return false;
		}

	} else {

		rc = globus_gram_client_job_request( rmContact, RSL,
					GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL, gramCallbackContact,
					&job_contact );

		if ( rc == GLOBUS_SUCCESS ) {
			newJM = false;
			jobContact = strdup( job_contact );
			globus_gram_client_job_contact_free( job_contact );
			jobState = G_SUBMITTED;

			rehashJobContact( this, NULL, jobContact );

			stateChanged = true;
			addJobUpdateEvent( this, JOB_UE_SUBMITTED );

			callbackRegistered = true;

			return true;
		} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT ) {
			newJM = true;
			jobContact = strdup( job_contact );
			globus_gram_client_job_contact_free( job_contact );
			jobState = G_UNSUBMITTED;

			rehashJobContact( this, NULL, jobContact );

			stateChanged = true;
			addJobUpdateEvent( this, JOB_UE_SUBMITTED );

			callbackRegistered = true;

			return true;
		} else {
			errorCode = jmFailureCode = rc;
			return false;
		}

	}
}

bool GlobusJob::stop_job_manager()
{
	int rc;
	int status;
	int failure_code;

	// Before sending the signal, set ignore_callbacks flag because
	// callbacks could arrive before the send signal function returns.
	ignore_callbacks = true;	

	rc = globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STOP_MANAGER, NULL,
								&status, &failure_code );
	if ( rc == GLOBUS_SUCCESS ) {
		return true;
	} else {
		errorCode = failure_code;
		return false;
	}
}


bool GlobusJob::commit()
{
	int rc;
	int status;
	int failure_code;

	if ( restartingJM || jobState == G_UNSUBMITTED ) {
		rc = globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST,
								NULL, &status, &failure_code );
	} else {
		rc = globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END,
								NULL, &status, &failure_code );
	}

	if ( rc == GLOBUS_SUCCESS ) {
		if ( restartingJM ) {
			restartingJM = false;
		} else if ( jobState == G_UNSUBMITTED ) {
			jobState = G_SUBMITTED;
			stateChanged = true;
			addJobUpdateEvent( this, JOB_UE_SUBMITTED );
		} else if ( jobState == G_DONE ) {
			rehashJobContact( this, jobContact, NULL );
			free( jobContact );
			jobContact = NULL;

			addJobUpdateEvent( this, JOB_UE_DONE );
		} else if ( jobState == G_FAILED ) {
			rehashJobContact( this, jobContact, NULL );
			free( jobContact );
			jobContact = NULL;

			addJobUpdateEvent( this, JOB_UE_FAILED );
		} else if ( jobState == G_CANCELED ) {
			rehashJobContact( this, jobContact, NULL );
			free( jobContact );
			jobContact = NULL;

			addJobUpdateEvent( this, JOB_UE_CANCELED );
		}

		return true;
	} else {
		errorCode = failure_code;
		return false;
	}
}

bool GlobusJob::callback_register()
{
	int status;
	int failure_code;
	int rc;

	if ( callbackRegistered ) {
		// we're already done, probably because we called start()
		return true;
	}

	// First, figure out if this is an enhanced jobmanager by seeing if it
	// recognizes job signals. If this test fails, then a callback register
	// would fail for the same reason. We want an all-or-nothing scenario:
	// either the callback register succeedded and we know if it's an
	// enhanced jobmanager, or the callback is not registered.
	rc = globus_gram_client_job_signal( jobContact, (globus_gram_protocol_job_signal_t)0, NULL, &status,
										&failure_code );
	if ( rc ==  GLOBUS_GRAM_PROTOCOL_ERROR_UNKNOWN_SIGNAL_TYPE ) {
		rc = GLOBUS_SUCCESS;
		newJM = true;
	} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOB_QUERY ||
				rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL ) {
		rc = GLOBUS_SUCCESS;
		newJM = false;
	}

	if ( rc == GLOBUS_SUCCESS ) {
		rc = globus_gram_client_job_callback_register( jobContact, 
						GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL, gramCallbackContact,
						&status, &failure_code );
	}

	rehashJobContact( this, NULL, jobContact );

	if ( rc == GLOBUS_SUCCESS && newJM ) {

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
							rsl, &status, &failure_code );

	}

    if ( rc == GLOBUS_SUCCESS ) {
		callbackRegistered = true;
		return true;
	} else {
		errorCode = failure_code;
		return false;
	}
}

bool GlobusJob::callback( int state = 0, int error = 0 )
{
	if (ignore_callbacks) {
		dprintf( D_FULLDEBUG, "Ignoring callback - state %d for job %d.%d\n",
				 state, procID.cluster, procID.proc );
		return true;
	}

	if ( state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING ) {
		if ( jobState != G_PENDING ) {
			jobState = G_PENDING;

			stateChanged = true;
			addJobUpdateEvent( this, JOB_UE_STATE_CHANGE );
		}
	} else if ( state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ) {
		if ( jobState != G_ACTIVE ) {
			jobState = G_ACTIVE;

			stateChanged = true;
			addJobUpdateEvent( this, JOB_UE_RUNNING );
		}
	} else if ( state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
		// Not sure what the right thing to do on a FAILED message from
		// globus. For now, we usually treat it like a completed job.
		// Note: we also get this message after the job was cancelled.
		// Note: I think we also get this message if the JobManager's proxy
		//       cert expires.  Check for this, and do not change the state.
		//       instead, we'll deal with it next time we probe the job, at
		//       which point we will notice the JM is gone and will restart

		// JobsByContact with a state of FAILED shouldn't be in the hash table,
		// but we'll check anyway.
		if ( jobState != G_FAILED ) {


			if ( removedByUser ) {
				// If the user removed the job anyhow, consider it cancelled.
				jobState = G_CANCELED;
				stateChanged = true;
				addJobUpdateEvent( this, JOB_UE_CANCELED );
			} else {
				// If it failed because the proxy expired or because
				// asked to shutdown, ignore the error if we can
				// restart the job manager.... and we'll simply
				// restart it next time we need it.
				if ( (error == GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED ||
					  error == GLOBUS_GRAM_PROTOCOL_ERROR_TTL_EXPIRED ||
					  error == GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED) &&
					  newJM ) {

						const char *err_str = 
									globus_gram_client_error_string(error);
						if ( !err_str ) {
							err_str = "unknown";
						}

						dprintf(D_ALWAYS,"Job Manager %s failed because %s, "
								"will restart it\n",jobContact,err_str);
				} else {
					// JM Failed with an error which cannot be handled
					// by just restarting it, or we are talking to a job
					// manager which cannot be restarted (old version).
					jobState = G_FAILED;
					jmFailureCode = error;
					stateChanged = true;
					addJobUpdateEvent( this, JOB_UE_FAILED );
					// TODO
					// put on hold instead? restart? (for certain errors)
					// userlog entry (what type of event is this?)
				}
			}


			if ( !newJM ) {
				rehashJobContact( this, jobContact, NULL );
				free( jobContact );
				jobContact = NULL;
			}
		}
	} else if ( state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {

		int fd;

		// Force the streamed stdout/err to disk
		if ( localOutput != NULL ) {
			fd = open( localOutput, O_WRONLY );
			if ( fd < 0 ) {
				dprintf( D_ALWAYS,
					"open failed for job %d.%d's output file %s (errno=%d)\n",
						 procID.cluster, procID.proc, localOutput, errno );
			}
			if ( fd >= 0 && fsync( fd ) < 0 ) {
				dprintf( D_ALWAYS,
					"fsync failed for job %d.%d's output file %s (errno=%d)\n",
						 procID.cluster, procID.proc, localOutput, errno );
			}
			if ( fd >= 0 && close( fd ) < 0 ) {
				dprintf( D_ALWAYS,
					"close failed for job %d.%d's output file %s (errno=%d)\n",
						 procID.cluster, procID.proc, localOutput, errno );
			}
		}

		if ( localError != NULL ) {
			fd = open( localError, O_WRONLY );
			if ( fd < 0 ) {
				dprintf( D_ALWAYS,
					"open failed for job %d.%d's error file %s (errno=%d)\n",
						 procID.cluster, procID.proc, localError, errno );
			}
			if ( fd >= 0 && fsync( fd ) < 0 ) {
				dprintf( D_ALWAYS,
					"fsync failed for job %d.%d's error file %s (errno=%d)\n",
						 procID.cluster, procID.proc, localError, errno );
			}
			if ( fd >= 0 && close( fd ) < 0 ) {
				dprintf( D_ALWAYS,
					"close failed for job %d.%d's error file %s (errno=%d)\n",
						 procID.cluster, procID.proc, localError, errno );
			}
		}

		jobState = G_DONE;
		stateChanged = true;
		addJobUpdateEvent( this, JOB_UE_DONE );

		if ( !newJM ) {
			rehashJobContact( this, jobContact, NULL );
			free( jobContact );
			jobContact = NULL;
		}

	} else if ( state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED ) {
		if ( jobState != G_SUSPENDED ) {
			jobState = G_SUSPENDED;

			stateChanged = true;
			addJobUpdateEvent( this, JOB_UE_STATE_CHANGE );
		}
	} else if ( state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) {
		commit();
		jobState = G_UNSUBMITTED;
		if ( jobContact ) {
			rehashJobContact( this, jobContact, NULL );
			free( jobContact );
			jobContact = NULL;
		}
		stateChanged = true;
		addJobUpdateEvent( this, JOB_UE_NOT_SUBMITTED );
	} else {
		dprintf( D_ALWAYS, "Unknown globus job state %d for job %d.%d\n",
				 state, procID.cluster, procID.proc );
	}

	return true;
}

bool GlobusJob::cancel()
{
	int rc = GLOBUS_SUCCESS;

	if ( durocRequest ) {
		if ( jobContact != NULL ) {
			rc = globus_duroc_control_job_cancel( &durocControl, jobContact );
		}
	} else {
		if ( jobContact != NULL ) {
			rc = globus_gram_client_job_cancel( jobContact );
		}
	}

	if ( rc == GLOBUS_SUCCESS ) {
		removedByUser = true;
		return true;
	} else {
		errorCode = rc;
		return false;
	}
}

bool GlobusJob::probe()
{
	int rc = GLOBUS_SUCCESS;
	int status;
	int error;

	if ( jobContact == NULL ) {
		return true;
	}

	if ( durocRequest ) {
		int subjob_count;
		int *subjob_states;
		char **subjob_labels;
		bool all_done = true;

		// Since we don't get callbacks on the status of DUROC jobs,
		// we check the status here in the probe call.
		rc = globus_duroc_control_subjob_states( &durocControl, jobContact,
												 &subjob_count, &subjob_states,
												 &subjob_labels );

		if ( rc != GLOBUS_SUCCESS ) {
			errorCode = rc;
			return false;
		}

		for ( int i = 0; i < subjob_count; i++ ) {
			if ( subjob_states[i] != GLOBUS_DUROC_SUBJOB_STATE_DONE &&
				 subjob_states[i] != GLOBUS_DUROC_SUBJOB_STATE_FAILED ) {
				all_done = false;
			}
			if ( subjob_labels[i] != NULL ) {
				globus_free( subjob_labels[i] );
			}
		}

		globus_free ( subjob_states );
		globus_free ( subjob_labels );

		if ( all_done ) {
			// Is there any time we should consider the job failed instead
			// of done?
			jobState = G_DONE;
			stateChanged = true;
			addJobUpdateEvent( this, JOB_UE_DONE );

			free( jobContact );
			jobContact = NULL;
		}

		return true;

	} else {

		rc = globus_gram_client_job_status( jobContact, &status, &error );

		if ( rc == GLOBUS_SUCCESS ) {
			return true;
		} else {
			errorCode = error;
			return false;
		}
	}
}

bool GlobusJob::restart()
{
	int rc;
	char *job_contact = NULL;
	char rsl[4096];
	char buffer[1024];
	struct stat file_status;

	// If we killed the job manager ourselves, ignore_callbacks is set
	// to True.  When we decide it is time to restart it, we must remember
	// to put ignore_callbacks back to False.
	ignore_callbacks = false;

	if ( newJM == false ) {
		return false;
	}

	if ( durocRequest ) {
		dprintf( D_FULLDEBUG, "Attempted to restart a DUROC job (%d.%d)\n",
				 procID.cluster, procID.proc );
		return false;
	}

	sprintf( rsl, "&(restart=%s)(remote_io_url=%s)", jobContact,
			 gassServerUrl );

	if ( localOutput ) {
		rc = stat( localOutput, &file_status );
		if ( rc < 0 ) {
			file_status.st_size = 0;
		}
		sprintf( buffer, "(stdout=%s%s)(stdout_position=%d)", gassServerUrl,
				 localOutput, file_status.st_size );
		strcat( rsl, buffer );
	}

	if ( localError ) {
		rc = stat( localError, &file_status );
		if ( rc < 0 ) {
			file_status.st_size = 0;
		}
		sprintf( buffer, "(stderr=%s%s)(stderr_position=%d)", gassServerUrl,
				 localError, file_status.st_size );
		strcat( rsl, buffer );
	}

	rc = globus_gram_client_job_request( rmContact, rsl,
        GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL, gramCallbackContact,
        &job_contact );

	if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT ) {
		newJM = true;

		rehashJobContact( this, jobContact, job_contact );
		free( jobContact );
		jobContact = strdup( job_contact );
		globus_gram_client_job_contact_free( job_contact );

		stateChanged = true;
		addJobUpdateEvent( this, JOB_UE_JM_RESTARTED );

		callbackRegistered = true;

		restartingJM = true;

		return true;
	} else {
		if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_EXE ) {
			newJM = false;
		}
		errorCode = rc;
		return false;
	}
}

const char *GlobusJob::errorString()
{
	if ( durocRequest ) {
		if ( globus_duroc_error_is_gram_client_error( errorCode ) ) {
			return globus_gram_client_error_string(
					globus_duroc_error_get_gram_client_error( errorCode ) );
		} else {
			return globus_duroc_error_string( errorCode );
		}
	} else {
		return globus_gram_client_error_string( errorCode );
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


