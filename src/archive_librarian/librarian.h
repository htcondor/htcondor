#pragma once

#include <string>
#include <memory>
#include "dbHandler.h"
#include "JobRecord.h"

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
    Librarian(const std::string& schemaPath,
              const std::string& dbPath,
              const std::string& historyFilePath,
              const std::string& epochHistoryFilePath,
              size_t jobCacheSize = 10000);

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


    DBHandler& getDBHandler() const { return *dbHandler_; }

private:

    // CONFIGS - right now, not used anywhere
    const size_t MAX_EPOCH_RECORDS_PER_UPDATE = 10000;  
    const size_t MAX_JOB_RECORDS_PER_UPDATE = 10000;    


    // Members
    std::string schemaPath_;
    std::string dbPath_;
    FileSet historyFileSet_;
    FileSet epochHistoryFileSet_;
    std::string historyFilePath_; // redundant, but hard to get rid of right now
    std::string epochHistoryFilePath_;
    size_t jobCacheSize_;
    std::unique_ptr<DBHandler> dbHandler_;
    StatusData statusData_;
    double EstimatedBytesPerJob_;

    bool readEpochRecords(std::vector<EpochRecord>& newEpochRecords, FileInfo& epochFileInfo);
    bool readJobRecords(std::vector<JobRecord>& newJobRecords, FileInfo& historyFileInfo);
    ArchiveChange trackAndUpdateFileSet(FileSet& fileSet);
    bool buildProcessingQueue(const FileSet& fileSet, const ArchiveChange& changes, std::vector<FileInfo>& queue);
    bool calculateEstimatedBytesPerJob();
    int calculateBacklogFromBytes(const Status& status);
    void updateStatusData(Status status);
    void estimateArrivalRateWhileAsleep();

};
