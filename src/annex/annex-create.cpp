#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "gahp-client.h"
#include "compat_classad.h"
#include "classad_command_util.h"
#include "classad_collection.h"

#include "annex.h"
#include "annex-create.h"
#include "generate-id.h"

#include "Functor.h"
#include "FunctorSequence.h"
#include "BulkRequest.h"
#include "GetFunction.h"
#include "PutRule.h"
#include "PutTargets.h"
#include "ReplyAndClean.h"
#include "OnDemandRequest.h"
#include "UploadFile.h"
#include "CheckConnectivity.h"

extern ClassAd * command;
extern ClassAdCollection * commandState;

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


	// Handle the instance tags.
    std::string buffer;
	std::vector< std::pair< std::string, std::string > > tags;
	if( command->LookupString( ATTR_EC2_TAG_NAMES, buffer ) ) {
		for (const auto& tagName: StringTokenIterator(buffer)) {
			std::string tagAttr(ATTR_EC2_TAG_PREFIX);
			tagAttr.append(tagName);

			char * tagValue = NULL;
			if(! command->LookupString(tagAttr, &tagValue)) {
				return FALSE;
			}

			tags.push_back( std::make_pair( tagName, tagValue ) );
			free( tagValue );
		}
    }


	// Is this less of a hack than handing the command ad to ReplyAndClean?
	std::string expectedDelay;
	command->LookupString( "ExpectedDelay", expectedDelay );
	if(! expectedDelay.empty()) {
		reply->Assign( "ExpectedDelay", expectedDelay );
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

	if( secretKeyFile != USE_INSTANCE_ROLE_MAGIC_STRING ) {
		struct stat sw = {};
		stat( secretKeyFile.c_str(), &sw );
		mode_t mode = sw.st_mode;
		if( mode & S_IRWXG || mode & S_IRWXO || getuid() != sw.st_uid ) {
			std::string errorString;
			formatstr( errorString, "Secret key file must be accessible only by owner.  Please verify that your user owns the file and that the file permissons are restricted to the owner." );
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
		command->LookupString( ATTR_ANNEX_NAME, annexName );
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

		delete gahp;
		delete eventsGahp;
		delete lambdaGahp;
		delete s3Gahp;

		delete scratchpad;
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

		// These two lines are deliberate BS; see comment in 'ReplyAndClean.cpp'.
		ClassAd * dummy = NULL;
		commandState->Lookup( "dummy", dummy );

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
		ReplyAndClean * last = new ReplyAndClean( reply, replyStream, gahp, scratchpad, eventsGahp, commandState, commandID, lambdaGahp, s3Gahp );

		// Note that the functor sequence takes responsibility for deleting the
		// functor objects; the functor objects would just delete themselves when
		// they're done, but implementing rollback means the functors themselves
		// can't know how long they should persist.
		//
		// The commandState, commandID, and scratchpad allow the functor sequence
		// to restart a rollback, if that becomes necessary.
		FunctorSequence * fs;
		if( uf ) {
			fs = new FunctorSequence( { cc, gf, uf, pr, pt, br }, last, commandState, commandID, scratchpad );
		} else {
			fs = new FunctorSequence( { cc, gf, pr, pt, br }, last, commandState, commandID, scratchpad );
		}

		// Create a timer for the gahp to fire when it gets a result.  We must
		// use TIMER_NEVER to ensure that the timer hasn't been reaped when the
		// GAHP client needs to fire it.
		int functorSequenceTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
			(void (Service::*)(int)) & FunctorSequence::timer,
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
			commandID, annexID, tags );

		time_t endOfLease = 0;
		command->LookupInteger( "EndOfLease", endOfLease );

		if( (! odr->validateAndStore( command, validationError )) || (! validateLease( endOfLease, validationError )) ) {
			delete cc;
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
		ReplyAndClean * last = new ReplyAndClean( reply, replyStream, gahp, scratchpad, eventsGahp, commandState, commandID, lambdaGahp, s3Gahp );

		FunctorSequence * fs;
		if( uf ) {
			fs = new FunctorSequence( { cc, gf, uf, pr, pt, odr }, last, commandState, commandID, scratchpad );
		} else {
			fs = new FunctorSequence( { cc, gf, pr, pt, odr }, last, commandState, commandID, scratchpad );
		}

		int functorSequenceTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
			(void (Service::*)(int)) & FunctorSequence::timer,
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
		delete s3Gahp;

		delete scratchpad;
		return FALSE;
}

void
callCreateOneAnnex(int /* tid */) {
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
		delete reply;
		DC_Exit( 6 );
	}
}
