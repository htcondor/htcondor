/**
 * DBHandler manages an SQLite database that stores job history.
 *
 * Major components:
 * - jobIdCache: avoids repeated DB hits for JobId/JobListId lookups
 * - insertUnseenJob: populates missing User, JobList, and Job entries
 * - insertJobFileRecords: batch inserts JobRecords and updates File state atomically
 * - writeFileInfo / updateFileInfo: track file-level processing state
 * - Status tracking: records how much of each file has been processed
 */

#pragma once

#include <map>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "archive_reader.h"
#include "librarian_types.h"
#include "cache.hpp"
#include "config.hpp"


class DBHandler {
public:

    // Constructor & Destructor
    DBHandler() = delete;
    explicit DBHandler(const LibrarianConfig& c) : config(c) {};

    DBHandler(const DBHandler&) = delete;
    DBHandler(DBHandler&&) = delete;
    DBHandler& operator=(const DBHandler&) = delete;
    DBHandler& operator=(DBHandler&&) = delete;

    ~DBHandler();

    // Database initialization and maintenance
    bool initialize();

    bool testDatabaseConnection();
    bool verifyDatabaseSchema(const std::string& schemaSQL);

    // === Core Data Operations ===

    // Batch record insertion; updates File table offset atomically.
    bool insertJobFileRecords(const std::vector<ArchiveRecord>& records, const ArchiveFile& info);

    // File Information Operations
    void writeFileInfo(ArchiveFile& info);
    void updateFileInfo(const ArchiveFile& info);
    void markFileDeleted(long fileId, int64_t deletionTime);

    // Status and Monitoring Operations
    bool writeStatusAndData(const Status& status, const StatusData& statusData, bool rotateStatusData = false);
    bool maybeRecoverStatusAndFiles(std::map<std::string, ArchiveFile>& archiveFiles,
                                    StatusData& statusData,
                                    const std::string& directory);

    // === Garbage Collection ===
    bool runGarbageCollection(const std::string& gcSql, int targetFileLimit);

    // === Testing Support ===
    sqlite3* getDB() const { return db_; }

private:

    int getSchemaVersion();
    void pruneStatusTable(int64_t retentionSeconds);

    // Job Record Operations
    std::pair<int,int> jobIdLookup(int clusterId, int procId);
    bool insertUnseenJob(const std::string& owner, int clusterId, int procId, int64_t timeOfCreation);
    bool batchInsertJobRecords(const std::vector<ArchiveRecord>& records, long fileId);

    const LibrarianConfig& config;

    sqlite3*      db_{nullptr};
    sqlite3_stmt* jobIdLookupStmt_{nullptr};
    sqlite3_stmt* userInsertStmt_{nullptr};
    sqlite3_stmt* userSelectStmt_{nullptr};
    sqlite3_stmt* jobListInsertStmt_{nullptr};
    sqlite3_stmt* jobListSelectStmt_{nullptr};
    sqlite3_stmt* jobInsertStmt_{nullptr};
    sqlite3_stmt* jobSelectStmt_{nullptr};

    Cache<std::pair<int, int>, std::pair<int, int>> jobIdCache_;
};
