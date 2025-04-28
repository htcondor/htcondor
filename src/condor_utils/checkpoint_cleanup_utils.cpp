#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include <filesystem>
#include <algorithm>

#include "condor_attributes.h"
#include "condor_daemon_core.h"
#include "spooled_job_files.h"
#include "dc_coroutines.h"
#include "checkpoint_cleanup_utils.h"
#include "set_user_priv_from_ad.h"


bool
fetchCheckpointDestinationCleanup( const std::string & checkpointDestination, std::string & argl, std::string & error ) {
	std::string cdmf;
	param( cdmf, "CHECKPOINT_DESTINATION_MAPFILE" );

	MapFile mf;
	int rv = mf.ParseCanonicalizationFile( cdmf, true, true, true );
	if( rv < 0 ) {
		formatstr( error,
			"Failed to parse checkpoint destination map file (%s), aborting",
			cdmf.c_str()
		);
		return false;
	}

	if( mf.GetCanonicalization( "*", checkpointDestination, argl ) != 0 ) {
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
		dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): not cleaning up job %d.%d: no %s attribute found!\n", cluster, proc, ATTR_JOB_CHECKPOINT_DESTINATION );
		return false;
	}

	std::string owner;
	if(! jobAd->LookupString( ATTR_OS_USER, owner )) {
		dprintf( D_ALWAYS, "spawnCheckpointCleanupProcess(): not cleaning up job %d.%d: no %s attribute found!\n", cluster, proc, ATTR_OS_USER );
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

		if(! init_user_ids_from_ad(*jobAd)) {
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


// This is something of a misnomer; it's actually just moving the MANIFEST
// and/or FAILURE files.
bool
moveCheckpointsToCleanupDirectory(
	int cluster, int proc, ClassAd * jobAd,
	const std::set<long> & checkpointsToSave
) {
	std::string owner;
	if(! jobAd->LookupString( ATTR_OWNER, owner )) {
		dprintf( D_ERROR, "moveCheckpointsToCleanupDirectory(): no owner attribute in job, not cleaning up.\n" );
		return false;
	}

	std::string spoolPath;
	SpooledJobFiles::getJobSpoolPath( jobAd, spoolPath );
	std::filesystem::path spool( spoolPath );

	if(! std::filesystem::exists( spool )) {
	    // Then the job never uploaded a MANIFEST (or FAILURE) file,
	    // and we have nothing to clean up (with).
	    return false;
	}

	std::filesystem::path SPOOL = spool.parent_path().parent_path().parent_path();
	std::filesystem::path checkpointCleanup = SPOOL / "checkpoint-cleanup";

	std::error_code errCode;
	if( std::filesystem::exists( checkpointCleanup ) ) {
		if(! std::filesystem::is_directory( checkpointCleanup )) {
			dprintf( D_ALWAYS, "moveCheckpointsToCleanupDirectory(): '%s' is a file and needs to be a directory in order to do checkpoint cleanup.\n", checkpointCleanup.string().c_str() );
			return false;
		}
	} else {
		TemporaryPrivSentry sentry(PRIV_CONDOR);
		std::filesystem::create_directory( checkpointCleanup, SPOOL, errCode );
		if( errCode ) {
			dprintf( D_ALWAYS, "moveCheckpointsToCleanupDirectory(): failed to create checkpoint clean-up directory '%s' (%d: %s), will not clean up.\n", checkpointCleanup.string().c_str(), errCode.value(), errCode.message().c_str() );
			return false;
		}
	}


	//
	// In order to make the directory itself removable, we need to put it
	// in a (permanent) directory owned by the job's owner.
	//
	std::filesystem::path owner_dir = checkpointCleanup / owner;

	// The owner-specific directory should have the same ownership
	// as the spool directory going into it.
	struct stat statbuf = {};
	stat(spool.string().c_str(), &statbuf);
	auto owner_uid = statbuf.st_uid;
	auto owner_gid = statbuf.st_gid;

	if( std::filesystem::exists( owner_dir ) ) {
		if(! std::filesystem::is_directory( owner_dir )) {
			dprintf( D_ALWAYS, "moveCheckpointsToCleanupDirectory(): '%s' is a file and needs to be a directory in order to do checkpoint cleanup.\n", owner_dir.string().c_str() );
			return false;
		}
	} else {
		// Sadly, std::filesystem doesn't handle ownership, and we can't
		// create the directory as the user we want, either.
		{
			TemporaryPrivSentry sentry(PRIV_CONDOR);
			std::filesystem::create_directory( owner_dir, spool, errCode );
			if( errCode ) {
				dprintf( D_ALWAYS, "moveCheckpointsToCleanupDirectory(): failed to create checkpoint clean-up directory '%s' (%d: %s), will not clean up.\n", owner_dir.string().c_str(), errCode.value(), errCode.message().c_str() );
				return false;
			}
		}

#if ! defined(WINDOWS)
		{
			TemporaryPrivSentry sentry(PRIV_ROOT);
			// dprintf( D_STATUS | D_VERBOSE, "chown(%s, %d, %d)\n", owner_dir.string().c_str(), owner_uid, owner_gid );
			int rv = chown( owner_dir.string().c_str(), owner_uid, owner_gid );
			if( rv == -1 ) {
				dprintf( D_ALWAYS, "moveCheckpointsToCleanupDirectory(): failed to chown() checkpoint clean-up directory '%s' (%d: %s), will not clean up.\n", owner_dir.string().c_str(), errno, strerror(errno) );
				return false;
			}
		}
#endif /* ! defined(WINDOWS) */
	}


	//
	// Move each MANIFEST or FAILURE file not in `checkpointsToSave`
	// from `spool` to `target_dir`.
	//
	// This loop is very similar to the code in pseudo_ops.cpp to
	// decide which checkpoint(s) to keep, but it's not clear it's
	// worth it to share code.
	//
	bool moved_a_manifest = false;
	std::filesystem::path target_dir = owner_dir / spool.filename();


	// If we assume that the SPOOL's directory's parents are all world-
	// traversable, we can just switch IDs to the user at this point.  We
	// don't actually enforce that assumption anywhere, but if we don't
	// make it, we have to become root four different times in this operation,
	// which the makes the security guru unhappy.

	// Of course, the priv-switching system assumes there's always only one
	// user of interest.  This is stupid, but rather than reimplement
	// TemporaryPrivSentry, let's assume that this code is called if and only
	// if either (a) PRIV_USER is the correct user already (this should be
	// true in the shadow) or (b) PRIV_USER is unset (this should be true in
	// the entirely bug-free schedd).

	bool clearUserIDs;
	if( user_ids_are_inited() ) {
		clearUserIDs = false;
	} else {
		set_user_ids(owner_uid, owner_gid);
		clearUserIDs = true;
	}
	TemporaryPrivSentry sentry(PRIV_USER, clearUserIDs);


	auto spool_dir = std::filesystem::directory_iterator(spool, errCode);
	if( errCode ) {
		dprintf( D_ALWAYS, "moveCheckpointsToCleanupDirectory(): "
			"for job (%d.%d), failed to iterate job spool directory %s, "
			"returning false.\n",
			cluster, proc, spool.string().c_str()
		);
		return false;
	}

	for( const auto & entry : spool_dir ) {
		const auto & stem = entry.path().stem();
		if( starts_with(stem.string(), "_condor_checkpoint_") ) {
			bool success = ends_with(stem.string(), "MANIFEST");
			bool failure = ends_with(stem.string(), "FAILURE");
			if( success || failure ) {
				char * endptr = NULL;
				const auto & suffix = entry.path().extension().string().substr(1);
				long manifestNumber = strtol( suffix.c_str(), & endptr, 10 );
				if( endptr == suffix.c_str() || *endptr != '\0' ) {
					dprintf( D_FULLDEBUG, "Unable to extract checkpoint number from '%s', skipping.\n", entry.path().string().c_str() );
					continue;
				}

				if( checkpointsToSave.count(manifestNumber) != 0 ) {
					dprintf( D_FULLDEBUG, "moveCheckpointsToCleanupDirectory(): skipping manifest %ld\n", manifestNumber );
					continue;
				}


				// TemporaryPrivSentry sentry(PRIV_ROOT);
				std::filesystem::create_directory( target_dir, spool, errCode );
				if( errCode ) {
					dprintf( D_ALWAYS, "moveCheckpointsToCleanupDirectory(): "
						"for job (%d.%d), failed to create %s, "
						"returning false.\n",
						cluster, proc, target_dir.string().c_str()
					);
					return false;
				}

				std::filesystem::rename( entry.path(), target_dir / entry.path().filename(), errCode );
				if( errCode ) {
					dprintf( D_ALWAYS, "moveCheckpointsToCleanupDirectory(): "
						"for job (%d.%d), failed to rename "
						"manifest %ld [%s to %s] (%d: %s); returning false.\n",
						cluster, proc, manifestNumber,
						entry.path().string().c_str(),
						(target_dir / entry.path().filename()).string().c_str(),
						errCode.value(), errCode.message().c_str()
					);
					return false;
				}

				dprintf( D_STATUS | D_VERBOSE, "moveCheckpointsToCleanupDirectory(): moved manifest %ld\n", manifestNumber );
				moved_a_manifest = true;
			}
		}
	}

	if(! moved_a_manifest) { return true; }


	// Drop a copy of the job ad into the directory in case this attempt
	// to clean up fails and we need to try it again later.
	FILE * jobAdFile = NULL;
	std::filesystem::path jobAdPath = target_dir / ".job.ad";
	jobAdFile = safe_fopen_wrapper( jobAdPath.string().c_str(), "w" );
	if( jobAdFile == NULL ) {
		dprintf( D_ALWAYS, "moveCheckpointsToCleanupDirectory(): failed to open job ad file '%s'\n", jobAdPath.string().c_str() );
		return false;
	}
	fPrintAd( jobAdFile, * jobAd );
	fclose( jobAdFile );

	return true;
}
