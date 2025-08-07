#pragma once

#include <string>
#include <memory>
#include "dbHandler.h"
#include "JobRecord.h"
#include "JobQueryStructures.h"

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
    Librarian(const std::string& dbPath,
              const std::string& historyFilePath,
              const std::string& epochHistoryFilePath,
              size_t jobCacheSize = 10000, 
              size_t databaseSize = 2ULL * 1024 * 1024 * 1024, 
              double schemaVersionNumber = 1.0);

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

    int query(int argc, char* argv[]);


    DBHandler& getDBHandler() const { return *dbHandler_; }

private:

    // CONFIGS - right now, not used anywhere
    const size_t MAX_EPOCH_RECORDS_PER_UPDATE = 1000000;
    const size_t MAX_JOB_RECORDS_PER_UPDATE = 1000000;    


    // Members
    std::string dbPath_;
    FileSet historyFileSet_;
    FileSet epochHistoryFileSet_;
    std::string historyFilePath_; // redundant, but hard to get rid of right now
    std::string epochHistoryFilePath_;
    std::string gcQuerySQL_{}; 
    size_t jobCacheSize_;
    std::unique_ptr<DBHandler> dbHandler_;
    StatusData statusData_;
    double EstimatedBytesPerJobInArchive_{0.0};
    int EstimatedJobsPerFileInArchive_{0};
    double EstimatedBytesPerJobInDatabase_{1024}; // Currently hard coded but ideally is calculated upon initialization
    double databaseSizeLimit_{0.0}; // Will be set by constructor (defaults to 2GB if not specified)
    double schemaVersionNumber_{1.0};

    // Helper methods for Librarian::initialize()
    std::string loadSchemaSQL();
    bool loadGCSQL();

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

    // Helper methods for Librarian::query()
    QueryResult executeQuery(const std::string& username, int clusterId, std::string historyDirectoryPath);
    std::vector<std::string> readJobsGroupedByFile(const std::vector<QueriedJobRecord>& jobRecords, std::string historyDirectoryPath);
    std::vector<ParsedJobRecord> parseClassAds(const std::vector<std::string>& rawClassAds);
    std::optional<ParsedJobRecord> parseClassAd(const std::string& classAdText);
    std::optional<std::string> readJobAtOffset(const std::string& filePath, long offset);

};
