
#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_debug.h"

#include "globus_gram_client.h"
#include "globus_gram_error.h"

#include "gm_common.h"
#include "globusjob.h"


GlobusJob::GlobusJob( GlobusJob& copy )
{
	callback_already_registered = copy.callback_already_registered;
	procID = copy.procID;
	jobContact = (copy.jobContact == NULL) ? NULL : strdup( copy.jobContact );
	jobState = copy.jobState;
	RSL = (copy.RSL == NULL) ? NULL : strdup( copy.RSL );
	rmContact = (copy.rmContact == NULL) ? NULL : strdup( copy.rmContact );
	errorCode = copy.errorCode;
	userLogFile = (copy.userLogFile == NULL) ? NULL : strdup( copy.userLogFile );
}

GlobusJob::GlobusJob( ClassAd *classad )
{
	char buf[4096];

	callback_already_registered = false;
	classad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
	classad->LookupInteger( ATTR_PROC_ID, procID.proc );
	buf[0] = '\0';
	classad->LookupString( ATTR_GLOBUS_CONTACT_STRING, buf );
	if ( buf[0] != '\0' )
		jobContact = strdup( buf );
	classad->LookupInteger( ATTR_GLOBUS_STATUS, jobState );
	buf[0] = '\0';
	classad->LookupString( ATTR_GLOBUS_RESOURCE, buf );
	if ( buf[0] != '\0' )
		rmContact = strdup( buf );
	errorCode = GLOBUS_SUCCESS;
	buf[0] = '\0';
	classad->LookupString( ATTR_ULOG_FILE, buf );
	if ( buf[0] != '\0' )
		userLogFile = strdup( buf );
	RSL = buildRSL( classad );
}

GlobusJob::~GlobusJob()
{
	if ( jobContact )
		free( jobContact );
	if ( RSL )
		free( RSL );
	if ( rmContact )
		free( rmContact );
	if ( userLogFile )
		free( userLogFile );
}

bool GlobusJob::start()
{
	int rc;
	char *job_contact = NULL;

    rc = globus_gram_client_job_request( rmContact, RSL,
        GLOBUS_GRAM_CLIENT_JOB_STATE_ALL, gramCallbackContact,
        &job_contact );

    if ( rc == GLOBUS_SUCCESS ) {
		jobContact = strdup( job_contact );
		globus_gram_client_job_contact_free( job_contact );
		jobState = G_PENDING;

		addJobUpdateEvent( this, JOB_UE_UPDATE_STATE );
		addJobUpdateEvent( this, JOB_UE_UPDATE_CONTACT );

		callback_already_registered = true;

		return true;
	} else {
		errorCode = rc;
		return false;
	}
}

bool GlobusJob::callback_register()
{
	int status;
	int failure_code;
	int rc;

	if ( callback_already_registered ) {
		// we're already done, probably because we called start()
		return true;
	}

    rc = globus_gram_client_job_callback_register( jobContact, 
        GLOBUS_GRAM_CLIENT_JOB_STATE_ALL, gramCallbackContact,
        &status, &failure_code );

    if ( rc == GLOBUS_SUCCESS ) {
		callback_already_registered = true;
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

			addJobUpdateEvent( this, JOB_UE_UPDATE_STATE );
		}
	} else if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_ACTIVE ) {
		if ( jobState != G_ACTIVE ) {
			jobState = G_ACTIVE;

			addJobUpdateEvent( this, JOB_UE_UPDATE_STATE );
			addJobUpdateEvent( this, JOB_UE_ULOG_EXECUTE );
		}
	} else if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_FAILED ) {
		// Not sure what the right thing to do on a FAILED message from
		// globus. For now, we treat it like a completed job.

		// JobsByContact with a state of FAILED shouldn't be in the hash table,
		// but we'll check anyway.
		if ( jobState != G_FAILED ) {
			jobState = G_FAILED;

			addJobUpdateEvent( this, JOB_UE_UPDATE_STATE );
			addJobUpdateEvent( this, JOB_UE_REMOVE_JOB );
			// put on hold instead? (for certain errors)

			// userlog entry (what type of event is this?)

			// email saying that the job failed?

			free( jobContact );
			jobContact = NULL;
		}
	} else if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_DONE ) {
		// JobsByContact with a state of DONE shouldn't be in the hash table,
		// but we'll check anyway.
		if ( jobState != G_DONE ) {
			jobState = G_DONE;

			addJobUpdateEvent( this, JOB_UE_UPDATE_STATE );
			addJobUpdateEvent( this, JOB_UE_REMOVE_JOB );
			addJobUpdateEvent( this, JOB_UE_ULOG_TERMINATE );

			// email saying the job is done?

			free( jobContact );
			jobContact = NULL;
		}
	} else if ( state == GLOBUS_GRAM_CLIENT_JOB_STATE_SUSPENDED ) {
		if ( jobState != G_SUSPENDED ) {
			jobState = G_SUSPENDED;

			addJobUpdateEvent( this, JOB_UE_UPDATE_STATE );
		}
	} else {
		dprintf( D_ALWAYS, "Unknown globus job state %d for job %d.%d\n",
				 state, procID.cluster, procID.proc );
	}

	return true;
}

bool GlobusJob::cancel()
{
	int rc;

	rc = globus_gram_client_job_cancel( jobContact );

	if ( rc == GLOBUS_SUCCESS ) {
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
	char rsl[8092];
	char buff[2048];
	char *iwd;

	if ( classad->LookupString(ATTR_JOB_IWD, buff) && *buff ) {
		strcat( buff, "/" );
		iwd = strdup( buff );
	} else {
		iwd = strdup( "/" );
	}

	//We're assuming all job clasads have a command attribute
	classad->LookupString( ATTR_JOB_CMD, buff );
	strcpy( rsl, "&(executable=" );
	strcat( rsl, gassServerUrl );
	if ( buff[0] != '/' )
		strcat( rsl, iwd );
	strcat( rsl, buff );

	if ( classad->LookupString(ATTR_JOB_ARGUMENTS, buff) && *buff ) {
		strcat( rsl, ")(arguments=" );
		strcat( rsl, buff );
	}

	if ( classad->LookupString(ATTR_JOB_INPUT, buff) && *buff ) {
		strcat( rsl, ")(stdin=" );
		strcat( rsl, gassServerUrl );
		if ( buff[0] != '/' )
			strcat( rsl, iwd );
		strcat( rsl, buff );
	}

	if ( classad->LookupString(ATTR_JOB_OUTPUT, buff) && *buff ) {
		strcat( rsl, ")(stdout=" );
		strcat( rsl, gassServerUrl );
		if ( buff[0] != '/' )
			strcat( rsl, iwd );
		strcat( rsl, buff );
	}

	if ( classad->LookupString(ATTR_JOB_ERROR, buff) && *buff ) {
		strcat( rsl, ")(stderr=" );
		strcat( rsl, gassServerUrl );
		if ( buff[0] != '/' )
			strcat( rsl, iwd );
		strcat( rsl, buff );
	}

	strcat( rsl, ")" );

	if ( classad->LookupString(ATTR_GLOBUS_RSL, buff) && *buff ) {
		strcat( rsl, buff );
	}

	return strdup( rsl );
}


