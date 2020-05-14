#include "condor_common.h"
#include "condor_config.h"
#include "gahp-client.h"
#include "Functor.h"
#include "gce-instance-insert.h"

GCEInstanceInsert::GCEInstanceInsert( GahpClient * gc,
    ClassAd * r, ClassAd * s,
    const std::array< std::string, 11 > & theMany,
    const std::vector< std::pair< std::string, std::string > > & l ) :
  reply(r), scratchpad(s), gahpClient(gc), labels(l) {
	std::tie( serviceURL, authFile, account, project, zone,
		instanceName, machineType, imageName,
		metadata, metadataFile, jsonFile ) = std::tuple_cat(theMany);
}

int
GCEInstanceInsert::operator() () {
	std::string instanceID;
	int rc = gahpClient->gce_instance_insert(
		serviceURL, authFile, account, project, zone,
		instanceName, machineType, imageName,
		metadata, metadataFile, false /* not interruptible */,
		jsonFile, labels, instanceID );

	int rv = TRUE;
	switch(rc) {
		case 0:
			// fprintf( stdout, "Instance %s started with ID %s\n", instanceName.c_str(), instanceID.c_str() );
			reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );

			// This is a phase 1 abomination.
			{ std::string bulkRequestID;
			reply->LookupString( "BulkRequestID", bulkRequestID );
			if( bulkRequestID.empty() ) {
				bulkRequestID = instanceID;
			} else {
				bulkRequestID += ", " + instanceID;
			}
			reply->Assign( "BulkRequestID", bulkRequestID ); }

			rv = PASS_STREAM;
			break;

		case 1:
			reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
			reply->Assign( ATTR_ERROR_STRING, gahpClient->getErrorString() );
			rv = FALSE;
			break;

		case GAHPCLIENT_COMMAND_NOT_SUBMITTED:
		case GAHPCLIENT_COMMAND_PENDING:
			return KEEP_STREAM;

		case GAHPCLIENT_COMMAND_NOT_SUPPORTED:
			// We don't EXCEPT() here in case there were functors in the
			// sequence before us that deserve a chance to clean up.
            reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
            reply->Assign( ATTR_ERROR_STRING, "Command not supported." );
			rv = FALSE;
			break;

		case GAHPCLIENT_COMMAND_TIMED_OUT:
            reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
            reply->Assign( ATTR_ERROR_STRING, gahpClient->getErrorString() );
			rv = FALSE;
			break;

		default:
			fprintf( stderr, "Unknown result (%d), aborting.\n", rc );
			rv = FALSE;
			break;
	}

    daemonCore->Reset_Timer( gahpClient->getNotificationTimerId(), 0, TIMER_NEVER );
    return rv;
}

int
GCEInstanceInsert::rollback() {
    dprintf( D_FULLDEBUG, "GCEInstanceInsert::rollback() called.\n" );

	// FIXME: call gce_instance_remove() appropriately.

	daemonCore->Reset_Timer( gahpClient->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
