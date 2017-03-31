#include "condor_common.h"
#include "condor_config.h"
#include "compat_classad.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "CheckConnectivity.h"

int
CheckConnectivity::operator() () {
	dprintf( D_FULLDEBUG, "CheckConnectivity::operator()\n" );

	std::string argumentBlob;
	std::string collectorHost;
	param( collectorHost, "COLLECTOR_HOST" );
	ASSERT(! collectorHost.empty());

	std::string host = collectorHost;
	std::string portNo = "9618";
	size_t c = collectorHost.rfind( ":" );
	if( c != std::string::npos ) {
		host = collectorHost.substr( 0, c );
		portNo = collectorHost.substr( c + 1 );
	}
	formatstr( argumentBlob, "{ "
		"\"timeout\": 5, "
		"\"condor_port\": %s, "
		"\"condor_host\": \"%s\" "
		"}",
		portNo.c_str(), host.c_str() );

	int rc;
	std::string returnBlob;
	std::string errorCode;
	rc = gahp->call_function(
				service_url, public_key_file, secret_key_file,
				functionARN, argumentBlob, returnBlob, errorCode );
	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	}

	if( rc == 0 ) {
		ClassAd result;
		classad::ClassAdJsonParser cajp;
		if(! cajp.ParseClassAd( returnBlob, result, true )) {
			reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
			reply->Assign( ATTR_ERROR_STRING, "Connectivity check result was nonsense." );
			rc = FALSE;
		} else {
			std::string success;
			result.LookupString( "success", success );
			if( success != "1" ) {
				std::string message;
				std::string description = "unknown problem";
				result.LookupString( "description", description );
				formatstr( message, "Connectivity check failed (%s); not starting annex.", description.c_str() );

				reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
				reply->Assign( ATTR_ERROR_STRING, message );
				rc = FALSE;
			} else {
				std::string resultInstanceID;
				result.LookupString( "result", resultInstanceID );
				if( resultInstanceID != instanceID ) {
					std::string message;
					formatstr( message, "Connectivity check found wrong collector (%s vs %s).", resultInstanceID.c_str(), instanceID.c_str() );

					reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
					reply->Assign( ATTR_ERROR_STRING, message );
					rc = FALSE;
				} else {
					reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
					rc = PASS_STREAM;
				}
			}
		}
	} else {
		std::string message;
		formatstr( message, "Failed to check connectivity: '%s' (%d): '%s'.",
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
CheckConnectivity::rollback() {
	dprintf( D_FULLDEBUG, "CheckConnectivity::rollback()\n" );

	// CheckConnectivity performs no actions (it just checks a pre-condition) and
	// as such has no rollbak function to perform.
	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}

