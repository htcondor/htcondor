/**
 * @file dbHandler.cpp
 * @brief DBHandler class that manages all interactions with the SQLite database.
 *
 * Responsibilities:
 * - Insert job records and metadata into relevant tables.
 * - Maintain and cache job ID mappings for fast lookup.
 * - Track which parts of history/epoch files have been processed.
 * - Provide user-level analytics (e.g., jobs per user).
 * - Expose testing/debug helpers for validation.
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
#include <unordered_set>
#include <functional>
#include <filesystem>
#include <list>
#include "JobRecord.h"
#include "dbHandler.h"

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
DBHandler::JobIdCache::JobIdCache(size_t maxSize)
    : maxSize_(maxSize) {}

DBHandler::JobIdCache::~JobIdCache() {
    if (stmt_) sqlite3_finalize(stmt_);
}

std::pair<int, int> DBHandler::JobIdCache::get(int clusterId, int procId) {
    Key key = {clusterId, procId};
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return {-1, -1}; // Not found
    }

    // Move accessed key to front (most recently used)
    usageOrder_.erase(std::get<3>(it->second));
    usageOrder_.push_front(key);
    std::get<3>(it->second) = usageOrder_.begin();

    return {std::get<0>(it->second), std::get<1>(it->second)};
}

void DBHandler::JobIdCache::put(int clusterId, int procId, int jobId, int jobListId, long timestamp) {
    Key key = {clusterId, procId};
    auto it = cache_.find(key);

    if (it != cache_.end()) {
        // Update existing entry and move to front
        std::get<0>(it->second) = jobId;
        std::get<1>(it->second) = jobListId;
        std::get<2>(it->second) = timestamp;

        usageOrder_.erase(std::get<3>(it->second));
        usageOrder_.push_front(key);
        std::get<3>(it->second) = usageOrder_.begin();

    } else {
        // Evict least recently used if cache is full
        if (cache_.size() >= maxSize_) {
            Key lruKey = usageOrder_.back();
            usageOrder_.pop_back();
            cache_.erase(lruKey);
        }

        // Insert new entry at front
        usageOrder_.push_front(key);
        cache_[key] = std::make_tuple(jobId, jobListId, timestamp, usageOrder_.begin());
    }
}

void DBHandler::JobIdCache::updateTimestamp(int clusterId, int procId, long timestamp) {
    Key key = {clusterId, procId};
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        std::get<2>(it->second) = std::max(std::get<2>(it->second), timestamp);

        // Move to front on update timestamp as usage
        usageOrder_.erase(std::get<3>(it->second));
        usageOrder_.push_front(key);
        std::get<3>(it->second) = usageOrder_.begin();
    }
}

bool DBHandler::JobIdCache::exists(int clusterId, int procId) const {
    return cache_.count({clusterId, procId}) > 0;
}

void DBHandler::JobIdCache::garbageCollect(long minTimestamp) {
    for (auto it = cache_.begin(); it != cache_.end(); ) {
        long timestamp = std::get<2>(it->second);
        if (timestamp < minTimestamp) {
            usageOrder_.erase(std::get<3>(it->second));
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void DBHandler::JobIdCache::remove(int clusterId, int procId) {
    Key key = {clusterId, procId};
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        usageOrder_.erase(std::get<3>(it->second));
        cache_.erase(it);
    }
}

void DBHandler::JobIdCache::clear() {
    cache_.clear();
    usageOrder_.clear();
}


// -------------------------
// DBHandler Constructor / Destructor
// -------------------------

DBHandler::DBHandler(const std::string& schemaPath, const std::string& dbPath, size_t maxCacheSize)
    : db(nullptr), jobIdLookupStmt_(nullptr), userInsertStmt_(nullptr), userSelectStmt_(nullptr), 
    jobListInsertStmt_(nullptr), jobListSelectStmt_(nullptr)
{
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db);
        sqlite3_close(db); // technically redundant if db is already bad, but harmless
        throw std::runtime_error("Failed to open SQLite DB: " + err); // TODO CHANGE THIS
    }

    if (!initializeFromSchema(schemaPath)) {
        sqlite3_close(db);
        db = nullptr;
        throw std::runtime_error("Failed to initialize schema");
    }

    jobIdCache = std::make_unique<JobIdCache>(maxCacheSize);

    const char* sql = "SELECT JobId, JobListId FROM Jobs WHERE ClusterId = ? AND ProcId = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &jobIdLookupStmt_, nullptr) != SQLITE_OK) {
        printf ("[ERROR] Failed to prepare jobIdLookupStmt_: %s \n ", sqlite3_errmsg(db));
        jobIdLookupStmt_ = nullptr;
    }

    const char* userInsertSQL = "INSERT OR IGNORE INTO Users (UserName) VALUES (?)";
    if (sqlite3_prepare_v2(db, userInsertSQL, -1, &userInsertStmt_, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare userInsertStmt: %s\n", sqlite3_errmsg(db));
        userInsertStmt_ = nullptr;
    }

    const char* userLookupSQL = "SELECT UserId FROM Users WHERE UserName = ?";
    if (sqlite3_prepare_v2(db, userLookupSQL, -1, &userSelectStmt_, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare userSelectStmt: %s\n", sqlite3_errmsg(db));
        userSelectStmt_ = nullptr;
    }

    const char* jobListInsertSQL = "INSERT OR IGNORE INTO JobLists (ClusterId, UserId) VALUES (?, ?)";
    if (sqlite3_prepare_v2(db, jobListInsertSQL, -1, &jobListInsertStmt_, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare jobListInsertStmt: %s\n", sqlite3_errmsg(db));
        jobListInsertStmt_ = nullptr;
    }

    const char* jobListLookupSQL = "SELECT JobListId FROM JobLists WHERE ClusterId = ? AND UserId = ?";
    if (sqlite3_prepare_v2(db, jobListLookupSQL, -1, &jobListSelectStmt_, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare jobListSelectStmt: %s\n", sqlite3_errmsg(db));
        jobListSelectStmt_ = nullptr;
    }

    const char* jobInsertSQL = "INSERT OR IGNORE INTO Jobs (ClusterId, ProcId, UserId, JobListId, TimeOfCreation) VALUES (?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db, jobInsertSQL, -1, &jobInsertStmt_, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare jobInsertStmt: %s\n", sqlite3_errmsg(db));
        jobInsertStmt_ = nullptr;
    }

    const char* jobLookupSQL = "SELECT JobId FROM Jobs WHERE ClusterId = ? AND ProcId = ?";
    if (sqlite3_prepare_v2(db, jobLookupSQL, -1, &jobSelectStmt_, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare jobSelectStmt: %s\n", sqlite3_errmsg(db));
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

    

    if (db) {
        int rc = sqlite3_close(db);
        if (rc != SQLITE_OK) {
            printf("[ERROR] Failed to close DB: %s \n", sqlite3_errmsg(db));
        }
        db = nullptr;
    }
}

bool DBHandler::initializeFromSchema(const std::string& schemaPath) {
    std::ifstream file(schemaPath);
    if (!file.is_open()) {
        printf("[ERROR] Failed to open schema file: %s\n", schemaPath.c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string schemaSQL = buffer.str();

    // Enable foreign key constraints
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        printf("[ERROR] Failed to enable foreign keys: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    rc = sqlite3_exec(db, schemaSQL.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        printf("[ERROR] Failed to initialize DB schema: %s\n", errMsg);
        sqlite3_free(errMsg);
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
    auto cached = jobIdCache->get(clusterId, procId);
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
        jobIdCache->put(clusterId, procId, jobId, jobListId, -1); 
        return {jobId, jobListId};
    }

    if (rc != SQLITE_DONE) {
        printf("[ERROR] sqlite3_step failed for ClusterId= %d, ProcId= %d - %s \n", 
        clusterId, procId, sqlite3_errmsg(db));
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
    sqlite3_step(insertStmt);
    if (sqlite3_changes(sqlite3_db_handle(insertStmt)) > 0) {
        id = static_cast<int>(sqlite3_last_insert_rowid(sqlite3_db_handle(insertStmt)));
    }
    sqlite3_reset(insertStmt);
    sqlite3_clear_bindings(insertStmt);

    if (id == -1) {
        bindSelect(selectStmt);
        if (sqlite3_step(selectStmt) == SQLITE_ROW) {
            id = extractId(selectStmt);
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
    jobIdCache->put(clusterId, procId, jobId, jobListId, timeOfCreation); 
    return true;
}

/**
 * Insert a JobRecord into the database, using job and file info.
 */
