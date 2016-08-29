#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "get_daemon_name.h"
#include "gahp-client.h"
#include "classad_command_util.h"

#include <algorithm>

// Required by GahpServer::Startup().
char * GridmanagerScratchDir = NULL;

// Start a GAHP client.  Each GAHP client can only have one request outstanding
// at a time, and it distinguishes between them based only on the name of the
// command.  As a result, we want each annex (or bulk request) to have its own
// GAHP client, to make sure that the requests don't collide.  If two GAHP
// clients shared a 'name' parameter, they will share a GAHP server process;
// to make the RequestLimitExceeded stuff work right, we start a GAHP server
// for each {public key file, service URL} tuple.
EC2GahpClient *
startOneGahpClient( const std::string & publicKeyFile, const std::string & serviceURL ) {
	std::string gahpName;
	formatstr( gahpName, "annex-%s@%s", publicKeyFile.c_str(), serviceURL.c_str() );

	ArgList args;

	// Configure dprintf using this name.
	args.AppendArg( "-l" );
	args.AppendArg( "ANNEX_GAHP" );

	// FIXME: The EC2 GAHP should accept a command-line argument from us
	// instead of looking up EC2_GAHP_RATE_LIMIT.

	args.AppendArg( "-w" );
	int minWorkerCount = param_integer( "ANNEX_GAHP_WORKER_MIN_NUM", 1 );
	args.AppendArg( minWorkerCount );

	args.AppendArg( "-m" );
	int maxWorkerCount = param_integer( "ANNEX_GAHP_WORKER_MAX_NUM", 1 );
	args.AppendArg( maxWorkerCount );

	args.AppendArg( "-d" );
	char * gahp_debug = param( "ANNEX_GAHP_DEBUG" );
	if( gahp_debug == NULL ) {
		args.AppendArg( "D_ALWAYS" );
	} else {
		args.AppendArg( gahp_debug );
		free( gahp_debug );
	}

	char * gahp_path = param( "ANNEX_GAHP" );
	if( gahp_path == NULL ) {
		EXCEPT( "ANNEX_GAHP must be defined." );
	}

	EC2GahpClient * gahp = new EC2GahpClient( gahpName.c_str(), gahp_path, & args );
	free( gahp_path );

	gahp->setMode( GahpClient::normal );

	int gct = param_integer( "ANNEX_GAHP_CALL_TIMEOUT", 10 * 60 );
	gahp->setTimeout( gct );

	if( gahp->Startup() == false ) {
		EXCEPT( "Failed to start GAHP." );
	}

	return gahp;
}

// Implement the demo hack.  In the future, we would probably want to
// record the arguments we passed to bulk_start(), rather than assume
// that the ones fetched from configuration haven't changed since the
// last time we looked them up.
class BulkRequest : public Service {
	public:
		BulkRequest( EC2GahpClient * egc,
		  const std::string & serviceURL,
		  const std::string & publicKeyFile,
		  const std::string & secretKeyFile
		  ) : gahp( egc ),
		      service_url( serviceURL ),
		      public_key_file( publicKeyFile ),
		      secret_key_file( secretKeyFile )
		{ };
		virtual ~BulkRequest() { }

		bool validateAndStore( ClassAd const * command, std::string & validationError );
		void operator() () const;

	protected:
		EC2GahpClient * gahp;

		std::string service_url, public_key_file, secret_key_file;
		std::string client_token, spot_price, target_capacity;
		std::string iam_fleet_role, allocation_strategy;

		std::vector< std::string > launch_specifications;
};

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
	} else if( errorCode == "NEED_CHECK_BULK_START" ) {
		// FIXME: We should probably retry, instead.
		dprintf( D_ALWAYS, "Bulk start request failed but may have left a Spot Fleet behind.\n" );
	} else {
		std::string gahpErrorString = gahp->getErrorString();
		dprintf( D_ALWAYS, "Bulk start request failed: '%s' (%d): '%s'.\n", errorCode.c_str(), rc, gahpErrorString.c_str() );
	}
}


