#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "get_daemon_name.h"

#include "gahp-client.h"
#include "compat_classad.h"
#include "classad_command_util.h"
#include "classad_collection.h"

#include <queue>

#include "Functor.h"
#include "BulkRequest.h"
#include "PutRule.h"
#include "PutTargets.h"
#include "ReplyAndClean.h"
#include "FunctorSequence.h"

// Stolen from EC2Job::build_client_token in condor_gridmanager/ec2job.cpp.
#include <uuid/uuid.h>
void generateCommandID( std::string & commandID ) {
	char uuid_str[37];
	uuid_t uuid;
	uuid_generate( uuid );
	uuid_unparse( uuid, uuid_str );
	uuid_str[36] = '\0';
	commandID.assign( uuid_str );
}

// Although the annex daemon uses a GAHP, it doesn't have a schedd managing
// its hard state in an existing job ad; it has to do that job on its own.
ClassAdCollection * hardState;
ClassAdCollection * commandState;

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

	// A GAHP used by the annex daemon is configured as an annex GAHP,
	// regardless of which GAHP it actually is.  This sets the subsystem.
	args.AppendArg( "-s" );
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

void
InsertOrUpdateAd( const std::string & id, ClassAd * command,
  ClassAdCollection * log ) {
	// We could call ListNewAdsInTransaction() and check to see if we've
	// already inserted or updated an ad with this id, but this should work
	// in either case, so let's keep things simple for now.
	ClassAd * existing = NULL;
	const char * key = id.c_str();
	if( log->LookupClassAd( HashKey( key ), existing ) && existing != NULL ) {
		log->DestroyClassAd( key );
	}

	log->NewClassAd( key, command );
}

