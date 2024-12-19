#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "gahp-client.h"
#include "compat_classad.h"
#include "classad_command_util.h"
#include "classad_collection.h"

#include "annex.h"
#include "annex-update.h"
#include "generate-id.h"

#include "Functor.h"
#include "FunctorSequence.h"
#include "PutRule.h"
#include "PutTargets.h"
#include "ReplyAndClean.h"

extern ClassAd * command;
extern ClassAdCollection * commandState;

int
updateOneAnnex( ClassAd * command, Stream * replyStream, ClassAd * reply ) {
	ClassAd * scratchpad = new ClassAd();

	std::string publicKeyFile, secretKeyFile, eventsURL;

	param( publicKeyFile, "ANNEX_DEFAULT_ACCESS_KEY_FILE" );
	command->LookupString( "PublicKeyFile", publicKeyFile );

	param( secretKeyFile, "ANNEX_DEFAULT_SECRET_KEY_FILE" );
	command->LookupString( "SecretKeyFile", secretKeyFile );

	param( eventsURL, "ANNEX_DEFAULT_CWE_URL" );
	command->LookupString( "EventsURL", eventsURL );

	std::string leaseFunctionARN;
	command->LookupString( "LeaseFunctionARN", leaseFunctionARN );

	std::string annexName;
	command->LookupString( ATTR_ANNEX_NAME, annexName );
	std::string annexID = annexName.substr( 0, 27 );
	scratchpad->Assign( "AnnexID", annexID );

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

	if( secretKeyFile != USE_INSTANCE_ROLE_MAGIC_STRING ) {
		struct stat sw = {};
		stat( secretKeyFile.c_str(), &sw );
		mode_t mode = sw.st_mode;
		if( mode & S_IRWXG || mode & S_IRWXO || getuid() != sw.st_uid ) {
			formatstr( errorString, "Secret key file must be accessible only by owner.  Please verify that your user owns the file and that the file permissons are restricted to the owner." );
		}
	}

	if(! errorString.empty()) {
		reply->Assign( ATTR_RESULT, getCAResultString( CA_INVALID_REQUEST ) );
		reply->Assign( ATTR_ERROR_STRING, errorString );

		if( replyStream ) {
			if(! sendCAReply( replyStream, "CA_BULK_REQUEST", reply )) {
				dprintf( D_ALWAYS, "Failed to reply to update request.\n" );
			}
		}

		return FALSE;
	}

	EC2GahpClient * eventsGahp = startOneGahpClient( publicKeyFile, eventsURL );

	std::string commandID;
	command->LookupString( "CommandID", commandID );
	if( commandID.empty() ) {
		generateCommandID( commandID );
		command->Assign( "CommandID", commandID );
	}
	scratchpad->Assign( "CommandID", commandID );

	commandState->BeginTransaction();
	{
		InsertOrUpdateAd( commandID, command, commandState );
	}
	commandState->CommitTransaction();

	// These two lines are deliberate BS; see comment in 'ReplyAndClean.cpp'.
	ClassAd * dummy = NULL;
	commandState->Lookup( "dummy", dummy );

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

	reply->Assign( "AlternateReply", "Lease updated." );
	ReplyAndClean * last = new ReplyAndClean( reply, replyStream,
		NULL, scratchpad, eventsGahp, commandState, commandID, NULL, NULL );

	FunctorSequence * fs = new FunctorSequence( { pr, pt }, last,
		commandState, commandID, scratchpad );

	int functorSequenceTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
		(void (Service::*)(int)) & FunctorSequence::timer,
		"updateOneAnnex() sequence", fs );
   	eventsGahp->setNotificationTimerId( functorSequenceTimer );

	return KEEP_STREAM;
}

void
callUpdateOneAnnex(int /* tid */) {
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
