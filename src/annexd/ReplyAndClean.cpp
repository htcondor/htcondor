#include "condor_common.h"
#include "compat_classad.h"
#include "gahp-client.h"
#include "Functor.h"
#include "ReplyAndClean.h"

#include "classad_command_util.h"

int
ReplyAndClean::operator() () {
	dprintf( D_ALWAYS, "ReplyAndClean()\n" );

	// Send whatever reply we have, then clean it up.
	if( reply && replyStream ) {
		if(! sendCAReply( replyStream, "CA_BULK_REQUEST", reply )) {
			dprintf( D_ALWAYS, "Failed to reply to CA_BULK_REQUEST.\n" );
		}

		delete reply;
	}

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
	// for both GAHPs.  If it's safe to cancel a timer twice, we should
	// probaly just cancel this gahp's timer as well.
	delete eventsGahp;

	delete scratchpad;

	// Clean ourselves up, since no one else can.
	delete this;

	// Anything other than KEEP_STREAM deletes the sequence, which is good,
	// because we just cancelled the timer.
	return TRUE;
}
