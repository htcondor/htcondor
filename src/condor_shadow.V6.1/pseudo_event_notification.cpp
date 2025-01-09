#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_attributes.h"

#include "shadow.h"
#include "guidance.h"

#include "spooled_job_files.h"
#include "checkpoint_cleanup_utils.h"
#include <filesystem>
#include "manifest.h"
#include "condor_daemon_core.h"
#include "dc_coroutines.h"
#include "condor_base64.h"
#include "shortfile.h"
#include "../condor_starter.V6.1/job_info_communicator.h"

extern RemoteResource * thisRemoteResource;

//
// Shadow-specific utility function for dealing with checkpoint notifications.
//
void
deleteCheckpoint( const ClassAd * jobAd, int checkpointNumber ) {
	// Record on disk that this checkpoint attempt failed, so that
	// we won't worry about not being able to entirely delete it.
	std::string jobSpoolPath;
	SpooledJobFiles::getJobSpoolPath(jobAd, jobSpoolPath);
	std::filesystem::path spoolPath(jobSpoolPath);

	std::string manifestName;
	formatstr( manifestName, "_condor_checkpoint_MANIFEST.%.4d", checkpointNumber );
	std::string failureName;
	formatstr( failureName, "_condor_checkpoint_FAILURE.%.4d", checkpointNumber );
	std::error_code errorCode;
	std::filesystem::rename( spoolPath / manifestName, spoolPath / failureName, errorCode );
	if( errorCode.value() != 0 ) {
		// If no MANIFEST file was written, we can't clean up
		// anyway, so it doesn't matter if we didn't rename it.
		dprintf( D_FULLDEBUG,
			"Failed to rename %s to %s on checkpoint upload/validation failure, error code %d (%s)\n",
			(spoolPath / manifestName).string().c_str(),
			(spoolPath / failureName).string().c_str(),
			errorCode.value(), errorCode.message().c_str()
		);

		return;
	}

	// Clean up just this checkpoint attempt.  We don't actually
	// care if it succeeds; condor_preen is back-stopping us.
	//
	std::string checkpointDestination;
	if(! jobAd->LookupString( ATTR_JOB_CHECKPOINT_DESTINATION, checkpointDestination ) ) {
		dprintf( D_ALWAYS,
			"While handling a checkpoint event, could not find %s in job ad, which is required to attempt a checkpoint.\n",
			ATTR_JOB_CHECKPOINT_DESTINATION
		);

		return;
	}

	std::string globalJobID;
	jobAd->LookupString( ATTR_GLOBAL_JOB_ID, globalJobID );
	ASSERT(! globalJobID.empty());
	std::replace( globalJobID.begin(), globalJobID.end(), '#', '_' );

	formatstr( checkpointDestination, "%s/%s/%.4d",
		checkpointDestination.c_str(), globalJobID.c_str(),
		checkpointNumber
	);

	// The clean-up script is entitled to a copy of the job ad,
	// and the only way to give it one is via the filesystem.
	// It's clumsy, but for now, rather than deal with cleaning
	// this job ad file up, just store it in the job's SPOOL.
	// (Jobs with a checkpoint destination set don't transfer
	// anything from SPOOL except for the MANIFEST files, so
	// this won't cause problems even if the .job.ad file is
	// written by the starter before file transfer rather than
	// after.)
	std::string jobAdPath = jobSpoolPath;

	// FIXME: This invocation assumes that it's OK to block here
	// in the shadow until the clean-up attempt is done.  We'll
	// probably need to (refactor it into the cleanup utils and)
	// call spawnCheckpointCleanupProcessWithTimeout() -- or
	// something very similar -- and plumb through an additional
	// option specifying which specific checkpoint to clean-up.
	std::string error;
	manifest::deleteFilesStoredAt( checkpointDestination,
		(spoolPath / failureName).string(),
		jobAdPath,
		error,
		true /* this was a failed checkpoint */
	);

	// It's OK that we don't remove the .job.ad file and the
	// corresponding parent directory after a successful clean-up;
	// we know we'll need them again later, since we aren't
	// deleting all of the job's checkpoints, and the schedd's
	// call to condor_manifest will delete them when job exits
	// the queue.
}


