/**
 * @file dbHandler.cpp
 * @brief DBHandler class that manages all interactions with the SQLite database.
 *
 * Responsibilities:
 * - Insert job records and metadata into relevant tables.
 * - Maintain and cache job ID mappings for fast lookup.
 * - Track which parts of history/epoch files have been processed.
 * - Provide user-level analytics (e.g., jobs per user).
 *
 * Notes:
 * - All common DB statements are prepared once at startup and reused.
 * - Most write functions use INSERT OR IGNORE + SELECT fallback.
 * - A small in-memory LRU cache is used to avoid repeated lookups for {ClusterId, ProcId}.
 * - I plan to split this into different files/modules
 * - I also plan to write a transaction.cpp file to wrap most transactions safely
 */
 
#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream> 
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <list>
#include "JobRecord.h"
#include "dbHandler.h"
#include "archiveMonitor.h"

namespace fs = std::filesystem;


// -------------------------
// JobIdCache Class Lookup & Insert
// -------------------------

/** 
* The JobIdCache class provides fast lookup and insertion
* of Job IDs to avoid repeated database queries and speed up processing.
* It maintains a cache of cluster+proc pairs and their an associated
* <JobId, JobListId, TimeOfCreation> tuple. 
* The cache uses LRU garbage processing. 
*/
DBHandler::JobIdCache::JobIdCache(size_t maxSize) : cache_(maxSize) {}

std::pair<int, int> DBHandler::JobIdCache::get(int clusterId, int procId) {
    Key key = {clusterId, procId};
    
    if (!cache_.contains(key)) {
        return {-1, -1}; // Not found
    }
    
    // Access the value (this updates the LRU automatically in the templated cache)
    const Value& value = cache_[key];
    return {value.jobId, value.jobListId};
}

void DBHandler::JobIdCache::put(int clusterId, int procId, int jobId, int jobListId, long timestamp) {
    Key key = {clusterId, procId};
    Value value(jobId, jobListId, timestamp);
    
    // The templated cache handles LRU eviction automatically
    cache_.insert(key, value);
}

void DBHandler::JobIdCache::updateTimestamp(int clusterId, int procId, long timestamp) {
    Key key = {clusterId, procId};
    
    if (cache_.contains(key)) {
        // Get current value, update timestamp, and re-insert
        Value& value = cache_[key]; // This updates LRU access time
        value.timestamp = std::max(value.timestamp, timestamp);
    }
}

bool DBHandler::JobIdCache::exists(int clusterId, int procId) const {
    Key key = {clusterId, procId};
    return cache_.contains(key);
}


void DBHandler::JobIdCache::remove(int clusterId, int procId) {
    Key key = {clusterId, procId};
    cache_.remove(key);
}

size_t DBHandler::JobIdCache::size() const {
    return cache_.size();
}

void DBHandler::JobIdCache::clear() {
    cache_.clear();
}


// -------------------------
// DBHandler Constructor / Destructor
// -------------------------