void DBHandler::insertJobRecord(const JobRecord& jobRecord) {
    // Lookup JobId using ClusterId and ProcId
    std::pair <int, int> jobIdCacheResults = jobIdLookup(jobRecord.ClusterId, jobRecord.ProcId);
    int jobId = jobIdCacheResults.first;
    int jobListId = jobIdCacheResults.second;

    if (jobId == -1) {
        printf("[ERROR] JobRecord was not preceded by Spawn ad, no matching JobId\n"
                "Writing JobAd for ClusterId %d, ProcId %d\n", 
                jobRecord.ClusterId, jobRecord.ProcId);
        insertUnseenJob(jobRecord.Owner, jobRecord.ClusterId, jobRecord.ProcId, jobRecord.CompletionDate);

        // Lookup freshly inserted info
        jobIdCacheResults = jobIdLookup(jobRecord.ClusterId, jobRecord.ProcId);
        jobId = jobIdCacheResults.first;
        jobListId = jobIdCacheResults.second;
    }

    const char* sql = R"(
        INSERT INTO JobRecords (Offset, CompletionDate, JobId, FileId, JobListId)
        VALUES (?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, jobRecord.Offset);
        sqlite3_bind_int64(stmt, 2, jobRecord.CompletionDate);
        sqlite3_bind_int(stmt, 3, jobId);
        sqlite3_bind_int(stmt, 4, jobRecord.FileId);
        sqlite3_bind_int(stmt, 5, jobListId);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            printf("[ERROR] Failed to insert into JobRecords: %s\n", sqlite3_errmsg(db));
        }
    } else {
            printf("[ERROR] Failed to prepare JobRecords insert: %s \n", sqlite3_errmsg(db));
    }

    if (stmt) sqlite3_finalize(stmt);
}

