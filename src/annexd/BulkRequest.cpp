#include "condor_common.h"

#include <string>
#include <vector>
#include "condor_classad.h"
#include "gahp-client.h"
#include "BulkRequest.h"

#include "classad_command_util.h"

bool BulkRequest::validateAndStore( ClassAd const * command, std::string & validationError ) {
	command->LookupString( "SpotPrice", spot_price );
	if( spot_price.empty() ) {
		validationError = "Attribute 'SpotPrice' missing or not a string.";
		return false;
	}

	int targetCapacity;
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
		launchConfiguration.LookupString( "UserData", blob[ "UserData" ] );
		launchConfiguration.LookupString( "InstanceType", blob[ "InstanceType" ] );
		launchConfiguration.LookupString( "SubnetId", blob[ "SubnetId" ] );
		launchConfiguration.LookupString( "WeightedCapacity", blob[ "WeightedCapacity" ] );

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
BulkRequest::operator() () const {
	int rc;
	std::string errorCode;
	std::string bulkRequestID;

	rc = gahp->bulk_start(
				service_url, public_key_file, secret_key_file,
				client_token, spot_price, target_capacity,
				iam_fleet_role, allocation_strategy,
				launch_specifications,
				bulkRequestID, errorCode );
	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We should exit here the first time.
		return;
	}

	if( rc == 0 ) {
		dprintf( D_ALWAYS, "Bulk start request ID: %s\n", bulkRequestID.c_str() );

		ClassAd reply;
		reply.Assign( "RequestVersion", 1 );
		reply.Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
		reply.Assign( "BulkRequestID", bulkRequestID );

		if(! sendCAReply( replyStream, "CA_BULK_REQUEST", & reply )) {
			dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
		}
	} else if( errorCode == "NEED_CHECK_BULK_START" ) {
		// FIXME: We should probably retry, instead.
		dprintf( D_ALWAYS, "Bulk start request failed but may have left a Spot Fleet behind.\n" );
	} else {
		std::string gahpErrorString = gahp->getErrorString();
		dprintf( D_ALWAYS, "Bulk start request failed: '%s' (%d): '%s'.\n", errorCode.c_str(), rc, gahpErrorString.c_str() );
	}

	// We're done replying, so clean the stream up.
	delete replyStream;

	// Cancel the timer we registered.  That's not the gahp client's
	// responsibility, but since it's keeping track of it ID for us...
	daemonCore->Cancel_Timer( gahp->getNotificationTimerId() );

	// We're done with this gahp, so clean it up.  The gahp server will
	// shut itself (and the GAHP) down cleanly roughly ten seconds after
	// the last corresponding gahp client is deleted.
	delete gahp;

	// Now that we've cancelled the timer, nobody else has a reference to
	// use, and we're done.  This, of course, must be the last thing we
	// do on the way out.
	delete this;
}

