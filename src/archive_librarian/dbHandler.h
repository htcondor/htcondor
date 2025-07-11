/**
 * DBHandler manages an SQLite database that stores job history.
 *
 * Major components:
 * - jobIdCache: avoids repeated DB hits for JobId/JobListId lookups
 * - insertUnseenJob: populates missing User, JobList, and Job entries
 * - batchInsertJobRecords / batchInsertEpochRecords: main entrypoints for history processing
 * - writeFileInfo / updateFileInfo: track file-level processing state
 * - Status tracking: records how much of each file has been processed
 */

#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
#include "JobRecord.h"
#include <unordered_map>
#include <list>

// Hash definition so we can use it later
struct pair_hash {
    std::size_t operator()(const std::pair<int, int>& p) const {
        return std::hash<int>{}(p.first) ^ (std::hash<int>{}(p.second) << 1);
    }
};

class JobIdLookup;

class DBHandler {
public:
    explicit DBHandler(const std::string& schemaPath, const std::string& dbPath, size_t maxCacheSize = 10000);
    ~DBHandler();

    // Paired write & read functions
    void insertJobRecord(const JobRecord& job);
    void batchInsertJobRecords(const std::vector<JobRecord>& jobs);
    std::vector<JobRecord> readAllJobs();

    void writeFileInfo(FileInfo &info);
    FileInfo readFileById(int fileId);
    std::vector<FileInfo> readAllFileInfos(); // Helpful for testing

    void writeStatus(const Status& status);
    Status readLastStatus();

    std::pair<int,int> jobIdLookup(int clusterId, int procId);
    void updateFileInfo(FileInfo epochHistoryFile, FileInfo historyFile);

    // Helper functions for archiveMonitor and status
    FileInfo lastFileRead(const std::string& type); // To get the last file read from the Status table
    int countUnprocessedFiles(); // Counts files with 0 lastOffset 

    // To read EpochHistory file with all record types
    bool insertUnseenJob(const std::string& owner, int clusterId, int procId, int64_t timeOfCreation);
    void batchInsertEpochRecords(const std::vector<EpochRecord>& records);

    // User query functions
    std::vector<std::tuple<int, int, int>> getJobsForUser(const std::string& username);
    std::vector<std::pair<std::string, int>> getJobCountsPerUser();

    // Helper function for getting the database for testing, setting up schema, and clearing database
    sqlite3* getDB() const { return db; }
    bool initializeFromSchema(const std::string& schemaPath);
    void writeTesterSpawnAd(int clusterId, int procId, const std::string& owner, long timeOfCreation);
    int countTable (const std::string& table) const;
    bool clearAllTables();
    void clearCache();


private:

    class JobIdCache {
    public:
        explicit JobIdCache(size_t maxSize = 10000);
        ~JobIdCache();

        std::pair<int,int> get(int clusterId, int procId);
        void put(int clusterId, int procId, int jobId, int jobListId, long timestamp);
        void updateTimestamp(int clusterId, int procId, long timestamp);
        bool exists(int clusterId, int procId) const;
        void garbageCollect(long minTimestamp);
        void remove(int clusterId, int procId);
        void clear();

    private:
        sqlite3* db_ = nullptr;
        sqlite3_stmt* stmt_ = nullptr;
        size_t maxSize_;

        // Key type alias for convenience
        using Key = std::pair<int,int>;

        // List to track usage order for LRU eviction (front = most recent)
        std::list<Key> usageOrder_;

        // Cache maps Key -> tuple(value + iterator into usageOrder)
        std::unordered_map<Key, std::tuple<int,int,long,std::list<Key>::iterator>, pair_hash> cache_;
    };


    sqlite3* db;
    sqlite3_stmt* jobIdLookupStmt_; // preprepared statement for more efficient JobIdLookups
    sqlite3_stmt* userInsertStmt_;
    sqlite3_stmt* userSelectStmt_;
    sqlite3_stmt* jobListInsertStmt_;
    sqlite3_stmt* jobListSelectStmt_;
    sqlite3_stmt* jobInsertStmt_;
    sqlite3_stmt* jobSelectStmt_;
    std::unique_ptr<JobIdCache> jobIdCache;
};
