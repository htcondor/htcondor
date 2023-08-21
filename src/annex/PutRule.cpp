#include "condor_common.h"
#include "compat_classad.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "PutRule.h"

int
PutRule::operator() () {
	static bool incrementTryCount = true;
	dprintf( D_FULLDEBUG, "PutRule::operator()\n" );

	//
	// The default account limit for rules is 50.  We would like to be
	// clever and use all five targets per rule, but that would require
	// checking to see if the rule has a target left, and that gets into
	// some nasty race conditions, since the logic for picking a new
	// rule if PutTargets() failed couldn't be here, where the name was
	// initially chosen.  Therefore, just use the bulk request ID for
	// which we're creating this lease to uniquify the name.
	//
	// Since the bulk request ID is unique and invariant (for each command,
	// once established), and PutRule() is idempotent, we don't need to
	// store any state for recovery; we can just do what we did last time
	// and get the same result (and we don't currently even use the rule ARN).
	//

	std::string ruleName = annexID;


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
	std::string ruleARN;
	rc = gahp->put_rule(
				service_url, public_key_file, secret_key_file,
				ruleName, "rate(5 minutes)", "ENABLED", ruleARN,
				errorCode );
	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	} else {
		incrementTryCount = true;
	}

	if( rc == 0 ) {
		reply->Assign( "RuleARN", ruleARN );
		reply->Assign( "RuleName", ruleName );

		// We could omit ruleARN and ruleName from the reply, but subsequent
		// functors in this sequence may need to know them.
		scratchpad->Assign( "RuleARN", ruleARN );
		scratchpad->Assign( "RuleName", ruleName );

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

// We assume here that delete_rule() succeeds if what it's trying to delete
// is already gone (or never existed in the first place).  This means it's
// OK to repeat a delete during rollback, and we don't have to keep track
// of if we've already deleted it -- in the same way that operator() is
// idempotent, so we'll repeat it if we retry in the forward direction.

int
PutRule::rollback() {
	dprintf( D_FULLDEBUG, "PutRule::rollback()\n" );

	std::string ruleName = annexID;

	int rc;
	std::string errorCode;
	rc = gahp->delete_rule(
				service_url, public_key_file, secret_key_file,
				ruleName, errorCode );
	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	}

	if( rc != 0 ) {
		// We're already rolling back, so the only thing we could do is retry.
        dprintf( D_ALWAYS, "Failed to remove rule '%s' ('%s').\n", ruleName.c_str(), errorCode.c_str() );
	}

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
