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
#include "UploadFile.h"

#include "CreateStack.h"
#include "WaitForStack.h"
#include "CheckForStack.h"
#include "SetupReply.h"
#include "GenerateConfigFile.h"
#include "CreateKeyPair.h"
#include "CheckConnectivity.h"

#include "user-config-dir.h"

// from annex.cpp
#include "condor_common.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "match_prefix.h"
#include "daemon.h"
#include "dc_annexd.h"
#include "stat_wrapper.h"
#include "condor_base64.h"

#include "my_username.h"
#include <iostream>

#include "daemon.h"

// Although the annex daemon uses a GAHP, it doesn't have a schedd managing
// its hard state in an existing job ad; it has to do that job on its own.
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
updateOneAnnex( ClassAd * command, Stream * replyStream, ClassAd * reply ) {
	std::string publicKeyFile, secretKeyFile, eventsURL;

	param( publicKeyFile, "ANNEX_DEFAULT_ACCESS_KEY_FILE" );
	command->LookupString( "PublicKeyFile", publicKeyFile );

	param( secretKeyFile, "ANNEX_DEFAULT_SECRET_KEY_FILE" );
	command->LookupString( "SecretKeyFile", secretKeyFile );

	param( eventsURL, "ANNEX_DEFAULT_CWE_URL" );
	command->LookupString( "EventsURL", eventsURL );

	std::string annexID, leaseFunctionARN;

	command->LookupString( "AnnexID", annexID );
	command->LookupString( "LeaseFunctionARN", leaseFunctionARN );

	time_t endOfLease = 0;
	command->LookupInteger( "EndOfLease", endOfLease );

	std::string errorString;
	if( publicKeyFile.empty() ) {
		formatstr( errorString, "%s%sPublic key file not specified in command or by defaults.", errorString.c_str(), errorString.empty() ? "" : "  " );
	}
	if( secretKeyFile.empty() ) {
		formatstr( errorString, "%s%sSecret key file not specified in command or defaults.", errorString.c_str(), errorString.empty() ? "" : "  " );
	}
	if( eventsURL.empty() ) {
		formatstr( errorString, "%s%sEvents URL missing or empty in command ad and ANNEX_DEFAULT_CWE_URL not set or empty in configuration.", errorString.c_str(), errorString.empty() ? "" : "  " );
	}
	if( annexID.empty() ) {
		formatstr( errorString, "%s%sAnnex ID missing or empty in command ad.", errorString.c_str(), errorString.empty() ? "" : "  " );
	}
	if( leaseFunctionARN.empty() ) {
		formatstr( errorString, "%s%sLease function ARN missing or empty in command ad.", errorString.c_str(), errorString.empty() ? "" : "  " );
	}
	validateLease( endOfLease, errorString );

	if(! errorString.empty()) {
		reply->Assign( ATTR_RESULT, getCAResultString( CA_INVALID_REQUEST ) );
		reply->Assign( ATTR_ERROR_STRING, errorString );

		if( replyStream ) {
			if(! sendCAReply( replyStream, "CA_BULK_REQUEST", reply )) {
				dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
			}
		}

		return FALSE;
	}

	ClassAd * scratchpad = new ClassAd();
	EC2GahpClient * eventsGahp = startOneGahpClient( publicKeyFile, eventsURL );

	std::string commandID;
	command->LookupString( "CommandID", commandID );
	if( commandID.empty() ) {
		generateCommandID( commandID );
		command->Assign( "CommandID", commandID );
	}
	scratchpad->Assign( "CommandID", commandID );

	// FIXME: For user-friendliness, if no other reason, we should check
	// if the annex exists before we "update" its lease.  Unfortunately,
	// that's an ODI/SFR -specific operation.  We could check to see if
	// the corresponding rule and target exist, instead, and refuse to
	// do anything if they don't.

	PutRule * pr = new PutRule(
		reply, eventsGahp, scratchpad,
		eventsURL, publicKeyFile, secretKeyFile,
		commandState, commandID, annexID );
	PutTargets * pt = new PutTargets( leaseFunctionARN, endOfLease,
		reply, eventsGahp, scratchpad,
		eventsURL, publicKeyFile, secretKeyFile,
		commandState, commandID, annexID );

	// I should feel bad.
	scratchpad->Assign( "BulkRequestID", "Lease updated." );
	ReplyAndClean * last = new ReplyAndClean( reply, replyStream,
		NULL, scratchpad, eventsGahp, commandState, commandID, NULL );

	FunctorSequence * fs = new FunctorSequence( { pr, pt }, last,
		commandState, commandID, scratchpad );

	int functorSequenceTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
		(void (Service::*)()) & FunctorSequence::operator(),
		"updateOneAnnex() sequence", fs );
   	eventsGahp->setNotificationTimerId( functorSequenceTimer );

	return KEEP_STREAM;
}


