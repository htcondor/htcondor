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

    // Testing whether database was correctly constructed and connection works
    bool testDatabaseConnection();
    bool verifyDatabaseSchema(const std::string& schemaSQL);

    // === Core Data Operations ===
    
    // Batch record insertion for file processing
    // Records 'FileRecords' - ie, it batches both the JobRecord insertion and the 'File' table update
    bool insertJobFileRecords(const std::vector<JobRecord>& jobs, const FileInfo& fileInfo);

    // File Information Operations
    void writeFileInfo(FileInfo &info);
    void updateFileInfo(FileInfo historyFile);

    // Status and Monitoring Operations
    bool writeStatusAndData(const Status& status, const StatusData& statusData);
    bool maybeRecoverStatusAndFiles(FileSet& historyFileSet_, StatusData& statusData_);

    // === File Archive Management ===
    
    // File monitoring and lifecycle operations
    bool insertNewFilesAndMarkOldOnes(ArchiveChange& fileSetChange);

    // === Garbage Collection ===
    // Runs garbageCollection query saved in Librarian and passed to dbHandler 
    bool runGarbageCollection(const std::string &gcSql, int targetFileLimit);

    // === Testing Support ===
    // Helper function to support testing utilities
    sqlite3* getDB() const { return db_; }

private:

    int getSchemaVersion();

    // Job Record Operations
    std::pair<int,int> jobIdLookup(int clusterId, int procId);
    bool insertUnseenJob(const std::string& owner, int clusterId, int procId, int64_t timeOfCreation);

    // Batch processes and inserts Epoch/Job records
    bool batchInsertJobRecords(const std::vector<JobRecord>& jobs);

    const LibrarianConfig& config;

    sqlite3* db_{nullptr};
    // TODO: Make map{name:stmt}?
    sqlite3_stmt* jobIdLookupStmt_{nullptr}; // preprepared statement for more efficient JobIdLookups
    sqlite3_stmt* userInsertStmt_{nullptr};
    sqlite3_stmt* userSelectStmt_{nullptr};
    sqlite3_stmt* jobListInsertStmt_{nullptr};
    sqlite3_stmt* jobListSelectStmt_{nullptr};
    sqlite3_stmt* jobInsertStmt_{nullptr};
    sqlite3_stmt* jobSelectStmt_{nullptr};
    // TODO: Split out caches: Owner->Id, Cluster->JobListId, <Cluster, Proc>->JobId
    Cache<std::pair<int, int>, std::pair<int, int>> jobIdCache_;
};
