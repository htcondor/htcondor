#pragma once

#include <string>
#include <memory>
#include "dbHandler.h"

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
    std::string schemaPath_;
    std::string dbPath_;
    std::string historyFilePath_;
    std::string epochHistoryFilePath_;
    size_t jobCacheSize_;
    std::unique_ptr<DBHandler> dbHandler_;

    bool updateFileInfo(FileInfo& epochFileInfo, FileInfo& historyFileInfo);
    bool readEpochRecords(std::vector<EpochRecord>& newEpochRecords, FileInfo& epochFileInfo, Status& status);
    bool readJobRecords(std::vector<JobRecord>& newJobRecords, FileInfo& historyFileInfo, Status& status);
    void insertRecords(const std::vector<EpochRecord>& epochRecords, const std::vector<JobRecord>& jobRecords);

};
