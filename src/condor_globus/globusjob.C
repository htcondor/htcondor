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

#include "gm_common.h"
#include "globusjob.h"


GlobusJob::GlobusJob( GlobusJob& copy )
{
	removedByUser = copy.removedByUser;
	callbackRegistered = copy.callbackRegistered;
	procID = copy.procID;
	exitValue = copy.exitValue;
	jobContact = (copy.jobContact == NULL) ? NULL : strdup( copy.jobContact );
	jobState = copy.jobState;
	RSL = (copy.RSL == NULL) ? NULL : strdup( copy.RSL );
	rmContact = (copy.rmContact == NULL) ? NULL : strdup( copy.rmContact );
	errorCode = copy.errorCode;
	userLogFile = (copy.userLogFile == NULL) ? NULL : strdup( copy.userLogFile );
	executeLogged = copy.executeLogged;
	exitLogged = copy.exitLogged;
	stateChanged = copy.stateChanged;
	newJM = copy.newJM;
}

GlobusJob::GlobusJob( ClassAd *classad )
{
	char buf[4096];

	removedByUser = false;
	callbackRegistered = false;
	classad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
	classad->LookupInteger( ATTR_PROC_ID, procID.proc );
	jobContact = NULL;
	RSL = NULL;
	rmContact = NULL;
	userLogFile = NULL;
	jobState = 0;
	errorCode = 0;
	exitValue = 0;
	executeLogged = false;
	exitLogged = false;
	stateChanged = false;
	newJM = false;
	// ad = NULL;

	buf[0] = '\0';
	classad->LookupString( ATTR_GLOBUS_CONTACT_STRING, buf );
	if ( strcmp( buf, NULL_JOB_CONTACT ) != 0 ) {
		jobContact = strdup( buf );
	}
	classad->LookupInteger( ATTR_GLOBUS_STATUS, jobState );
	buf[0] = '\0';
	classad->LookupString( ATTR_GLOBUS_RESOURCE, buf );
	if ( buf[0] != '\0' ) {
		rmContact = strdup( buf );
	}
	errorCode = GLOBUS_SUCCESS;
	buf[0] = '\0';
	userLogFile = NULL;
	classad->LookupString( ATTR_ULOG_FILE, buf );
	if ( buf[0] != '\0' ) {
		userLogFile = strdup( buf );
	}
	RSL = buildRSL( classad );
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
	if ( userLogFile ) {
		free( userLogFile );
	}
	//if ( ad ) {
		//delete ad;
	//}
}

bool GlobusJob::start()
{
	int rc;
	char *job_contact = NULL;

    rc = globus_gram_client_job_request( rmContact, RSL,
        GLOBUS_GRAM_CLIENT_JOB_STATE_ALL, gramCallbackContact,
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
	} else if ( rc == GLOBUS_GRAM_CLIENT_ERROR_WAITING_FOR_COMMIT ) {
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
		errorCode = rc;
		return false;
	}
}

