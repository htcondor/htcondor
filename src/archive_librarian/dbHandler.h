/**
 * DBHandler manages an SQLite database that stores job history.
 *
 * Major components:
 * - jobIdCache: avoids repeated DB hits for JobId/JobListId lookups
 * - insertUnseenJob: populates missing User, JobList, and Job entries
 * - insertJobFileRecords/insertEpochFileRecords: batch inserts JobRecords and updates their File info in one transaction
 * - writeFileInfo / updateFileInfo: track file-level processing state
 * - Status tracking: records how much of each file has been processed
 */

#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <list>
#include "JobRecord.h"
#include "archiveMonitor.h"
#include "cache.hpp"
#include "JobQueryStructures.h"


class DBHandler {
public:

    // Constructor & Destructor
    DBHandler() = delete;
    explicit DBHandler(const std::string& schemaPath, const std::string& dbPath, size_t maxCacheSize = 10000);
    DBHandler(const DBHandler&) = delete;
    DBHandler(DBHandler&&) = delete;
    DBHandler& operator=(const DBHandler&) = delete;
    DBHandler& operator=(DBHandler&&) = delete;
    ~DBHandler();

    // Database initialization and maintenance
    bool initializeFromSchema(const std::string& schemaSQL);
    bool clearCache();

    // Testing whether database was correctly constructed and connection works
    bool testDatabaseConnection();
    bool verifyDatabaseSchema(const std::string& schemaSQL);

    // === Core Data Operations ===
    
    // Batch record insertion for file processing
    // Records 'FileRecords' - ie, it batches both the JobRecord insertion and the 'File' table update
    bool insertEpochFileRecords(const std::vector<EpochRecord>& records, const FileInfo& fileInfo);
    bool insertJobFileRecords(const std::vector<JobRecord>& jobs, const FileInfo& fileInfo);

    // File Information Operations
    void writeFileInfo(FileInfo &info);
    void updateFileInfo(FileInfo epochHistoryFile, FileInfo historyFile);

    // Status and Monitoring Operations
    bool writeStatusAndData(const Status& status, const StatusData& statusData);
    bool maybeRecoverStatusAndFiles(FileSet& historyFileSet_, FileSet& epochHistoryFileSet_, StatusData& statusData_);

    // === File Archive Management ===
    
    // File monitoring and lifecycle operations
    bool insertNewFilesAndMarkOldOnes(ArchiveChange& fileSetChange);

    // === Garbage Collection ===
    
    // Runs garbageCollection query saved in Librarian and passed to dbHandler 
    bool runGarbageCollection(const std::string &gcSql, int targetFileLimit);
;
    // === User Query Interface ===
    
    // User-facing data retrieval functions
    std::vector<std::tuple<int, int, int>> getJobsForUser(const std::string& username);
    std::vector<std::pair<std::string, int>> getJobCountsPerUser();
    bool queryJobRecordsForJobList(const std::string& username, int clusterId, std::vector<QueriedJobRecord>& jobRecords);

    // === Testing Support ===
    
    // Helper function to support testing utilities
    sqlite3* getDB() const { return db_; }

private:

    // Job Record Operations
    std::pair<int,int> jobIdLookup(int clusterId, int procId);
    bool insertUnseenJob(const std::string& owner, int clusterId, int procId, int64_t timeOfCreation);

    // Batch processes and inserts Epoch/Job records
    void batchInsertEpochRecords(const std::vector<EpochRecord>& records);
    bool batchInsertJobRecords(const std::vector<JobRecord>& jobs);

    sqlite3* db_{nullptr};
    sqlite3_stmt* jobIdLookupStmt_{nullptr}; // preprepared statement for more efficient JobIdLookups
    sqlite3_stmt* userInsertStmt_{nullptr};
    sqlite3_stmt* userSelectStmt_{nullptr};
    sqlite3_stmt* jobListInsertStmt_{nullptr};
    sqlite3_stmt* jobListSelectStmt_{nullptr};
    sqlite3_stmt* jobInsertStmt_{nullptr};
    sqlite3_stmt* jobSelectStmt_{nullptr};
    Cache<std::pair<int, int>, std::pair<int, int>> jobIdCache_;
};