DBHandler::DBHandler(const std::string& schemaPath, const std::string& dbPath, size_t maxCacheSize)
    : db_(nullptr), jobIdLookupStmt_(nullptr), userInsertStmt_(nullptr), userSelectStmt_(nullptr), 
    jobListInsertStmt_(nullptr), jobListSelectStmt_(nullptr)
{
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_); // technically redundant if db is already bad, but harmless
        throw std::runtime_error("Failed to open SQLite DB: " + err); // TODO CHANGE THIS
    }

    if (!initializeFromSchema(schemaPath)) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to initialize schema");
    }

    jobIdCache_ = std::make_unique<JobIdCache>(maxCacheSize);

    const char* sql = "SELECT JobId, JobListId FROM Jobs WHERE ClusterId = ? AND ProcId = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &jobIdLookupStmt_, nullptr) != SQLITE_OK) {
        printf ("[ERROR] Failed to prepare jobIdLookupStmt_: %s \n ", sqlite3_errmsg(db_));
        jobIdLookupStmt_ = nullptr;
    }

    const char* userInsertSQL = "INSERT OR IGNORE INTO Users (UserName) VALUES (?)";
    if (sqlite3_prepare_v2(db_, userInsertSQL, -1, &userInsertStmt_, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare userInsertStmt: %s\n", sqlite3_errmsg(db_));
        userInsertStmt_ = nullptr;
    }

    const char* userLookupSQL = "SELECT UserId FROM Users WHERE UserName = ?";
    if (sqlite3_prepare_v2(db_, userLookupSQL, -1, &userSelectStmt_, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare userSelectStmt: %s\n", sqlite3_errmsg(db_));
        userSelectStmt_ = nullptr;
    }

    const char* jobListInsertSQL = "INSERT OR IGNORE INTO JobLists (ClusterId, UserId) VALUES (?, ?)";
    if (sqlite3_prepare_v2(db_, jobListInsertSQL, -1, &jobListInsertStmt_, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare jobListInsertStmt: %s\n", sqlite3_errmsg(db_));
        jobListInsertStmt_ = nullptr;
    }

    const char* jobListLookupSQL = "SELECT JobListId FROM JobLists WHERE ClusterId = ? AND UserId = ?";
    if (sqlite3_prepare_v2(db_, jobListLookupSQL, -1, &jobListSelectStmt_, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare jobListSelectStmt: %s\n", sqlite3_errmsg(db_));
        jobListSelectStmt_ = nullptr;
    }

    const char* jobInsertSQL = "INSERT OR IGNORE INTO Jobs (ClusterId, ProcId, UserId, JobListId, TimeOfCreation) VALUES (?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db_, jobInsertSQL, -1, &jobInsertStmt_, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare jobInsertStmt: %s\n", sqlite3_errmsg(db_));
        jobInsertStmt_ = nullptr;
    }

    const char* jobLookupSQL = "SELECT JobId FROM Jobs WHERE ClusterId = ? AND ProcId = ?";
    if (sqlite3_prepare_v2(db_, jobLookupSQL, -1, &jobSelectStmt_, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare jobSelectStmt: %s\n", sqlite3_errmsg(db_));
        jobSelectStmt_ = nullptr;
    }

}

DBHandler::~DBHandler() {
    if (jobIdLookupStmt_) {
        sqlite3_finalize(jobIdLookupStmt_);
        jobIdLookupStmt_ = nullptr;
    }

    if (userInsertStmt_) {
        sqlite3_finalize(userInsertStmt_);
        userInsertStmt_ = nullptr;
    }   

    if (userSelectStmt_) {
        sqlite3_finalize(userSelectStmt_);
        userSelectStmt_ = nullptr;
    }

    if (jobListInsertStmt_) {
        sqlite3_finalize(jobListInsertStmt_);
        jobListInsertStmt_ = nullptr;
    }

    if (jobListSelectStmt_) {
        sqlite3_finalize(jobListSelectStmt_);
        jobListSelectStmt_ = nullptr;
    }

    if (jobInsertStmt_) {
        sqlite3_finalize(jobInsertStmt_);
        jobInsertStmt_ = nullptr;
    }

    if (jobSelectStmt_) {
        sqlite3_finalize(jobSelectStmt_);
        jobSelectStmt_ = nullptr;
    }

    

    if (db_) {
        int rc = sqlite3_close(db_);
        if (rc != SQLITE_OK) {
            printf("[ERROR] Failed to close db: %s \n", sqlite3_errmsg(db_));
        }
        db_ = nullptr;
    }
}


bool DBHandler::initializeFromSchema(const std::string& schemaSQL) {
    // Enable foreign key constraints
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        printf("[ERROR] Failed to enable foreign keys: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    rc = sqlite3_exec(db_, schemaSQL.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        printf("[ERROR] Failed to initialize DB schema: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool DBHandler::clearCache() const {
    jobIdCache_->clear();
    size_t currentSize = jobIdCache_->size();
    return(currentSize == 0);
}

// For Librarian to check whether DBHandler has been initialized and works properly upon startup
bool DBHandler::testConnection() {
    // 1. Check database handle exists
    if (!db_) {
        fprintf(stderr, "[DBHandler] Database handle is null\n");
        return false;
    }
    
    // 2. Test basic connectivity
    int result = sqlite3_exec(db_, "SELECT 1;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        fprintf(stderr, "[DBHandler] Basic query failed: %s\n", sqlite3_errmsg(db_));
        return false;
    }
    
    // 3. Verify required tables exist
    const char* checkTablesQuery = R"(
        SELECT name FROM sqlite_master 
        WHERE type='table' AND name IN (
            'Files',
            'Users',
            'JobLists',
            'Jobs',
            'JobRecords',
            'Status',
            'StatusData'
        );
    )";

    sqlite3_stmt* stmt;
    result = sqlite3_prepare_v2(db_, checkTablesQuery, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        fprintf(stderr, "[DBHandler] Table check prepare failed: %s\n", 
                sqlite3_errmsg(db_));
        return false;
    }
    
    int tableCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        tableCount++;
    }
    sqlite3_finalize(stmt);
    
    if (tableCount < 7) {  
        fprintf(stderr, "[DBHandler] Missing required tables (found %d, expected 4)\n", 
                tableCount);
        return false;
    }
    
    return true;
}


// -------------------------
// JobRecord Insertion
// -------------------------

// Checks cache for {clusterId, procID} pair before checking database
// Returns {-1, -1} if there was an error, 
// or if it was not in the cache or the database
std::pair<int, int> DBHandler::jobIdLookup(int clusterId, int procId) {
    std::pair<int, int> key = {clusterId, procId};

    // Step 1: Try cache
    auto cached = jobIdCache_->get(clusterId, procId);
    if (cached.first != -1 && cached.second != -1) {
        return cached;
    }

    // Step 2: Fallback to DB query
    if (!jobIdLookupStmt_) {
        printf("[ERROR] jobIdLookupStmt_ is null\n");
        return {-1, -1};
    }

    sqlite3_reset(jobIdLookupStmt_);
    sqlite3_clear_bindings(jobIdLookupStmt_);
    sqlite3_bind_int(jobIdLookupStmt_, 1, clusterId);
    sqlite3_bind_int(jobIdLookupStmt_, 2, procId);

    // If found in db and not cache, insert into cache for future entries 
    int rc = sqlite3_step(jobIdLookupStmt_);
    if (rc == SQLITE_ROW) {
        int jobId = sqlite3_column_int(jobIdLookupStmt_, 0);
        int jobListId = sqlite3_column_int(jobIdLookupStmt_, 1);
        jobIdCache_->put(clusterId, procId, jobId, jobListId, -1); 
        return {jobId, jobListId};
    }

    if (rc != SQLITE_DONE) {
        printf("[ERROR] sqlite3_step failed for ClusterId= %d, ProcId= %d - %s \n", 
        clusterId, procId, sqlite3_errmsg(db_));
    } 

    return {-1, -1};
}


int getOrInsertIdPrepared(
    sqlite3_stmt* insertStmt,
    sqlite3_stmt* selectStmt,
    std::function<void(sqlite3_stmt*)> bindInsert,
    std::function<void(sqlite3_stmt*)> bindSelect,
    std::function<int(sqlite3_stmt*)> extractId
) {
    int id = -1;

    bindInsert(insertStmt);
    int rc = sqlite3_step(insertStmt);

    if (rc == SQLITE_DONE) {
        // Insert succeeded
        id = static_cast<int>(sqlite3_last_insert_rowid(sqlite3_db_handle(insertStmt)));
    } else if (rc != SQLITE_CONSTRAINT) {
        // Duplication errors are possible, so make sure this isn't that
        // Something else unexpected happened
        fprintf(stderr, "[ERROR] Insert failed: %s\n", sqlite3_errmsg(sqlite3_db_handle(insertStmt)));
    }

    sqlite3_reset(insertStmt);
    sqlite3_clear_bindings(insertStmt);

    if (id == -1) {
        // Insert either failed due to constraint or didn't run at all — do SELECT fallback
        bindSelect(selectStmt);
        rc = sqlite3_step(selectStmt);
        if (rc == SQLITE_ROW) {
            id = extractId(selectStmt);
        } else {
            fprintf(stderr, "[ERROR] Select fallback failed: %s\n", sqlite3_errmsg(sqlite3_db_handle(selectStmt)));
        }
        sqlite3_reset(selectStmt);
        sqlite3_clear_bindings(selectStmt);
    }

    return id;
}

/**
 * Insert an unseen job and its associated metadata into the database and cache
 * Inserts entries into 'User', 'JobLists', and 'Job' tables in database
 */
bool DBHandler::insertUnseenJob(const std::string& owner, int clusterId, int procId, int64_t timeOfCreation) {
    // 1. Insert User and get userId
    int userId = getOrInsertIdPrepared(
        userInsertStmt_,
        userSelectStmt_,
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, owner.c_str(), -1, SQLITE_TRANSIENT);
        },
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, owner.c_str(), -1, SQLITE_TRANSIENT);
        },
        [](sqlite3_stmt* stmt) {
            return sqlite3_column_int(stmt, 0);
        }
    );
    if(userId == -1) {
        printf("[ERROR] Failed to insert/lookup user: %s\n", owner.c_str());
        return false;
    }

    // 2. Insert JobList and get jobListId
    int jobListId = getOrInsertIdPrepared(
        jobListInsertStmt_,
        jobListSelectStmt_,
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, clusterId);
            sqlite3_bind_int(stmt, 2, userId);
        },
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, clusterId);
            sqlite3_bind_int(stmt, 2, userId);
        },
        [](sqlite3_stmt* stmt) {
            return sqlite3_column_int(stmt, 0);
        }
    );
    if(jobListId == -1) {
        printf("[ERROR] Failed to insert/lookup jobListId for {cluster, proc} : {%d, %d}\n", clusterId, procId);
        return false;
    }

    // 3. Insert Job and get jobId
    int jobId = getOrInsertIdPrepared(
        jobInsertStmt_,
        jobSelectStmt_,
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, clusterId);
            sqlite3_bind_int(stmt, 2, procId);
            sqlite3_bind_int(stmt, 3, userId);
            sqlite3_bind_int(stmt, 4, jobListId);
            sqlite3_bind_int64(stmt, 5, timeOfCreation);  // assume column for time
        },
        [&](sqlite3_stmt* stmt) {
            sqlite3_bind_int(stmt, 1, clusterId);
            sqlite3_bind_int(stmt, 2, procId);
        },
        [](sqlite3_stmt* stmt) {
            return sqlite3_column_int(stmt, 0);
        }
    );
    if(jobId == -1) {
        printf("[ERROR] Failed to insert/lookup jobId for {cluster, proc} : {%d, %d}\n", clusterId, procId);
        return false;
    }

    // 4. Cache the result
    jobIdCache_->put(clusterId, procId, jobId, jobListId, timeOfCreation); 
    return true;
}

