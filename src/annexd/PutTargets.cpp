#include "condor_common.h"
#include "condor_config.h"
#include "compat_classad.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "PutTargets.h"

int
PutTargets::operator() () {
	dprintf( D_ALWAYS, "PutTargets()\n" );

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

	std::string spotFleetRequestID;
	scratchpad->LookupString( "BulkRequestID", spotFleetRequestID );
	ASSERT(! spotFleetRequestID.empty());
	std::string targetID = spotFleetRequestID;

	std::string functionARN;
	param( functionARN, "ANNEX_DEFAULT_LEASE_FUNCTION_ARN" );
	ASSERT(! functionARN.empty());

	//
	// Construct the input JSON string.
	//
	ClassAd input;
	input.Assign( "SpotFleetRequestID", spotFleetRequestID );
	input.Assign( "RuleID", ruleName );
	input.Assign( "TargetID", targetID );
	input.Assign( "LeaseExpiration", this->leaseExpiration );

	std::string inputString;
	classad::ClassAdJsonUnParser cajup;
	cajup.Unparse( inputString, & input );


	int rc;
	std::string errorCode;
	rc = gahp->put_targets(
				service_url, public_key_file, secret_key_file,
				ruleName, targetID, functionARN, inputString, errorCode );
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
