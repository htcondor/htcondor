#include "condor_common.h"
#include "condor_config.h"
#include "compat_classad.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "BulkRequest.h"
#include "generate-id.h"

BulkRequest::BulkRequest( ClassAd * r, EC2GahpClient * egc, ClassAd * s,
	const std::string & su, const std::string & pkf, const std::string & skf,
	ClassAdCollection * c, const std::string & cid,
	const std::string & aid ) :
  gahp( egc ), reply( r ), scratchpad( s ), annexID( aid ),
  service_url( su ), public_key_file( pkf ), secret_key_file( skf ),
  commandID( cid ), commandState( c ) {
  	ClassAd * commandState;
	if( c->Lookup( commandID, commandState ) ) {
		commandState->LookupString( "State_ClientToken", client_token );
		commandState->LookupString( "State_BulkRequestID", bulkRequestID );
	}

	// Generate a client token if we didn't get one from the log.
	if( client_token.empty() ) {
		generateClientToken( annexID, client_token );
		if( reply != NULL) { reply->Assign( "ClientToken", client_token ); }
	}
}

bool
BulkRequest::validateAndStore( ClassAd const * command, std::string & validationError ) {
	command->LookupString( "SpotPrice", spot_price );
	if( spot_price.empty() ) {
		validationError = "Attribute 'SpotPrice' missing or not a string.";
		return false;
	}

	int targetCapacity = 0;
	if(! command->LookupInteger( "TargetCapacity", targetCapacity )) {
		validationError = "Attribute 'TargetCapacity' missing or not an integer.";
		return false;
	} else {
		formatstr( target_capacity, "%d", targetCapacity );
	}

	command->LookupString( "IamFleetRole", iam_fleet_role );
	if( iam_fleet_role.empty() ) {
		validationError = "Attribute 'IamFleetRole' missing or not a string.";
		return false;
	}

	command->LookupString( "AllocationStrategy", allocation_strategy );
	if( allocation_strategy.empty() ) { allocation_strategy = "lowestPrice"; }

	command->LookupString( "ValidUntil", valid_until );
	if( valid_until.empty() ) {
		time_t now = time( NULL );
		time_t interval = param_integer( "ANNEX_PROVISIONING_DELAY", 5 * 60 );
		time_t fiveMinutesFromNow = now + interval;
		struct tm fMFN;
		gmtime_r( & fiveMinutesFromNow, & fMFN );
		char buffer[ 4 + 1 + 2 + 1 + 2 + 1 /* T */ + 2 + 1 + 2 + 1 + 2 + 1 /* Z */ + 1];
		strftime( buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", & fMFN );
		valid_until = buffer;
	}

	ExprTree * launchConfigurationsTree = command->Lookup( "LaunchSpecifications" );
	if(! launchConfigurationsTree) {
		validationError = "Attribute 'LaunchSpecifications' missing.";
		return false;
	}
	classad::ExprList * launchConfigurations = dynamic_cast<classad::ExprList *>( launchConfigurationsTree );
	if(! launchConfigurations) {
		validationError = "Attribute 'LaunchSpecifications' not a list.";
		return false;
	}

	auto lcIterator = launchConfigurations->begin();
	for( ; lcIterator != launchConfigurations->end(); ++lcIterator ) {
		classad::ClassAd * ca = dynamic_cast<classad::ClassAd *>( * lcIterator );
		if( ca == NULL ) {
			validationError = "'LaunchSpecifications[x]' not a ClassAd.";
			return false;
		}
		ClassAd launchConfiguration( * ca );

		// Convert to the GAHP's single-level JSON.
		std::map< std::string, std::string > blob;
		launchConfiguration.LookupString( "ImageId", blob[ "ImageId" ] );
		launchConfiguration.LookupString( "SpotPrice", blob[ "SpotPrice" ] );
		launchConfiguration.LookupString( "KeyName", blob[ "KeyName" ] );
		// We assume that user data is converted to base 64 before it's
		// sent on the wire.
		launchConfiguration.LookupString( "UserData", blob[ "UserData" ] );
		launchConfiguration.LookupString( "InstanceType", blob[ "InstanceType" ] );
		launchConfiguration.LookupString( "SubnetId", blob[ "SubnetId" ] );
		launchConfiguration.LookupString( "WeightedCapacity", blob[ "WeightedCapacity" ] );


		std::string b;
		StringList taglist;
		formatstr( b, "%s=%s", "htcondor:AnnexName", annexID.c_str() );
		taglist.append( b.c_str() );

		std::string buffer;
		if( command->LookupString( ATTR_EC2_TAG_NAMES, buffer ) ) {
			StringList tagNames(buffer);

			char * tagName = NULL;
			tagNames.rewind();
			while( (tagName = tagNames.next()) ) {
				std::string tagAttr(ATTR_EC2_TAG_PREFIX);
				tagAttr.append(tagName);

				char * tagValue = NULL;
				if(! command->LookupString(tagAttr, &tagValue)) {
					return FALSE;
				}

				formatstr( b, "%s=%s", tagName, tagValue );
				taglist.append( b.c_str() );

				free( tagValue );
			}
		}

		char * s = taglist.print_to_string();
		blob[ "Tags" ] = s;
		free( s );


		ExprTree * iipTree = launchConfiguration.Lookup( "IamInstanceProfile" );
		if( iipTree != NULL ) {
			classad::ClassAd * ca = dynamic_cast<classad::ClassAd *>( iipTree );
			if( ca == NULL ) {
				validationError = "Element 'LaunchSpecifications[x].IamInstanceProfile' is not a ClassAd.";
				return false;
			}
			ClassAd iamInstanceProfile( * ca );

			iamInstanceProfile.LookupString( "Arn", blob[ "IAMProfileARN" ] );
			iamInstanceProfile.LookupString( "Name", blob[ "IAMProfileName" ] );
		}

		ExprTree * sgTree = launchConfiguration.Lookup( "SecurityGroups" );
		if( sgTree != NULL ) {
			classad::ExprList * sgList = dynamic_cast<classad::ExprList *>( sgTree );
			if( sgList == NULL ) {
				validationError = "Element 'LaunchSpecifications[x].SecurityGroups' is not a ClassAd.";
				return false;
			}

			StringList sgIDList, sgNameList;
			auto sgIterator = sgList->begin();
			for( ; sgIterator != sgList->end(); ++sgIterator ) {
				classad::ClassAd * ca = dynamic_cast<classad::ClassAd *>( * sgIterator );
				if( ca == NULL ) {
					validationError = "Element 'LaunchSpecifications[x].SecurityGroups[y]' is not a ClassAd.";
					return false;
				}
				ClassAd securityGroup( * ca );

				std::string groupID, groupName;
				securityGroup.LookupString( "GroupId", groupID );
				securityGroup.LookupString( "GroupName", groupName );
				if(! groupID.empty()) { sgIDList.append( groupID.c_str() ); }
				if(! groupName.empty()){ sgNameList.append( groupName.c_str() ); }
			}

			char * fail = sgIDList.print_to_delimed_string( ", " );
			if( fail ) {
				blob[ "SecurityGroupIDs" ] = fail;
				free( fail );
			}

			char * suck = sgNameList.print_to_delimed_string( ", " );
			if( suck ) {
				blob[ "SecurityGroupNames" ] = suck;
				free( suck );
			}
		}

		ExprTree * bdmTree = launchConfiguration.Lookup( "BlockDeviceMappings" );
		if( bdmTree != NULL ) {
			classad::ExprList * blockDeviceMappings = dynamic_cast<classad::ExprList *>( bdmTree );
			if(! blockDeviceMappings) {
				validationError = "Attribute 'LaunchSpecifications[x].BlockDeviceMappings' not a list.";
				return false;
			}

			StringList bdmList;
			auto bdmIterator = blockDeviceMappings->begin();
			for( ; bdmIterator != blockDeviceMappings->end(); ++bdmIterator ) {
				classad::ClassAd * ca = dynamic_cast<classad::ClassAd *>( * bdmIterator );
				if( ca == NULL ) {
					validationError = "Attribute 'LaunchSpecifications[x].BlockDeviceMappings[y] not a ClassAd.";
					return false;
				}
				ClassAd blockDeviceMapping( * ca );

				std::string dn;
				blockDeviceMapping.LookupString( "DeviceName", dn );
				if( dn.empty() ) {
					validationError = "Attribute 'LaunchSpecifications[x].BlockDeviceMappings[y].DeviceName missing or not a string.";
					return false;
				}

				ExprTree * ebsTree = blockDeviceMapping.Lookup( "Ebs" );
				classad::ClassAd * cb = dynamic_cast< classad::ClassAd *>( ebsTree );
				if( cb == NULL ) {
					validationError = "Attribute 'LaunchSpecifications[x].BlockDeviceMappings[y].Ebs missing or not a ClassAd.";
					return false;
				}
				ClassAd ebs( * cb );

				std::string si;
				ebs.LookupString( "SnapshotId", si );
				if( si.empty() ) {
					validationError = "Attribute 'LaunchSpecifications[x].BlockDeviceMappings[y].Ebs.SnapshotId missing or not a string.";
					return false;
				}

				std::string vt;
				ebs.LookupString( "VolumeType", vt );
				if( vt.empty() ) {
					validationError = "Attribute 'LaunchSpecifications[x].BlockDeviceMappings[y].Ebs.VolumeType missing or not a string.";
					return false;
				}

				int vs;
				if(! ebs.LookupInteger( "VolumeSize", vs )) {
					validationError = "Attribute 'LaunchSpecifications[x].BlockDeviceMappings[y].Ebs.VolumeSize missing or not an integer.";
					return false;
				}

				bool dot;
				if(! ebs.LookupBool( "DeleteOnTermination", dot )) {
					validationError = "Attribute 'LaunchSpecifications[x].BlockDeviceMappings[y].Ebs.DeleteOnTermination missing or not a boolean.";
					return false;
				}

				// <DeviceName>=<SnapshotId>:<VolumeSize>:<DeleteOnTermination>:<VolumeType>
				std::string bdmString;
				formatstr( bdmString, "%s=%s:%d:%s:%s",
					dn.c_str(), si.c_str(), vs, dot ? "true" : "false", vt.c_str() );
				bdmList.append( bdmString.c_str() );
			}

			char * fail = bdmList.print_to_delimed_string( ", " );
			if( fail != NULL ) {
				blob[ "BlockDeviceMapping" ] = fail;
				free( fail );
			}
		}

		std::string lcString = "{";
		for( auto i = blob.begin(); i != blob.end(); ++i ) {
			if( i->second.empty() ) { continue; }
			formatstr( lcString, "%s \"%s\": \"%s\",",
				lcString.c_str(), i->first.c_str(), i->second.c_str() );
		}
		lcString.erase( lcString.length() - 1 );
		lcString += " }";

		dprintf( D_FULLDEBUG, "Using launch specification string '%s'.\n", lcString.c_str() );
		launch_specifications.push_back( lcString );
	}

	return true;
}

void
BulkRequest::log() {
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
		if(! client_token.empty()) {
			std::string quoted; formatstr( quoted, "\"%s\"", client_token.c_str() );
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
	}
	commandState->CommitTransaction();
}

int
BulkRequest::operator() () {
	static bool incrementTryCount = true;
	dprintf( D_FULLDEBUG, "BulkRequest::operator()\n" );

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

		ClassAd * commandAd;
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

		// We have to call bulk_start() at least twice (once to issue the
		// command, and at least once to get the result), so we should
		// probably do something clever here and only log once.
		this->log();
		rc = gahp->bulk_start(
					service_url, public_key_file, secret_key_file,
					client_token, spot_price, target_capacity,
					iam_fleet_role, allocation_strategy, valid_until,
					launch_specifications,
					bulkRequestID, errorCode );

		if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
			// We should exit here the first time.
			return KEEP_STREAM;
		} else {
			incrementTryCount = true;
		}
	}

	if( rc == 0 ) {
		dprintf( D_ALWAYS, "SFR ID: %s\n", bulkRequestID.c_str() );
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
		formatstr( message, "Bulk (SFR) start request failed: '%s' (%d): '%s'.",
			errorCode.c_str(), rc, gahp->getErrorString() );
		dprintf( D_ALWAYS, "%s\n", message.c_str() );

		// The previous argument for retries doesn't make any sense anymore,
		// what with the client tokens and all, but maybe it'll come in handy
		// later?
		if( tryCount < 3 || errorCode == "NEED_CHECK_BULK_START" ) {
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
BulkRequest::rollback() {
	dprintf( D_FULLDEBUG, "BulkRequest::rollback()\n" );

	if(! bulkRequestID.empty()) {
		int rc;
		std::string errorCode;
		rc = gahp->bulk_stop(
					service_url, public_key_file, secret_key_file,
					bulkRequestID,
					errorCode );
		if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
			// We should exit here the first time.
			return KEEP_STREAM;
		}

		if( rc != 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel spot fleet request '%s' ('%s').\n", bulkRequestID.c_str(), errorCode.c_str() );
		}
	}

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
