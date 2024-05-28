#include "condor_common.h"
#include "condor_config.h"
#include "compat_classad.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "OnDemandRequest.h"
#include "generate-id.h"

OnDemandRequest::OnDemandRequest( ClassAd * r, EC2GahpClient * egc, ClassAd * s,
	const std::string & su, const std::string & pkf, const std::string & skf,
	ClassAdCollection * c, const std::string & cid,
	const std::string & aid,
	const std::vector< std::pair< std::string, std::string > > & t ) :
  gahp( egc ), reply( r ), scratchpad( s ),
  service_url( su ), public_key_file( pkf ), secret_key_file( skf ),
  targetCapacity(0),
  commandID( cid ), commandState( c ), annexID( aid ), tags( t ) {
  	ClassAd * commandState;
	if( c->Lookup( commandID, commandState ) ) {
		commandState->LookupString( "State_ClientToken", clientToken );
		commandState->LookupString( "State_BulkRequestID", bulkRequestID );

		std::string iidString;
		commandState->LookupString( "State_InstanceIDs", iidString );
		instanceIDs = split(iidString, ",");
	}

	// Generate a client token if we didn't get one from the log.
	if( clientToken.empty() ) {
		generateClientToken( annexID, clientToken );
		if( reply != NULL) { reply->Assign( "ClientToken", clientToken ); }
	}
}

bool
OnDemandRequest::validateAndStore( ClassAd const * command, std::string & validationError ) {
	if(! command->LookupInteger( "TargetCapacity", targetCapacity )) {
		validationError = "Attribute 'TargetCapacity' missing or not an integer.";
		return false;
	}

	command->LookupString( "InstanceType", instanceType );
	if( instanceType.empty() ) {
		validationError = "Attribute 'InstanceType' missing or not a string.";
		return false;
	}

	command->LookupString( "ImageID", imageID );
	if( imageID.empty() ) {
		validationError = "Attribute 'ImageID' missing or not a string.";
		return false;
	}

	command->LookupString( "InstanceProfileARN", instanceProfileARN );
	if( instanceProfileARN.empty() ) {
		validationError = "Attribute 'InstanceProfileARN' missing or not a string.";
		return false;
	}

	// It's OK for these two to be empty; they'll get the account's defaults.
	command->LookupString( "KeyName", keyName );
	command->LookupString( "SecurityGroupIDs", securityGroupIDs );

	return true;
}

void
OnDemandRequest::log() {
	if( commandState == NULL ) {
		dprintf( D_FULLDEBUG, "log() called without a log.\n" );
		return;
	}

	if( commandID.empty() ) {
		dprintf( D_FULLDEBUG, "log() called without a command ID.\n" );
		return;
	}

	commandState->BeginTransaction();
	{
		if(! clientToken.empty()) {
			std::string quoted; formatstr( quoted, "\"%s\"", clientToken.c_str() );
			commandState->SetAttribute( commandID,
				"State_ClientToken", quoted.c_str() );
		} else {
			commandState->DeleteAttribute( commandID,
				"State_ClientToken" );
		}

		if(! bulkRequestID.empty()) {
			std::string quoted; formatstr( quoted, "\"%s\"", bulkRequestID.c_str() );
			commandState->SetAttribute( commandID,
				"State_BulkRequestID", quoted.c_str() );
		} else {
			commandState->DeleteAttribute( commandID,
				"State_BulkRequestID" );
		}

		if( instanceIDs.size() != 0 ) {
			std::string sl = "\"";
			sl += join(instanceIDs, ",");
			sl += '"';
			commandState->SetAttribute( commandID,
				"State_InstanceIDs", sl.c_str() );
		} else {
			commandState->DeleteAttribute( commandID,
				"State_InstanceIDs" );
		}
	}
	commandState->CommitTransaction();
}

