#include "condor_common.h"
#include "compat_classad.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "UploadFile.h"

int
UploadFile::operator() () {
	static bool incrementTryCount = true;
	dprintf( D_FULLDEBUG, "UploadFile::operator()\n" );

	std::string bucketName, fileName;
	size_t separator = uploadTo.find( '/' );
	bucketName = uploadTo.substr( 0, separator );
	fileName = uploadTo.substr( separator + 1 );
	if( bucketName.empty() || fileName.empty() ) {
		std::string message;
		formatstr( message, "Invalid upload-to specifier '%s'; "
			"must have the form <bucketName>/<fileName>.\n",
			uploadFrom.c_str() );
		dprintf( D_ALWAYS, "%s\n", message.c_str() );

		reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
		reply->Assign( ATTR_ERROR_STRING, message );

		daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
		return FALSE;
	}

	int tryCount = 0;
	ClassAd * commandAd = nullptr;
	commandState->Lookup( commandID, commandAd );
	commandAd->LookupInteger( "State_TryCount", tryCount );
	if( incrementTryCount ) {
		++tryCount;

		std::string value;
		formatstr( value, "%d", tryCount );
		commandState->BeginTransaction();
		{
			commandState->SetAttribute( commandID, "State_TryCount", value.c_str() );
		}
		commandState->CommitTransaction();

		incrementTryCount = false;
	}

	int rc;
	std::string errorCode;
	rc = gahp->s3_upload(
				service_url, public_key_file, secret_key_file,
				bucketName, fileName, uploadFrom,
				errorCode );
	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	} else {
		incrementTryCount = true;
	}

	if( rc == 0 ) {
		reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
		commandState->BeginTransaction();
		{
			commandState->DeleteAttribute( commandID, "State_TryCount" );
		}
		commandState->CommitTransaction();
		rc = PASS_STREAM;

		// While the tool and daemon are stapled together, delete the
		// tarball we created, now that we've uploaded it.
		unlink( uploadFrom.c_str() );
	} else {
		std::string message;
		formatstr( message, "Upload failed: '%s' (%d): '%s'.",
			errorCode.c_str(), rc, gahp->getErrorString() );
		dprintf( D_ALWAYS, "%s\n", message.c_str() );

		if( tryCount < 3 ) {
			dprintf( D_ALWAYS, "Retrying, after %d attempt(s).\n", tryCount );
			rc = KEEP_STREAM;
		} else {
			reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
			reply->Assign( ATTR_ERROR_STRING, message );
			rc = FALSE;
		}
	}

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return rc;
}

int
UploadFile::rollback() {
	dprintf( D_FULLDEBUG, "UploadFile::rollback()\n" );

	// There's nothing to do on a failed upload; S3 defines uploads as either
	// failed or entirely succesful.  Once we've succeeded at uploading,
	// there's nothing to do, because there's no point in keeping a back-up
	// (if it was safe to replace the file in the first place, nothing but
	// this annex instance would have been using it anyway).

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}