/**
 * Batch insert multiple JobRecords in a single transaction.
 */
void DBHandler::batchInsertJobRecords(const std::vector<JobRecord>& jobs) {
    if (jobs.empty()) {
        return;
    }

    const char* sql = R"(
        INSERT INTO JobRecords (Offset, CompletionDate, JobId, FileId, JobListId)
        VALUES (?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        printf("[ERROR] Failed to prepare JobRecords insert: %s \n", sqlite3_errmsg(db_));
        return;
    }

    for (const auto& job : jobs) {
        std::pair <int, int> jobIdCacheResults = jobIdLookup(job.ClusterId, job.ProcId);
        int jobId = jobIdCacheResults.first;
        int jobListId = jobIdCacheResults.second;

        if (jobId == -1) { 
            printf("[ERROR] JobRecord was not preceded by Spawn ad, no matching JobId\n"
                "Writing JobAd for ClusterId %d, ProcId %d\n", 
                job.ClusterId, job.ProcId);

            bool insertSuccess = insertUnseenJob(job.Owner, job.ClusterId, job.ProcId, job.CompletionDate);
            if(!insertSuccess){
                printf("JobAd for ClusterId %d, ProcId %d failed! Skipping ... \n", job.ClusterId, job.ProcId);
                continue;
            }
            // Lookup freshly inserted info
            jobIdCacheResults = jobIdLookup(job.ClusterId, job.ProcId);
            jobId = jobIdCacheResults.first;
            jobListId = jobIdCacheResults.second;
        }
        else {
            printf("[DEBUG] For ClusterId=%d, ProcId=%d JobId was %d and jobListId was %d\n", 
            job.ClusterId, job.ProcId, jobId, jobListId);
        }

        printf("FileId is %ld\n",job.FileId);
        sqlite3_bind_int64(stmt, 1, job.Offset);
        sqlite3_bind_int64(stmt, 2, job.CompletionDate);
        sqlite3_bind_int(stmt, 3, jobId);
        sqlite3_bind_int(stmt, 4, job.FileId);
        sqlite3_bind_int(stmt, 5, jobListId);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            printf("[ERROR] Failed to insert JobRecord for ClusterId=%d, ProcId=%d: %s\n",
                job.ClusterId, job.ProcId, sqlite3_errmsg(db_)); 
        }

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    sqlite3_finalize(stmt);
}