/**
 * Batch insert multiple JobRecords in a single transaction.
 */
void DBHandler::batchInsertJobRecords(const std::vector<JobRecord>& jobs) {
    if (jobs.empty()) {
        return;
    }

    // Begin transaction
    int rc = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        printf("Failed to begin transaction: %s \n", sqlite3_errmsg(db));
        return;
    }

    const char* sql = R"(
        INSERT INTO JobRecords (Offset, CompletionDate, JobId, FileId, JobListId)
        VALUES (?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        printf("[ERROR] Failed to prepare JobRecords insert: %s \n", sqlite3_errmsg(db));
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
                job.ClusterId, job.ProcId, sqlite3_errmsg(db)); 
        }

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    // Commit transaction
    rc = sqlite3_exec(db, "END TRANSACTION;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        printf("Failed to commit transaction:%s \n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
}


/**
 * Insert jobs from epoch records (if not already in cache/db)
 * Inserts Spawn ads read from Epoch History file
 */
void DBHandler::batchInsertEpochRecords(const std::vector<EpochRecord>& records) {
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

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
                printf("JobAd for ClusterId %d, ProcId %d failed! Skipping ... \n", record.ClusterId, record.ProcId);
                continue;
            }
        }
    }
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
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

    if (sqlite3_prepare_v2(db, insertSql, -1, &insertStmt, nullptr) != SQLITE_OK) {
        printf("Failed to prepare insert statement: %s\n", sqlite3_errmsg(db));
        return;
    }

    sqlite3_bind_text(insertStmt, 1, info.FileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(insertStmt, 2, info.FileInode);
    sqlite3_bind_text(insertStmt, 3, info.FileHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(insertStmt, 4, info.LastOffset);

    if (sqlite3_step(insertStmt) != SQLITE_DONE) {
        printf("Insert (or ignore) failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(insertStmt);
        return;
    }

    sqlite3_finalize(insertStmt);

    // Now fetch FileId and LastOffset (existing or newly inserted)
    const char *selectSql = R"(
        SELECT FileId, LastOffset FROM Files WHERE FileInode = ? AND FileHash = ?
    )";

    sqlite3_stmt *selectStmt = nullptr;
    if (sqlite3_prepare_v2(db, selectSql, -1, &selectStmt, nullptr) != SQLITE_OK) {
        printf("Failed to prepare select statement: %s\n", sqlite3_errmsg(db));
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
    if (sqlite3_prepare_v2(db, updateSQL, -1, &stmt, nullptr) != SQLITE_OK) {
        printf("[DBHandler] Failed to prepare update statement: %s\n", sqlite3_errmsg(db));
        return;
    }

    auto update = [&](const FileInfo& fi) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, fi.LastOffset);
        sqlite3_bind_text(stmt, 2, fi.FileName.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            printf("[DBHandler] Failed to update offset for file %s: %s\n", fi.FileName.c_str(), sqlite3_errmsg(db));
        } 
    };

    update(epochHistoryFile);
    update(historyFile);

    sqlite3_finalize(stmt);
}