bool GlobusJob::commit()
{
	int rc;
	int status;
	int failure_code;

	rc = globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_CLIENT_JOB_SIGNAL_COMMIT, NULL,
								&status, &failure_code );
	if ( rc == GLOBUS_SUCCESS ) {
		if ( jobState == G_UNSUBMITTED ) {
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

    rc = globus_gram_client_job_callback_register( jobContact, 
        GLOBUS_GRAM_CLIENT_JOB_STATE_ALL, gramCallbackContact,
        &status, &failure_code );

	rehashJobContact( this, NULL, jobContact );

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
	if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_PENDING ) {
		if ( jobState != G_PENDING ) {
			jobState = G_PENDING;

			stateChanged = true;
			addJobUpdateEvent( this, JOB_UE_STATE_CHANGE );
		}
	} else if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_ACTIVE ) {
		if ( jobState != G_ACTIVE ) {
			jobState = G_ACTIVE;

			stateChanged = true;
			addJobUpdateEvent( this, JOB_UE_RUNNING );
		}
	} else if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_FAILED ) {
		// Not sure what the right thing to do on a FAILED message from
		// globus. For now, we treat it like a completed job.
		// Note: we also get this message after the job was cancelled.

		// JobsByContact with a state of FAILED shouldn't be in the hash table,
		// but we'll check anyway.
		if ( jobState != G_FAILED ) {
			jobState = G_FAILED;

			stateChanged = true;

			//dprintf(D_ALWAYS,"TODD DEBUGCHECK %s:%d\n",__FILE__,__LINE__);
			if ( removedByUser ) {
				// we got here cuz user removed the job with condor_rm
				// log this fact into the user log.
			//dprintf(D_ALWAYS,"TODD DEBUGCHECK %s:%d\n",__FILE__,__LINE__);
				jobState = G_CANCELED;
				addJobUpdateEvent( this, JOB_UE_CANCELED );
			} else {
				addJobUpdateEvent( this, JOB_UE_FAILED );
			}

			// TODO

			// put on hold instead? (for certain errors)

			// userlog entry (what type of event is this?)

			// email saying that the job failed?

			if ( !newJM ) {
				rehashJobContact( this, jobContact, NULL );
				free( jobContact );
				jobContact = NULL;
			}
		}
	} else if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_DONE ) {
		// JobsByContact with a state of DONE shouldn't be in the hash table,
		// but we'll check anyway.
		if ( jobState != G_DONE ) {
			jobState = G_DONE;

			stateChanged = true;
			addJobUpdateEvent( this, JOB_UE_DONE );

			// email saying the job is done?

			if ( !newJM ) {
				rehashJobContact( this, jobContact, NULL );
				free( jobContact );
				jobContact = NULL;
			}
		}
	} else if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_SUSPENDED ) {
		if ( jobState != G_SUSPENDED ) {
			jobState = G_SUSPENDED;

			stateChanged = true;
			addJobUpdateEvent( this, JOB_UE_STATE_CHANGE );
		}
	} else {
		dprintf( D_ALWAYS, "Unknown globus job state %d for job %d.%d\n",
				 state, procID.cluster, procID.proc );
	}

	return true;
}

bool GlobusJob::cancel()
{
	int rc = GLOBUS_SUCCESS;

	if ( jobContact != NULL ) {
		rc = globus_gram_client_job_cancel( jobContact );
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
	int rc;
	int status;
	int error;

	rc = globus_gram_client_job_status( jobContact, &status, &error );

	if ( rc == GLOBUS_SUCCESS ) {
		return true;
	} else {
		errorCode = error;
		return false;
	}
}

const char *GlobusJob::errorString()
{
	return globus_gram_client_error_string( errorCode );
}


char *GlobusJob::buildRSL( ClassAd *classad )
{
	int transfer;
	char rsl[15000];
	char buff[11000];
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
	if ( classad->LookupString(ATTR_JOB_OUTPUT, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {
		strcat( rsl, ")(stdout=" );
		if ( !classad->LookupBool( ATTR_TRANSFER_OUTPUT, transfer ) || transfer ) {
			strcat( rsl, gassServerUrl );
			if ( buff[0] != '/' ) {
				strcat( rsl, iwd );
			}
		}
		strcat( rsl, buff );
	}

	buff[0] = '\0';
	if ( classad->LookupString(ATTR_JOB_ERROR, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {
		strcat( rsl, ")(stderr=" );
		if ( !classad->LookupBool( ATTR_TRANSFER_ERROR, transfer ) || transfer ) {
			strcat( rsl, gassServerUrl );
			if ( buff[0] != '/' ) {
				strcat( rsl, iwd );
			}
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

	sprintf( buff, ")(save_state=yes)(two_phase=%d)", JM_COMMIT_TIMEOUT );
	strcat( rsl, buff );

	if ( classad->LookupString(ATTR_GLOBUS_RSL, buff) && *buff ) {
		strcat( rsl, buff );
	}

	if (iwd) {
		free(iwd);
	}

	return strdup( rsl );
}


