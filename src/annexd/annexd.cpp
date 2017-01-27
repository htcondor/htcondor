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
#include "GetFunction.h"
#include "PutRule.h"
#include "PutTargets.h"
#include "ReplyAndClean.h"
#include "FunctorSequence.h"
#include "generate-id.h"
#include "OnDemandRequest.h"

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
startOneGahpClient( const std::string & publicKeyFile, const std::string & /* serviceURL */ ) {
	std::string gahpName;

	// This makes me sad, now that the annexd needs to use three endpoints, so
	// let's ignore the technical limitation about RequestLimitExceeded for
	// now, since we shouldn't encounter it and the only consequence is, I
	// think, backoffs for endpoints that don't need them.  (If this becomes
	// a problem, we should probably change this to configuration knob,
	// because that means we probably want to split the load up among multiple
	// GAHPs anyway.)
	// formatstr( gahpName, "annex-%s@%s", publicKeyFile.c_str(), serviceURL.c_str() );

	// The unfixable technical restriction is actually that all credentials
	// passed to the same GAHP must be accessible by it.  Since the GAHP
	// (will) run as a particular user for precisely this reason (not just
	// isolation but also network filesystems), we could instead name the
	// GAHP after the key into the authorized user map we used.  For now,
	// since people aren't likely to have that many different credentials,
	// just use the name of the credentials.
	formatstr( gahpName, "annex-%s", publicKeyFile.c_str() );

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

bool
validateLease( time_t endOfLease, std::string & validationError ) {
	time_t now = time( NULL );
	if( endOfLease < now ) {
		validationError = "The lease must end in the future.";
		return false;
	}
	return true;
}

int
createOneAnnex( ClassAd * command, Stream * replyStream ) {
	// dPrintAd( D_FULLDEBUG, * command );

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

	std::string serviceURL, eventsURL, lambdaURL;

	param( serviceURL, "ANNEX_DEFAULT_EC2_URL" );
	// FIXME: look up service URL from authorized user map.
	command->LookupString( "ServiceURL", serviceURL );

	param( eventsURL, "ANNEX_DEFAULT_CWE_URL" );
	// FIXME: look up events URL from authorized user map.
	command->LookupString( "EventsURL", eventsURL );

	param( lambdaURL, "ANNEX_DEFAULT_LAMBDA_URL" );
	// FIXME: look up lambda URL from authorized user map.
	command->LookupString( "LambdaURL", lambdaURL );

	std::string publicKeyFile, secretKeyFile, leaseFunctionARN;

	// FIXME: look up public key file from authorized user map.
	param( publicKeyFile, "ANNEX_DEFAULT_ACCESS_KEY_FILE" );
	command->LookupString( "PublicKeyFile", publicKeyFile );

	// FIXME: look up secret key file from authorized user map.
	param( secretKeyFile, "ANNEX_DEFAULT_SECRET_KEY_FILE" );
	command->LookupString( "SecretKeyFile", secretKeyFile );

	// FIXME: look up lease function ARN from authorized user map
	// (from the endpoint-specific sub-map).  There will actually
	// be two listings, one for SFR and one for ODI, so we need to
	// know which type of annex we'll be making before doing this look-up.
	command->LookupString( "LeaseFunctionARN", leaseFunctionARN );

	// Validate parameters.  We could have the functors do this, but that
	// would mean adding invalid requests to the hard state, which is bad.
	if( serviceURL.empty() || eventsURL.empty() || lambdaURL.empty() ||
			publicKeyFile.empty() || secretKeyFile.empty() ||
			leaseFunctionARN.empty() ) {
		std::string errorString;
		if( serviceURL.empty() ) {
			formatstr( errorString, "Service URL missing or empty in command ad and ANNEX_DEFAULT_EC2_URL not set or empty in configuration." );
		}
		if( eventsURL.empty() ) {
			formatstr( errorString, "%s%sEvents URL missing or empty in command ad and ANNEX_DEFAULT_CWE_URL not set or empty in configuration.", errorString.c_str(), errorString.empty() ? "" : "  " );
		}
		if( lambdaURL.empty() ) {
			formatstr( errorString, "%s%sLambda URL missing or empty in command ad and ANNEX_DEFAULT_LAMBDA_URL not set or empty in configuration.", errorString.c_str(), errorString.empty() ? "" : "  " );
		}
		if( publicKeyFile.empty() ) {
			formatstr( errorString, "%s%sPublic key file not specified in command or defaults.", errorString.c_str(), errorString.empty() ? "" : "  " );
		}
		if( secretKeyFile.empty() ) {
			formatstr( errorString, "%s%sSecret key file not specified in command or defaults.", errorString.c_str(), errorString.empty() ? "" : "  " );
		}
		if( leaseFunctionARN.empty() ) {
			formatstr( errorString, "%s%sLease function ARN not specified in command or defaults.", errorString.c_str(), errorString.empty() ? "" : "  " );
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

	// Create the GAHP client.  The first time we call a GAHP client function,
	// it will send that command to the GAHP server, but the GAHP server may
	// take some time to get the result.  The GAHP client will fire the
	// notification timer when the result is ready, and we can get it by
	// calling the GAHP client function a second time with the same arguments.
	EC2GahpClient * gahp = startOneGahpClient( publicKeyFile, serviceURL );

	// The lease requires a different endpoint to implement.
	EC2GahpClient * eventsGahp = startOneGahpClient( publicKeyFile, eventsURL );

	// Checking for the existence of the lease function requires a different
	// endpoint as well.  Maybe we should fiddle the GAHP to make it less
	// traumatic to use different endpoints?
	EC2GahpClient * lambdaGahp = startOneGahpClient( publicKeyFile, lambdaURL );

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

	// The annex ID will form part of the SFR (or ODI) client token, which
	// allows the corresponding lease function to cnacel them all without
	// having to continually add or update the target(s) in the lease.  Since
	// we don't need to know the SFR (or instance) ID -- or even its entire
	// client token -- we can create the lease /before/ we start instances.
	//
	// If the user specified an annex name, we'll use the first 36 characters,
	// the same length as the UUID we'd otherwise generate.
	std::string annexID;
	command->LookupString( "AnnexID", annexID );
	if( annexID.empty() ) {
		std::string annexName;
		command->LookupString( "AnnexName", annexName );
		if( annexName.empty() ) {
			generateAnnexID( annexID );
		} else {
			annexID = annexName.substr( 0, 36 );
		}
		command->Assign( "AnnexID", annexID );
	}
	scratchpad->Assign( "AnnexID", annexID );


	std::string annexType;
	command->LookupString( "AnnexType", annexType );
	if( annexType.empty() || (annexType != "odi" && annexType != "sfr") ) {
		std::string errorString = "AnnexType unspecified or unknown.";
		dprintf( D_ALWAYS, "%s (%s)\n", errorString.c_str(), annexType.c_str() );

		reply->Assign( ATTR_RESULT, getCAResultString( CA_INVALID_REQUEST ) );
		reply->Assign( ATTR_ERROR_STRING, errorString );

		if( replyStream ) {
			if(! sendCAReply( replyStream, "CA_BULK_REQUEST", reply )) {
				dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
			}
		}

		return FALSE;
	}


	std::string validationError;
	if( annexType == "sfr" ) {
		// Each step in the sequence handles its own de/serialization.
		BulkRequest * br = new BulkRequest( reply, gahp, scratchpad,
			serviceURL, publicKeyFile, secretKeyFile, commandState,
			commandID, annexID );

		time_t endOfLease = 0;
		command->LookupInteger( "EndOfLease", endOfLease );

		// Now that we've created the bulk request, it can fully validate the
		// command ad, storing what it needs as it goes.  Really, each functor
		// in sequence should have a validateAndStore() method, but the second
		// two don't need one.
		if( (! br->validateAndStore( command, validationError )) || (! validateLease( endOfLease, validationError )) ) {
			delete br;
			goto cleanup;
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

		// Verify the existence of the specified function before starting any
		// instances.  Otherwise, the lease may not fire.
		GetFunction * gf = new GetFunction( leaseFunctionARN,
			reply, lambdaGahp, scratchpad,
	    	lambdaURL, publicKeyFile, secretKeyFile,
			commandState, commandID );
		PutRule * pr = new PutRule( reply, eventsGahp, scratchpad,
			eventsURL, publicKeyFile, secretKeyFile,
			commandState, commandID, annexID );
		PutTargets * pt = new PutTargets( leaseFunctionARN,
			reply, eventsGahp, scratchpad,
			eventsURL, publicKeyFile, secretKeyFile, endOfLease,
			commandState, commandID, annexID );
		// We now only call last->operator() on success; otherwise, we roll back
		// and call last->rollback() after we've given up.  We can therefore
		// remove the command ad from the commandState in this functor.
		ReplyAndClean * last = new ReplyAndClean( reply, replyStream, gahp, scratchpad, eventsGahp, commandState, commandID, lambdaGahp );

		// Note that the functor sequence takes responsibility for deleting the
		// functor objects; the functor objects would just delete themselves when
		// they're done, but implementing rollback means the functors themselves
		// can't know how long they should persist.
		//
		// The commandState, commandID, and scratchpad allow the functor sequence
		// to restart a rollback, if that becomes necessary.
		FunctorSequence * fs = new FunctorSequence( { gf, pr, pt, br }, last, commandState, commandID, scratchpad );

		// Create a timer for the gahp to fire when it gets a result.  We must
		// use TIMER_NEVER to ensure that the timer hasn't been reaped when the
		// GAHP client needs to fire it.
		int functorSequenceTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
			(void (Service::*)()) & FunctorSequence::operator(),
			"GetFunction, PutRule, PutTarget, BulkRequest", fs );
		gahp->setNotificationTimerId( functorSequenceTimer );
    	eventsGahp->setNotificationTimerId( functorSequenceTimer );
    	lambdaGahp->setNotificationTimerId( functorSequenceTimer );

		return KEEP_STREAM;
	} else if( annexType == "odi" ) {
		OnDemandRequest * odr = new OnDemandRequest( reply, gahp, scratchpad,
			serviceURL, publicKeyFile, secretKeyFile, commandState,
			commandID, annexID );

		time_t endOfLease = 0;
		command->LookupInteger( "EndOfLease", endOfLease );

		if( (! odr->validateAndStore( command, validationError )) || (! validateLease( endOfLease, validationError )) ) {
			delete odr;
			goto cleanup;
		}

		// See corresponding comment above.
		commandState->BeginTransaction();
		{
			InsertOrUpdateAd( commandID, command, commandState );
		}
		commandState->CommitTransaction();

		GetFunction * gf = new GetFunction( leaseFunctionARN,
			reply, lambdaGahp, scratchpad,
	    	lambdaURL, publicKeyFile, secretKeyFile,
			commandState, commandID );
		PutRule * pr = new PutRule( reply, eventsGahp, scratchpad,
			eventsURL, publicKeyFile, secretKeyFile,
			commandState, commandID, annexID );
		PutTargets * pt = new PutTargets( leaseFunctionARN,
			reply, eventsGahp, scratchpad,
			eventsURL, publicKeyFile, secretKeyFile, endOfLease,
			commandState, commandID, annexID );
		ReplyAndClean * last = new ReplyAndClean( reply, replyStream, gahp, scratchpad, eventsGahp, commandState, commandID, lambdaGahp );

		FunctorSequence * fs = new FunctorSequence( { gf, pr, pt, odr }, last, commandState, commandID, scratchpad );

		int functorSequenceTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
			(void (Service::*)()) & FunctorSequence::operator(),
			"GetFunction, PutRule, PutTarget, OnDemandRequest", fs );
		gahp->setNotificationTimerId( functorSequenceTimer );
    	eventsGahp->setNotificationTimerId( functorSequenceTimer );
    	lambdaGahp->setNotificationTimerId( functorSequenceTimer );

		return KEEP_STREAM;
	} else {
		ASSERT( 0 );
	}

	cleanup:
		reply->Assign( ATTR_RESULT, getCAResultString( CA_INVALID_REQUEST ) );
		reply->Assign( ATTR_ERROR_STRING, validationError );

		if( replyStream ) {
			if(! sendCAReply( replyStream, "CA_BULK_REQUEST", reply )) {
				dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
			}
		}

		delete gahp;
		delete eventsGahp;
		delete lambdaGahp;

		delete reply;
		delete scratchpad;

		return FALSE;
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
			commandAd.Assign( "CommandID", (const char *)NULL );
			// Make sure we don't collide with another annex.
			commandAd.Assign( "AnnexID", (const char *)NULL );
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