int
createOneAnnex( ClassAd * command, Stream * replyStream ) {
	// Validate the request (basic).
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

		if( replyStream ) {
			if(! sendCAReply( replyStream, "CA_BULK_REQUEST", & reply )) {
				dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
			}
		}

		return FALSE;
	}

	//
	// Construct the GAHPs.  We do this before anything else because we
	// need pointers the GAHPs to hand off to the nonblocking sequence
	// implementation.
	//

	std::string serviceURL, eventsURL, publicKeyFile, secretKeyFile;
	param( serviceURL, "ANNEX_DEFAULT_EC2_URL" );
	command->LookupString( "ServiceURL", serviceURL );
	param( eventsURL, "ANNEX_DEFAULT_CWE_URL" );
	command->LookupString( "EventsURL", eventsURL );

	// FIXME: Look up the authenticated user who sent this command in a
	// map, but prefer the command-line options.
	command->LookupString( "PublicKeyFile", publicKeyFile );
	command->LookupString( "SecretKeyFile", secretKeyFile );

	// Validate GAHP parameters.
	if( serviceURL.empty() || publicKeyFile.empty() || secretKeyFile.empty() ) {
		std::string errorString;
		if( serviceURL.empty() ) {
			formatstr( errorString, "ServiceURL missing or empty in command ad and ANNEX_DEFAULT_SERVICE_URL not set or empty in configuration." );
		}
		if( publicKeyFile.empty() ) {
			formatstr( errorString, "%s%sPublic key file not specified in command or defaults.", errorString.c_str(), errorString.empty() ? "" : "  " );
		}
		if( secretKeyFile.empty() ) {
			formatstr( errorString, "%s%sSecret key file not specified in command or defaults.", errorString.c_str(), errorString.empty() ? "" : "  " );
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


	//
	// Construct the bulk request, create rule, and add target functors,
	// then register them as a sequence in a timer.  This lets us get back
	// to DaemonCore more quickly and makes the code in operator() more
	// familiar (in terms of the idiomatic usage of the gahp client
	// interface established by the grid manager).
	//

	// Each step in the sequence may add to the reply, and may need to pass
	// information to the next step, so create those two ads first.
	ClassAd * reply = new ClassAd();
	reply->Assign( "RequestVersion", 1 );

	ClassAd * scratchpad = new ClassAd();

	// Each step in the sequence will handle its own logging for fault
	// recovery.  The command ad is indexed by the command ID, so
	// look up or generate it now, so we can tell it to the steps.
	std::string commandID;
	command->LookupString( "CommandID", commandID );
	if( commandID.empty() ) {
		generateCommandID( commandID );
		command->Assign( "CommandID", commandID );
	}
	scratchpad->Assign( "CommandID", commandID );

	// Each step in the sequence handles its own de/serialization.
	BulkRequest * br = new BulkRequest( reply, gahp, scratchpad,
		serviceURL, publicKeyFile, secretKeyFile, commandState, commandID );

	// Now that we've created the bulk request, it can fully validate the
	// command ad, storing what it needs as it goes.  Really, each functor
	// in sequence should have a validateAndStore() method, but the second
	// two don't need one.
	std::string validationError;
	if(! br->validateAndStore( command, validationError )) {
		reply->Assign( ATTR_RESULT, getCAResultString( CA_INVALID_REQUEST ) );
		reply->Assign( ATTR_ERROR_STRING, validationError );

		if(! sendCAReply( replyStream, "CA_BULK_REQUEST", reply )) {
			dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
		}

		delete gahp;
		delete eventsGahp;

		delete reply;
		delete scratchpad;

		delete br;

		return FALSE;
	}

	//
	// After this point, we no longer exit because of an invalid request, so
	// this is the first point at which we want to log the command ad.  Since
	// none of the functors have run yet, we haven't made any changes to the
	// cloud's state yet.  We'll pass responsibility off to the functors for
	// logging their own individual cloud-state changes, which means now is
	// the right time to log the command ad.
	//
	// Note that moving the retry/fault-tolerance code into the functors means
	// that some of policy (not accepting user client tokens and the default
	// validity period) has to move into the functors as well.
	//

	commandState->BeginTransaction();
	{
		InsertOrUpdateAd( commandID, command, commandState );
	}
	commandState->CommitTransaction();

	time_t now = time( NULL );
	PutRule * pr = new PutRule( reply, eventsGahp, scratchpad,
		eventsURL, publicKeyFile, secretKeyFile,
		commandState, commandID );
	PutTargets * pt = new PutTargets( reply, eventsGahp, scratchpad,
		eventsURL, publicKeyFile, secretKeyFile, now + (15 * 60),
		commandState, commandID );
	// We now only call last->operator() on success; otherwise, we roll back
	// and call last->rollback() after we've given up.  We can therefore
	// remove the command ad from the commandState in this functor.
	ReplyAndClean * last = new ReplyAndClean( reply, replyStream, gahp, scratchpad, eventsGahp, commandState, commandID );

	// Note that the functor sequence takes responsibility for deleting the
	// functor objects; the functor objects would just delete themselves when
	// they're done, but implementing rollback means the functors themselves
	// can't know how long they should persist.
	//
	// The commandState, commandID, and scratchpad allow the functor sequence
	// to restart a rollback, if that becomes necessary.
	FunctorSequence * fs = new FunctorSequence( { br, pr, pt }, last, commandState, commandID, scratchpad );

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

std::string commandStateFile;
// std::string hardStateFile;

void
main_config() {
	// Update param() globals.
	updateInterval = param_integer( "ANNEXD_UPDATE_INTERVAL", 5 * MINUTE );
	commandStateFile = param( "ANNEXD_COMMAND_STATE" );

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

	// FIXME: Record the authorized user in the command ad, so that'll
	// be available when createOneAnnex() needs it during recovery.

	switch( command ) {
		case CA_BULK_REQUEST: {
			// Do not allow users to provide their own command IDs.
			commandAd.Assign( "CommandID", NULL );
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

class IncompleteCommand : public Service {
	public:
		IncompleteCommand( ClassAd * c ) : commandAd( c ) { }

		void operator() () {
			// We know the command ad will validate, because we don't store
			// invalid command ads.  createOneAnnex() will therefore return
			// KEEP_STREAM (which isn't relevant without a stream), so we
			// can ignore the return value.
			createOneAnnex( commandAd, NULL );
		}

	protected:
		ClassAd * commandAd;
};

void
main_init( int /* argc */, char ** /* argv */ ) {
	// Make sure that, e.g., updateInterval is set.
	main_config();

	HashKey currentKey;
	ClassAd * currentAd;
	commandState = new ClassAdCollection( NULL, commandStateFile.c_str() );
	commandState->StartIterateAllClassAds();
	while( commandState->IterateAllClassAds( currentAd, currentKey ) ) {
		if( strcmp( "Command", GetMyTypeName( * currentAd ) ) != 0 ) { continue; }
		dprintf( D_ALWAYS, "Found command state ad '%s'.\n", currentKey.value() );

		//
		// Fire a timer for each interrupted command.
		//
		IncompleteCommand * ic = new IncompleteCommand( currentAd );
		daemonCore->Register_Timer( 0, 0,
			(void (Service::*)()) & IncompleteCommand::operator(),
			currentKey.value(), ic );
	}

	// hardState = new ClassAdCollection( NULL, hardStateFile.c_str() );

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
