#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "get_daemon_name.h"

#include "gahp-client.h"
#include "compat_classad.h"
#include "classad_command_util.h"

// #include <algorithm>
#include <queue>

#include "Functor.h"
#include "BulkRequest.h"
#include "PutRule.h"
#include "AddTarget.h"
#include "ReplyAndClean.h"
#include "FunctorSequence.h"


// Stolen from EC2Job::build_client_token in condor_gridmanager/ec2job.cpp
// and duplicated here because I espect to need to fiddle with it.
#include <uuid/uuid.h>
void generateClientToken( std::string & ct ) {
	char uuid_str[37];
	uuid_t uuid;
	uuid_generate( uuid );
	uuid_unparse( uuid, uuid_str );
	uuid_str[36] = '\0';
	ct.assign( uuid_str );
}

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

int
createOneAnnex( ClassAd * command, Stream * replyStream ) {
	int requestVersion = -1;
	command->LookupInteger( "RequestVersion", requestVersion );
	if( requestVersion != 1 ) {
		std::string errorString;
		if( requestVersion == -1 ) {
			errorString = "Missing (or non-integer) RequestVersion.";
		} else {
			formatstr( errorString, "Unknown RequestVersion (%d).", requestVersion );
		}
		dprintf( D_ALWAYS, "%s\n", errorString.c_str() );

		ClassAd reply;
		reply.Assign( "RequestVersion", 1 );
		reply.Assign( ATTR_RESULT, getCAResultString( CA_INVALID_REQUEST ) );
		reply.Assign( ATTR_ERROR_STRING, errorString );

		if(! sendCAReply( replyStream, "CA_BULK_REQUEST", & reply )) {
			dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
		}

		return FALSE;
	}

	std::string serviceURL, eventsURL, publicKeyFile, secretKeyFile;
	param( serviceURL, "ANNEX_DEFAULT_EC2_URL" );
	command->LookupString( "ServiceURL", serviceURL );
	param( eventsURL, "ANNEX_DEFAULT_CWE_URL" );
	command->LookupString( "EventsURL", eventsURL );
	param( publicKeyFile, "ANNEX_DEFAULT_PUBLIC_KEY_FILE" );
	param( secretKeyFile, "ANNEX_DEFAULT_SECRET_KEY_FILE" );

	if( serviceURL.empty() || publicKeyFile.empty() || secretKeyFile.empty() ) {
		std::string errorString;
		if( serviceURL.empty() ) {
			formatstr( errorString, "ServiceURL missing or empty in command ad and ANNEX_DEFAULT_SERVICE_URL not set or empty in configuration." );
		}
		if( publicKeyFile.empty() ) {
			formatstr( errorString, "%s%sANNEX_DEFAULT_PUBLIC_KEY_FILE not set in configuration.", errorString.c_str(), errorString.empty() ? "" : "  " );
		}
		if( secretKeyFile.empty() ) {
			formatstr( errorString, "%s%sANNEX_DEFAULT_SECRET_KEY_FILE not set in configuration.", errorString.c_str(), errorString.empty() ? "" : "  " );
		}
		dprintf( D_ALWAYS, "%s\n", errorString.c_str() );

		ClassAd reply;
		reply.Assign( "RequestVersion", 1 );
		reply.Assign( ATTR_RESULT, getCAResultString( CA_INVALID_REQUEST ) );
		reply.Assign( ATTR_ERROR_STRING, errorString );

		if(! sendCAReply( replyStream, "CA_BULK_REQUEST", & reply )) {
			dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
		}

		return FALSE;
	}

	// Create the GAHP client.  The first time we call a GAHP client function,
	// it will send that command to the GAHP server, but the GAHP server may
	// take some time to get the result.  The GAHP client will fire the
	// notification timer when the result is ready, and we can get it by
	// calling the GAHP client function a second time with the same arguments.
	EC2GahpClient * gahp = startOneGahpClient( publicKeyFile, serviceURL );

	// The lease requires a different endpoint to implement.
	EC2GahpClient * eventsGahp = startOneGahpClient( publicKeyFile, eventsURL );


	// Construct the bulk request, create rule, and add target functors,
	// then register them as a sequence in a timer.  This lets us get back
	// to DaemonCore more quickly and makes the code in operator() more
	// familiar (in terms of the idiomatic usage of the gahp client
	// interface established by the grid manager).
	ClassAd * reply = new ClassAd();
	reply->Assign( "RequestVersion", 1 );

	ClassAd * scratchpad = new ClassAd();

	BulkRequest * br = new BulkRequest( reply, gahp, scratchpad,
		serviceURL, publicKeyFile, secretKeyFile );

	std::string validationError;
	if(! br->validateAndStore( command, validationError )) {
		reply->Assign( ATTR_RESULT, getCAResultString( CA_INVALID_REQUEST ) );
		reply->Assign( ATTR_ERROR_STRING, validationError );

		if(! sendCAReply( replyStream, "CA_BULK_REQUEST", reply )) {
			dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
		}

		delete reply;
		delete scratchpad;

		return FALSE;
	}

	if(! br->isValidUntilSet()) {
		time_t now = time( NULL );
		time_t fifteenMinutesFromNow = now + (15 * 60);
		struct tm fMFN;
		gmtime_r( & fifteenMinutesFromNow, & fMFN );
		char buffer[ 4 + 1 + 2 + 1 + 2 + 1 /* T */ + 2 + 1 + 2 + 1 + 2 + 1 /* Z */ + 1];
		strftime( buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", & fMFN );
		br->setValidUntil( buffer );
	}

	// Ignore any user-specified client token.  Client token generation
	// stolen from build_client_token() in condor_gridmanager/ec2job.cpp.
	// We may need to use some of the remaining 28 ASCII characters for
	// tracking purposes....
	std::string clientToken;
	generateClientToken( clientToken );
	reply->Assign( "ClientToken", clientToken );
	br->setClientToken( clientToken );

	PutRule * cr = new PutRule( reply, eventsGahp, scratchpad,
		eventsURL, publicKeyFile, secretKeyFile );
	AddTarget * at = new AddTarget( reply, eventsGahp, scratchpad,
		eventsURL, publicKeyFile, secretKeyFile );
	ReplyAndClean * last = new ReplyAndClean( reply, replyStream, gahp, scratchpad );

	FunctorSequence * fs = new FunctorSequence( { br, cr, at }, last );

	// Create a timer for the gahp to fire when it gets a result.  We must
	// use TIMER_NEVER to ensure that the timer hasn't been reaped when the
	// GAHP client needs to fire it.
	int functorSequenceTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
		(void (Service::*)()) & FunctorSequence::operator(),
		"BulkRequest, PutRule, AddTarget", fs );
	gahp->setNotificationTimerId( functorSequenceTimer );
    eventsGahp->setNotificationTimerId( functorSequenceTimer );

	return KEEP_STREAM;
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

int
handleClassAdCommand( Service *, int dcCommandInt, Stream * s ) {
	ASSERT( dcCommandInt == CA_AUTH_CMD );

	ClassAd commandAd;
	ReliSock * rsock = (ReliSock *)s;
	int command = getCmdFromReliSock( rsock, & commandAd, true );

	std::string commandString;
	if(! commandAd.LookupString( ATTR_COMMAND, commandString ) ) {
		commandString = "command not found";
	}

	switch( command ) {
		case CA_BULK_REQUEST: {
			return createOneAnnex( & commandAd, s );
		} break;

		default:
			dprintf( D_ALWAYS, "Unknown command (%s) in CA_AUTH_CMD ad, ignoring.\n", commandString.c_str() );
			return FALSE;
		break;
	}

	EXCEPT( "... and you may ask yourself: Well, how did I get here?" );
	return FALSE;
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
