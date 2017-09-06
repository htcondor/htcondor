#include "condor_common.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "SetupReply.h"

#include "classad_command_util.h"

int
SetupReply::operator() () {
	dprintf( D_FULLDEBUG, "SetupReply::operator()\n" );

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
				dprintf( D_AUDIT | D_IDENT | D_PID, getuid(), "%s\n", successString.c_str() );
				fprintf( stdout, "%s\n", successString.c_str() );
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

	// We're done with the stream, now, so clean it up.
	if( replyStream ) { delete replyStream; }

	// The gahp needs to know about the timer anyway, so it's a
	// convenient place to keep track of it.
	daemonCore->Cancel_Timer( cfGahp->getNotificationTimerId() );

	// We're done with this gahp, so clean it up.  The gahp server will
	// shut itself (and the GAHP) down cleanly roughly ten seconds after
	// the last corresponding gahp client is deleted.
	delete cfGahp;

	// See commentary in ReplyAndClean.
	daemonCore->Cancel_Timer( ec2Gahp->getNotificationTimerId() );
	delete ec2Gahp;

	// We're done with the scratchpad, too.
	delete scratchpad;

	// Anything other than KEEP_STREAM deletes the sequence, which is good,
	// because we just cancelled the timer.
	return TRUE;
}

int
SetupReply::rollback() {
	dprintf( D_FULLDEBUG, "SetupReply::rollback() - calling operator().\n" );
	return (* this)();
}
