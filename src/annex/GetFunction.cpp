#include "condor_common.h"
#include "condor_config.h"
#include "compat_classad.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "GetFunction.h"

int
GetFunction::operator() () {
	static bool incrementTryCount = true;
	dprintf( D_FULLDEBUG, "GetFunction::operator()\n" );

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
	std::string hash;
	std::string errorCode;
	rc = gahp->get_function(
				service_url, public_key_file, secret_key_file,
				functionARN, hash, errorCode );
	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	} else {
		incrementTryCount = true;
	}

	if( rc == 0 ) {
		if(! hash.empty()) {
			reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
			commandState->BeginTransaction();
			{
				commandState->DeleteAttribute( commandID, "State_TryCount" );
			}
			commandState->CommitTransaction();
			rc = PASS_STREAM;
		} else {
			std::string message;
			formatstr( message, "Function ARN '%s' not found.", functionARN.c_str() );

			reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
			reply->Assign( ATTR_ERROR_STRING, message );
			rc = FALSE;
		}
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

int
GetFunction::rollback() {
	dprintf( D_FULLDEBUG, "GetFunction::rollback()\n" );

	// GetFunction performs no actions (it just checks a pre-condition) and
	// as such has no rollbak function to perform.
	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}

