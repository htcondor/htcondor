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
#include <unordered_map>
#include <list>
#include "JobRecord.h"
#include "archiveMonitor.h"
#include "cache.hpp"

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
    bool initializeFromSchema(const std::string& schemaPath);
    bool clearCache() const;
    bool testConnection();

    // Paired write & read functions
    void insertJobRecord(const JobRecord& job);
    void batchInsertJobRecords(const std::vector<JobRecord>& jobs);

    void writeFileInfo(FileInfo &info);
    FileInfo readFileById(int fileId);

    StatusData getStatusDataFromDB();
    bool writeStatus(const Status& status);
    bool DBHandler::writeStatusAndData(const Status& status, const StatusData& statusData);
    Status readLastStatus();
    bool maybeRecoverStatusAndFiles(FileSet& historyFileSet_,
                           FileSet& epochHistoryFileSet_,
                           StatusData& statusData_);
    

    std::pair<int,int> jobIdLookup(int clusterId, int procId);
    void updateFileInfo(FileInfo epochHistoryFile, FileInfo historyFile);

    // Helper functions for archiveMonitor and status
    FileInfo lastFileRead(const std::string& type); // To get the last file read from the Status table
    int countUnprocessedFiles(); // Counts files with 0 lastOffset 
    bool insertNewFiles(const std::pair<int, std::string>& rotatedFile, std::vector<FileInfo>& newFiles);
    bool deleteFiles(const std::vector<int>& deletedFileIds);
    bool insertEpochFileRecords(const std::vector<EpochRecord>& records, const FileInfo& fileInfo);
    bool insertJobFileRecords(const std::vector<JobRecord>& jobs, const FileInfo& fileInfo);
    


    // To read EpochHistory file with all record types
    bool insertUnseenJob(const std::string& owner, int clusterId, int procId, int64_t timeOfCreation);
    void batchInsertEpochRecords(const std::vector<EpochRecord>& records);

    // User query functions
    std::vector<std::tuple<int, int, int>> getJobsForUser(const std::string& username);
    std::vector<std::pair<std::string, int>> getJobCountsPerUser();

    // Helper function for getting the database for dbHandlerTestUtils
    sqlite3* getDB() const { return db_; }

private:

    class JobIdCache {
    public:
        // Composite key type for (clusterId, procId)
        struct Key {
            int clusterId;
            int procId;
            
            bool operator<(const Key& other) const {
                if (clusterId != other.clusterId) {
                    return clusterId < other.clusterId;
                }
                return procId < other.procId;
            }
            
            bool operator==(const Key& other) const {
                return clusterId == other.clusterId && procId == other.procId;
            }
        };
        
        // Value type storing jobId, jobListId, and timestamp
        struct Value {
            int jobId;
            int jobListId;
            long timestamp;
            
            Value() : jobId(-1), jobListId(-1), timestamp(0) {}
            Value(int jId, int jListId, long ts) : jobId(jId), jobListId(jListId), timestamp(ts) {}
        };

        explicit JobIdCache(size_t maxSize = 10000);
        ~JobIdCache() = default;  

        std::pair<int,int> get(int clusterId, int procId);
        void put(int clusterId, int procId, int jobId, int jobListId, long timestamp); 
        void updateTimestamp(int clusterId, int procId, long timestamp);  
        bool exists(int clusterId, int procId) const;
        void remove(int clusterId, int procId);
        size_t size() const;
        void clear();


    private:
        Cache<Key, Value> cache_;  // Replace all the old private members with this
    };


    sqlite3* db_{nullptr};
    sqlite3_stmt* jobIdLookupStmt_; // preprepared statement for more efficient JobIdLookups
    sqlite3_stmt* userInsertStmt_;
    sqlite3_stmt* userSelectStmt_;
    sqlite3_stmt* jobListInsertStmt_;
    sqlite3_stmt* jobListSelectStmt_;
    sqlite3_stmt* jobInsertStmt_;
    sqlite3_stmt* jobSelectStmt_;
    std::unique_ptr<JobIdCache> jobIdCache_;
};