int
createOneAnnex( ClassAd * command, Stream * replyStream, ClassAd * reply ) {
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

		reply->Assign( "RequestVersion", 1 );
		reply->Assign( ATTR_RESULT, getCAResultString( CA_INVALID_REQUEST ) );
		reply->Assign( ATTR_ERROR_STRING, errorString );

		if( replyStream ) {
			if(! sendCAReply( replyStream, "CA_BULK_REQUEST", reply )) {
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

	std::string serviceURL, eventsURL, lambdaURL, s3URL;

	param( serviceURL, "ANNEX_DEFAULT_EC2_URL" );
	command->LookupString( "ServiceURL", serviceURL );

	param( eventsURL, "ANNEX_DEFAULT_CWE_URL" );
	command->LookupString( "EventsURL", eventsURL );

	param( lambdaURL, "ANNEX_DEFAULT_LAMBDA_URL" );
	command->LookupString( "LambdaURL", lambdaURL );

	param( s3URL, "ANNEX_DEFAULT_S3_URL" );
	command->LookupString( "S3URL", s3URL );

	std::string publicKeyFile, secretKeyFile, leaseFunctionARN;

	param( publicKeyFile, "ANNEX_DEFAULT_ACCESS_KEY_FILE" );
	command->LookupString( "PublicKeyFile", publicKeyFile );

	param( secretKeyFile, "ANNEX_DEFAULT_SECRET_KEY_FILE" );
	command->LookupString( "SecretKeyFile", secretKeyFile );

	command->LookupString( "LeaseFunctionARN", leaseFunctionARN );

	// Validate parameters.  We could have the functors do this, but that
	// would mean adding invalid requests to the hard state, which is bad.
	if( serviceURL.empty() || eventsURL.empty() || lambdaURL.empty() ||
			publicKeyFile.empty() || secretKeyFile.empty() ||
			leaseFunctionARN.empty() || s3URL.empty() ) {
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
		if( s3URL.empty() ) {
			formatstr( errorString, "%s%sS3 URL missing or empty in command ad and ANNEX_DEFAULT_S3_URL not set or empty in configuration.", errorString.c_str(), errorString.empty() ? "" : "  " );
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

		reply->Assign( "RequestVersion", 1 );
		reply->Assign( ATTR_RESULT, getCAResultString( CA_INVALID_REQUEST ) );
		reply->Assign( ATTR_ERROR_STRING, errorString );

		if( replyStream ) {
			if(! sendCAReply( replyStream, "CA_BULK_REQUEST", reply )) {
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

	// For uploading to S3.
	EC2GahpClient * s3Gahp = startOneGahpClient( publicKeyFile, s3URL );

	//
	// Construct the bulk request, create rule, and add target functors,
	// then register them as a sequence in a timer.  This lets us get back
	// to DaemonCore more quickly and makes the code in operator() more
	// familiar (in terms of the idiomatic usage of the gahp client
	// interface established by the grid manager).
	//

	// Each step in the sequence may add to the reply, and may need to pass
	// information to the next step, so create those two ads first.
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

	std::string annexID;
	command->LookupString( "AnnexID", annexID );
	if( annexID.empty() ) {
		std::string annexName;
		command->LookupString( "AnnexName", annexName );
		if( annexName.empty() ) {
			generateAnnexID( annexID );
		} else {
			annexID = annexName.substr( 0, 27 );
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

		UploadFile * uf = NULL;
		std::string uploadFrom;
		command->LookupString( "UploadFrom", uploadFrom );
		std::string uploadTo;
		command->LookupString( "UploadTo", uploadTo );
		if( (! uploadFrom.empty()) && (! uploadTo.empty()) ) {
			uf = new UploadFile( uploadFrom, uploadTo,
				reply, s3Gahp, scratchpad,
				s3URL, publicKeyFile, secretKeyFile,
				commandState, commandID, annexID );
		}

		// Verify that we're talking to the right collector before we do
		// anything else.
		std::string instanceID, connectivityFunctionARN;
		command->LookupString( "CollectorInstanceID", instanceID );
		param( connectivityFunctionARN, "ANNEX_DEFAULT_CONNECTIVITY_FUNCTION_ARN" );
		CheckConnectivity * cc = new CheckConnectivity(
			connectivityFunctionARN, instanceID,
			reply, lambdaGahp, scratchpad,
			lambdaURL, publicKeyFile, secretKeyFile,
			commandState, commandID );

		PutRule * pr = new PutRule(
			reply, eventsGahp, scratchpad,
			eventsURL, publicKeyFile, secretKeyFile,
			commandState, commandID, annexID );
		PutTargets * pt = new PutTargets( leaseFunctionARN, endOfLease,
			reply, eventsGahp, scratchpad,
			eventsURL, publicKeyFile, secretKeyFile,
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
		FunctorSequence * fs = new FunctorSequence( { cc, gf, uf, pr, pt, br }, last, commandState, commandID, scratchpad );

		// Create a timer for the gahp to fire when it gets a result.  We must
		// use TIMER_NEVER to ensure that the timer hasn't been reaped when the
		// GAHP client needs to fire it.
		int functorSequenceTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
			(void (Service::*)()) & FunctorSequence::operator(),
			"GetFunction, PutRule, PutTarget, BulkRequest", fs );
		gahp->setNotificationTimerId( functorSequenceTimer );
    	eventsGahp->setNotificationTimerId( functorSequenceTimer );
    	lambdaGahp->setNotificationTimerId( functorSequenceTimer );
    	s3Gahp->setNotificationTimerId( functorSequenceTimer );

		return KEEP_STREAM;
	} else if( annexType == "odi" ) {
		// Verify that we're talking to the right collector before we do
		// anything else.
		std::string instanceID, connectivityFunctionARN;
		command->LookupString( "CollectorInstanceID", instanceID );
		param( connectivityFunctionARN, "ANNEX_DEFAULT_CONNECTIVITY_FUNCTION_ARN" );
		CheckConnectivity * cc = new CheckConnectivity(
			connectivityFunctionARN, instanceID,
			reply, lambdaGahp, scratchpad,
			lambdaURL, publicKeyFile, secretKeyFile,
			commandState, commandID );

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

		UploadFile * uf = NULL;
		std::string uploadFrom;
		command->LookupString( "UploadFrom", uploadFrom );
		std::string uploadTo;
		command->LookupString( "UploadTo", uploadTo );
		if( (! uploadFrom.empty()) && (! uploadTo.empty()) ) {
			uf = new UploadFile( uploadFrom, uploadTo,
				reply, s3Gahp, scratchpad,
				s3URL, publicKeyFile, secretKeyFile,
				commandState, commandID, annexID );
		}

		PutRule * pr = new PutRule(
			reply, eventsGahp, scratchpad,
			eventsURL, publicKeyFile, secretKeyFile,
			commandState, commandID, annexID );
		PutTargets * pt = new PutTargets( leaseFunctionARN, endOfLease,
			reply, eventsGahp, scratchpad,
			eventsURL, publicKeyFile, secretKeyFile,
			commandState, commandID, annexID );
		ReplyAndClean * last = new ReplyAndClean( reply, replyStream, gahp, scratchpad, eventsGahp, commandState, commandID, lambdaGahp );

		FunctorSequence * fs;
		if( uf ) {
			fs = new FunctorSequence( { cc, gf, uf, pr, pt, odr }, last, commandState, commandID, scratchpad );
		} else {
			fs = new FunctorSequence( { cc, gf, pr, pt, odr }, last, commandState, commandID, scratchpad );
		}

		int functorSequenceTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
			(void (Service::*)()) & FunctorSequence::operator(),
			"GetFunction, PutRule, PutTarget, OnDemandRequest", fs );
		gahp->setNotificationTimerId( functorSequenceTimer );
    	eventsGahp->setNotificationTimerId( functorSequenceTimer );
    	lambdaGahp->setNotificationTimerId( functorSequenceTimer );
    	s3Gahp->setNotificationTimerId( functorSequenceTimer );

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

std::string commandStateFile;

void
main_config() {
	commandStateFile = param( "ANNEXD_COMMAND_STATE" );
}

// from annex.cpp
bool
createConfigTarball(	const char * configDir,
						const char * annexName,
						const char * owner,
						long unclaimedTimeout,
						std::string & tarballPath,
						std::string & tarballError,
						std::string & instanceID ) {
	char * cwd = get_current_dir_name();

	int rv = chdir( configDir );
	if( rv != 0 ) {
		formatstr( tarballError, "unable to change to config dir '%s' (%d): '%s'",
			configDir, errno, strerror( errno ) );
		return false;
	}

	// Must be readable by the 'condor' user on the instance, but it
	// will be owned by root.
	int fd = safe_open_wrapper_follow( "00ec2-dynamic.config",
		O_WRONLY | O_CREAT | O_TRUNC, 0644 );
	if( fd < 0 ) {
		formatstr( tarballError, "failed to open config file '%s' for writing: '%s' (%d)",
			"00ec2-dynamic.config", strerror( errno ), errno );
		return false;
	}


	std::string passwordFile = "password_file.pl";

	std::string collectorHost;
	param( collectorHost, "COLLECTOR_HOST" );
	if( collectorHost.empty() ) {
		formatstr( tarballError, "COLLECTOR_HOST empty or undefined" );
		return false;
	}

	Daemon collector( DT_COLLECTOR, collectorHost.c_str() );
	if(! collector.locate()) {
		formatstr( tarballError, "unable to find collector defined by COLLECTOR_HOST (%s)", collectorHost.c_str() );
		return false;
	}
	if(! collector.getInstanceID( instanceID )) {
		formatstr( tarballError, "unable to get collector's instance ID" );
		return false;
	}

	std::string startExpression = "START = MayUseAWS == TRUE\n";
	if( owner != NULL ) {
		formatstr( startExpression,
			"START = (MayUseAWS == TRUE) && stringListMember( Owner, \"%s\" )\n"
			"\n",
			owner );
	}

	std::string contents;
	formatstr( contents,
		"use security : host_based\n"
		"LOCAL_HOSTS = $(FULL_HOSTNAME) $(IP_ADDRESS) 127.0.0.1 $(TCP_FORWARDING_HOST)\n"
		"CONDOR_HOST = condor_pool@*/* $(LOCAL_HOSTS)\n"
		"COLLECTOR_HOST = %s\n"
		"\n"
		"SEC_DEFAULT_AUTHENTICATION = REQUIRED\n"
		"SEC_DEFAULT_AUTHENTICATION_METHODS = FS, PASSWORD\n"
		"SEC_DEFAULT_INTEGRITY = REQUIRED\n"
		"SEC_DEFAULT_ENCRYPTION = REQUIRED\n"
		"\n"
		"SEC_PASSWORD_FILE = /etc/condor/config.d/%s\n"
		"ALLOW_WRITE = condor_pool@*/* $(LOCAL_HOSTS)\n"
		"\n"
		"AnnexName = \"%s\"\n"
		"STARTD_ATTRS = $(STARTD_ATTRS) AnnexName\n"
		"MASTER_ATTRS = $(MASTER_ATTRS) AnnexName\n"
		"\n"
		"STARTD_NOCLAIM_SHUTDOWN = %ld\n"
		"\n"
		"%s",
		collectorHost.c_str(), passwordFile.c_str(),
		annexName, unclaimedTimeout, startExpression.c_str()
	);

	rv = write( fd, contents.c_str(), contents.size() );
	if( rv == -1 ) {
		formatstr( tarballError, "error writing to '%s': '%s' (%d)",
			"00ec2-dynamic.config", strerror( errno ), errno );
		return false;
	} else if( rv != (int)contents.size() ) {
		formatstr( tarballError, "short write to '%s': '%s' (%d)",
			"00ec2-dynamic.config", strerror( errno ), errno );
		return false;
	}
	close( fd );


	std::string localPasswordFile;
	param( localPasswordFile, "SEC_PASSWORD_FILE" );
	if( passwordFile.empty() ) {
		formatstr( tarballError, "SEC_PASSWORD_FILE empty or undefined" );
		return false;
	}

	// FIXME: Rewrite without system().
	std::string cpCommand;
	formatstr( cpCommand, "cp '%s' '%s'", localPasswordFile.c_str(), passwordFile.c_str() );
	int status = system( cpCommand.c_str() );
	if(! (WIFEXITED( status ) && (WEXITSTATUS( status ) == 0))) {
		formatstr( tarballError, "failed to copy '%s' to '%s'",
			localPasswordFile.c_str(), passwordFile.c_str() );
		return false;
	}


	std::string tbn;
	formatstr( tbn, "%s/temporary.tar.gz.XXXXXX", cwd );
	char * tarballName = strdup( tbn.c_str() );
	// Some version of mkstemp() create files with 0666, instead of 0600,
	// which would be bad.  I claim without checking that we don't compile
	// on platforms whose version of mkstemp() is broken.
	int tfd = mkstemp( tarballName );
	if( tfd == -1 ) {
		formatstr( tarballError, "failed to create temporary filename for tarball" );
		return false;
	}

	// FIXME: Rewrite without system().
	std::string command;
	formatstr( command, "tar -z -c -f '%s' *", tarballName );
	status = system( command.c_str() );
	if(! (WIFEXITED( status ) && (WEXITSTATUS( status ) == 0))) {
		formatstr( tarballError, "failed to create tarball" );
		return false;
	}
	tarballPath = tarballName;
	free( tarballName );

	rv = chdir( cwd );
	if( rv != 0 ) {
		formatstr( tarballError, "unable to change back to working dir '%s' (%d): '%s'",
			cwd, errno, strerror( errno ) );
		return false;
	}
	close( tfd );
	free( cwd );
	return true;
}

// Why don't c-style timer callbacks have context pointers?
ClassAd * command = NULL;

void
callCreateOneAnnex() {
	Stream * s = NULL;
	ClassAd * reply = new ClassAd();

	// Otherwise, reply-and-clean will take care of user notification.
	if( createOneAnnex( command, s, reply ) != KEEP_STREAM ) {
		std::string resultString;
		reply->LookupString( ATTR_RESULT, resultString );
		CAResult result = getCAResultNum( resultString.c_str() );
		ASSERT( result != CA_SUCCESS );

		std::string errorString;
		reply->LookupString( ATTR_ERROR_STRING, errorString );
		ASSERT(! errorString.empty());
		fprintf( stderr, "%s\n", errorString.c_str() );
		DC_Exit( 6 );
	}
}

void
callUpdateOneAnnex() {
	Stream * s = NULL;
	ClassAd * reply = new ClassAd();

	// Otherwise, reply-and-clean will take care of user notification.
	if( updateOneAnnex( command, s, reply ) != KEEP_STREAM ) {
		std::string resultString;
		reply->LookupString( ATTR_RESULT, resultString );
		CAResult result = getCAResultNum( resultString.c_str() );
		ASSERT( result != CA_SUCCESS );

		std::string errorString;
		reply->LookupString( ATTR_ERROR_STRING, errorString );
		ASSERT(! errorString.empty());
		fprintf( stderr, "%s\n", errorString.c_str() );
		DC_Exit( 6 );
	}
}

// from annex.cpp
// Modelled on BulkRequest::validateAndStore() from annexd/BulkRequest.cpp.
bool
assignUserData( ClassAd & command, const char * ud, bool replace, std::string & validationError ) {
	ExprTree * launchConfigurationsTree = command.Lookup( "LaunchSpecifications" );
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

		std::string userData;
		launchConfiguration.LookupString( "UserData", userData );
		if( userData.empty() || replace ) {
			ca->InsertAttr( "UserData", ud );
		}
	}

	return true;
}

// from annex.cpp
// Modelled on readShortFile() from ec2_gahp/amazonCommands.cpp.
bool
readShortFile( const char * fileName, std::string & contents ) {
    int fd = safe_open_wrapper_follow( fileName, O_RDONLY, 0600 );

    if( fd < 0 ) {
    	fprintf( stderr, "Failed to open file '%s' for reading: '%s' (%d).\n",
            fileName, strerror( errno ), errno );
        return false;
    }

    StatWrapper sw( fd );
    unsigned long fileSize = sw.GetBuf()->st_size;

    char * rawBuffer = (char *)malloc( fileSize + 1 );
    assert( rawBuffer != NULL );
    unsigned long totalRead = full_read( fd, rawBuffer, fileSize );
    close( fd );
    if( totalRead != fileSize ) {
    	fprintf( stderr, "Failed to completely read file '%s'; needed %lu but got %lu.\n",
            fileName, fileSize, totalRead );
        free( rawBuffer );
        return false;
    }
    contents.assign( rawBuffer, fileSize );
    free( rawBuffer );

    return true;
}

void checkOneParameter( const char * pName, int & rv, std::string & pValue ) {
	param( pValue, pName );
	if( pValue.empty() ) {
		if( rv == 0 ) { fprintf( stderr, "Your setup is incomplete:\n" ); }
		fprintf( stderr, "\t%s is unset.\n", pName );
		rv = 1;
	}
}

void
checkOneParameter( const char * pName, int & rv ) {
	std::string pValue;
	checkOneParameter( pName, rv, pValue );
}

int
check_account_setup( const std::string & publicKeyFile, const std::string & privateKeyFile, const std::string & cfURL, const std::string & ec2URL ) {
	ClassAd * reply = new ClassAd();
	ClassAd * scratchpad = new ClassAd();

	std::string commandID;
	generateCommandID( commandID );

	Stream * replyStream = NULL;

	EC2GahpClient * cfGahp = startOneGahpClient( publicKeyFile, cfURL );
	EC2GahpClient * ec2Gahp = startOneGahpClient( publicKeyFile, ec2URL );

	std::string bucketStackName = "HTCondorAnnex-ConfigurationBucket";
	std::string bucketStackDescription = "configuration bucket";
	CheckForStack * bucketCFS = new CheckForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		bucketStackName, bucketStackDescription,
		commandState, commandID );

	std::string lfStackName = "HTCondorAnnex-LambdaFunctions";
	std::string lfStackDescription = "Lambda functions";
	CheckForStack * lfCFS = new CheckForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		lfStackName, lfStackDescription,
		commandState, commandID );

	std::string rStackName = "HTCondorAnnex-InstanceProfile";
	std::string rStackDescription = "instance profile";
	CheckForStack * rCFS = new CheckForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		rStackName, rStackDescription,
		commandState, commandID );

	std::string sgStackName = "HTCondorAnnex-SecurityGroup";
	std::string sgStackDescription = "security group";
	CheckForStack * sgCFS = new CheckForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		sgStackName, sgStackDescription,
		commandState, commandID );

	SetupReply * sr = new SetupReply( reply, cfGahp, ec2Gahp, "Your setup looks OK.\n", scratchpad,
		replyStream, commandState, commandID );

	FunctorSequence * fs = new FunctorSequence(
		{ bucketCFS, lfCFS, rCFS, sgCFS }, sr,
		commandState, commandID, scratchpad );

	int setupTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
		 (void (Service::*)()) & FunctorSequence::operator(),
		 "CheckForStacks", fs );
	cfGahp->setNotificationTimerId( setupTimer );
	ec2Gahp->setNotificationTimerId( setupTimer );

	return 0;
}

int
check_setup() {
	int rv = 0;

	std::string accessKeyFile, secretKeyFile, cfURL, ec2URL;
	checkOneParameter( "ANNEX_DEFAULT_ACCESS_KEY_FILE", rv, accessKeyFile );
	checkOneParameter( "ANNEX_DEFAULT_SECRET_KEY_FILE", rv, secretKeyFile );
	checkOneParameter( "ANNEX_DEFAULT_CF_URL", rv, cfURL );
	checkOneParameter( "ANNEX_DEFAULT_EC2_URL", rv, ec2URL );

	checkOneParameter( "ANNEX_DEFAULT_S3_BUCKET", rv );
	checkOneParameter( "ANNEX_DEFAULT_ODI_SECURITY_GROUP_IDS", rv );
	checkOneParameter( "ANNEX_DEFAULT_ODI_LEASE_FUNCTION_ARN", rv );
	checkOneParameter( "ANNEX_DEFAULT_SFR_LEASE_FUNCTION_ARN", rv );
	checkOneParameter( "ANNEX_DEFAULT_ODI_INSTANCE_PROFILE_ARN", rv );

	if( rv != 0 ) {
		return rv;
	} else {
		return check_account_setup( accessKeyFile, secretKeyFile, cfURL, ec2URL );
	}
}

void
setup_usage() {
	fprintf( stdout,
		"\n"
		"To do the one-time setup for an AWS account:\n"
		"\tcondor_annex -setup\n"
		"\n"
		"To specify the files for the access (public) key and secret (private) keys:\n"
		"\tcondor_annex -setup\n"
		"\t\t<path/to/access-key-file>\n"
		"\t\t<path/to/private-key-file>\n"
		"\n"
		"Expert mode (to specify the region, you must specify the key paths):\n"
		"\tcondor_annex -aws-ec2-url https://ec2.<region>.amazonaws.com\n"
		"\t\t-setup <path/to/access-key-file>\n"
		"\t\t<path/to/private-key-file>\n"
		"\t\t<https://cloudformation.<region>.amazonaws.com/>\n"
		"\n"
	);
}

int
setup( const char * pukf, const char * prkf, const char * cloudFormationURL, const char * serviceURL ) {
	std::string publicKeyFile, privateKeyFile;
	if( pukf != NULL && prkf != NULL ) {
		publicKeyFile = pukf;
		privateKeyFile = prkf;
	} else {
		std::string userDirectory;
		if(! createUserConfigDir( userDirectory )) {
			fprintf( stderr, "You must therefore specify the public and private key files on the command-line.\n" );
			setup_usage();
			return 1;
		} else {
			publicKeyFile = userDirectory + "/publicKeyFile";
			privateKeyFile = userDirectory + "/privateKeyFile";
		}
	}

	int fd = safe_open_no_create_follow( publicKeyFile.c_str(), O_RDONLY );
	if( fd == -1 ) {
		fprintf( stderr, "Unable to open public key file '%s': '%s' (%d).\n",
			publicKeyFile.c_str(), strerror( errno ), errno );
		setup_usage();
		return 1;
	}
	close( fd );

	fd = safe_open_no_create_follow( privateKeyFile.c_str(), O_RDONLY );
	if( fd == -1 ) {
		fprintf( stderr, "Unable to open private key file '%s': '%s' (%d).\n",
			privateKeyFile.c_str(), strerror( errno ), errno );
		setup_usage();
		return 1;
	}
	close( fd );


	std::string cfURL = cloudFormationURL ? cloudFormationURL : "";
	if( cfURL.empty() ) {
		// FIXME: At some point, the argument to 'setup' should be the region,
		// not the CloudFormation URL.
		param( cfURL, "ANNEX_DEFAULT_CF_URL" );
	}
	if( cfURL.empty() ) {
		fprintf( stderr, "No CloudFormation URL specified on command-line and ANNEX_DEFAULT_CF_URL is not set or empty in configuration.\n" );
		return 1;
	}

	std::string ec2URL = serviceURL ? serviceURL : "";
	if( ec2URL.empty() ) {
		param( ec2URL, "ANNEX_DEFAULT_EC2_URL" );
	}
	if( ec2URL.empty() ) {
		fprintf( stderr, "No EC2 URL specified on command-line and ANNEX_DEFAULT_EC2_URL is not set or empty in configuration.\n" );
		return 1;
	}

	ClassAd * reply = new ClassAd();
	ClassAd * scratchpad = new ClassAd();

	// This 100% redundant, but it moves all our config-file generation
	// code to the same place, so it's worth it.
	scratchpad->Assign( "AccessKeyFile", publicKeyFile );
	scratchpad->Assign( "SecretKeyFile", privateKeyFile );

	std::string commandID;
	generateCommandID( commandID );

	Stream * replyStream = NULL;

	EC2GahpClient * cfGahp = startOneGahpClient( publicKeyFile, cfURL );
	EC2GahpClient * ec2Gahp = startOneGahpClient( publicKeyFile, ec2URL );

	// FIXME: Do something cleverer for versioning.
	std::string bucketStackURL = "https://s3.amazonaws.com/condor-annex/bucket-7.json";
	std::string bucketStackName = "HTCondorAnnex-ConfigurationBucket";
	std::string bucketStackDescription = "configuration bucket (this takes less than a minute)";
	std::map< std::string, std::string > bucketParameters;
	CreateStack * bucketCS = new CreateStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		bucketStackName, bucketStackURL, bucketParameters,
		commandState, commandID );
	WaitForStack * bucketWFS = new WaitForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		bucketStackName, bucketStackDescription,
		commandState, commandID );

	// FIXME: Do something cleverer for versioning.
	std::string lfStackURL = "https://s3.amazonaws.com/condor-annex/template-7.json";
	std::string lfStackName = "HTCondorAnnex-LambdaFunctions";
	std::string lfStackDescription = "Lambda functions (this takes about a minute)";
	std::map< std::string, std::string > lfParameters;
	lfParameters[ "S3BucketName" ] = "<scratchpad>";
	CreateStack * lfCS = new CreateStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		lfStackName, lfStackURL, lfParameters,
		commandState, commandID );
	WaitForStack * lfWFS = new WaitForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		lfStackName, lfStackDescription,
		commandState, commandID );

	// FIXME: Do something cleverer for versioning.
	std::string rStackURL = "https://s3.amazonaws.com/condor-annex/role-7.json";
	std::string rStackName = "HTCondorAnnex-InstanceProfile";
	std::string rStackDescription = "instance profile (this takes about two minutes)";
	std::map< std::string, std::string > rParameters;
	rParameters[ "S3BucketName" ] = "<scratchpad>";
	CreateStack * rCS = new CreateStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		rStackName, rStackURL, rParameters,
		commandState, commandID );
	WaitForStack * rWFS = new WaitForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		rStackName, rStackDescription,
		commandState, commandID );

	// FIXME: Do something cleverer for versioning.
	std::string sgStackURL = "https://s3.amazonaws.com/condor-annex/security-group-7.json";
	std::string sgStackName = "HTCondorAnnex-SecurityGroup";
	std::string sgStackDescription = "security group (this takes less than a minute)";
	std::map< std::string, std::string > sgParameters;
	CreateStack * sgCS = new CreateStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		sgStackName, sgStackURL, sgParameters,
		commandState, commandID );
	WaitForStack * sgWFS = new WaitForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		sgStackName, sgStackDescription,
		commandState, commandID );

	CreateKeyPair * ckp = new CreateKeyPair( reply, ec2Gahp, scratchpad,
		ec2URL, publicKeyFile, privateKeyFile,
		commandState, commandID );

	GenerateConfigFile * gcf = new GenerateConfigFile( cfGahp, scratchpad );

	SetupReply * sr = new SetupReply( reply, cfGahp, ec2Gahp, "Setup successful.\n", scratchpad,
		replyStream, commandState, commandID );

	FunctorSequence * fs = new FunctorSequence(
		{ bucketCS, bucketWFS, lfCS, lfWFS, rCS, rWFS, sgCS, sgWFS, ckp, gcf }, sr,
		commandState, commandID, scratchpad );


	int setupTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
		 (void (Service::*)()) & FunctorSequence::operator(),
		 "CreateStack, DescribeStacks, WriteConfigFile", fs );
	cfGahp->setNotificationTimerId( setupTimer );
	ec2Gahp->setNotificationTimerId( setupTimer );

	return 0;
}


