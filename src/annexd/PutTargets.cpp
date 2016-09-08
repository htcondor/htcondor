#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "compat_classad.h"
#include "gahp-client.h"
#include "Functor.h"
#include "PutTargets.h"

int
PutTargets::operator() () {
	dprintf( D_ALWAYS, "PutTargets()\n" );

	std::string ruleName;
	scratchpad->LookupString( "ruleName", ruleName );
	ASSERT(! ruleName.empty());

	std::string id = "FIXME-targetID";

	std::string arn;
	param( arn, "ANNEX_DEFAULT_LEASE_FUNCTION_ARN" );
	ASSERT(! arn.empty());

	// Escaping escaped JSON.  Lovely.  See the FIXME in amazonCommands.cpp.
	std::string input = "{ \\\"FIXME\\\": \\\"FIXME-function-input-data\\\" }";

	int rc;
	std::string errorCode;
	rc = gahp->put_targets(
				service_url, public_key_file, secret_key_file,
				ruleName, id, arn, input, errorCode );
	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	}

	if( rc == 0 ) {
		reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
		rc = PASS_STREAM;
	} else {
		std::string message;
		formatstr( message, "Lease construction failed: '%s' (%d): '%s'.",
			errorCode.c_str(), rc, gahp->getErrorString() );
		dprintf( D_ALWAYS, "%s\n", message.c_str() );

		reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
		reply->Assign( ATTR_ERROR_STRING, message );
		rc = FALSE;
	}

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	delete this;
	return rc;
}