//
// This syscall MUST ignore information it doesn't know how to deal with.
//
// The thinking for this syscall is the following: as Miron asks for more
// metrics, it's highly likely that there will be other event notifications
// that the starter needs to send to the shadow.  Rather than create a
// semantically significant remote syscall for each one, let's just create
// a single one that they can all use.  (As a new syscall, pools won't see
// correct values for the new metrics until both the shadow and the starter
// have been upgraded.)  However, rather than writing a bunch of complicated
// code to determine which version(s) can send what events to which
// version(s), we can just declare and require that this function just
// ignore attributes it doesn't know how to deal with (and not fail if an
// attribute has the wrong type).
//

int
pseudo_event_notification( const ClassAd & ad ) {
	std::string eventType;
	if(! ad.LookupString( "EventType", eventType )) {
		return GENERIC_EVENT_RV_NO_ETYPE;
	}

	ClassAd * jobAd = Shadow->getJobAd();
	ASSERT(jobAd);

	if( eventType == "ActivationExecutionExit" ) {
		thisRemoteResource->recordActivationExitExecutionTime(time(NULL));
	} else if( eventType == "SuccessfulCheckpoint" ) {
		std::string checkpointDestination;
		if(! jobAd->LookupString( ATTR_JOB_CHECKPOINT_DESTINATION, checkpointDestination ) ) {
			dprintf( D_TEST, "Not attempting to clean up checkpoints going to SPOOL.\n" );
			return GENERIC_EVENT_RV_OK;
		}

		int checkpointNumber = -1;
		if( ad.LookupInteger( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber ) ) {
			unsigned long numToKeep = 1 + param_integer( "DEFAULT_NUM_EXTRA_CHECKPOINTS", 1 );
			dprintf( D_STATUS | D_VERBOSE, "Checkpoint number %d succeeded, deleting all but the most recent %lu successful checkpoints.\n", checkpointNumber, numToKeep );

			// Before we can delete a checkpoint, it needs to be moved from the
			// job's spool to the job owner's checkpoint-cleanup directory.  So
			// we need to figure out which checkpoint(s) we want to keep and
			// move all of the other ones.

			std::string spoolPath;
			SpooledJobFiles::getJobSpoolPath( jobAd, spoolPath );
			std::filesystem::path spool( spoolPath );
			if(! (std::filesystem::exists(spool) && std::filesystem::is_directory(spool))) {
				dprintf(D_STATUS, "Checkpoint suceeded but job spool directory either doesn't exist or isn't a directory; not trying to clean up old checkpoints.\n" );
				return GENERIC_EVENT_RV_OK;
			}

			std::error_code errCode;
			std::set<long> checkpointsToSave;
			auto spoolDir = std::filesystem::directory_iterator(spool, errCode);
			if( errCode ) {
				dprintf( D_STATUS, "Checkpoint suceeded but job spool directory couldn't be checked for old checkpoints, not trying to clean them up: %d '%s'.\n", errCode.value(), errCode.message().c_str() );
				return GENERIC_EVENT_RV_OK;
			}
			for( const auto & entry : spoolDir ) {
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

						if( success ) {
							checkpointsToSave.insert(manifestNumber);
							if( checkpointsToSave.size() > numToKeep ) {
								checkpointsToSave.erase(checkpointsToSave.begin());
							}
						}
					}
				}
			}

			std::string buffer;
			formatstr( buffer, "Last %lu succesful checkpoints were: ", numToKeep );
			for( const auto & value : checkpointsToSave ) {
				formatstr_cat( buffer, "%ld ", value );
			}
			dprintf( D_STATUS | D_VERBOSE, "%s\n", buffer.c_str() );

			// Move all but checkpointsToSave's MANIFEST/FAILURE files.  The
			// schedd won't interrupt because it never does anything to the
			// job's spool while the shadow is running, and we won't be moving
			// (or removing) the shared intermediate directories: the schedd
			// will clean those up when the job leaves the queue, as normal.

			int cluster, proc;
			jobAd->LookupInteger( ATTR_CLUSTER_ID, cluster );
			jobAd->LookupInteger( ATTR_PROC_ID, proc );
			if(! moveCheckpointsToCleanupDirectory(
				cluster, proc, jobAd, checkpointsToSave
			)) {
				// Then there's nothing we can do here.
				return GENERIC_EVENT_RV_OK;
			}

			int CLEANUP_TIMEOUT = param_integer( "SHADOW_CHECKPOINT_CLEANUP_TIMEOUT", 300 );
			spawnCheckpointCleanupProcessWithTimeout( cluster, proc, jobAd, CLEANUP_TIMEOUT );
		}
	} else if( eventType == "FailedCheckpoint" ) {
		int checkpointNumber = -1;
		if( ad.LookupInteger( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber ) ) {
			dprintf( D_STATUS, "Checkpoint number %d failed, deleting it and updating schedd.\n", checkpointNumber );

			deleteCheckpoint( jobAd, checkpointNumber );

			// If the checkpoint upload succeeded, we have an on-disk (in
			// SPOOL) record of its success, and we'll find whether or not the
			// next iteration of the starter has the most-recent checkpoint
			// number.  Howver, if the checkpoint upload failed hard enough,
			// we may not have a failure manifest, so we should make sure the
			// next iteration of the starter doesn't attempt to overwrite an
			// existing checkpoint (and fail as a result).
			jobAd->Assign( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber );
			Shadow->updateJobInQueue( U_PERIODIC );
		}
	} else if( eventType == "InvalidCheckpointDownload" ) {
		int checkpointNumber = -1;
		if(! ad.LookupInteger( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber )) {
			dprintf( D_ALWAYS, "Starter sent an InvalidCheckpointDownload event notification, but the job has no checkpoint number; ignoring.\n" );
			return GENERIC_EVENT_RV_INCOMPLETE;
		}

		//
		// The starter can't just restart the job from scratch, because
		// the sandbox may have been modified by the attempt to transfer
		// the checkpoint -- we don't require that our plug-ins are
		// all-or-nothing.
		//
		dprintf( D_STATUS, "Checkpoint number %d was invalid, rolling back.\n", checkpointNumber );

		//
		// We should absolutely write an event to job's event log here
		// noting that the checkpoint was invalid and that we're rolling
		// back, but I'd have to think about what that event should
		// look like.
		//

		//
		// This moves the MANIFEST file out of the way, so that  even if
		// the deletion proper fails, we won't try to use the checkpoint
		// again.
		//
		// Arguably, we should distinguish between storage and retrieval
		// failures when we move the MANIFEST file, but I think it makes
		// sense to accept not being able to fully-delete a corrupt checkpoint.
		//
		deleteCheckpoint( jobAd, checkpointNumber );

		// Do NOT set the checkpoint number.  Either the most-recent
		// checkpoint failed to validate, in which case it's redundant,
		// or an earlier one did, and we don't want to cause an
		// upload failure later by overwriting it.  (Checkpoint numbers
		// should be monotonically increasing.)

		//
		// If it becomes necessary, the shadow could go into its
		// exit-to-requeue routine here.
		//
		return GENERIC_EVENT_RV_OK;
	} else if( eventType == ETYPE_DIAGNOSTIC_RESULT ) {
		std::string diagnostic;
		if(! ad.LookupString( ATTR_DIAGNOSTIC, diagnostic )) {
			dprintf( D_ALWAYS, "Starter sent a diagnostic result, but did not name which diagnostic; ignoring.\n" );
			return GENERIC_EVENT_RV_INCOMPLETE;
		}

		std::string result;
		if(! ad.LookupString( "Result", result ) ) {
			dprintf( D_ALWAYS, "Starter sent a diagnostic result for '%s', but it had no result.\n", diagnostic.c_str() );
			return GENERIC_EVENT_RV_INCOMPLETE;
		}

		if( result != "Completed" ) {
			dprintf( D_ALWAYS, "Diagnostic '%s' did not complete: '%s'\n",
				diagnostic.c_str(), result.c_str()
			);
			return GENERIC_EVENT_RV_OK;
		}


		int exitStatus;
		if(! ad.LookupInteger( "ExitStatus", exitStatus ) ) {
			dprintf( D_ALWAYS, "Starter sent a completed diagnostic result for '%s', but it had no exit status.\n", diagnostic.c_str() );
			return GENERIC_EVENT_RV_INCOMPLETE;
		}

		if( exitStatus != 0 ) {
			dprintf( D_ALWAYS, "Starter sent a completed diagnostic result for '%s', but its exit status was non-zero (%d)\n", diagnostic.c_str(), exitStatus );
			return GENERIC_EVENT_RV_OK;
		}


		std::string contents;
		if(! ad.LookupString( "Contents", contents ) ) {
			dprintf( D_ALWAYS, "Starter sent a completed diagnostic result for '%s', but it had no contents.\n", diagnostic.c_str() );
			return GENERIC_EVENT_RV_INCOMPLETE;
		}

		int decoded_bytes = 0;
		unsigned char * decoded = NULL;
		condor_base64_decode( contents.c_str(), & decoded, & decoded_bytes, false );
		if( decoded == NULL ) {
			dprintf( D_ALWAYS, "Failed to decode contents of diagnostic result for '%s'.\n", diagnostic.c_str() );
			return GENERIC_EVENT_RV_INCOMPLETE;
		}
		decoded[decoded_bytes] = '\0';


		if( diagnostic != DIAGNOSTIC_SEND_EP_LOGS ) {
			dprintf( D_ALWAYS, "Starter sent an unexpected diagnostic result (for '%s'); ignoring.\n", diagnostic.c_str() );
			dprintf( D_FULLDEBUG, "Result was '%s'\n", decoded );
			return GENERIC_EVENT_RV_CONFUSED;
		}

		// Write `decoded` to a well-known location.  We should probably
		// add a job-ad attribute which controls the location.
		std::string jobIWD;
		if( jobAd->LookupString( ATTR_JOB_IWD, jobIWD ) ) {
			std::filesystem::path iwd(jobIWD);
			std::filesystem::path diagnostic_dir( iwd / ".diagnostic" );

			if(! std::filesystem::exists( diagnostic_dir )) {
				std::error_code ec;
				std::filesystem::create_directory( diagnostic_dir, ec );
			}

			// I'm thinking that any logging here would be uninteresting.
			long long int clusterID = 0, procID = 0, numJobStarts = 0;
			jobAd->LookupInteger( ATTR_CLUSTER_ID, clusterID );
			jobAd->LookupInteger( ATTR_PROC_ID, procID );
			jobAd->LookupInteger( ATTR_NUM_JOB_STARTS, numJobStarts );

			std::string name;
			formatstr( name, "%s.%lld.%lld.%lld", diagnostic.c_str(), clusterID, procID, numJobStarts );
			std::filesystem::path output( diagnostic_dir / name );
			if(! htcondor::writeShortFile( output.string(), decoded, decoded_bytes )) {
				dprintf( D_ALWAYS, "Failed to write output for diagnostic '%s' to %s\n", diagnostic.c_str(), output.string().c_str() );
			}
		} else {
			dprintf( D_ALWAYS, "No IWD in job ad, not writing output for diagnostic '%s'\n", diagnostic.c_str() );
		}

		free( decoded );
		return GENERIC_EVENT_RV_OK;
	} else {
		dprintf( D_ALWAYS, "Ignoring unknown event type '%s'\n", eventType.c_str() );
	}

	return GENERIC_EVENT_RV_UNKNOWN;
}


