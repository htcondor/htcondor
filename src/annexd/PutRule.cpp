#include "condor_common.h"
#include "condor_daemon_core.h"
#include "compat_classad.h"
#include "gahp-client.h"
#include "Functor.h"
#include "PutRule.h"

int
PutRule::operator() () {
	dprintf( D_ALWAYS, "PutRule()\n" );

	// The default account limit for rules is 50.  We should try to be
	// clever and use all five targets per rule, but that would require
	// checking to see if the rule has a target left, and that gets into
	// some nasty race conditions, since the logic for picking a new
	// rule if PutTargets() failed couldn't be here, where the name was
	// initially chosen.  Therefore, just use the bulk request ID for
	// which we're creating this lease to uniquify the name.
	std::string ruleName;
	scratchpad->LookupString( "BulkRequestID", ruleName );

	int rc;
	std::string errorCode;
	std::string ruleARN;
	rc = gahp->put_rule(
				service_url, public_key_file, secret_key_file,
				ruleName, "rate(5 minutes)", "ENABLED", ruleARN,
				errorCode );
	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	}

	if( rc == 0 ) {
		reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
		reply->Assign( "RuleARN", ruleARN );
		reply->Assign( "RuleName", ruleName );

		// We could omit ruleARN and ruleName from the reply, but subsequent
		// functors in this sequence may need to know them.
		scratchpad->Assign( "RuleARN", ruleARN );
		scratchpad->Assign( "RuleName", ruleName );

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