/**
 * Read info about a file using FileId into a FileInfo struct
 */
FileInfo DBHandler::readFileById(int fileId) {
    FileInfo info{};
    const char* sql = "SELECT FileId, FileName, FileInode, FileHash, LastOffset FROM Files WHERE FileId = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db)); 
        return info;
    }

    sqlite3_bind_int(stmt, 1, fileId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        info.FileId = sqlite3_column_int(stmt, 0);
        info.FileName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        info.FileInode = sqlite3_column_int64(stmt, 2);
        info.FileHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        info.LastOffset = sqlite3_column_int64(stmt, 4);
    } else {
        printf("No file found with FileId %d\n", fileId);
    }

    sqlite3_finalize(stmt);
    return info;
}

/**
 * Return the most recent file processed (of type "history" or "epoch") from the 'Status' table
 */
FileInfo DBHandler::lastFileRead(const std::string& type) {
    int fileId = 0;

    const char* sql = nullptr;
    if (type == "history") {
        sql = "SELECT HistoryFileIdLastRead FROM Status ORDER BY TimeOfUpdate DESC LIMIT 1;";
    } else if (type == "epoch") {
        sql = "SELECT EpochFileIdLastRead FROM Status ORDER BY TimeOfUpdate DESC LIMIT 1;";
    } else {
        throw std::invalid_argument("Invalid file type requested");
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            fileId = sqlite3_column_int(stmt, 0);
        }
    }
    if (stmt) sqlite3_finalize(stmt);

    return readFileById(fileId); 
}


// -------------------------
// Status Tracking
// -------------------------

/**
 * Insert a status snapshot into the Status table.
 * Includes History/Epoch FileIds, Offset, as well as Number of Jobs read during this run
 * And how long the latest update took
 */
void DBHandler::writeStatus(const Status& status) {
    const char* sql = R"(
        INSERT INTO Status (
            TimeOfUpdate,
            HistoryFileIdLastRead,
            HistoryFileOffsetLastRead,
            EpochFileIdLastRead,
            EpochFileOffsetLastRead,
            TotalJobsRead,
            TotalEpochsRead,
            DurationMs
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, status.TimeOfUpdate);
        sqlite3_bind_int(stmt, 2, status.HistoryFileIdLastRead);
        sqlite3_bind_int64(stmt, 3, status.HistoryFileOffsetLastRead);
        sqlite3_bind_int(stmt, 4, status.EpochFileIdLastRead);
        sqlite3_bind_int64(stmt, 5, status.EpochFileOffsetLastRead);
        sqlite3_bind_int(stmt, 6, status.TotalJobsRead);
        sqlite3_bind_int(stmt, 7, status.TotalEpochsRead);
        sqlite3_bind_int(stmt, 8, status.DurationMs);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    } else {
        printf("[ERROR] Failed to prepare writeStatus statement: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * Retrieve the most recent Status entry.
 */
Status DBHandler::readLastStatus() {
    Status status{};
    const char* sql = R"(
        SELECT
            TimeOfUpdate,
            HistoryFileIdLastRead,
            HistoryFileOffsetLastRead,
            EpochFileIdLastRead,
            EpochFileOffsetLastRead,
            TotalJobsRead,
            TotalEpochsRead,
            DurationMs
        FROM Status
        ORDER BY TimeOfUpdate DESC
        LIMIT 1;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            status.TimeOfUpdate = sqlite3_column_int64(stmt, 0);
            status.HistoryFileIdLastRead = sqlite3_column_int(stmt, 1);
            status.HistoryFileOffsetLastRead = sqlite3_column_int64(stmt, 2);
            status.EpochFileIdLastRead = sqlite3_column_int(stmt, 3);
            status.EpochFileOffsetLastRead = sqlite3_column_int64(stmt, 4);
            status.TotalJobsRead = sqlite3_column_int(stmt, 5);
            status.TotalEpochsRead = sqlite3_column_int(stmt, 6);
            status.DurationMs = sqlite3_column_int(stmt, 7);
        }
    } else {
        printf("[ERROR] Failed to prepare readLastStatus: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return status;
}

/**
 * Count the number of files with LastOffset == 0 (unprocessed).
 * Meant to be used by ArchiveMonitor upon implementation. 
 */
int DBHandler::countUnprocessedFiles() {
    int count = 0;
    const char* sql = "SELECT COUNT(*) FROM Files WHERE LastOffset = 0;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return count;
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

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int clusterId = sqlite3_column_int(stmt, 0);
            int procId = sqlite3_column_int(stmt, 1);
            int timeCreated = sqlite3_column_int(stmt, 2);
            results.emplace_back(clusterId, procId, timeCreated);
        }
    } else {
        fprintf(stderr, "[DBHandler] Failed to prepare getJobsForUser: %s\n", sqlite3_errmsg(db));
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

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* nameText = sqlite3_column_text(stmt, 0);
            int count = sqlite3_column_int(stmt, 1);

            std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
            results.emplace_back(name, count);
        }
    } else {
        fprintf(stderr, "[DBHandler] Failed to prepare getJobCountsPerUser: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return results;
}


// -------------------------
// Testing / Validation
// -------------------------

/**
 * Return all JobRecords currently in the database.
 */
std::vector<JobRecord> DBHandler::readAllJobs() { 
    std::vector<JobRecord> jobs;
    const char* sql = "SELECT JobId, CompletionDate, Offset, FileId FROM JobRecords;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            JobRecord job;
            job.JobId = sqlite3_column_int(stmt, 0);
            job.CompletionDate = sqlite3_column_int64(stmt, 1);
            job.Offset = sqlite3_column_int64(stmt, 2);
            job.FileId = sqlite3_column_int(stmt, 3);
            jobs.push_back(job);
        }
    } else {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db)); 
    }

    sqlite3_finalize(stmt);
    return jobs;
}