void
createOneAnnex( ClassAd * command, ClassAd * reply ) {
	// FIXME: Look up the service URL and keyfiles in the map, if they're
	// not defined in the command.
	std::string serviceURL = "https://ec2.us-east-1.amazonaws.com";
	std::string publicKeyFile = "/home/tlmiller/condor/test/ec2/accessKeyFile";
	std::string secretKeyFile = "/home/tlmiller/condor/test/ec2/secretKeyFile";

	// Create the GAHP client.  The first time we call a GAHP client function,
	// it will send that command to the GAHP server, but the GAHP server may
	// take some time to get the result.  The GAHP client will fire the
	// notification timer when the result is ready, and we can get it by
	// calling the GAHP client function a second time with the same arguments.
	EC2GahpClient * gahp = startOneGahpClient( publicKeyFile, serviceURL );

	// Create a timer for the gahp to fire when it gets a result.  Use it,
	// as long as we have to create it anyway, to make the initial bulk
	// request.  We must use TIMER_NEVER to ensure that the timer hasn't
	// been reaped when the GAHP client needs to fire it.
	BulkRequest * br = new BulkRequest( gahp, serviceURL, publicKeyFile, secretKeyFile );

	std::string validationError;
	if(! br->validateAndStore( command, validationError )) {
		reply->Assign( ATTR_RESULT, getCAResultString( CA_INVALID_REQUEST ) );
		reply->Assign( ATTR_ERROR_STRING, validationError );
		return;
	}

	// FIXME: We may need to do something clever here.  Also, do NOT allow
	// the user to specify the client token.
	// br->setClientToken();

	// FIXME: What's the idiom for delaying our reply until we hear back from
	// the service?  Or do we just define success, from the command-line's
	// POV, as having submitted a valid request?
	reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );

	int gahpNotificationTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
		(void (Service::*)()) & BulkRequest::operator(),
		"BulkRequest::operator()()", br );
	gahp->setNotificationTimerId( gahpNotificationTimer );
}

int updateTimerID;
int updateInterval;
ClassAd annexDaemonAd;

void
updateCollectors() {
	daemonCore->publish( & annexDaemonAd );
	daemonCore->dc_stats.Publish( annexDaemonAd );
	daemonCore->monitor_data.ExportData( & annexDaemonAd );
	daemonCore->sendUpdates( UPDATE_AD_GENERIC, & annexDaemonAd, NULL, true );

	daemonCore->Reset_Timer( updateTimerID, updateInterval, updateInterval );
}

void
main_config() {
	// Update param() globals.
	updateInterval = param_integer( "ANNEXD_UPDATE_INTERVAL", 5 * MINUTE );

	// Reset our classAd.
	annexDaemonAd = ClassAd();
	SetMyTypeName( annexDaemonAd, GENERIC_ADTYPE );
	SetTargetTypeName( annexDaemonAd, "Cloud" );

	char * adn = NULL;
	std::string annexDaemonName;
	param( annexDaemonName, "ANNEXD_NAME" );
	if( annexDaemonName.empty() ) {
		adn = default_daemon_name();
	} else {
		adn = build_valid_daemon_name( annexDaemonName.c_str() );
	}
	annexDaemonAd.Assign( ATTR_NAME, adn );
	delete [] adn;
}

void
handleClassAdCommand( Service *, int dcCommandInt, Stream * s ) {
	ASSERT( dcCommandInt == CA_AUTH_CMD );

	ClassAd commandAd;
	ReliSock * rsock = (ReliSock *)s;
	int command = getCmdFromReliSock( rsock, & commandAd, true );

	std::string commandString;
	if(! commandAd.LookupString( ATTR_COMMAND, commandString ) ) {
		commandString = "not found";
	}

	switch( command ) {
		case CA_BULK_REQUEST: {
			ClassAd reply;
			reply.Assign( "RequestVersion", 1 );

			createOneAnnex( & commandAd, & reply );
			if(! sendCAReply( s, commandString.c_str(), & reply )) {
				dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
			}
		} break;

		default:
			dprintf( D_ALWAYS, "Unknown command (%s) in CA_AUTH_CMD ad, ignoring.\n", commandString.c_str() );
			return;
		break;
	}
}

void
main_init( int /* argc */, char ** /* argv */ ) {
	// Make sure that, e.g., updateInterval is set.
	main_config();

	// All our commands are ClassAds.  At some point, we're going to want
	// to know the authenticated identity of the requester, so make that
	// happen here.  (Forcing authentication in getCmdFromReliSock() doesn't
	// actually generate an authenticated identity for some reason -- I get
	// 'unauthorized@unmapped', probably because host-based security has
	// already happened.)
	daemonCore->Register_Command( CA_AUTH_CMD, "CA_AUTH_CMD",
		(CommandHandler)handleClassAdCommand, "handleClassAdCommand",
		NULL, ADMINISTRATOR, D_COMMAND, true );

	// Make sure the command-line tool can find us.
	updateTimerID = daemonCore->Register_Timer( 0, updateInterval, & updateCollectors, "updateCollectors()" );
}

void
main_shutdown_fast() {
	DC_Exit( 0 );
}

void
main_shutdown_graceful() {
	DC_Exit( 0 );
}

void
main_pre_dc_init( int /* argc */, char ** /* argv */ ) {
}

void
main_pre_command_sock_init() {
}

int
main( int argc, char ** argv ) {
	set_mySubSystem( "ANNEXD", SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = & main_init;
	dc_main_config = & main_config;
	dc_main_shutdown_fast = & main_shutdown_fast;
	dc_main_shutdown_graceful = & main_shutdown_graceful;
	dc_main_pre_dc_init = & main_pre_dc_init;
	dc_main_pre_command_sock_init = & main_pre_command_sock_init;

	return dc_main( argc, argv );
}
