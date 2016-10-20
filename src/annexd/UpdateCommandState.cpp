#include "condor_common.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "UpdateCommandState.h"

int
UpdateCommandState::operator() () {
	dprintf( D_FULLDEBUG, "UpdateCommandState()::operator()\n" );

	commandState->BeginTransaction();
	{
		std::string commandID;
		scratchpad->LookupString( "CommandID", commandID );
		if(! commandID.empty()) {
			commandState->DestroyClassAd( commandID.c_str() );
		}
	}
	commandState->CommitTransaction();

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}

int
UpdateCommandState::rollback() {
	dprintf( D_FULLDEBUG, "UpdateCommandState::rollback()\n" );

	// This functor never returns anything except PASS_STREAM, and should
	// also be the last functor in any given sequence.  Seeing the above
	// message therefore indicates a bug.
	EXCEPT( "UpdateCommandState::operator() can't fail and should be last functor in a sequence.  Its rollback() function should never be called." );

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
