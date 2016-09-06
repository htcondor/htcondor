#include "condor_common.h"
#include "condor_daemon_core.h"
#include "compat_classad.h"
#include "gahp-client.h"
#include "Functor.h"
#include "AddTarget.h"

int
AddTarget::operator() () {
	dprintf( D_ALWAYS, "AddTarget()\n" );

	// FIXME

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