/**
 * Insert jobs from epoch records (if not already in cache/db)
 * Inserts Spawn ads read from Epoch History file
 */
void DBHandler::batchInsertEpochRecords(const std::vector<EpochRecord>& records) {

    for (const auto& record : records) {
        // Lookup JobId using ClusterId and ProcId
        std::pair <int, int> jobIdCacheResults = jobIdLookup(record.ClusterId, record.ProcId);
        int jobId = jobIdCacheResults.first;
        int jobListId = jobIdCacheResults.second;
        
        if(jobId != -1){ // We found this Job info already, which means this job info has already been processed 
            continue;
        }
        else { // We haven't seen this Job info before, it wasn't in db nor cache
            bool insertSuccess = insertUnseenJob(record.Owner, record.ClusterId, record.ProcId, record.CurrentTime);
            if(!insertSuccess){
                printf("[ERROR] Failed to insert EpochRecord - ClusterId: %d, ProcId: %d, Owner: %s, Time: %lld, AtOffset: %lld\n", 
                       record.ClusterId, 
                       record.ProcId, 
                       record.Owner.c_str(),
                       record.CurrentTime,
                       record.Offset);
                continue;
            }
        }
    }
}

/**
 * Insert epoch records and update file processing status atomically
 * This wrapper ensures both epoch records insertion and file offset update 
 * happen in a single transaction
 */
bool DBHandler::insertEpochFileRecords(const std::vector<EpochRecord>& records, const FileInfo& fileInfo) {
    // Begin outer transaction for atomicity
    int result = sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        printf("Failed to begin transaction for epoch file records insertion\n");
        return false;
    }

    // Insert epoch records using existing function (now transaction-less)
    batchInsertEpochRecords(records);

    // Update the Files table with the new LastOffset
    const char* updateFilesSql = "UPDATE Files SET LastOffset = ? WHERE FileId = ?;";
    sqlite3_stmt* stmt;
    
    result = sqlite3_prepare_v2(db_, updateFilesSql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        printf("Failed to prepare Files update statement: %s\n", sqlite3_errmsg(db_));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    // Bind parameters (LastOffset is a long)
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(fileInfo.LastOffset));
    sqlite3_bind_int(stmt, 2, fileInfo.FileId);

    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        printf("Failed to update Files table for FileId %d: %s\n", 
               fileInfo.FileId, sqlite3_errmsg(db_));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    // Commit the transaction
    result = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        printf("Failed to commit transaction: %s\n", sqlite3_errmsg(db_));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    return true;
}

/**
 * Insert job records and update file processing status atomically
 * This wrapper ensures both job records insertion and file offset update 
 * happen in a single transaction
 */
bool DBHandler::insertJobFileRecords(const std::vector<JobRecord>& jobs, const FileInfo& fileInfo) {
    // Begin outer transaction for atomicity
    int result = sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        printf("Failed to begin transaction for job file records insertion\n");
        return false;
    }

    // Insert job records using existing function (now transaction-less)
    batchInsertJobRecords(jobs);

    // Update the Files table with the new LastOffset
    const char* updateFilesSql = "UPDATE Files SET LastOffset = ? WHERE FileId = ?;";
    sqlite3_stmt* stmt;
    
    result = sqlite3_prepare_v2(db_, updateFilesSql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        printf("Failed to prepare Files update statement: %s\n", sqlite3_errmsg(db_));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    // Bind parameters (LastOffset is a long)
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(fileInfo.LastOffset));
    sqlite3_bind_int(stmt, 2, fileInfo.FileId);

    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        printf("Failed to update Files table for FileId %d: %s\n", 
               fileInfo.FileId, sqlite3_errmsg(db_));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    // Commit the transaction
    result = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        printf("Failed to commit transaction: %s\n", sqlite3_errmsg(db_));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    return true;
}

// -------------------------
// File Info Insertion / Update
// ------------------------

/**
 * Insert or fetch metadata about a file (FileId and LastOffset)
 * Populate data into FileInfo struct 
 */
