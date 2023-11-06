#include "condor_common.h"
#include "compat_classad.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "PutTargets.h"

int
PutTargets::operator() () {
	static bool incrementTryCount = true;
	dprintf( D_FULLDEBUG, "PutTargets::operator()\n" );

	std::string ruleName;
	scratchpad->LookupString( "ruleName", ruleName );
	ASSERT(! ruleName.empty());

	//
	// To make sure the rule ID is unique, make it the ID of the spot fleet
	// request that the lease will be deleting.
	//
	// Since the bulk request ID is unique and invariant (for each command,
	// once established), and PutTargets() is idempotent, we don't need to
	// store any state for recovery; we can just do what we did last time
	// and get the same result (which we ignore anyway).
	//

	//
	// Construct the input JSON string.
	//
	ClassAd input;
	input.Assign( "AnnexID", annexID );

	ClassAd * commandAd = nullptr;
	commandState->Lookup( commandID, commandAd );
	std::string uploadTo;
	commandAd->LookupString( "UploadTo", uploadTo );
	size_t separator = uploadTo.find( '/' );
	std::string bucketName = uploadTo.substr( 0, separator );
	input.Assign( "S3BucketName", bucketName );

	input.Assign( "LeaseExpiration", this->leaseExpiration );

	std::string inputString;
	classad::ClassAdJsonUnParser cajup;
	cajup.Unparse( inputString, & input );


	int tryCount = 0;
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
	rc = gahp->put_targets(
				service_url, public_key_file, secret_key_file,
				ruleName, annexID, target, inputString, errorCode );
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
	} else {
		std::string message;
		formatstr( message, "Lease construction failed: '%s' (%d): '%s'.",
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

// We assume here that remove_targets() succeeds if what it's trying to delete
// is already gone (or never existed in the first place).  This means it's
// OK to repeat a delete during rollback, and we don't have to keep track
// of if we've already deleted it -- in the same way that operator() is
// idempotent, so we'll repeat it if we retry in the forward direction.

int
PutTargets::rollback() {
	dprintf( D_FULLDEBUG, "PutTargets::rollback()\n" );

	std::string ruleName;
	scratchpad->LookupString( "ruleName", ruleName );
	ASSERT(! ruleName.empty());

	int rc;
	std::string errorCode;
	rc = gahp->remove_targets(
				service_url, public_key_file, secret_key_file,
				ruleName, annexID, errorCode );
	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	}

	if( rc != 0 ) {
		// We're already rolling back, so the only thing we could do is retry.
		dprintf( D_ALWAYS, "Failed to remove target '%s' from rule '%s' ('%s'); this will probably cause rule removal to fail.\n", annexID.c_str(), ruleName.c_str(), errorCode.c_str() );
	}

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}

