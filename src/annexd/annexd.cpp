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

// from annex.cpp
#include "condor_common.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "match_prefix.h"
#include "daemon.h"
#include "dc_annexd.h"
#include "stat_wrapper.h"
#include "condor_base64.h"

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

	std::string serviceURL, eventsURL, lambdaURL, s3URL;

	param( serviceURL, "ANNEX_DEFAULT_EC2_URL" );
	// FIXME: look up service URL from authorized user map.
	command->LookupString( "ServiceURL", serviceURL );

	param( eventsURL, "ANNEX_DEFAULT_CWE_URL" );
	// FIXME: look up events URL from authorized user map.
	command->LookupString( "EventsURL", eventsURL );

	param( lambdaURL, "ANNEX_DEFAULT_LAMBDA_URL" );
	// FIXME: look up lambda URL from authorized user map.
	command->LookupString( "LambdaURL", lambdaURL );

	param( s3URL, "ANNEX_DEFAULT_S3_URL" );
	// FIXME: look up lambda URL from authorized user map.
	command->LookupString( "S3URL", s3URL );

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
			fs = new FunctorSequence( { gf, uf, pr, pt, odr }, last, commandState, commandID, scratchpad );
		} else {
			fs = new FunctorSequence( { gf, pr, pt, odr }, last, commandState, commandID, scratchpad );
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
void
// FIXME: The exit()s in here should probably be returns so that clean-up
// can be done, and this function has its own clean-up to do as well.
createConfigTarball(	const char * configDir,
						const char * annexName,
						std::string & tarballPath ) {
	char * cwd = get_current_dir_name();

	int rv = chdir( configDir );
	if( rv != 0 ) {
		fprintf( stderr, "Unable to change to config dir '%s' (%d): '%s'.\n",
			configDir, errno, strerror( errno ) );
		exit( 4 );
	}

	int fd = safe_open_wrapper_follow( "99ec2-dynamic.config",
		O_WRONLY | O_CREAT | O_TRUNC, 0600 );
	if( fd < 0 ) {
		fprintf( stderr, "Failed to open config file '%s' for writing: '%s' (%d).\n",
			"99ec2-dynamic.config", strerror( errno ), errno );
		exit( 4 );
	}


	std::string passwordFile = "password_file.pl";

	std::string collectorHost;
	param( collectorHost, "COLLECTOR_HOST" );
	if( collectorHost.empty() ) {
		fprintf( stderr, "COLLECTOR_HOST empty or undefined, aborting.\n" );
		exit( 5 );
	} else {
		fprintf( stdout, "Using COLLECTOR_HOST '%s'\n", collectorHost.c_str() );
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
		"\n"
		"SEC_PASSWORD_FILE = /etc/condor/config.d/%s\n"
		"ALLOW_WRITE = condor_pool@*/* $(LOCAL_HOSTS)\n"
		"\n"
		"AnnexName = \"%s\"\n"
		"STARTD_ATTRS = $(STARTD_ATTRS) AnnexName\n"
		"MASTER_ATTRS = $(MASTER_ATTRS) AnnexName\n"
		"\n",
		collectorHost.c_str(), passwordFile.c_str(), annexName );

	rv = write( fd, contents.c_str(), contents.size() );
	if( rv == -1 ) {
		fprintf( stderr, "Error writing to '%s': '%s' (%d).\n",
			"99ec2-dynamic.config", strerror( errno ), errno );
	} else if( rv != (int)contents.size() ) {
		fprintf( stderr, "Short write to '%s': '%s' (%d).\n",
			"99ec2-dynamic.config", strerror( errno ), errno );
		exit( 4 );
	}
	close( fd );


	std::string localPasswordFile;
	param( localPasswordFile, "SEC_PASSWORD_FILE" );
	if( passwordFile.empty() ) {
		fprintf( stderr, "SEC_PASSWORD_FILE empty or undefined, aborting.\n" );
		exit( 5 );
	} else {
		fprintf( stdout, "Using SEC_PASSWORD_FILE '%s'\n", localPasswordFile.c_str() );
	}

	// FIXME: Rewrite without system().
	std::string cpCommand;
	formatstr( cpCommand, "cp '%s' '%s'", localPasswordFile.c_str(), passwordFile.c_str() );
	int status = system( cpCommand.c_str() );
	if(! (WIFEXITED( status ) && (WEXITSTATUS( status ) == 0))) {
		fprintf( stderr, "Failed to copy '%s' to '%s', aborting.\n",
			localPasswordFile.c_str(), passwordFile.c_str() );
		exit( 5 );
	}


	// ...
	status = system( "tar --exclude config.tar.gz -z -c -f config.tar.gz *" );
	if(! (WIFEXITED( status ) && (WEXITSTATUS( status ) == 0))) {
		fprintf( stderr, "Failed to create tarball, aborting.\n" );
		exit( 5 );
	}


	rv = chdir( cwd );
	if( rv != 0 ) {
		fprintf( stderr, "Unable to change back to working dir '%s' (%d): '%s'.\n",
			cwd, errno, strerror( errno ) );
		exit( 4 );
	}
	free( cwd );

	formatstr( tarballPath, "%s/%s", configDir, "config.tar.gz" );
}

// Why don't c-style timer callbacks have context pointers?
ClassAd * command = NULL;

void
callCreateOneAnnex() {
	Stream * s = NULL;
	createOneAnnex( command, s );
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

// from annex.cpp
void
help( const char * argv0 ) {
	fprintf( stdout, "usage: %s\n"
		"\t-annex-name <annex-name>\n"
		"\t-duration <lease-duration>\n"
		"\t-slots <desired-capacity> | -count <desired-instances>\n"
		"\t[-odi | -sfr]\n"
		"\t[-access-key-file <access-key-file>]\n"
		"\t[-secret-key-file <secret-key-file>]\n"
		"\t[-config-dir <path>]\n"
/*		"\t[-pool <pool>] [-name <name>]\n"
*/		"\t[-service-url <service-url>]\n"
		"\t[-events-url <events-url>]\n"
		"\t[-lambda-url <lambda-url>]\n"
		"\t[-s3-url <lambda-url>]\n"
		"\t[-sfr-lease-function-arn <sfr-lease-function-arn>]\n"
		"\t[-odi-lease-function-arn <odi-lease-function-arn>]\n"
		"\t[-[default-]user-data[-file] <data|file> ]\n"
		"\t[-debug] [-help]\n"
		"\t[-sfr-config-file <spot-fleet-configuration-file>]\n"
		"\t[-odi-instance-type <instance-type>]\n"
		"\t[-odi-image-id <image-ID>\n"
		"\t[-odi-security-group-ids <group-ID[,groupID]*>\n"
		"\t[-odi-key-name <key-name>\n"
		"\t[-odi-instance-profile-arn <instance-profile-arn>]\n"
		, argv0 );
	fprintf( stdout, "%s defaults to On-Demand Instances (-odi).  "
		"For Spot Fleet Requests, use -sfr.  Specifying "
		"-sfr-config-file implies -sfr, as does -slots.  "
		"Specifying -count implies -odi.  You may not specify or imply "
		"-odi and -sfr in the same command.  Specifying -odi-* implies -odi."
		"\n", argv0 );
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
	const char * pool = NULL;
	const char * name = NULL;
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
	const char * odiImageID = NULL;
	const char * odiInstanceProfileARN = NULL;
	const char * odiS3ConfigPath = NULL;
	const char * odiKeyName = NULL;
	const char * odiSecurityGroupIDs = NULL;
	bool annexTypeIsSFR = false;
	bool annexTypeIsODI = false;
	long int leaseDuration = 0;
	long int count = 0;
	for( int i = 1; i < argc; ++i ) {
		if( is_dash_arg_prefix( argv[i], "f", 1 ) ) {
			// Ignored for compatibility with daemon core.
			continue;
		} else if( is_dash_arg_prefix( argv[i], "pool", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				pool = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -pool requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "name", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				name = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -name requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "service-url", 7 ) ) {
			++i;
			if( argv[i] != NULL ) {
				serviceURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -service-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "events-url", 6 ) ) {
			++i;
			if( argv[i] != NULL ) {
				eventsURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -events-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "lambda-url", 6 ) ) {
			++i;
			if( argv[i] != NULL ) {
				lambdaURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -lambda-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "s3-url", 2 ) ) {
			++i;
			if( argv[i] != NULL ) {
				s3URL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -s3-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "user-data-file", 10 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				userDataFileName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -user-data-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "user-data", 9 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				userData = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -user-data requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "default-user-data-file", 18 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				clUserDataWins = false;
				userDataFileName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -user-data-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "default-user-data", 17 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				clUserDataWins = false;
				userData = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -user-data requires an argument.\n", argv[0] );
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
		} else if( is_dash_arg_prefix( argv[i], "sfr-config-file", 12 ) ) {
			++i;
			if( argv[i] != NULL ) {
				sfrConfigFile = argv[i];
				annexTypeIsSFR = true;
				continue;
			} else {
				fprintf( stderr, "%s: -sfr-config-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "public-key-file", 6 ) ||
					is_dash_arg_prefix( argv[1], "access-key-file", 6 ) ) {
			++i;
			if( argv[i] != NULL ) {
				publicKeyFile = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -{public|access}-key-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "secret-key-file", 6 ) ||
					is_dash_arg_prefix( argv[i], "private-key-file", 7 ) ) {
			++i;
			if( argv[i] != NULL ) {
				secretKeyFile = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -{secret|private}-key-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "sfr-lease-function-arn", 11 ) ) {
			++i;
			if( argv[i] != NULL ) {
				sfrLeaseFunctionARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -sfr-lease-function-arn requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-lease-function-arn", 11 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiLeaseFunctionARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -odi-lease-function-arn requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-instance-type", 14 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiInstanceType = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -odi-instance-type requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-key-name", 7 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiKeyName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -odi-key-name requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-security-group-ids", 21 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiSecurityGroupIDs = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -odi-security-group-ids requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-image-id", 11 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiImageID = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -odi-image-id requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-instance-profile-arn", 22 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiInstanceProfileARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -lease-function-arn requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-s3-config-path", 18 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiS3ConfigPath = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -odi-s3-config-path requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "duration", 8 ) ) {
			++i;
			if( argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ld = argv[i];
				leaseDuration = strtol( ld, & endptr, 0 );
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -duration requires an integer argument.\n", argv[0] );
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
					fprintf( stderr, "%s: the count must be greater than zero.\n", argv[0] );
					return 1;
				}
				annexTypeIsSFR = true;
				continue;
			} else {
				fprintf( stderr, "%s: -count requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "count", 5 ) ) {
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
					fprintf( stderr, "%s: the count must be greater than zero.\n", argv[0] );
					return 1;
				}
				annexTypeIsODI = true;
				continue;
			} else {
				fprintf( stderr, "%s: -count requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "sfr", 3 ) ) {
			annexTypeIsSFR = true;
			continue;
		} else if( is_dash_arg_prefix( argv[i], "odi", 3 ) ) {
			annexTypeIsODI = true;
			continue;
		} else if( is_dash_arg_prefix( argv[i], "debug", 1 ) ) {
			dprintf_set_tool_debug( "TOOL", 0 );
			continue;
		} else if( is_dash_arg_prefix( argv[i], "help", 1 ) ) {
			help( argv[0] );
			return 1;
		} else if( argv[i][0] == '-' && argv[i][1] != '\0' ) {
			fprintf( stderr, "%s: unrecognized option (%s).\n", argv[0], argv[i] );
			return 1;
		}
	}

	if( udSpecifications > 1 ) {
		fprintf( stderr, "%s: you may specify no more than one of -[default-]user-data[-file].\n", argv[0] );
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
		fprintf( stderr, "%s: you must specify (a positive) -duration.\n", argv[0] );
		return 1;
	}

	if( annexTypeIsSFR && annexTypeIsODI ) {
		fprintf( stderr, "You must not specify more than one of -odi and -sfr.  If you specify -sfr-config-file, you must not specify -odi.  Specifying -count implies -odi and specifying -slots implies -sfr.\n" );
		return 1;
	}
	if(! (annexTypeIsSFR || annexTypeIsODI )) {
		annexTypeIsODI = true;
	}

	if( annexTypeIsSFR && sfrConfigFile == NULL ) {
		sfrConfigFile = param( "ANNEX_DEFAULT_SFR_CONFIG_FILE" );
		if( sfrConfigFile == NULL ) {
			fprintf( stderr, "Spot Fleet Requests require the -sfr-config-file flag.\n" );
			return 1;
		}
	}
	if( sfrConfigFile != NULL && ! annexTypeIsSFR ) {
		fprintf( stderr, "Unwilling to ignore -sfr-config-file flag being set for an ODI annex.\n" );
		return 1;
	}


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

	std::string tarballTarget;
	param( tarballTarget, "ANNEX_DEFAULT_ODI_S3_CONFIG_PATH" );
	if( odiS3ConfigPath != NULL ) {
		tarballTarget = odiS3ConfigPath;
	}
	if( tarballTarget.empty() ) {
		fprintf( stderr, "If you don't specify -odi-s3-config-path, ANNEX_DEFAULT_ODI_S3_CONFIG_PATH must be set in configuration.\n" );
		return 1;
	}
	spotFleetRequest.Assign( "UploadTo", tarballTarget );

	// Create the config tarball locally, then specify it.  That will
	// allow us to switch over to file transfer later if necessary.
	std::string tarballPath;
	if( configDir == NULL ) {
		char dirNameTemplate[] = ".condor_annex.XXXXXX";
		const char * tempDir = mkdtemp( dirNameTemplate );
		if( tempDir == NULL ) {
			fprintf( stderr, "Failed to create temporary directory (%d): '%s'.\n", errno, strerror( errno ) );
			return 2;
		}
		createConfigTarball( tempDir, annexName, tarballPath );
		// FIXME: copy the config tarball somewhere sane.
		rmdir( tempDir );
	} else {
		createConfigTarball( configDir, annexName, tarballPath );
	}
	spotFleetRequest.Assign( "UploadFrom", tarballPath );

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
			return 2;
		}
	}

	// condor_base64_encode() segfaults on the empty string.
	if(! userData.empty()) {
		std::string validationError;
		char * base64Encoded = condor_base64_encode( (const unsigned char *)userData.c_str(), userData.length() );
		if(! assignUserData( spotFleetRequest, base64Encoded, clUserDataWins, validationError )) {
			fprintf( stderr, "Failed to set user data in request ad (%s).\n", validationError.c_str() );
			return 6;
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

	dc_main_init = & main_init;
	dc_main_config = & main_config;
	dc_main_shutdown_fast = & main_shutdown_fast;
	dc_main_shutdown_graceful = & main_shutdown_graceful;
	dc_main_pre_dc_init = & main_pre_dc_init;
	dc_main_pre_command_sock_init = & main_pre_command_sock_init;

	// FIXME: force the '-f' option to be on by default.
	return dc_main( argc, argv );
}