void DBHandler::writeFileInfo(FileInfo &info) {
    const char *insertSql = R"(
        INSERT OR IGNORE INTO Files(FileName, FileInode, FileHash, LastOffset)
        VALUES (?, ?, ?, ?)
    )";

    sqlite3_stmt *insertStmt = nullptr;

    if (sqlite3_prepare_v2(db_, insertSql, -1, &insertStmt, nullptr) != SQLITE_OK) {
        printf("Failed to prepare insert statement: %s\n", sqlite3_errmsg(db_));
        return;
    }

    sqlite3_bind_text(insertStmt, 1, info.FileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(insertStmt, 2, info.FileInode);
    sqlite3_bind_text(insertStmt, 3, info.FileHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(insertStmt, 4, info.LastOffset);

    if (sqlite3_step(insertStmt) != SQLITE_DONE) {
        printf("Insert (or ignore) failed: %s\n", sqlite3_errmsg(db_));
        sqlite3_finalize(insertStmt);
        return;
    }

    sqlite3_finalize(insertStmt);

    // Now fetch FileId and LastOffset (existing or newly inserted)
    const char *selectSql = R"(
        SELECT FileId, LastOffset FROM Files WHERE FileInode = ? AND FileHash = ?
    )";

    sqlite3_stmt *selectStmt = nullptr;
    if (sqlite3_prepare_v2(db_, selectSql, -1, &selectStmt, nullptr) != SQLITE_OK) {
        printf("Failed to prepare select statement: %s\n", sqlite3_errmsg(db_));
        return;
    }

    sqlite3_bind_int64(selectStmt, 1, info.FileInode);
    sqlite3_bind_text(selectStmt, 2, info.FileHash.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(selectStmt) == SQLITE_ROW) {
        info.FileId = sqlite3_column_int(selectStmt, 0);
        info.LastOffset = sqlite3_column_int64(selectStmt, 1);
    } else {
        printf("Failed to fetch FileId/LastOffset after insert/ignore.\n");
    }

    sqlite3_finalize(selectStmt);
}

/**
 * Update LastOffset for the history and epoch files after parsing.
 * Change offset to match progress in reading Epoch + History files
 */
void DBHandler::updateFileInfo(FileInfo epochHistoryFile, FileInfo historyFile) {
    const char* updateSQL = R"(
        UPDATE Files
        SET LastOffset = ?
        WHERE FileName = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, updateSQL, -1, &stmt, nullptr) != SQLITE_OK) {
        printf("[DBHandler] Failed to prepare update statement: %s\n", sqlite3_errmsg(db_));
        return;
    }

    auto update = [&](const FileInfo& fi) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, fi.LastOffset);
        sqlite3_bind_text(stmt, 2, fi.FileName.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            printf("[DBHandler] Failed to update offset for file %s: %s\n", fi.FileName.c_str(), sqlite3_errmsg(db_));
        } 
    };

    update(epochHistoryFile);
    update(historyFile);

    sqlite3_finalize(stmt);
}


// -------------------------
// Status Tracking
// -------------------------

/**
 * Insert a status snapshot into the 'Status' table and update the single row in 'StatusData' table
 * Includes Status but also running averages of various performance stats
 */
bool DBHandler::writeStatusAndData(const Status& status, const StatusData& statusData) {
    const char* statusSql = R"(
        INSERT INTO Status (
            TimeOfUpdate,
            HistoryFileIdLastRead,
            HistoryFileOffsetLastRead,
            EpochFileIdLastRead,
            EpochFileOffsetLastRead,
            TotalJobsRead,
            TotalEpochsRead,
            DurationMs,
            JobBacklogEstimate,
            HitMaxIngestLimit,
            GarbageCollectionRun
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    // Update StatusData SQL - now updates the single row instead of inserting
    const char* statusDataSqlWithArrivalHz = R"(
        UPDATE StatusData SET
            AvgAdsIngestedPerCycle = ?,
            AvgIngestDurationMs = ?,
            MeanIngestHz = ?,
            MeanArrivalHz = ?,
            MeanBacklogEstimate = ?,
            TotalCycles = ?,
            TotalAdsIngested = ?,
            HitMaxIngestLimitRate = ?,
            LastRunLeftBacklog = ?,
            TimeOfLastUpdate = ?
        WHERE StatusDataId = 1;
    )";

    const char* statusDataSqlWithoutArrivalHz = R"(
        UPDATE StatusData SET
            AvgAdsIngestedPerCycle = ?,
            AvgIngestDurationMs = ?,
            MeanIngestHz = ?,
            MeanBacklogEstimate = ?,
            TotalCycles = ?,
            TotalAdsIngested = ?,
            HitMaxIngestLimitRate = ?,
            LastRunLeftBacklog = ?,
            TimeOfLastUpdate = ?
        WHERE StatusDataId = 1;
    )";

    // Start transaction
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        printf("[ERROR] Failed to begin transaction: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    bool success = false;

    // Insert into Status
    if (sqlite3_prepare_v2(db_, statusSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, status.TimeOfUpdate);
        sqlite3_bind_int(stmt, 2, status.HistoryFileIdLastRead);
        sqlite3_bind_int64(stmt, 3, status.HistoryFileOffsetLastRead);
        sqlite3_bind_int(stmt, 4, status.EpochFileIdLastRead);
        sqlite3_bind_int64(stmt, 5, status.EpochFileOffsetLastRead);
        sqlite3_bind_int(stmt, 6, status.TotalJobsRead);
        sqlite3_bind_int(stmt, 7, status.TotalEpochsRead);
        sqlite3_bind_int(stmt, 8, status.DurationMs);
        sqlite3_bind_int(stmt, 9, status.JobBacklogEstimate);
        sqlite3_bind_int(stmt, 10, status.HitMaxIngestLimit ? 1 : 0);
        sqlite3_bind_int(stmt, 11, status.GarbageCollectionRun ? 1 : 0);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            printf("[ERROR] Failed to insert into Status: %s\n", sqlite3_errmsg(db_));
        } else {
            success = true;
        }
    } else {
        printf("[ERROR] Failed to prepare Status insert: %s\n", sqlite3_errmsg(db_));
    }
    sqlite3_finalize(stmt);

    // Update StatusData (if Status insert succeeded)
    if (success) {
        const bool hasArrivalHz = statusData.MeanArrivalHz != 0.0;
        const char* sql = hasArrivalHz ? statusDataSqlWithArrivalHz : statusDataSqlWithoutArrivalHz;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            int idx = 1;
            sqlite3_bind_double(stmt, idx++, statusData.AvgAdsIngestedPerCycle);
            sqlite3_bind_double(stmt, idx++, statusData.AvgIngestDurationMs);
            sqlite3_bind_double(stmt, idx++, statusData.MeanIngestHz);
            if (hasArrivalHz) {
                sqlite3_bind_double(stmt, idx++, statusData.MeanArrivalHz);
            }
            sqlite3_bind_double(stmt, idx++, statusData.MeanBacklogEstimate);
            sqlite3_bind_int64(stmt, idx++, statusData.TotalCycles);
            sqlite3_bind_int64(stmt, idx++, statusData.TotalAdsIngested);
            sqlite3_bind_double(stmt, idx++, statusData.HitMaxIngestLimitRate);
            sqlite3_bind_int(stmt, idx++, statusData.LastRunLeftBacklog ? 1 : 0);
            sqlite3_bind_int64(stmt, idx++, statusData.TimeOfLastUpdate);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                printf("[ERROR] Failed to update StatusData: %s\n", sqlite3_errmsg(db_));
                success = false;
            }
        } else {
            printf("[ERROR] Failed to prepare StatusData update: %s\n", sqlite3_errmsg(db_));
            success = false;
        }
        sqlite3_finalize(stmt);
    }

    // Finalize transaction
    if (success) {
        if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            printf("[ERROR] Failed to commit transaction: %s\n", errMsg);
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    } else {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
}