int
OnDemandRequest::operator() () {
	static bool incrementTryCount = true;
	dprintf( D_FULLDEBUG, "OnDemandRequest::operator()\n" );

	// The idea here, of course, is that we can behave just like the
	// BulkRequest, except that we set the BulkRequestID to the client
	// token (prefix) we just made up instead of getting it back from AWS
	// (when we would otherwise get the BulkRequestID).

	int rc;
	int tryCount = 0;
	std::string errorCode;

	// If we already know the BulkRequestID, we don't need to do anything.
	if(! bulkRequestID.empty()) {
		dprintf( D_FULLDEBUG, "BulkRequest: found existing bulk request id (%s), not making another requst.\n", bulkRequestID.c_str() );
		rc = 0;
	} else {
		// Otherwise, continue as normal.  If the client token happens to be
		// from a previous request, the idempotency of spot fleet requests
		// means it both safe to repeat the request and that we'll get back
		// the information we want (the spot fleet request ID).

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

		// We have to call ec2_vm_start() at least twice (once to issue the
		// command, and at least once to get the result), so we should
		// probably do something clever here and only log once.
		this->log();

		std::string user_data, user_data_file;
		std::string availability_zone, vpc_subnet, vpc_id;
		std::string block_device_mapping, iam_profile_name;
		std::vector<std::string> group_names;
		std::vector<std::string> parameters_and_values;
		std::vector<std::string> group_ids = split(securityGroupIDs);

		// Specify the htcondor:AnnexName = ${annexName} tag.
		parameters_and_values.emplace_back( "TagSpecification.1.ResourceType" );
		parameters_and_values.emplace_back( "instance" );
		parameters_and_values.emplace_back( "TagSpecification.1.Tag.1.Key" );
		parameters_and_values.emplace_back( "htcondor:AnnexName" );
		parameters_and_values.emplace_back( "TagSpecification.1.Tag.1.Value" );
		parameters_and_values.emplace_back( annexID.c_str() );

        std::string buffer;
        for( unsigned i = 0; i < tags.size(); ++i ) {
            formatstr( buffer, "TagSpecification.1.Tag.%u.Key", i + 2 );
            parameters_and_values.emplace_back( buffer );
            parameters_and_values.emplace_back( tags[i].first );
            formatstr( buffer, "TagSpecification.1.Tag.%u.Value", i + 2 );
            parameters_and_values.emplace_back( buffer );
            parameters_and_values.emplace_back( tags[i].second );
        }

		// Earlier versions of the API don't know about TagSpecification.
		parameters_and_values.emplace_back( "Version" );
		parameters_and_values.emplace_back( "2016-11-15" );

		rc = gahp->ec2_vm_start( service_url, public_key_file, secret_key_file,
					imageID, keyName, user_data, user_data_file,
					instanceType, availability_zone, vpc_subnet, vpc_id,
					clientToken,
					block_device_mapping, instanceProfileARN, iam_profile_name,
					targetCapacity,
					group_names, group_ids, parameters_and_values,
					this->instanceIDs, errorCode );
		if( rc == 0 && this->instanceIDs.size() != 0 ) {
			bulkRequestID = clientToken;
		}

		if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
			// We should exit here the first time.
			return KEEP_STREAM;
		} else {
			incrementTryCount = true;
		}
	}

	if( rc == 0 ) {
		dprintf( D_ALWAYS, "ODI ID: %s\n", bulkRequestID.c_str() );
		reply->Assign( "BulkRequestID", bulkRequestID );

		// We may decide to omit the bulk request ID from the reply, but
		// subsequent functors in this sequence may need to know the bulk
		// request ID.
		scratchpad->Assign( "BulkRequestID", bulkRequestID );
		this->log();

		reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
		commandState->BeginTransaction();
		{
			commandState->DeleteAttribute( commandID, "State_TryCount" );
		}
		commandState->CommitTransaction();
		rc = PASS_STREAM;
	} else {
		std::string message;
		formatstr( message, "Bulk (ODI) start request failed: '%s' (%d): '%s'.",
			errorCode.c_str(), rc, gahp->getErrorString() );
		dprintf( D_ALWAYS, "%s\n", message.c_str() );

		// The previous argument for retries doesn't make any sense anymore,
		// what with the client tokens and all, but maybe it'll come in handy
		// later?
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
OnDemandRequest::rollback() {
	dprintf( D_FULLDEBUG, "OnDemandRequest::rollback()\n" );

	if( instanceIDs.size() != 0 ) {
		int rc;
		std::string errorCode;
		// Assumes we have fewer than 1000 instances.
		rc = gahp->ec2_vm_stop(
					service_url, public_key_file, secret_key_file,
					instanceIDs,
					errorCode );
		if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
			// We should exit here the first time.
			return KEEP_STREAM;
		}

		if( rc != 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel on-demand instances with client token '%s' ('%s').\n", bulkRequestID.c_str(), errorCode.c_str() );
		}
	}

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
