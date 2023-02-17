#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include <filesystem>
#include <algorithm>

#include "spooled_job_files.h"
#include "scheduler.h"
#include "qmgmt.h"

#include "checkpoint_cleanup_utils.h"


int
Scheduler::checkpointCleanUpReaper( int pid, int status ) {
	dprintf( D_ZKM, "checkpoint clean-up proc %d returned %d\n", pid, status );

	return 0;
}


bool
Scheduler::doCheckpointCleanUp( int cluster, int proc, ClassAd * jobAd ) {
	static int cleanup_reaper_id = -1;
	if( cleanup_reaper_id == -1 ) {
		cleanup_reaper_id = daemonCore->Register_Reaper(
			"externally-stored checkpoint reaper",
			(ReaperHandlercpp) & Scheduler::checkpointCleanUpReaper,
			"externally-stored checkpoint reaper",
			this
		);
	}


	//
	// The schedd stores spooled files in a directory tree whose first branch
	// is the job's cluster ID modulo 1000.  This means that only the schedd
	// can safely remove that directory; any other process might remove it
	// between when the schedd checks for its existence and when it tries to
	// create the next subdirectory.  So we rename the job-specific directory
	// out of the way.
	//
	dprintf( D_ZKM, "doCheckpointCleanup(): renaming job (%d.%d) spool directory to permit cleanup.\n", cluster, proc );

	std::string spoolPath;
	SpooledJobFiles::getJobSpoolPath( jobAd, spoolPath );
	std::filesystem::path spool( spoolPath );

	std::filesystem::path SPOOL = spool.parent_path().parent_path().parent_path();
	std::filesystem::path checkpointCleanup = SPOOL / "checkpoint-cleanup";
	std::filesystem::path target_dir = checkpointCleanup / spool.filename();

	std::error_code errCode;
	if( std::filesystem::exists( checkpointCleanup ) ) {
		if(! std::filesystem::is_directory( checkpointCleanup )) {
			dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): '%s' is a file and needs to be a directory in order to do checkpoint cleanup.\n", checkpointCleanup.string().c_str() );
			return false;
		}
	} else {
		std::filesystem::create_directory( checkpointCleanup, SPOOL, errCode );
		if( errCode ) {
			dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): failed to create checkpoint clean-up directory '%s' (%d: %s), will not clean up.\n", checkpointCleanup.string().c_str(), errCode.value(), errCode.message().c_str() );
			return false;
		}
	}

	dprintf( D_ZKM, "spawnCheckpointCleanupProcess(): renaming job (%d.%d) spool directory from '%s' to '%s'.\n", cluster, proc, spool.string().c_str(), target_dir.string().c_str() );
	std::filesystem::rename( spool, target_dir, errCode );
	if( errCode ) {
		dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): failed to rename job (%d.%d) spool directory (%d: %s), will not clean up.\n", cluster, proc, errCode.value(), errCode.message().c_str() );
		return false;
	}


	// Drop a copy of the job ad into the directory in case this attempt
	// to clean up fails and we need to try it again later.
	std::filesystem::path jobAdPath = target_dir / ".job.ad";
	FILE * jobAdFile = safe_fopen_wrapper( jobAdPath.string().c_str(), "w" );
	if( jobAdFile == NULL ) {
		dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): failed to open job ad file '%s'\n", jobAdPath.string().c_str() );
		return false;
	}
	fPrintAd( jobAdFile, * jobAd );
	fclose( jobAdFile );


	std::string error;
	int spawned_pid = -1;
	return spawnCheckpointCleanupProcess(
		cluster, proc, jobAd, cleanup_reaper_id,
		spawned_pid, error
	);
}