/**
 * Recovers the librarian's local cache information from the database if the process
 * caches are empty or uninitialized (e.g., after a restart or crash). This function
 * repopulates file tracking state, status data, and reading positions to resume
 * processing from where it left off.
 */
bool DBHandler::maybeRecoverStatusAndFiles(FileSet& historyFileSet_,
                           FileSet& epochHistoryFileSet_,
                           StatusData& statusData_)
{
	bool recoverHistory = historyFileSet_.fileMap.empty() || historyFileSet_.lastFileReadId == -1;
	bool recoverEpoch = epochHistoryFileSet_.fileMap.empty() || epochHistoryFileSet_.lastFileReadId == -1;
	bool recoverStatusData = (statusData_.TimeOfLastUpdate == 0);

	if (!recoverHistory && !recoverEpoch && !recoverStatusData) {
		return true; // Nothing to recover, success
	}

	char* errMsg = nullptr;
	int rc = sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) {
		std::cerr << "Failed to begin transaction: " << (errMsg ? errMsg : "unknown") << "\n";
		if (errMsg) sqlite3_free(errMsg);
		return false;
	}

	bool success = true;

	// Lambda for rollback and ending function early on error
	auto rollbackAndReturnFalse = [&]() {
		if (sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
			std::cerr << "Failed to rollback transaction: " << (errMsg ? errMsg : "unknown") << "\n";
			if (errMsg) sqlite3_free(errMsg);
		}
		return false;
	};

	// 2. Recover StatusData - now reads from the single row
	if (recoverStatusData) {
		const std::string statusDataSql =
			"SELECT AvgAdsIngestedPerCycle, AvgIngestDurationMs, MeanIngestHz, MeanArrivalHz, "
			"MeanBacklogEstimate, TotalCycles, TotalAdsIngested, HitMaxIngestLimitRate, LastRunLeftBacklog, "
			"TimeOfLastUpdate FROM StatusData WHERE StatusDataId = 1;";

		sqlite3_stmt* stmt = nullptr;
		rc = sqlite3_prepare_v2(db_, statusDataSql.c_str(), -1, &stmt, nullptr);
		if (rc != SQLITE_OK) {
			std::cerr << "Failed to prepare StatusData query\n";
			success = rollbackAndReturnFalse();
			return success;
		}
		rc = sqlite3_step(stmt);
		if (rc == SQLITE_ROW) {
			statusData_.AvgAdsIngestedPerCycle = sqlite3_column_double(stmt, 0);
			statusData_.AvgIngestDurationMs = sqlite3_column_double(stmt, 1);
			statusData_.MeanIngestHz = sqlite3_column_double(stmt, 2);
			statusData_.MeanArrivalHz = sqlite3_column_double(stmt, 3);
			statusData_.MeanBacklogEstimate = sqlite3_column_double(stmt, 4);
			statusData_.TotalCycles = sqlite3_column_int64(stmt, 5);
			statusData_.TotalAdsIngested = sqlite3_column_int64(stmt, 6);
			statusData_.HitMaxIngestLimitRate = sqlite3_column_double(stmt, 7);
			statusData_.LastRunLeftBacklog = sqlite3_column_int(stmt, 8) != 0;
			statusData_.TimeOfLastUpdate = sqlite3_column_int64(stmt, 9);
		}
		sqlite3_finalize(stmt);
	}

	// 3. Recover Status
	const std::string statusSql =
		"SELECT TimeOfUpdate, HistoryFileIdLastRead, HistoryFileOffsetLastRead, "
		"EpochFileIdLastRead, EpochFileOffsetLastRead FROM Status ORDER BY TimeOfUpdate DESC LIMIT 1;";

	sqlite3_stmt* stmtStatus = nullptr;
	rc = sqlite3_prepare_v2(db_, statusSql.c_str(), -1, &stmtStatus, nullptr);
	if (rc != SQLITE_OK) {
		std::cerr << "Failed to prepare Status query\n";
		success = rollbackAndReturnFalse();
		return success;
	}
	rc = sqlite3_step(stmtStatus);
	if (rc == SQLITE_ROW) {
		int64_t timeOfUpdate = sqlite3_column_int64(stmtStatus, 0);
		int historyFileId = sqlite3_column_int(stmtStatus, 1);
		int64_t historyFileOffset = sqlite3_column_int64(stmtStatus, 2);
		int epochFileId = sqlite3_column_int(stmtStatus, 3);
		int64_t epochFileOffset = sqlite3_column_int64(stmtStatus, 4);

		statusData_.TimeOfLastUpdate = timeOfUpdate;

		if (recoverHistory) {
			historyFileSet_.lastFileReadId = historyFileId;
			historyFileSet_.lastStatusTime = timeOfUpdate;
			// Store offset if needed
		}
		if (recoverEpoch) {
			epochHistoryFileSet_.lastFileReadId = epochFileId;
			epochHistoryFileSet_.lastStatusTime = timeOfUpdate;
			// Store offset if needed
		}
	}
	sqlite3_finalize(stmtStatus);

	// 4. Recover Files
	const std::string filesSql =
		"SELECT FileId, FileName, FileInode, FileHash, LastOffset FROM Files WHERE DateOfRotation IS NULL;";

	sqlite3_stmt* stmtFiles = nullptr;
	rc = sqlite3_prepare_v2(db_, filesSql.c_str(), -1, &stmtFiles, nullptr);
	if (rc != SQLITE_OK) {
		std::cerr << "Failed to prepare Files query\n";
		success = rollbackAndReturnFalse();
		return success;
	}
	while ((rc = sqlite3_step(stmtFiles)) == SQLITE_ROW) {
		FileInfo fileInfo{};
		fileInfo.FileId = sqlite3_column_int64(stmtFiles, 0);
		fileInfo.FileName = reinterpret_cast<const char*>(sqlite3_column_text(stmtFiles, 1));
		fileInfo.FileInode = sqlite3_column_int64(stmtFiles, 2);
		fileInfo.FileHash = reinterpret_cast<const char*>(sqlite3_column_text(stmtFiles, 3));
		fileInfo.LastOffset = sqlite3_column_int64(stmtFiles, 4);
		fileInfo.FileSize = -1;      // No size in table
		fileInfo.LastModified = 0;   // Not available

		if (fileInfo.FileName.rfind(historyFileSet_.historyNameConfig, 0) == 0) {
			historyFileSet_.fileMap[fileInfo.FileId] = fileInfo;
		}
		else if (fileInfo.FileName.rfind(epochHistoryFileSet_.historyNameConfig, 0) == 0) {
			epochHistoryFileSet_.fileMap[fileInfo.FileId] = fileInfo;
		}
	}
	sqlite3_finalize(stmtFiles);

	rc = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) {
		std::cerr << "Failed to commit transaction: " << (errMsg ? errMsg : "unknown") << "\n";
		if (errMsg) sqlite3_free(errMsg);
		// Try rollback if commit failed
		if (sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
			std::cerr << "Failed to rollback transaction: " << (errMsg ? errMsg : "unknown") << "\n";
			if (errMsg) sqlite3_free(errMsg);
		}
		return false;
	}

	return true;
}