/**
 * Return all file records currently in the database.
 */
std::vector<FileInfo> DBHandler::readAllFileInfos() {
    std::vector<FileInfo> files;
    const char* sql = "SELECT FileId, FileName, FileInode, FileHash, LastOffset FROM Files;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db)); 
        return files;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FileInfo info;
        info.FileId = sqlite3_column_int(stmt, 0);
        info.FileName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        info.FileInode = sqlite3_column_int64(stmt, 2);
        info.FileHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        info.LastOffset = sqlite3_column_int64(stmt, 4);

        files.push_back(info);
    }

    sqlite3_finalize(stmt);
    return files;
}

/**
 * Clear JobRecords, EpochRecords, Transfers, Jobs, Files, and Status tables.
 */
bool DBHandler::clearAllTables() {
    const char* wipeSQL = R"(
        DELETE FROM JobRecords;
        DELETE FROM EpochRecords;
        DELETE FROM TransferRecords;
        DELETE FROM Jobs;
        DELETE FROM Files;
        DELETE FROM Status;
    )";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, wipeSQL, nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        printf("[ERROR] Failed to clear tables: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

/**
 * Clear the internal job ID cache.
 */
void DBHandler::clearCache(){
    jobIdCache->clear();
}


/**
 * Return row count for a given table (if valid).
 */

int DBHandler::countTable(const std::string& table) const {
    // Optional: whitelist valid table names to prevent SQL injection
    static const std::unordered_set<std::string> validTables = {
        "Jobs",
        "Files",
        "JobLists", 
        "JobRecords", 
        "Status", 
        "Users"
    };

    if (validTables.find(table) == validTables.end()) {
        printf("[ERROR] Invalid table name in countTable: %s\n", table.c_str());
        return -1;
    }

    std::string sql = "SELECT COUNT(*) FROM " + table;
    sqlite3_stmt* stmt = nullptr;
    sqlite3* db = getDB(); // Make sure getDB() is marked const

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        printf("[ERROR] Failed to prepare countTable query for %s: %s\n", 
            table.c_str(), sqlite3_errmsg(db));
        return -1;
    }

    int count = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    } else {
        printf("[ERROR] Failed to step countTable query for %s\n", table.c_str());
    }

    sqlite3_finalize(stmt);
    return count;
}


/**
 * Insert a synthetic job for testing as if from a spawn ad.
 */
void DBHandler::writeTesterSpawnAd(int clusterId, int procId, const std::string& owner, long completionDate) {
    if (!insertUnseenJob(owner, clusterId, procId, completionDate)) {
        printf("[ERROR] Failed to write tester spawn ad for ClusterId=%d, ProcId=%d, Owner=%s\n",
               clusterId, procId, owner.c_str());
    }
}




