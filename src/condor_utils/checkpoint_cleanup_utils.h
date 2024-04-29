#ifndef   CHECKPOINT_CLEANUP_UTILS_H
#define   CHECKPOINT_CLEANUP_UTILS_H

bool fetchCheckpointDestinationCleanup(
    // Input paramters.
    const std::string & checkpointDestination,
    // Output parameters.
    std::string & cleanup, std::string & error
);

bool spawnCheckpointCleanupProcess(
    // Input parameters.
    int cluster, int proc, ClassAd * jobAd, int cleanup_reaper_id,
    // Output parameters.
    int & spawned_id, std::string & error
);

bool moveCheckpointsToCleanupDirectory(
    int cluster, int proc, ClassAd * jobAd,
    const std::set<long> & checkpointsToSave
);

#endif /* CHECKPOINT_CLEANUP_UTILS_H */