// -------------------------
// ArchiveChange Handlers
// -------------------------

bool DBHandler::insertNewFilesAndDeleteOldOnes(ArchiveChange& fileSetChange) {

    std::pair<int, std::string>& rotatedFile = fileSetChange.rotatedFile;
    std::vector<FileInfo>& newFiles = fileSetChange.newFiles;
    std::vector<int>& deletedFileIds = fileSetChange.deletedFileIds;

    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    bool success = true;

    // 1. Handle rotated file
    if (!rotatedFile.second.empty()) {
        int oldFileId = rotatedFile.first;
        const std::string& newName = rotatedFile.second;

        std::string sql = "UPDATE Files SET FileName = ?, DateOfRotation = ? WHERE FileId = ?";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare update statement: %s\n", sqlite3_errmsg(db_));
            success = false;
        } else {
            sqlite3_bind_text(stmt, 1, newName.c_str(), -1, SQLITE_STATIC);

            // Set DateOfRotation to current UNIX timestamp
            std::time_t now = std::time(nullptr);
            sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(now));

            sqlite3_bind_int(stmt, 3, oldFileId);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                fprintf(stderr, "Failed to update rotated file: %s\n", sqlite3_errmsg(db_));
                success = false;
            }

            sqlite3_finalize(stmt);
        }

        if (!success) {
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
    }

    // 2. Insert new files and record FileIds back into FileInfo objects
    std::string insertSQL = "INSERT INTO Files (FileName) VALUES (?)";
    sqlite3_stmt* insertStmt = nullptr;

    if (sqlite3_prepare_v2(db_, insertSQL.c_str(), -1, &insertStmt, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare insert statement: %s\n", sqlite3_errmsg(db_));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (FileInfo& newFile : newFiles) {
        sqlite3_bind_text(insertStmt, 1, newFile.FileName.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(insertStmt) != SQLITE_DONE) {
            fprintf(stderr, "Failed to insert new file: %s\n", sqlite3_errmsg(db_));
            sqlite3_finalize(insertStmt);
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        int insertedId = static_cast<int>(sqlite3_last_insert_rowid(db_));
        newFile.FileId = insertedId;

        sqlite3_reset(insertStmt);
    }

    sqlite3_finalize(insertStmt);

    // 3. Mark deleted files with deletion timestamp
    if (!deletedFileIds.empty()) {
       std::string deleteSQL = "UPDATE Files SET DateOfDeletion = ? WHERE FileId = ?";
       sqlite3_stmt* deleteStmt = nullptr;

       if (sqlite3_prepare_v2(db_, deleteSQL.c_str(), -1, &deleteStmt, nullptr) != SQLITE_OK) {
           fprintf(stderr, "Failed to prepare delete marking statement: %s\n", sqlite3_errmsg(db_));
           sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
           return false;
       }

       std::time_t now = std::time(nullptr);
       
       for (int fileId : deletedFileIds) {
           sqlite3_bind_int64(deleteStmt, 1, static_cast<sqlite3_int64>(now));
           sqlite3_bind_int(deleteStmt, 2, fileId);

           if (sqlite3_step(deleteStmt) != SQLITE_DONE) {
               fprintf(stderr, "Failed to mark file as deleted: %s\n", sqlite3_errmsg(db_));
               sqlite3_finalize(deleteStmt);
               sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
               return false;
           }

           sqlite3_reset(deleteStmt);
       }

       sqlite3_finalize(deleteStmt);
    }


    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}


// -------------------------
// Garbage Collection
// -------------------------

// NOTE: This function would not work if more of the statements had parameters that need to be bound
// It currently only accepts bindings for the very first statement, which is where we indicate 
// the n number of jobs that we need to delete to get to the low watermark
bool DBHandler::runGarbageCollection(const std::string &gcQuerySQL, int fileLimit) {
    if (!db_) return false;

    char *errMsg = nullptr;

    if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        printf("[GC] Begin transaction failed: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    size_t pos = gcQuerySQL.find(';');
    if (pos == std::string::npos) {
        printf("[GC] Invalid GC SQL - no statement terminator\n");
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    std::string step1Sql = gcQuerySQL.substr(0, pos + 1);
    std::string step2Sql = gcQuerySQL.substr(pos + 1);

    // Prepare step1, bind ?, execute
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, step1Sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        printf("[GC] Prepare step1 failed: %s\n", sqlite3_errmsg(db_));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    if (sqlite3_bind_int(stmt, 1, fileLimit) != SQLITE_OK) {
        printf("[GC] Bind failed: %s\n", sqlite3_errmsg(db_));
        sqlite3_finalize(stmt);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        printf("[GC] Execute step1 failed: %s\n", sqlite3_errmsg(db_));
        sqlite3_finalize(stmt);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_finalize(stmt);

    // Exec the rest
    if (sqlite3_exec(db_, step2Sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        printf("[GC] Exec step2 failed: %s\n", errMsg);
        sqlite3_free(errMsg);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        printf("[GC] Commit failed: %s\n", errMsg);
        sqlite3_free(errMsg);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    printf("[GC] Garbage collection successful.\n");
    return true;
}



// -------------------------
// User Query Helpers
// -------------------------

/**
 * Return jobs submitted by a specific user, sorted by creation time.
 */
std::vector<std::tuple<int, int, int>> DBHandler::getJobsForUser(const std::string& username) {
    const char* sql = R"(
        SELECT Jobs.ClusterId, Jobs.ProcId, Jobs.TimeOfCreation
        FROM Jobs
        JOIN Users ON Jobs.UserId = Users.UserId
        WHERE Users.UserName = ?
        ORDER BY Jobs.TimeOfCreation DESC
    )";

    sqlite3_stmt* stmt = nullptr;
    std::vector<std::tuple<int, int, int>> results;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int clusterId = sqlite3_column_int(stmt, 0);
            int procId = sqlite3_column_int(stmt, 1);
            int timeCreated = sqlite3_column_int(stmt, 2);
            results.emplace_back(clusterId, procId, timeCreated);
        }
    } else {
        fprintf(stderr, "[DBHandler] Failed to prepare getJobsForUser: %s\n", sqlite3_errmsg(db_));
    }

    sqlite3_finalize(stmt);
    return results;
}


/**
 * Count jobs per user and return (username, job count) pairs.
 */
std::vector<std::pair<std::string, int>> DBHandler::getJobCountsPerUser() {
    const char* sql = R"(
        SELECT Users.UserName, COUNT(*) AS JobCount
        FROM Jobs
        JOIN Users ON Jobs.UserId = Users.UserId
        GROUP BY Users.UserId
        ORDER BY JobCount DESC
    )";

    sqlite3_stmt* stmt = nullptr;
    std::vector<std::pair<std::string, int>> results;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* nameText = sqlite3_column_text(stmt, 0);
            int count = sqlite3_column_int(stmt, 1);

            std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
            results.emplace_back(name, count);
        }
    } else {
        fprintf(stderr, "[DBHandler] Failed to prepare getJobCountsPerUser: %s\n", sqlite3_errmsg(db_));
    }

    sqlite3_finalize(stmt);
    return results;
}
