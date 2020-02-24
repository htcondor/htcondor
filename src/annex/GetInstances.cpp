#include "condor_common.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "GetInstances.h"

const char * emptyStringIfNull( const char * str ) {
	if( strcmp( str, "NULL" ) == 0 ) { return ""; } else { return str; }
}

int
GetInstances::operator() () {
	dprintf( D_FULLDEBUG, "GetInstances::operator()\n" );

	std::string annexID;
	scratchpad->LookupString( "AnnexID", annexID );

	std::string errorCode;
	StringList returnStatus;
	int rc = gahp->ec2_vm_status_all(
		service_url, public_key_file, secret_key_file,
		"tag-key", "htcondor:AnnexName",
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
		ASSERT( returnStatus.number() % 8 == 0 );
		for( int i = 0; i < returnStatus.number(); i += 8 ) {
			std::string instanceID = returnStatus.next();
			std::string status = returnStatus.next();
			std::string clientToken = returnStatus.next();
			std::string keyName = returnStatus.next();
			std::string stateReasonCode = returnStatus.next();
			std::string publicDNSName = returnStatus.next();
			std::string spotFleetRequestID = emptyStringIfNull( returnStatus.next() );
			std::string annexName = emptyStringIfNull( returnStatus.next() );

			// If it doesn't have an annex name, it isn't an annex.
			if( annexName.empty()) {
				continue;
			}

			// If we're looking for a specific annex's instances, filter.
			if(! annexID.empty()) {
				if( strcasecmp( annexName.c_str(), annexID.c_str() ) != 0 ) {
					continue;
				}
			}

			formatstr( iName, "Instance%d", count++ );
			scratchpad->Assign( (iName + ".instanceID"), instanceID );
			scratchpad->Assign( (iName + ".status"), status );
			scratchpad->Assign( (iName + ".clientToken"), clientToken );
			scratchpad->Assign( (iName + ".keyName"), keyName );
			scratchpad->Assign( (iName + ".stateReasonCode"), stateReasonCode );
			scratchpad->Assign( (iName + ".publicDNSName"), publicDNSName );
			scratchpad->Assign( (iName + ".annexName"), annexName );
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
