#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include <filesystem>
#include <algorithm>

#include "spooled_job_files.h"
#include "scheduler.h"
#include "qmgmt.h"

#include "dc_coroutines.h"
#include "checkpoint_cleanup_utils.h"


void
Scheduler::doCheckpointCleanUp( int cluster, int proc, ClassAd * jobAd ) {
	//
	// The schedd stores spooled files in a directory tree whose first branch
	// is the job's cluster ID modulo 1000.  This means that only the schedd
	// can safely remove that directory; any other process might remove it
	// between when the schedd checks for its existence and when it tries to
	// create the next subdirectory.  So we rename the job-specific directory
	// out of the way.
	//
	std::set<long> skip_no_checkpoints;
	if(! moveCheckpointsToCleanupDirectory( cluster, proc, jobAd, skip_no_checkpoints )) {
		return;
	}

	int CLEANUP_TIMEOUT = param_integer( "SCHEDD_CHECKPOINT_CLEANUP_TIMEOUT", 300 );

	// We can convert back to a bool return type for this function
	// (even though the return type is currently ignored) when we
	// implement dc::coroutine<bool>, although in practice it might
	// be better/easier to pass a boolean reference that will be set
	// to false if-and-only-if spawning failed.
	//
	// We could also consider spawning the process in _this_ function,
	// so that there'd be no reason to return bool from the coroutine
	// implementing the time-out, but that would require getting the
	// reaper ID from the coroutine's AwaitableDeadlineReaper, which
	// would be even _more_ annoying.
	spawnCheckpointCleanupProcessWithTimeout( cluster, proc, jobAd, CLEANUP_TIMEOUT );
}
