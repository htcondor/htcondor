#include "condor_common.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "UpdateCommandState.h"

int
UpdateCommandState::operator() () {
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
	delete this;
	return PASS_STREAM;
}
