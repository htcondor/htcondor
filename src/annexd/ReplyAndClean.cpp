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
	if( reply && replyStream ) {
		if(! sendCAReply( replyStream, "CA_BULK_REQUEST", reply )) {
			dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
		}

		delete reply;
	}

	// Once we've sent the reply, forget about the command.  (We either
	// succeed or roll back after retrying, so this is safe to do --
	// we've either succeeded and can forget, or have given up and should
	// forget.)
	commandState->BeginTransaction();
	{
		std::string commandID;
		scratchpad->LookupString( "CommandID", commandID );
		if(! commandID.empty()) {
			commandState->DestroyClassAd( commandID.c_str() );
		}
	}
	commandState->CommitTransaction();

	// We're done with the stream, now, so clean it up.
	if( replyStream ) { delete replyStream; }

	// The gahp needs to know about the timer anyway, so it's a
	// convenient place to keep track of it.
	daemonCore->Cancel_Timer( gahp->getNotificationTimerId() );

	// We're done with this gahp, so clean it up.  The gahp server will
	// shut itself (and the GAHP) down cleanly roughly ten seconds after
	// the last corresponding gahp client is deleted.
	delete gahp;

	// Note that the annex daemon code happened to use the same timer
	// for both GAHPs.  Since it's safe to cancel a timer twice, we should
	// probaly just cancel this gahp's timer as well.  This produces an
	// ugly warning in the log, but that's not worth changing right now.
	daemonCore->Cancel_Timer( eventsGahp->getNotificationTimerId() );
	delete eventsGahp;

	// We're done with the scratchpad, too.
	delete scratchpad;

	// Anything other than KEEP_STREAM deletes the sequence, which is good,
	// because we just cancelled the timer.
	return TRUE;
}

int
ReplyAndClean::rollback() {
	dprintf( D_FULLDEBUG, "ReplyAndClean::rollback() - calling operator().\n" );
	return (* this)();
}
