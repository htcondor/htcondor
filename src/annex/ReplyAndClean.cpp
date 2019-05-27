#include "condor_common.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "ReplyAndClean.h"

#include "classad_command_util.h"

int
ReplyAndClean::operator() () {
	dprintf( D_FULLDEBUG, "ReplyAndClean::operator()\n" );

	// Send whatever reply we have, then clean it up.
	if( reply ) {
		if( replyStream ) {
			if(! sendCAReply( replyStream, "CA_BULK_REQUEST", reply )) {
				dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
			}
		} else {
			// Stolen from Daemon::sendCACmd().
			std::string resultString;
			reply->LookupString( ATTR_RESULT, resultString );
			CAResult result = getCAResultNum( resultString.c_str() );

			if( result == CA_SUCCESS ) {
				std::string bulkRequestID;
				reply->LookupString( "BulkRequestID", bulkRequestID );
				if( bulkRequestID.empty() ) {
					std::string alternateReply;
					reply->LookupString( "AlternateReply", alternateReply );
					if( alternateReply.empty() ) {
						fprintf( stderr, "The success reply did not include the bulk request ID.\n" );
					} else {
						dprintf( D_AUDIT | D_IDENT | D_PID, getuid(),
							"%s\n", alternateReply.c_str() );
						fprintf( stdout, "%s\n", alternateReply.c_str() );
					}
				} else {
					std::string message;
					formatstr( message, "Annex started.  Its identity with the cloud provider is '%s'.", bulkRequestID.c_str() );

					dprintf( D_AUDIT | D_IDENT | D_PID, getuid(),
						"%s\n", message.c_str() );
					fprintf( stdout, "%s", message.c_str() );

					std::string expectedDelay;
					reply->LookupString( "ExpectedDelay", expectedDelay );
					if(! expectedDelay.empty()) {
						fprintf( stdout, "%s", expectedDelay.c_str() );
					}
					fprintf( stdout, "\n" );
				}
			} else {
				std::string errorString;
				reply->LookupString( ATTR_ERROR_STRING, errorString );
				if( errorString.empty() ) {
					dprintf( D_AUDIT | D_IDENT | D_PID, getuid(), "The error reply (%s) did not include an error string.\n", resultString.c_str() );
					fprintf( stderr, "The error reply (%s) did not include an error string.\n", resultString.c_str() );
				} else {
					dprintf( D_AUDIT | D_IDENT | D_PID, getuid(), "%s\n", errorString.c_str() );
					fprintf( stderr, "%s\n", errorString.c_str() );
				}
			}
		}

		delete reply;
	}

	// Once we've sent the reply, forget about the command.  (We either
	// succeed or roll back after retrying, so this is safe to do --
	// we've either succeeded and can forget, or have given up and should
	// forget.)
	if(! commandID.empty()) {
		commandState->BeginTransaction();
		{
			commandState->DestroyClassAd( commandID );
		}
		commandState->CommitTransaction();
	}

	// We're done with the stream, now, so clean it up.
	if( replyStream ) { delete replyStream; }

	if( gahp ) {
		// The gahp needs to know about the timer anyway, so it's a
		// convenient place to keep track of it.
		daemonCore->Cancel_Timer( gahp->getNotificationTimerId() );

		// We're done with this gahp, so clean it up.  The gahp server will
		// shut itself (and the GAHP) down cleanly roughly ten seconds after
		// the last corresponding gahp client is deleted.
		delete gahp;
	}

	if( eventsGahp ) {
		// Note that the annex daemon code happened to use the same timer
		// for both GAHPs.  Since it's safe to cancel a timer twice, we should
		// probaly just cancel this gahp's timer as well.  This produces an
		// ugly warning in the log, but that's not worth changing right now.
		daemonCore->Cancel_Timer( eventsGahp->getNotificationTimerId() );
		delete eventsGahp;
	}

	if( lambdaGahp ) {
		// See above.
		daemonCore->Cancel_Timer( lambdaGahp->getNotificationTimerId() );
		delete lambdaGahp;
	}

	if( s3Gahp ) {
		// See above.
		daemonCore->Cancel_Timer( s3Gahp->getNotificationTimerId() );
		delete s3Gahp;
	}

	if( scratchpad ) {
		// We're done with the scratchpad, too.
		delete scratchpad;
	}

	// Anything other than KEEP_STREAM deletes the sequence, which is good,
	// because we just cancelled the timer.
	return TRUE;
}

int
ReplyAndClean::rollback() {
	dprintf( D_FULLDEBUG, "ReplyAndClean::rollback() - calling operator().\n" );
	return (* this)();
}
