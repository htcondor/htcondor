#include "condor_common.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "GetSFRs.h"

int
GetSFRs::operator() () {
	dprintf( D_FULLDEBUG, "GetSFRs::operator()\n" );

	std::string annexID;
	scratchpad->LookupString( "AnnexID", annexID );

	std::string errorCode;
	StringList returnStatus;
	int rc = gahp->bulk_query(
		service_url, public_key_file, secret_key_file,
		returnStatus, errorCode
	);
	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	}

	if( rc == 0 ) {
		unsigned count = 0;
		std::string aName;

		int i = 0;
		returnStatus.rewind();
		ASSERT( returnStatus.number() % 3 == 0 );
		for( ; i < returnStatus.number(); i += 3 ) {
			std::string spotFleetRequestID = returnStatus.next();
			std::string createTime = returnStatus.next();
			std::string clientToken = returnStatus.next();

			// We could probably be a little more cautious about this match,
			// but this has the nice property that we can use the same code
			// for looking up both bulk request IDs and annex names.  (AWS
			// can't filter on regexes or prefixes.)
			if( clientToken.find( annexID ) != 0 ) { continue; }

			formatstr( aName, "SpotFleetRequest%d", count++ );
			scratchpad->Assign( (aName + ".ID").c_str(), spotFleetRequestID );
			scratchpad->Assign( (aName + ".createTime").c_str(), createTime );
			scratchpad->Assign( (aName + ".clientToken").c_str(), clientToken );
		}
		scratchpad->Assign( "SpotFleetRequestCount", count );

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
GetSFRs::rollback() {
	dprintf( D_FULLDEBUG, "GetSFRs::rollback()\n" );

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
