#include "condor_common.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "GetInstances.h"

void
assign( ClassAd * c, const std::string & a, const std::string & v ) {
	c->Assign( a.c_str(), v );
}

int
GetInstances::operator() () {
	dprintf( D_FULLDEBUG, "GetInstances::operator()\n" );

	std::string annexID;
	scratchpad->LookupString( "AnnexID", annexID );

	std::string errorCode;
	StringList returnStatus;
	// Efficiency would improve if this accepted a filter argument.
	int rc = gahp->ec2_vm_status_all(
		service_url, public_key_file, secret_key_file,
		returnStatus, errorCode
	);
	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	}

	if( rc == 0 ) {
		unsigned count = 0;
		std::string iName;

		returnStatus.rewind();
		ASSERT( returnStatus.number() % 6 == 0 );
		for( int i = 0; i < returnStatus.number(); i += 6 ) {
			std::string instanceID = returnStatus.next();
			std::string status = returnStatus.next();
			std::string clientToken = returnStatus.next();
			std::string keyName = returnStatus.next();
			std::string stateReasonCode = returnStatus.next();
			std::string publicDNSName = returnStatus.next();

			// We could probably be a little more cautious about this match,
			// but this has the nice property that we can use the same code
			// for looking up both bulk request IDs and annex names.  (AWS
			// can't filter on regexes or prefixes.)
			if( clientToken.find( annexID ) != 0 ) { continue; }

			formatstr( iName, "Instance%d", count++ );
			assign( scratchpad, iName + ".instanceID", instanceID );
			assign( scratchpad, iName + ".status", status );
			assign( scratchpad, iName + ".clientToken", clientToken );
			assign( scratchpad, iName + ".keyName", keyName );
			assign( scratchpad, iName + ".stateReasonCode", stateReasonCode );
			assign( scratchpad, iName + ".publicDNSName", publicDNSName );
		}

		reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
		rc = PASS_STREAM;
	} else {
		std::string message;
		formatstr( message, "Failed to get instances: '%s' (%d): '%s'.",
			errorCode.c_str(), rc, gahp->getErrorString() );

		dprintf( D_ALWAYS, "%s\n", message.c_str() );
		reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
		reply->Assign( ATTR_ERROR_STRING, message );
		rc = FALSE;
	}

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return rc;
}

int
GetInstances::rollback() {
	dprintf( D_FULLDEBUG, "GetInstances::rollback()\n" );

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