// from annex.cpp
void
help( const char * argv0 ) {
	fprintf( stdout, "usage: %s -annex-name <annex-name> -count|-slots <number>\n"
		"For on-demand instances:\n"
		"\t[-aws-on-demand]\n"
		"\t-count <integer number of instances>\n"
		"\n"
		"For spot fleet instances:\n"
		"\t[-aws-spot-fleet]\n"
		"\t-slots <integer number of weighted capacity units>\n"
		"\n"
		"To set the lease or idle durations:\n"
		"\t[-duration <lease duration in decimal hours>]\n"
		"\t[-idle <idle duration in decimal hours>]\n"
		"\n"
		"To customize the annex's HTCondor configuration:\n"
		"\t[-config-dir </full/path/to/config.d>]\n"
		"\n"
		"To set the instances' user data:\n"
		"\t[-aws-[default-]user-data[-file] <data|path/to/file> ]\n"
		"\n"
		"To customize an AWS on-demand annex:\n"
		"\t[-aws-on-demand-instance-type <instance-type>]\n"
		"\t[-aws-on-demand-ami-id <AMI-ID>\n"
		"\t[-aws-on-demand-security-group-ids <group-ID[,groupID]*>\n"
		"\t[-aws-on-demand-key-name <key-name>]\n"
		"\n"
		"To use a customize an AWS Spot Fleet annex, specify a JSON config file:\n"
		"\t[-aws-spot-fleet-config-file </full/path/to/config.json>]\n"
		"\n"
		"To specify who may use your annex:\n"
		"\t[-owner <username[, username]*>\n"
		"To specify that anyone may use your annex:\n"
		"\t[-no-owner]\n"
		"\n"
		"To reprint this help:\n"
		"\t[-help]\n"
		"\n"
		"Expert mode (specify account and region):\n"
		"\t[-aws-access-key-file <path/to/access-key-file>]\n"
		"\t[-aws-secret-key-file <path/to/secret-key-file>]\n"
		"\t[-aws-ec2-url https://ec2.<region>.amazonaws.com]\n"
		"\t[-aws-events-url https://events.<region>.amazonaws.com]\n"
		"\t[-aws-lambda-url https://lambda.<region>.amazonaws.com]\n"
		"\t[-aws-s3-url https://s3.<region>.amazonaws.com]\n"
		"\n"
		"Expert mode (specify implementation details):\n"
		"\t[-aws-spot-fleet-lease-function-arn <sfr-lease-function-arn>]\n"
		"\t[-aws-on-demand-lease-function-arn <odi-lease-function-arn>]\n"
		"\t[-aws-on-demand-instance-profile-arn <instance-profile-arn>]\n"
		"\n"
		"OR, to do the one-time setup for an AWS account:\n"
		"%s -setup [<path/to/access-key-file> <path/to/secret-key-file> [<CloudFormation URL>]]\n"
		"\n"
		, argv0, argv0 );
}

