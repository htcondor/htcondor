
#include "condor_attributes.h"

#include "globus_gram_client.h"
#include "globus_gram_error.h"

#include "globusjob.h"


GlobusJob::GlobusJob( GlobusJob& copy )
{
	procID = copy.procID;
	jobContact = (copy.jobContact == NULL) ? NULL : strdup( copy.jobContact );
	jobState = copy.jobState;
	RSL = (copy.RSL == NULL) ? NULL : strdup( copy.RSL );
	rmContact = (copy.rmContact == NULL) ? NULL : strdup( copy.rmContact );
	errorCode = copy.errorCode;
	userLogFile = (copy.userLogFile == NULL) ? NULL : strdup( copy.userLogFile );
	updateSchedd = copy.updateSchedd;
}

GlobusJob::GlobusJob( ClassAd *classad )
{
	char buf[4096];

	classad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
	classad->LookupInteger( ATTR_PROC_ID, procID.proc );
	buf[0] = '\0';
	classad->LookupString( ATTR_GLOBUS_CONTACT_STRING, buf );
	if ( buf[0] != '\0' )
		jobContact = strdup( buf );
	classad->LookupInteger( ATTR_GLOBUS_STATUS, jobState );
	buf[0] = '\0';
	classad->LookupString( ATTR_GLOBUS_RSL, buf );
	if ( buf[0] != '\0' )
		RSL = strdup( buf );
	buf[0] = '\0';
	classad->LookupString( ATTR_GLOBUS_RESOURCE, buf );
	if ( buf[0] != '\0' )
		rmContact = strdup( buf );
	errorCode = GLOBUS_SUCCESS;
	buf[0] = '\0';
	classad->LookupString( ATTR_ULOG_FILE, buf );
	if ( buf[0] != '\0' )
		userLogFile = strdup( buf );
	updateSchedd = false;
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

bool GlobusJob::submit( const char *callback_contact )
{
	int rc;
	char *job_contact = NULL;

    rc = globus_gram_client_job_request( rmContact, RSL,
        GLOBUS_GRAM_CLIENT_JOB_STATE_ALL, callback_contact,
        &job_contact );

    if ( rc == GLOBUS_SUCCESS ) {
		jobContact = strdup( job_contact );
		globus_gram_client_job_contact_free( job_contact );
		jobState = G_PENDING;

		return true;
	} else {
		errorCode = rc;
		return false;
	}
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

