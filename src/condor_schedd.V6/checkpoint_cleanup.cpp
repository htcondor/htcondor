#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include <filesystem>
#include <algorithm>

#include "spooled_job_files.h"
#include "scheduler.h"
#include "qmgmt.h"

int
Scheduler::checkpointCleanUpReaper( int pid, int status ) {
	// If the clean-up plug-in succeeded, remove the job's spool directory?
	// (We'd need to determine it before spawning the plug-in and store the
	// result, because we won't have ready access to the job ad by the time
	// the plug-in finishes.  I'm tempted to say that the plug-in deletes
	// MANIFEST files after it finishes deleting their contents, and that
	// preen will take care of deleting the empty directories.)

	dprintf( D_ZKM, "checkpoint clean-up proc %d returned %d\n", pid, status );
	return 0;
}

bool
Scheduler::doCheckpointCleanUp( int cluster, int proc ) {
	// In case we ever want it.
	std::string error;

	ClassAd * jobAd = GetJobAd( cluster, proc );
	if( jobAd == NULL ) {
		error = "GetJobAd( %d, %d ) failed, aborting";
		dprintf( D_ZKM, "%s\n", error.c_str() );
		return false;
	}

	std::string checkpointDestination;
	if(! jobAd->LookupString( ATTR_JOB_CHECKPOINT_DESTINATION, checkpointDestination ) ) {
		return true;
	}


	dprintf( D_ZKM, "Cleaning up checkpoint of job %d.%d...\n", cluster, proc );

	//
	// Create a subprocess to invoke the deletion plug-in.  Its
	// reaper will call JobIsFinished() and JobIsFinishedDone().
	//
	// The call will be
	//
	// `/full/path/to/condor_manifest deleteFilesStoredAt
	//      /full/path/to/protocol-specific-clean-up-plug-in
	//      checkpointDestination/globalJobID
	//      /full/path/to/manifest-file-trailing-checkpoint-number
	//      lowCheckpointNo highCheckpointNo`
	//
	// Moving the iteration over checkpoint numbers into condor_manifest
	// means that we don't have to worry about spawning more than one
	// process here (because of ARG_MAX limitations).
	//

	static int cleanup_reaper_id = -1;
	if( cleanup_reaper_id == -1 ) {
		cleanup_reaper_id = daemonCore->Register_Reaper(
			"externally-stored checkpoint reaper",
			(ReaperHandlercpp) & Scheduler::checkpointCleanUpReaper,
			"externally-stored checkpoint reaper",
			this
		);
	}


	// Determine and validate `full/path/to/condor_manifest`.
	std::string binPath;
	param( binPath, "BIN" );
	std::filesystem::path BIN( binPath );
	std::filesystem::path condor_manifest = BIN / "condor_manifest";
	if(! std::filesystem::exists(condor_manifest)) {
		formatstr( error, "'%s' does not exist, aborting", condor_manifest.string().c_str() );
		dprintf( D_ZKM, "%s\n", error.c_str() );
		return false;
	}


	// Determine and validate `full/path/to/protocol-specific-clean-up-plug-in`.
	std::string protocol = checkpointDestination.substr( 0, checkpointDestination.find( "://" ) );
	if( protocol == checkpointDestination ) {
		formatstr( error,
			"Invalid checkpoint destination (%s) in checkpoint clean-up attempt should be impossible, aborting.",
			checkpointDestination.c_str()
		);
		dprintf( D_ALWAYS, "%s\n", error.c_str() );
		return false;
	}

	std::string cleanupPluginConfigKey;
	formatstr( cleanupPluginConfigKey, "%s_CLEANUP_PLUGIN", protocol.c_str() );
	std::string destinationSpecificBinary;
	param( destinationSpecificBinary, cleanupPluginConfigKey.c_str() );
	if(! std::filesystem::exists( destinationSpecificBinary )) {
		formatstr( error,
			"Clean-up plug-in for '%s' (%s) does not exist, aborting",
			protocol.c_str(), destinationSpecificBinary.c_str()
		);
		dprintf( D_ALWAYS, "%s\n", error.c_str() );
		return false;
	}


	// We need this to construct the checkpoint-specific location.
	std::string globalJobID;
	if(! jobAd->LookupString( ATTR_GLOBAL_JOB_ID, globalJobID )) {
		error = "Failed to find global job ID in job ad, aborting";
		dprintf( D_ALWAYS, "%s\n", error.c_str() );
		return false;
	}
	std::replace( globalJobID.begin(), globalJobID.end(), '#', '_' );

	// We need this to construct the MANIFEST file's path.
	std::string spoolPath;
	SpooledJobFiles::getJobSpoolPath( jobAd, spoolPath );
	std::filesystem::path spool( spoolPath );


	int checkpointNumber = -1;
	if(! jobAd->LookupInteger( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber )) {
		error = "Failed to find checkpoint number in job ad, aborting";
		dprintf( D_ALWAYS, "%s\n", error.c_str() );
		return false;
	}

	std::string separator = "/";
	if( ends_with( checkpointDestination, "/" ) ) {
		separator = "";
	}


	std::string specificCheckpointDestination;

	std::vector< std::string > cleanup_process_args;
	cleanup_process_args.push_back( condor_manifest.string() );
	cleanup_process_args.push_back( "deleteFilesStoredAt" );
	cleanup_process_args.push_back( destinationSpecificBinary );

	// Construct the checkpoint-specific location prefix.
	formatstr(
		specificCheckpointDestination,
		"%s%s%s",
		checkpointDestination.c_str(), separator.c_str(), globalJobID.c_str()
	);

	// Construct the manifest file prefix.
	std::string manifestFileName = "_condor_checkpoint_MANIFEST";
	std::filesystem::path manifestFilePath = spool / manifestFileName;

	cleanup_process_args.push_back( specificCheckpointDestination );
	cleanup_process_args.push_back( manifestFilePath.string() );
	cleanup_process_args.push_back( "0" );

	std::string buffer;
	formatstr(buffer, "%d", checkpointNumber );
	cleanup_process_args.push_back( buffer );


	OptionalCreateProcessArgs cleanup_process_opts;
	auto pid = daemonCore->CreateProcessNew(
		condor_manifest.string(),
		cleanup_process_args,
		cleanup_process_opts.reaperID(cleanup_reaper_id)
	);

	// FIXME: Update condor_preen to invoke condor_manifest
	// appropriately as well.  May involve refactorign this
	// function a little bit. ;)

	dprintf( D_ZKM, "... checkpoint clean-up for job %d.%d spawned as pid %d.\n", cluster, proc, pid );
	return true;
}
