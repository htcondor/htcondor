#pragma once

#include <string>
#include <memory>
#include "dbHandler.h"
#include "JobRecord.h"
#include "config.hpp"

/**
 * @class Librarian
 * @brief Orchestrates history processing: reads static HTCondor files and syncs them to a database.
 *
 * Responsibilities:
 * - Initialize DB schema and handlers.
 * - Run update protocol:
 *   - Collect file info
 *   - Parse epoch and standard history files incrementally
 *   - Insert new job/epoch records into database
 *   - Update file metadata
 *
 * Notes:
 * - Currently supports static files only.
 * - Future versions will support file monitoring and rotation.
 */
class Librarian {
public:

    /**
     * @brief Initializes DBHandler and sets up the schema.
     * @return true on success, false on failure.
     */
    bool initialize();

    /**
     * @brief Runs the update protocol: parses new records and syncs to DB.
     * @return true on success, false on failure.
     */
    bool update();

    // Configuration manager
    LibrarianConfig config{};

private:

    // CONFIGS - right now, not used anywhere
    const size_t MAX_EPOCH_RECORDS_PER_UPDATE = 1000000;
    const size_t MAX_JOB_RECORDS_PER_UPDATE = 1000000;    


    // Members
    DBHandler dbHandler_{config};
    FileSet historyFileSet_;
    FileSet epochHistoryFileSet_;

    // TODO: cleanup status code
    StatusData statusData_;
    double EstimatedBytesPerJobInArchive_{0.0};
    int EstimatedJobsPerFileInArchive_{0};
    double EstimatedBytesPerJobInDatabase_{1024}; // Currently hard coded but ideally is calculated upon initialization
    double databaseSizeLimit_{0.0}; // Will be set by constructor (defaults to 2GB if not specified)
    double schemaVersionNumber_{1.0};

    // Helper methods for Librarian::update()
    bool readEpochRecords(std::vector<EpochRecord>& newEpochRecords, FileInfo& epochFileInfo);
    bool readJobRecords(std::vector<JobRecord>& newJobRecords, FileInfo& historyFileInfo);
    ArchiveChange trackAndUpdateFileSet(FileSet& fileSet);
    bool buildProcessingQueue(const FileSet& fileSet, const ArchiveChange& changes, std::vector<FileInfo>& queue);
    bool calculateEstimatedBytesPerJob();
    int calculateBacklogFromBytes(const Status& status);
    void updateStatusData(Status status);
    void estimateArrivalRateWhileAsleep();

    // Librarian::update() : Step 6 - Garbage Collection
    bool cleanupDatabaseIfNeeded();

};
