#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include <filesystem>
#include <algorithm>

#include "condor_attributes.h"
#include "condor_daemon_core.h"
#include "spooled_job_files.h"


bool
fetchCheckpointDestinationCleanup( const std::string & checkpointDestination, std::string & argl, std::string & error ) {
	std::string cdmf;
	param( cdmf, "CHECKPOINT_DESTINATION_MAPFILE" );

	MapFile mf;
	int rv = mf.ParseCanonicalizationFile( cdmf.c_str(), true, true, true );
	if( rv < 0 ) {
		formatstr( error,
			"Failed to parse checkpoint destination map file (%s), aborting",
			cdmf.c_str()
		);
		return false;
	}

	if( mf.GetCanonicalization( "*", checkpointDestination.c_str(), argl ) != 0 ) {
		formatstr( error,
		    "Failed to find checkpoint destination %s in map file, aborting",
		    checkpointDestination.c_str()
		);
		return false;
	}

    return true;
}

bool
spawnCheckpointCleanupProcess(
    int cluster, int proc, ClassAd * jobAd, int cleanup_reaper_id,
    int & pid, std::string & error
) {
	dprintf( D_TEST, "spawnCheckpointCleanupProcess(): for job %d.%d\n", cluster, proc );

	std::string checkpointDestination;
	if(! jobAd->LookupString( ATTR_JOB_CHECKPOINT_DESTINATION, checkpointDestination ) ) {
		return true;
	}

	std::string owner;
	if(! jobAd->LookupString( ATTR_OWNER, owner )) {
		dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): not cleaning up job %d.%d: no owner attribute found!\n", cluster, proc );
		return false;
	}

	std::string dummy;
	if(! fetchCheckpointDestinationCleanup( checkpointDestination, dummy, error )) {
		dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): not cleaning up"
		         " job %d.%d: no clean-up plug-in registered for checkpoint"
		         " destination '%s' (%s).\n",
		         cluster, proc, checkpointDestination.c_str(), error.c_str() );
		return false;
	}

	//
	// Create a subprocess to invoke the deletion plug-in.  Its
	// reaper will call JobIsFinished() and JobIsFinishedDone().
	//
	// The call will be
	//
	// `/full/path/to/condor_manifest deleteFilesStoredAt
	//      checkpointDestination/globalJobID
	//      /full/path/to/manifest-file-trailing-checkpoint-number
	//      lowCheckpointNo highCheckpointNo`
	//
	// Moving the iteration over checkpoint numbers into condor_manifest
	// means that we don't have to worry about spawning more than one
	// process here (because of ARG_MAX limitations).
	//

	// Determine and validate `full/path/to/condor_manifest`.
	std::string binPath;
	param( binPath, "BIN" );
	std::filesystem::path BIN( binPath );
	std::filesystem::path condor_manifest = BIN / "condor_manifest";
	if(! std::filesystem::exists(condor_manifest)) {
		formatstr( error, "'%s' does not exist, aborting", condor_manifest.string().c_str() );
		return false;
	}


	std::string spoolPath;
	SpooledJobFiles::getJobSpoolPath( jobAd, spoolPath );
	std::filesystem::path spool( spoolPath );
	std::filesystem::path SPOOL = spool.parent_path().parent_path().parent_path();
	std::filesystem::path checkpointCleanup = SPOOL / "checkpoint-cleanup";
	std::filesystem::path owner_dir = checkpointCleanup / owner;
	std::filesystem::path target_dir = owner_dir / spool.filename();


	// We need this to construct the checkpoint-specific location.
	std::string globalJobID;
	if(! jobAd->LookupString( ATTR_GLOBAL_JOB_ID, globalJobID )) {
		error = "Failed to find global job ID in job ad, aborting";
		dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): %s\n", error.c_str() );
		return false;
	}
	std::replace( globalJobID.begin(), globalJobID.end(), '#', '_' );


	int checkpointNumber = -1;
	if(! jobAd->LookupInteger( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber )) {
		error = "Failed to find checkpoint number in job ad, aborting";
		dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): %s\n", error.c_str() );
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

	// Construct the checkpoint-specific location prefix.
	formatstr(
		specificCheckpointDestination,
		"%s%s%s",
		checkpointDestination.c_str(), separator.c_str(), globalJobID.c_str()
	);

	// Construct the manifest file prefix.
	std::string manifestFileName = "_condor_checkpoint_MANIFEST";
	std::filesystem::path manifestFilePath = target_dir / manifestFileName;

	cleanup_process_args.push_back( specificCheckpointDestination );
	cleanup_process_args.push_back( manifestFilePath.string() );
	cleanup_process_args.push_back( "0" );

	std::string buffer;
	formatstr(buffer, "%d", checkpointNumber );
	cleanup_process_args.push_back( buffer );


	uid_t uid = (uid_t) -1; 
	gid_t gid = (gid_t) -1;
	bool use_old_user_ids = user_ids_are_inited();
	bool switch_users = param_boolean("RUN_CLEANUP_PLUGINS_AS_OWNER", true);

#if ! defined(WINDOWS)
	if( switch_users ) {
		if( use_old_user_ids ) {
			uid = get_user_uid();
			gid = get_user_gid();
		}

		if(! init_user_ids( owner.c_str(), NULL )) {
			dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): not cleaning up job %d.%d: unable to switch to user '%s'.!\n", cluster, proc, owner.c_str() );
			return false;
		}
		if(! use_old_user_ids) {
			uid = get_user_uid();
			gid = get_user_gid();
		}
	}
#endif /* ! defined(WINDOWS) */

	if( IsDebugLevel( D_TEST ) ) {
		std::string cl;
		for( const auto & a : cleanup_process_args ) {
			formatstr_cat( cl, " %s", a.c_str() );
		}
		dprintf( D_TEST, "spawnCheckpointCleanupProcess(): %s\n", cl.c_str() );
	}

	OptionalCreateProcessArgs cleanup_process_opts;
	pid = daemonCore->CreateProcessNew(
		condor_manifest.string(),
		cleanup_process_args,
		cleanup_process_opts.reaperID(cleanup_reaper_id).priv(PRIV_USER_FINAL)
	);

#if ! defined(WINDOWS)
	if( switch_users ) {
		if(! set_user_ids(uid, gid)) {
			dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): unable to switch back to user %d gid %d, ignoring.\n", uid, gid );
		}
	}
#endif /* ! defined(WINDOWS) */

	dprintf( D_TEST, "spawnCheckpointCleanupProcess(): ... checkpoint clean-up for job %d.%d spawned as pid %d.\n", cluster, proc, pid );
	return true;
}