int _argc;
char ** _argv;

// from annex.cpp
int
annex_main( int argc, char ** argv ) {
	bool clUserDataWins = true;
	std::string userData;
	const char * userDataFileName = NULL;

argc = _argc;
argv = _argv;

	int udSpecifications = 0;
	const char * sfrConfigFile = NULL;
	const char * annexName = NULL;
	const char * configDir = NULL;
	const char * serviceURL = NULL;
	const char * eventsURL = NULL;
	const char * lambdaURL = NULL;
	const char * s3URL = NULL;
	const char * publicKeyFile = NULL;
	const char * secretKeyFile = NULL;
	const char * sfrLeaseFunctionARN = NULL;
	const char * odiLeaseFunctionARN = NULL;
	const char * odiInstanceType = NULL;
	bool odiInstanceTypeSpecified = false;
	const char * odiImageID = NULL;
	const char * odiInstanceProfileARN = NULL;
	const char * odiS3ConfigPath = NULL;
	const char * odiKeyName = NULL;
	const char * odiSecurityGroupIDs = NULL;
	bool annexTypeIsSFR = false;
	bool annexTypeIsODI = false;
	long int unclaimedTimeout = 0;
	bool unclaimedTimeoutSpecified = false;
	long int leaseDuration = 0;
	bool leaseDurationSpecified = false;
	long int count = 0;
	char * owner = NULL;
	bool ownerSpecified = false;
	bool noOwner = false;
	for( int i = 1; i < argc; ++i ) {
		if( is_dash_arg_prefix( argv[i], "aws-ec2-url", 11 ) ) {
			++i;
			if( argv[i] != NULL ) {
				serviceURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-ec2-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-events-url", 14 ) ) {
			++i;
			if( argv[i] != NULL ) {
				eventsURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-events-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-lambda-url", 14 ) ) {
			++i;
			if( argv[i] != NULL ) {
				lambdaURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-lambda-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-s3-url", 10 ) ) {
			++i;
			if( argv[i] != NULL ) {
				s3URL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-s3-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-user-data-file", 14 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				userDataFileName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-user-data-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-user-data", 13 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				userData = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-user-data requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-default-user-data-file", 22 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				clUserDataWins = false;
				userDataFileName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-user-data-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-default-user-data", 21 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				clUserDataWins = false;
				userData = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-user-data requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "annex-name", 6 ) ) {
			++i;
			if( argv[i] != NULL ) {
				annexName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -annex-name requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "config-dir", 10 ) ) {
			++i;
			if( argv[i] != NULL ) {
				configDir = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -config-dir requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "owner", 5 ) ) {
			++i;
			if( argv[i] != NULL ) {
				owner = argv[i];
				ownerSpecified = true;
				continue;
			} else {
				fprintf( stderr, "%s: -owner requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "no-owner", 7 ) ) {
			++i;
			noOwner = true;
		} else if( is_dash_arg_prefix( argv[i], "aws-spot-fleet-config-file", 22 ) ) {
			++i;
			if( argv[i] != NULL ) {
				sfrConfigFile = argv[i];
				annexTypeIsSFR = true;
				continue;
			} else {
				fprintf( stderr, "%s: -aws-spot-fleet-config-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-public-key-file", 19 ) ||
				   is_dash_arg_prefix( argv[i], "aws-access-key-file", 19 ) ) {
			++i;
			if( argv[i] != NULL ) {
				publicKeyFile = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-access-key-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-secret-key-file", 19 ) ||
				   is_dash_arg_prefix( argv[i], "aws-private-key-file", 20 ) ) {
			++i;
			if( argv[i] != NULL ) {
				secretKeyFile = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-secret-key-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-spot-fleet-lease-function-arn", 22 ) ) {
			++i;
			if( argv[i] != NULL ) {
				sfrLeaseFunctionARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-spot-fleet-lease-function-arn requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand-lease-function-arn", 21 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiLeaseFunctionARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-on-demand-lease-function-arn requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand-instance-type", 24 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiInstanceType = argv[i];
				odiInstanceTypeSpecified = true;
				continue;
			} else {
				fprintf( stderr, "%s: -aws-on-demand-instance-type requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand-key-name", 17 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiKeyName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-on-demand-key-name requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand-security-group-ids", 31 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiSecurityGroupIDs = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-on-demand-security-group-ids requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand-ami-id", 20 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiImageID = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-on-demand-ami-id requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand-instance-profile-arn", 22 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiInstanceProfileARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-on-demand-instance-profile-arn requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-s3-config-path", 18 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiS3ConfigPath = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-s3-config-path requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "duration", 8 ) ) {
			++i;
			if( argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ld = argv[i];
				double fractionHours = strtod( ld, & endptr );
				leaseDuration = fractionHours * 60 * 60;
				leaseDurationSpecified = true;
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -duration accepts a decimal number of hours.\n", argv[0] );
					return 1;
				}
				if( leaseDuration <= 0 ) {
					fprintf( stderr, "%s: the duration must be greater than zero.\n", argv[0] );
					return 1;
				}
				continue;
			} else {
				fprintf( stderr, "%s: -duration requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "idle", 4 ) ) {
			++i;
			if( argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ut = argv[i];
				double fractionalHours = strtod( ut, & endptr );
				unclaimedTimeout = fractionalHours * 60 * 60;
				unclaimedTimeoutSpecified = true;
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -idle accepts a decimal number of hours.\n", argv[0] );
					return 1;
				}
				if( unclaimedTimeout <= 0 ) {
					fprintf( stderr, "%s: the idle time must be greater than zero.\n", argv[0] );
					return 1;
				}
				continue;
			} else {
				fprintf( stderr, "%s: -idle requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "slots", 5 ) ) {
			++i;
			if( argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ld = argv[i];
				count = strtol( ld, & endptr, 0 );
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -slots requires an integer argument.\n", argv[0] );
					return 1;
				}
				if( count <= 0 ) {
					fprintf( stderr, "%s: the number of requested slots must be greater than zero.\n", argv[0] );
					return 1;
				}
				annexTypeIsSFR = true;
				continue;
			} else {
				fprintf( stderr, "%s: -slots requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "count", 5 ) ) {
			++i;
			if( argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ld = argv[i];
				count = strtol( ld, & endptr, 0 );
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -count requires an integer argument.\n", argv[0] );
					return 1;
				}
				if( count <= 0 ) {
					fprintf( stderr, "%s: the number of requested instances must be greater than zero.\n", argv[0] );
					return 1;
				}
				annexTypeIsODI = true;
				continue;
			} else {
				fprintf( stderr, "%s: -count requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-spot-fleet", 14 ) ) {
			annexTypeIsSFR = true;
			continue;
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand", 13 ) ) {
			annexTypeIsODI = true;
			continue;
		} else if( is_dash_arg_prefix( argv[i], "help", 1 ) ) {
			help( argv[0] );
			return 1;
		} else if( is_dash_arg_prefix( argv[i], "check-setup", 11 ) ) {
			return check_setup();
		} else if( is_dash_arg_prefix( argv[i], "setup", 5 ) ) {
			// FIXME: At some point, we should accept a region flag instead
			// of requiring the user to specify two different URLs.
			return setup(	argc >= 2 ? argv[2] : NULL,
							argc >= 3 ? argv[3] : NULL,
							argc >= 4 ? argv[4] : NULL,
							serviceURL );
		} else if( argv[i][0] == '-' && argv[i][1] != '\0' ) {
			fprintf( stderr, "%s: unrecognized option (%s).\n", argv[0], argv[i] );
			return 1;
		}
	}

	if( udSpecifications > 1 ) {
		fprintf( stderr, "%s: you may specify no more than one of -aws-[default-]user-data[-file].\n", argv[0] );
		return 1;
	}

	if( annexName == NULL ) {
		fprintf( stderr, "%s: you must specify -annex-name.\n", argv[0] );
		return 1;
	}

	if( count == 0 ) {
		fprintf( stderr, "%s: you must specify (a positive) -count.\n", argv[0] );
		return 1;
	}

	if( leaseDuration == 0 ) {
		leaseDuration = param_integer( "ANNEX_DEFAULT_LEASE_DURATION", 3000 );
	}

	if( unclaimedTimeout == 0 ) {
		unclaimedTimeout = param_integer( "ANNEX_DEFAULT_UNCLAIMED_TIMEOUT", 900 );
	}

	if( annexTypeIsSFR && annexTypeIsODI ) {
		fprintf( stderr, "An annex may not be both on-demand and spot fleet.  "
			"Specifying -count implies -aws-on-demand; specifying -slots "
			"implies -aws-spot-fleet.\n" );
		return 1;
	}
	if(! (annexTypeIsSFR || annexTypeIsODI )) {
		annexTypeIsODI = true;
	}

	if( annexTypeIsSFR && sfrConfigFile == NULL ) {
		sfrConfigFile = param( "ANNEX_DEFAULT_SFR_CONFIG_FILE" );
		if( sfrConfigFile == NULL ) {
			fprintf( stderr, "Spot Fleet Requests require the -aws-spot-fleet-config-file flag.\n" );
			return 1;
		}
	}
	if( sfrConfigFile != NULL && ! annexTypeIsSFR ) {
		fprintf( stderr, "Unwilling to ignore -spot-fleet-request-config-file flag being set for an on-demand annex.\n" );
		return 1;
	}


	//
	// Construct enough of the command ad that we can ask the user if what
	// we're doing is OK.
	//

	ClassAd spotFleetRequest;
	if( sfrConfigFile != NULL ) {
		FILE * file = NULL;
		bool closeFile = true;
		if( strcmp( sfrConfigFile, "-" ) == 0 ) {
			file = stdin;
			closeFile = false;
		} else {
			file = safe_fopen_wrapper_follow( sfrConfigFile, "r" );
			if( file == NULL ) {
				fprintf( stderr, "Unable to open annex specification file '%s'.\n", sfrConfigFile );
				return 1;
			}
		}

		CondorClassAdFileIterator ccafi;
		if(! ccafi.begin( file, closeFile, CondorClassAdFileParseHelper::Parse_json )) {
			fprintf( stderr, "Failed to start parsing spot fleet request.\n" );
			return 2;
		} else {
			int numAttrs = ccafi.next( spotFleetRequest );
			if( numAttrs <= 0 ) {
				fprintf( stderr, "Failed to parse spot fleet request, found no attributes.\n" );
				return 2;
			} else if( numAttrs > 11 ) {
				fprintf( stderr, "Failed to parse spot fleet reqeust, found too many attributes.\n" );
				return 2;
			}
		}
	} else {
		if(! odiInstanceType) {
			odiInstanceType = param( "ANNEX_DEFAULT_ODI_INSTANCE_TYPE" );
		}
		if( odiInstanceType ) {
			spotFleetRequest.Assign( "InstanceType", odiInstanceType );
		}

		if(! odiImageID) {
			odiImageID = param( "ANNEX_DEFAULT_ODI_IMAGE_ID" );
		}
		if( odiImageID ) {
			spotFleetRequest.Assign( "ImageID", odiImageID );
		}

		if(! odiInstanceProfileARN) {
			odiInstanceProfileARN = param( "ANNEX_DEFAULT_ODI_INSTANCE_PROFILE_ARN" );
		}
		if( odiInstanceProfileARN ) {
			spotFleetRequest.Assign( "InstanceProfileARN", odiInstanceProfileARN );
		}

		if(! odiKeyName) {
			odiKeyName = param( "ANNEX_DEFAULT_ODI_KEY_NAME" );
		}
		if( odiKeyName ) {
			spotFleetRequest.Assign( "KeyName", odiKeyName );
		}

		if(! odiSecurityGroupIDs) {
			odiSecurityGroupIDs = param( "ANNEX_DEFAULT_ODI_SECURITY_GROUP_IDS" );
		}
		if( odiSecurityGroupIDs ) {
			spotFleetRequest.Assign( "SecurityGroupIDs", odiSecurityGroupIDs );
		}
	}

	if( annexTypeIsSFR ) {
		spotFleetRequest.Assign( "AnnexType", "sfr" );
		if(! sfrLeaseFunctionARN) {
			sfrLeaseFunctionARN = param( "ANNEX_DEFAULT_SFR_LEASE_FUNCTION_ARN" );
		}
		spotFleetRequest.Assign( "LeaseFunctionARN", sfrLeaseFunctionARN );
	}
	if( annexTypeIsODI ) {
		spotFleetRequest.Assign( "AnnexType", "odi" );
		if(! odiLeaseFunctionARN) {
			odiLeaseFunctionARN = param( "ANNEX_DEFAULT_ODI_LEASE_FUNCTION_ARN" );
		}
		spotFleetRequest.Assign( "LeaseFunctionARN", odiLeaseFunctionARN );
	}

	spotFleetRequest.Assign( "AnnexName", annexName );
	spotFleetRequest.Assign( "TargetCapacity", count );
	time_t now = time( NULL );
	spotFleetRequest.Assign( "EndOfLease", now + leaseDuration );


	//
	// Ask the user if what we're doing is OK.  If it isn't, tell the
	// user how to change it.
	//

	if( annexTypeIsODI ) {
		fprintf( stdout,
			"Will request %ld %s on-demand instance%s for %.2f hours.  "
			"Each instance will terminate after being idle for %.2f hours.\n",
			count, odiInstanceType, count == 1 ? "" : "s",
			leaseDuration / 3600.0, unclaimedTimeout / 3600.0
		);
	}

	if( annexTypeIsSFR ) {
		fprintf( stdout,
			"Will request %ld spot instance%s for %.2f hours.  "
			"Each instance will terminate after being idle for %.2f hours.\n",
			count,
			count == 1 ? "" : "s",
			leaseDuration / 3600.0,
			unclaimedTimeout / 3600.0
		);
	}

	std::string response;
	fprintf( stdout, "Is that OK?  (Type 'yes' or 'no'): " );
	std::getline( std::cin, response );

	if( strcasecmp( response.c_str(), "yes" ) != 0 ) {
		if( annexTypeIsODI ) {
			if(! odiInstanceTypeSpecified) {
				fprintf( stdout, "To change the instance type, use the -aws-on-demand-instance-type flag.\n" );
			}
		}

		if(! leaseDurationSpecified) {
			fprintf( stdout, "To change the lease duration, use the -duration flag.\n" );
		}
		if(! unclaimedTimeoutSpecified) {
			fprintf( stdout, "To change how long an idle instance will wait before terminating itself, use the -idle flag.\n" );
		}

		return 1;
	}


	//
	// Build the (dynamic) configuration tarball.
	//

	std::string tarballTarget;
	param( tarballTarget, "ANNEX_DEFAULT_S3_BUCKET" );
	if( odiS3ConfigPath != NULL ) {
		tarballTarget = odiS3ConfigPath;
	} else {
		formatstr( tarballTarget, "%s/config-%s.tar.gz",
			tarballTarget.c_str(), annexName );
	}
	if( tarballTarget.empty() ) {
		fprintf( stderr, "If you don't specify -aws-s3-config-path, ANNEX_DEFAULT_S3_BUCKET must be set in configuration.\n" );
		return 1;
	}
	spotFleetRequest.Assign( "UploadTo", tarballTarget );

	// Create a temporary directory.  If a config directory was specified,
	// copy its contents into the temporary directory.  Create (or copy)
	// the dynamic configuration files into the temporary directory.  Create
	// a tarball of the temporary directory's contents in the cwd.  Delete
	// the temporary directory (and all of its contents).  While the tool
	// and the daemon are joined at the hip, just delete the tarball after
	// uploading it; otherwise, we'll probably do file transfer and have
	// the tool and the daemon clean up after themselves.
	char dirNameTemplate[] = ".condor_annex.XXXXXX";
	const char * tempDir = mkdtemp( dirNameTemplate );
	if( tempDir == NULL ) {
		fprintf( stderr, "Failed to create temporary directory (%d): '%s'.\n", errno, strerror( errno ) );
		return 2;
	}

	// FIXME: Rewrite without system().
	if( configDir != NULL ) {
		std::string command;
		formatstr( command, "cp -a '%s'/* '%s'", configDir, tempDir );
		int status = system( command.c_str() );
		if(! (WIFEXITED( status ) && (WEXITSTATUS( status ) == 0))) {
			fprintf( stderr, "Failed to copy '%s' to '%s', aborting.\n",
				configDir, tempDir );
			return 2;
		}
	}

	//
	// A job's Owner attribute is almost aways set by condor_submit to be
	// the return of my_username(), and this value is checked by the schedd.
	//
	// However, if the user submitted their jobs with +Owner = undefined,
	// then what the mapfile says goes.  Rather than have this variable
	// be configurable (because we may end up where condor_annex doesn't
	// have per-user configuration), just require users in this rare
	// situation to run condor_ping -type schedd WRITE and use the
	// answer in a config directory (or command-line argument, if that
	// becomes a thing) for setting the START expression.
	//

	std::string tarballPath, tarballError, instanceID;
	if(! ownerSpecified) { owner = my_username(); }
	bool createdTarball = createConfigTarball( tempDir, annexName,
		noOwner ? NULL : owner, unclaimedTimeout, tarballPath, tarballError,
		instanceID );
	if(! ownerSpecified) { free( owner ); }
	spotFleetRequest.Assign( "CollectorInstanceID", instanceID );

	// FIXME: Rewrite without system().
	std::string cmd;
	formatstr( cmd, "rm -fr '%s'", tempDir );
	int status = system( cmd.c_str() );
	if(! (WIFEXITED( status ) && (WEXITSTATUS( status ) == 0))) {
		fprintf( stderr, "Failed to delete temporary directory '%s'.\n",
			tempDir );
		return 2;
	}

	if(! createdTarball) {
		fprintf( stderr, "Failed to create config tarball (%s); aborting.\n", tarballError.c_str() );
		return 3;
	}

	spotFleetRequest.Assign( "UploadFrom", tarballPath );

	//
	// Finish constructing the spot fleet request.
	//

	if( serviceURL != NULL ) {
		spotFleetRequest.Assign( "ServiceURL", serviceURL );
	}

	if( eventsURL != NULL ) {
		spotFleetRequest.Assign( "EventsURL", eventsURL );
	}

	if( lambdaURL != NULL ) {
		spotFleetRequest.Assign( "LambdaURL", lambdaURL );
	}

	if( s3URL != NULL ) {
		spotFleetRequest.Assign( "S3URL", s3URL );
	}

	if( publicKeyFile != NULL ) {
		spotFleetRequest.Assign( "PublicKeyFile", publicKeyFile );
	}

	if( secretKeyFile != NULL ) {
		spotFleetRequest.Assign( "SecretKeyFile", secretKeyFile );
	}

	// Handle user data.
	if( userDataFileName != NULL ) {
		if(! readShortFile( userDataFileName, userData ) ) {
			return 4;
		}
	}

	// condor_base64_encode() segfaults on the empty string.
	if(! userData.empty()) {
		std::string validationError;
		char * base64Encoded = condor_base64_encode( (const unsigned char *)userData.c_str(), userData.length() );
		if(! assignUserData( spotFleetRequest, base64Encoded, clUserDataWins, validationError )) {
			fprintf( stderr, "Failed to set user data in request ad (%s).\n", validationError.c_str() );
			return 5;
		}
		free( base64Encoded );
	}


	// -------------------------------------------------------------------------
	command = new ClassAd( spotFleetRequest );
	command->Assign( ATTR_COMMAND, getCommandString( CA_BULK_REQUEST ) );
	command->Assign( "RequestVersion", 1 );

	command->Assign( "CommandID", (const char *)NULL );
    command->Assign( "AnnexID", (const char *)NULL );

	daemonCore->Register_Timer( 0, TIMER_NEVER,
		& callCreateOneAnnex, "callCreateOneAnnex()" );

	return 0;
}

void
main_init( int argc, char ** argv ) {
	// Make sure that, e.g., updateInterval is set.
	main_config();

	commandState = new ClassAdCollection( NULL, commandStateFile.c_str() );

	int rv = annex_main( argc, argv );
	if( rv != 0 ) { DC_Exit( rv ); }
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

// This is dumb, but easier than fighting daemon core about parsing.
_argc = argc;
_argv = (char **)malloc( argc * sizeof( char * ) );
for( int i = 0; i < argc; ++i ) {
	_argv[i] = strdup( argv[i] );
}

// This is also dumb, but less dangerous than (a) reaching into daemon
// core to set a flag and (b) hoping that my command-line arguments and
// its command-line arguments don't conflict.
char ** dcArgv = (char **)malloc( 4 * sizeof( char * ) );
dcArgv[0] = argv[0];
// Force daemon core to run in the foreground.
dcArgv[1] = strdup( "-f" );
// Disable the daemon core command socket.
dcArgv[2] = strdup( "-p" );
dcArgv[3] = strdup(  "0" );
argc = 4;
argv = dcArgv;

	dc_main_init = & main_init;
	dc_main_config = & main_config;
	dc_main_shutdown_fast = & main_shutdown_fast;
	dc_main_shutdown_graceful = & main_shutdown_graceful;
	dc_main_pre_dc_init = & main_pre_dc_init;
	dc_main_pre_command_sock_init = & main_pre_command_sock_init;

	return dc_main( argc, argv );
}
