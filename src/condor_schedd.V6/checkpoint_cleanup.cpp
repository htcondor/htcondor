#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include <filesystem>
#include <algorithm>

#include "spooled_job_files.h"
#include "scheduler.h"
#include "qmgmt.h"

#include "checkpoint_cleanup_utils.h"

#include "dc_coroutines.h"
using namespace condor;


dc::void_coroutine
spawnCheckpointCleanupProcessWithTimeout( int cluster, int proc, ClassAd * jobAd, time_t timeout ) {
	dc::AwaitableDeadlineReaper logansRun;

	std::string error;
	int spawned_pid = -1;
	bool rv = spawnCheckpointCleanupProcess(
		cluster, proc, jobAd, logansRun.reaper_id(),
		spawned_pid, error
	);
	if(! rv) { co_return /* false */; }

	logansRun.born( spawned_pid, timeout );
	auto [pid, timed_out, status] = co_await( logansRun );
	// This pointer could have been invalidated while we were co_await()ing.
	jobAd = NULL;

	if( timed_out ) {
		daemonCore->Shutdown_Graceful( pid );
		dprintf( D_TEST, "checkpoint clean-up proc %d timed out after %ld seconds\n", pid, timeout );
		// This keeps the awaitable deadline reaper alive until the process
		// we just killed is reaped, which prevents a log message about an
		// unknown process dying.
		co_await( logansRun );
	} else {
		dprintf( D_TEST, "checkpoint clean-up proc %d returned %d\n", pid, status );
	}

	co_return /* true */;
}


void
Scheduler::doCheckpointCleanUp( int cluster, int proc, ClassAd * jobAd ) {
	std::string owner;
	if(! jobAd->LookupString( ATTR_OWNER, owner )) {
		dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): no owner attribute in job, not cleaning up.\n" );
		return /* false */;
	}


	//
	// The schedd stores spooled files in a directory tree whose first branch
	// is the job's cluster ID modulo 1000.  This means that only the schedd
	// can safely remove that directory; any other process might remove it
	// between when the schedd checks for its existence and when it tries to
	// create the next subdirectory.  So we rename the job-specific directory
	// out of the way.
	//

	std::string spoolPath;
	SpooledJobFiles::getJobSpoolPath( jobAd, spoolPath );
	std::filesystem::path spool( spoolPath );

	std::filesystem::path SPOOL = spool.parent_path().parent_path().parent_path();
	std::filesystem::path checkpointCleanup = SPOOL / "checkpoint-cleanup";

	std::error_code errCode;
	if( std::filesystem::exists( checkpointCleanup ) ) {
		if(! std::filesystem::is_directory( checkpointCleanup )) {
			dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): '%s' is a file and needs to be a directory in order to do checkpoint cleanup.\n", checkpointCleanup.string().c_str() );
			return /* false */;
		}
	} else {
		std::filesystem::create_directory( checkpointCleanup, SPOOL, errCode );
		if( errCode ) {
			dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): failed to create checkpoint clean-up directory '%s' (%d: %s), will not clean up.\n", checkpointCleanup.string().c_str(), errCode.value(), errCode.message().c_str() );
			return /* false */;
		}
	}


	//
	// In order to make the directory itself removable, we need to put it
	// in a (permanent) directory owned by the job's owner.
	//
	std::filesystem::path owner_dir = checkpointCleanup / owner;

	// The owner-specific directory should have the same ownership
	// as the spool directory going into it.
	StatWrapper sw( spool.string() );
	auto owner_uid = sw.GetBuf()->st_uid;
	auto owner_gid = sw.GetBuf()->st_gid;

	if( std::filesystem::exists( owner_dir ) ) {
		if(! std::filesystem::is_directory( owner_dir )) {
			dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): '%s' is a file and needs to be a directory in order to do checkpoint cleanup.\n", owner_dir.string().c_str() );
			return /* false */;
		}
	} else {
		// Sadly, std::filesystem doesn't handle ownership, and we can't
		// create the directory as the user we want, either.
		std::filesystem::create_directory( owner_dir, checkpointCleanup, errCode );
		if( errCode ) {
			dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): failed to create checkpoint clean-up directory '%s' (%d: %s), will not clean up.\n", owner_dir.string().c_str(), errCode.value(), errCode.message().c_str() );
			return /* false */;
		}

#if ! defined(WINDOWS)
		{
			TemporaryPrivSentry sentry(PRIV_ROOT);
			int rv = chown( owner_dir.string().c_str(), owner_uid, owner_gid );
			if( rv == -1 ) {
				dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): failed to chown() checkpoint clean-up directory '%s' (%d: %s), will not clean up.\n", owner_dir.string().c_str(), errno, strerror(errno) );
				return /* false */;
			}
		}
#endif /* ! defined(WINDOWS) */
	}


	std::filesystem::path target_dir = owner_dir / spool.filename();
	{
		TemporaryPrivSentry sentry(PRIV_ROOT);
		std::filesystem::rename( spool, target_dir, errCode );
	}
	if( errCode ) {
		dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): failed to rename job (%d.%d) spool directory (%d: %s), will not clean up.\n", cluster, proc, errCode.value(), errCode.message().c_str() );
		return /* false */;
	}


	// Drop a copy of the job ad into the directory in case this attempt
	// to clean up fails and we need to try it again later.
	FILE * jobAdFile = NULL;
	std::filesystem::path jobAdPath = target_dir / ".job.ad";
#if ! defined(WINDOWS)
	{
		TemporaryPrivSentry sentry(PRIV_ROOT);
		jobAdFile = safe_fopen_wrapper( jobAdPath.string().c_str(), "w" );
		// It's annoying if this fails, but not fatal; the directory owner
		// (job owner) can remove it even if it's still owned by root.
		std::ignore = chown( jobAdPath.string().c_str(), owner_uid, owner_gid );
	}
#endif /* ! defined(WINDOWS ) */
	if( jobAdFile == NULL ) {
		dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): failed to open job ad file '%s'\n", jobAdPath.string().c_str() );
		return /* false */;
	}
	fPrintAd( jobAdFile, * jobAd );
	fclose( jobAdFile );

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
