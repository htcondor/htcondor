/**
 * @file dbHandler.cpp
 * @brief DBHandler class that manages all interactions with the SQLite database.
 *
 * Responsibilities:
 * - Insert job records and metadata into relevant tables.
 * - Maintain and cache job ID mappings for fast lookup.
 * - Track which parts of history files have been processed (via ArchiveFile).
 * - Provide rolling status metrics.
 */

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"

#include <filesystem>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <tuple>

#include <sqlite3.h>

#ifndef _WIN32
    #include <sys/stat.h>
#endif

#include "dbHandler.h"
#include "SavedQueries.h"

namespace conf = LibrarianConfigOptions;
namespace fs   = std::filesystem;


#define ROLLBACK_AND_RETURN()\
    char* err; \
    if (sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, &err) != SQLITE_OK) { \
        dprintf(D_ERROR, "Error: Failed to rollback: %s\n", err); \
        sqlite3_free(err); \
    } \
    return false;


// -------------------------
// DBHandler Initialization / Teardown
// -------------------------

bool DBHandler::initialize() {
    jobIdCache_.setLimit(static_cast<size_t>(config[conf::i::DBMaxJobCacheSize]));

    if (sqlite3_open(config[conf::str::DBPath].c_str(), &db_) != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        dprintf(D_ERROR, "Failed to open SQLite database: %s\n", err.c_str());
        std::ignore = sqlite3_close(db_);
        return false;
    }

#ifndef _WIN32
    if (chmod(config[conf::str::DBPath].c_str(), 0644) != 0) {
        dprintf(D_ERROR, "DBHandler::initialize: failed to set permissions on '%s': %s\n",
                config[conf::str::DBPath].c_str(), strerror(errno));
    }
#endif

    char* errMsg = nullptr;
    int rc;

    rc = sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to enable foreign keys: %s\n", errMsg ? errMsg : "Unknown");
        sqlite3_free(errMsg);
        return false;
    }

    int version = getSchemaVersion();
    if (version == -1) {
        dprintf(D_ERROR, "Failed to get current schema version.\n");
        return false;
    }

    dprintf(D_ALWAYS, "Database Schema Version: %d\n", version);

    rc = sqlite3_exec(db_, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to begin transaction: %s\n", errMsg ? errMsg : "Unknown");
        sqlite3_free(errMsg);
        return false;
    }

    rc = sqlite3_exec(db_, SavedQueries::SCHEMA_SQL.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to initialize DB schema: %s\n", errMsg ? errMsg : "Unknown");
        sqlite3_free(errMsg);
        ROLLBACK_AND_RETURN();
    }

    constexpr int SCHEMA_VERSION = 1;

    if (version > SCHEMA_VERSION) {
        dprintf(D_ALWAYS, "Database schema version (%d) is newer than my version (%d).\n",
                version, SCHEMA_VERSION);
        ROLLBACK_AND_RETURN();
    } else if (version < SCHEMA_VERSION) {
        std::string version_stmt;
        formatstr(version_stmt, "PRAGMA user_version = %d;", SCHEMA_VERSION);
        switch (version) {
            case 0: [[fallthrough]];
            default:
                if (sqlite3_exec(db_, version_stmt.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
                    dprintf(D_ERROR, "Failed to set new schema version: %s\n", errMsg ? errMsg : "Unknown");
                    sqlite3_free(errMsg);
                    ROLLBACK_AND_RETURN();
                }
        }
    }

    rc = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to commit schema transaction: %s\n", errMsg ? errMsg : "Unknown");
        sqlite3_free(errMsg);
        ROLLBACK_AND_RETURN();
    }

    const char* jobLookupSQL = "SELECT JobId, JobListId FROM Jobs WHERE ClusterId = ? AND ProcId = ?";
    if (sqlite3_prepare_v2(db_, jobLookupSQL, -1, &jobIdLookupStmt_, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare jobIdLookupStmt_: %s\n", sqlite3_errmsg(db_));
        jobIdLookupStmt_ = nullptr;
    }

    const char* userInsertSQL = "INSERT OR IGNORE INTO Users (UserName) VALUES (?)";
    if (sqlite3_prepare_v2(db_, userInsertSQL, -1, &userInsertStmt_, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare userInsertStmt: %s\n", sqlite3_errmsg(db_));
        userInsertStmt_ = nullptr;
    }

    const char* userLookupSQL = "SELECT UserId FROM Users WHERE UserName = ?";
    if (sqlite3_prepare_v2(db_, userLookupSQL, -1, &userSelectStmt_, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare userSelectStmt: %s\n", sqlite3_errmsg(db_));
        userSelectStmt_ = nullptr;
    }

    const char* jobListInsertSQL = "INSERT OR IGNORE INTO JobLists (ClusterId, UserId) VALUES (?, ?)";
    if (sqlite3_prepare_v2(db_, jobListInsertSQL, -1, &jobListInsertStmt_, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare jobListInsertStmt: %s\n", sqlite3_errmsg(db_));
        jobListInsertStmt_ = nullptr;
    }

    const char* jobListLookupSQL = "SELECT JobListId FROM JobLists WHERE ClusterId = ? AND UserId = ?";
    if (sqlite3_prepare_v2(db_, jobListLookupSQL, -1, &jobListSelectStmt_, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare jobListSelectStmt: %s\n", sqlite3_errmsg(db_));
        jobListSelectStmt_ = nullptr;
    }

    const char* jobInsertSQL =
        "INSERT OR IGNORE INTO Jobs (ClusterId, ProcId, UserId, JobListId, TimeOfCreation) "
        "VALUES (?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db_, jobInsertSQL, -1, &jobInsertStmt_, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare jobInsertStmt: %s\n", sqlite3_errmsg(db_));
        jobInsertStmt_ = nullptr;
    }

    const char* jobSelectSQL = "SELECT JobId FROM Jobs WHERE ClusterId = ? AND ProcId = ?";
    if (sqlite3_prepare_v2(db_, jobSelectSQL, -1, &jobSelectStmt_, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare jobSelectStmt: %s\n", sqlite3_errmsg(db_));
        jobSelectStmt_ = nullptr;
    }

    return true;
}

DBHandler::~DBHandler() {
    if (jobIdLookupStmt_)   { std::ignore = sqlite3_finalize(jobIdLookupStmt_);    jobIdLookupStmt_   = nullptr; }
    if (userInsertStmt_)    { std::ignore = sqlite3_finalize(userInsertStmt_);     userInsertStmt_    = nullptr; }
    if (userSelectStmt_)    { std::ignore = sqlite3_finalize(userSelectStmt_);     userSelectStmt_    = nullptr; }
    if (jobListInsertStmt_) { std::ignore = sqlite3_finalize(jobListInsertStmt_);  jobListInsertStmt_ = nullptr; }
    if (jobListSelectStmt_) { std::ignore = sqlite3_finalize(jobListSelectStmt_);  jobListSelectStmt_ = nullptr; }
    if (jobInsertStmt_)     { std::ignore = sqlite3_finalize(jobInsertStmt_);      jobInsertStmt_     = nullptr; }
    if (jobSelectStmt_)     { std::ignore = sqlite3_finalize(jobSelectStmt_);      jobSelectStmt_     = nullptr; }

    if (db_) {
        int rc = sqlite3_close(db_);
        if (rc != SQLITE_OK) {
            dprintf(D_ERROR, "Failed to close db: %s\n", sqlite3_errmsg(db_));
        }
        db_ = nullptr;
    }
}

bool DBHandler::testDatabaseConnection() {
    if ( ! db_) {
        dprintf(D_ERROR, "Database handle is null\n");
        return false;
    }
    int result = sqlite3_exec(db_, "SELECT 1;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        dprintf(D_ERROR, "Basic query failed: %s\n", sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

bool DBHandler::verifyDatabaseSchema(const std::string& schemaSQL) {
    std::string searchStr = "CREATE TABLE";
    int expectedTableCount = 0;
    size_t pos = 0;
    while ((pos = schemaSQL.find(searchStr, pos)) != std::string::npos) {
        expectedTableCount++;
        pos += searchStr.length();
    }
    if (expectedTableCount == 0) {
        dprintf(D_ERROR, "No CREATE TABLE statements found in schema SQL\n");
        return false;
    }

    const char* countTablesQuery = R"(
        SELECT COUNT(*) FROM sqlite_master
        WHERE type='table' AND name NOT LIKE 'sqlite_%';
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, countTablesQuery, -1, &stmt, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Table count prepare failed: %s\n", sqlite3_errmsg(db_));
        return false;
    }
    int actualTableCount = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        actualTableCount = sqlite3_column_int(stmt, 0);
    }
    std::ignore = sqlite3_finalize(stmt);

    if (actualTableCount != expectedTableCount) {
        dprintf(D_ERROR, "Table count mismatch. Expected: %d, Found: %d\n",
                expectedTableCount, actualTableCount);
        return false;
    }
    return true;
}

int DBHandler::getSchemaVersion() {
    if ( ! db_) {
        dprintf(D_ERROR, "Database connection is not created.\n");
        return -1;
    }
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA user_version;", -1, &stmt, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare version query: %s\n", sqlite3_errmsg(db_));
        return -1;
    }
    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    std::ignore = sqlite3_finalize(stmt);
    return version;
}


// -------------------------
// JobRecord Insertion
// -------------------------

std::pair<int, int> DBHandler::jobIdLookup(int clusterId, int procId) {
    std::pair<int, int> key = {clusterId, procId};

    if (jobIdCache_.contains(key)) {
        return jobIdCache_[key];
    }

    if ( ! jobIdLookupStmt_) {
        dprintf(D_ERROR, "No prepared job id lookup statement\n");
        return {-1, -1};
    }

    std::ignore = sqlite3_reset(jobIdLookupStmt_);
    std::ignore = sqlite3_clear_bindings(jobIdLookupStmt_);
    std::ignore = sqlite3_bind_int(jobIdLookupStmt_, 1, clusterId);
    std::ignore = sqlite3_bind_int(jobIdLookupStmt_, 2, procId);

    int rc = sqlite3_step(jobIdLookupStmt_);
    if (rc == SQLITE_ROW) {
        int jobId     = sqlite3_column_int(jobIdLookupStmt_, 0);
        int jobListId = sqlite3_column_int(jobIdLookupStmt_, 1);
        jobIdCache_.insert(key, {jobId, jobListId});
        return {jobId, jobListId};
    }

    if (rc != SQLITE_DONE) {
        dprintf(D_ERROR, "sqlite3_step failed for ClusterId=%d, ProcId=%d: %s\n",
                clusterId, procId, sqlite3_errmsg(db_));
    }
    return {-1, -1};
}

static int getOrInsertIdPrepared(
    sqlite3_stmt* insertStmt,
    sqlite3_stmt* selectStmt,
    std::function<void(sqlite3_stmt*)> bindInsert,
    std::function<void(sqlite3_stmt*)> bindSelect,
    std::function<int(sqlite3_stmt*)>  extractId)
{
    int id = -1;

    bindSelect(selectStmt);
    int rc = sqlite3_step(selectStmt);
    if (rc == SQLITE_ROW) {
        id = extractId(selectStmt);
        std::ignore = sqlite3_reset(selectStmt);
        std::ignore = sqlite3_clear_bindings(selectStmt);
        return id;
    } else if (rc != SQLITE_DONE) {
        dprintf(D_ERROR, "Select failed: %s\n", sqlite3_errmsg(sqlite3_db_handle(selectStmt)));
        std::ignore = sqlite3_reset(selectStmt);
        std::ignore = sqlite3_clear_bindings(selectStmt);
        return -1;
    }
    std::ignore = sqlite3_reset(selectStmt);
    std::ignore = sqlite3_clear_bindings(selectStmt);

    bindInsert(insertStmt);
    rc = sqlite3_step(insertStmt);
    if (rc == SQLITE_DONE) {
        id = static_cast<int>(sqlite3_last_insert_rowid(sqlite3_db_handle(insertStmt)));
    } else if (rc == SQLITE_CONSTRAINT) {
        std::ignore = sqlite3_reset(insertStmt);
        std::ignore = sqlite3_clear_bindings(insertStmt);
        bindSelect(selectStmt);
        rc = sqlite3_step(selectStmt);
        if (rc == SQLITE_ROW) {
            id = extractId(selectStmt);
        } else {
            dprintf(D_ERROR, "Race condition SELECT fallback failed: %s\n",
                    sqlite3_errmsg(sqlite3_db_handle(selectStmt)));
        }
        std::ignore = sqlite3_reset(selectStmt);
        std::ignore = sqlite3_clear_bindings(selectStmt);
    } else {
        dprintf(D_ERROR, "Insert failed: %s\n", sqlite3_errmsg(sqlite3_db_handle(insertStmt)));
    }

    std::ignore = sqlite3_reset(insertStmt);
    std::ignore = sqlite3_clear_bindings(insertStmt);
    return id;
}

bool DBHandler::insertUnseenJob(const std::string& owner, int clusterId, int procId, int64_t timeOfCreation) {
    int userId = getOrInsertIdPrepared(
        userInsertStmt_, userSelectStmt_,
        [&](sqlite3_stmt* s) { std::ignore = sqlite3_bind_text(s, 1, owner.c_str(), -1, SQLITE_TRANSIENT); },
        [&](sqlite3_stmt* s) { std::ignore = sqlite3_bind_text(s, 1, owner.c_str(), -1, SQLITE_TRANSIENT); },
        [](sqlite3_stmt* s)  { return sqlite3_column_int(s, 0); }
    );
    if (userId == -1) {
        dprintf(D_ERROR, "Failed to insert/lookup user: %s\n", owner.c_str());
        return false;
    }

    int jobListId = getOrInsertIdPrepared(
        jobListInsertStmt_, jobListSelectStmt_,
        [&](sqlite3_stmt* s) {
            std::ignore = sqlite3_bind_int(s, 1, clusterId);
            std::ignore = sqlite3_bind_int(s, 2, userId);
        },
        [&](sqlite3_stmt* s) {
            std::ignore = sqlite3_bind_int(s, 1, clusterId);
            std::ignore = sqlite3_bind_int(s, 2, userId);
        },
        [](sqlite3_stmt* s)  { return sqlite3_column_int(s, 0); }
    );
    if (jobListId == -1) {
        dprintf(D_ERROR, "Failed to insert/lookup jobListId for {cluster, user}: {%d, %d}\n",
                clusterId, userId);
        return false;
    }

    int jobId = getOrInsertIdPrepared(
        jobInsertStmt_, jobSelectStmt_,
        [&](sqlite3_stmt* s) {
            std::ignore = sqlite3_bind_int(s, 1, clusterId);
            std::ignore = sqlite3_bind_int(s, 2, procId);
            std::ignore = sqlite3_bind_int(s, 3, userId);
            std::ignore = sqlite3_bind_int(s, 4, jobListId);
            std::ignore = sqlite3_bind_int64(s, 5, timeOfCreation);
        },
        [&](sqlite3_stmt* s) {
            std::ignore = sqlite3_bind_int(s, 1, clusterId);
            std::ignore = sqlite3_bind_int(s, 2, procId);
        },
        [](sqlite3_stmt* s) { return sqlite3_column_int(s, 0); }
    );
    if (jobId == -1) {
        dprintf(D_ERROR, "Failed to insert/lookup jobId for {cluster, proc}: {%d, %d}\n",
                clusterId, procId);
        return false;
    }

    jobIdCache_.insert({clusterId, procId}, {jobId, jobListId});
    return true;
}

bool DBHandler::batchInsertJobRecords(const std::vector<ArchiveRecord>& records, long fileId) {
    if (records.empty()) {
        dprintf(D_FULLDEBUG, "No records to batch insert\n");
        return false;
    }

    const char* sql = R"(
        INSERT INTO JobRecords (Offset, CompletionDate, JobId, FileId, JobListId)
        VALUES (?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare JobRecords insert: %s\n", sqlite3_errmsg(db_));
        return false;
    }

    for (const auto& rec : records) {
        int clusterId = 0, procId = 0;
        long completionDate = 0;
        std::string owner;
        rec.Banner().LookupInteger("ClusterId",      clusterId);
        rec.Banner().LookupInteger("ProcId",         procId);
        rec.Banner().LookupInteger("CompletionDate", completionDate);
        rec.Banner().LookupString( "Owner",          owner);
        int64_t offset = rec.GetRecordOffset();

        int jobId, jobListId;
        std::tie(jobId, jobListId) = jobIdLookup(clusterId, procId);

        if (jobId == -1) {
            if ( ! insertUnseenJob(owner, clusterId, procId, completionDate)) {
                dprintf(D_ERROR, "Failed to create job info for %d.%d\n", clusterId, procId);
                sqlite3_finalize(stmt);
                return false;
            }
            std::tie(jobId, jobListId) = jobIdLookup(clusterId, procId);
        }

        std::ignore = sqlite3_bind_int64(stmt, 1, offset);
        std::ignore = sqlite3_bind_int64(stmt, 2, completionDate);
        std::ignore = sqlite3_bind_int(stmt,  3, jobId);
        std::ignore = sqlite3_bind_int64(stmt, 4, fileId);
        std::ignore = sqlite3_bind_int(stmt,  5, jobListId);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            dprintf(D_ERROR, "Failed to insert record at offset %lld for %d.%d: %s\n",
                    (long long)offset, clusterId, procId, sqlite3_errmsg(db_));
        }

        std::ignore = sqlite3_reset(stmt);
        std::ignore = sqlite3_clear_bindings(stmt);
    }

    std::ignore = sqlite3_finalize(stmt);
    return true;
}

// Converts a rotation timestamp string ("YYYYMMDDTHHMMSS") to a Unix timestamp.
static int64_t convertRotationStringToTimestamp(const std::string& rotationStr) {
    if (rotationStr.empty()) return 0;
    std::tm tm = {};
    std::istringstream ss(rotationStr);
    ss >> std::get_time(&tm, "%Y%m%dT%H%M%S");
    return static_cast<int64_t>(std::mktime(&tm));
}

/**
 * Inserts ArchiveRecords and updates the File table offset atomically.
 */
bool DBHandler::insertJobFileRecords(const std::vector<ArchiveRecord>& records, const ArchiveFile& info) {
    char* errMsg = nullptr;
    int result = sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);
    if (result != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to begin transaction for job file records insertion: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    if ( ! batchInsertJobRecords(records, info.id)) { ROLLBACK_AND_RETURN(); }

    std::string updateFilesSql;
    if ( ! info.rotation_time.empty()) {
        updateFilesSql = "UPDATE Files SET LastOffset = ?, DateOfRotation = ?, FullyRead = ?, "
                         "AvgRecordSize = ?, RecordsRead = ? WHERE FileId = ?;";
    } else {
        updateFilesSql = "UPDATE Files SET LastOffset = ?, FullyRead = ?, "
                         "AvgRecordSize = ?, RecordsRead = ? WHERE FileId = ?;";
    }

    sqlite3_stmt* stmt = nullptr;
    result = sqlite3_prepare_v2(db_, updateFilesSql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare Files update statement: %s\n", sqlite3_errmsg(db_));
        ROLLBACK_AND_RETURN();
    }

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(info.last_offset));

    if ( ! info.rotation_time.empty()) {
        int64_t rotationTimestamp = convertRotationStringToTimestamp(info.rotation_time);
        std::ignore = sqlite3_bind_int64(stmt,  2, rotationTimestamp);
        std::ignore = sqlite3_bind_int(stmt,    3, info.fully_read ? 1 : 0);
        std::ignore = sqlite3_bind_double(stmt, 4, info.avg_record_size);
        std::ignore = sqlite3_bind_int64(stmt,  5, info.records_read);
        std::ignore = sqlite3_bind_int64(stmt,  6, info.id);
    } else {
        std::ignore = sqlite3_bind_int(stmt,    2, info.fully_read ? 1 : 0);
        std::ignore = sqlite3_bind_double(stmt, 3, info.avg_record_size);
        std::ignore = sqlite3_bind_int64(stmt,  4, info.records_read);
        std::ignore = sqlite3_bind_int64(stmt,  5, info.id);
    }

    result = sqlite3_step(stmt);
    std::ignore = sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        dprintf(D_ERROR, "Failed to update Files table for FileId %" PRId64 ": %s\n",
                info.id, sqlite3_errmsg(db_));
        ROLLBACK_AND_RETURN();
    }

    result = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to commit transaction: %s\n", sqlite3_errmsg(db_));
        ROLLBACK_AND_RETURN();
    }

    return true;
}


// -------------------------
// File Info Insertion / Update
// -------------------------

/**
 * Inserts a new file row (INSERT OR IGNORE) then reads back FileId and LastOffset.
 * Populates info.id and info.last_offset on return.
 */
void DBHandler::writeFileInfo(ArchiveFile& info) {
    const char* insertSql = R"(
        INSERT OR IGNORE INTO Files(FileName, FileInode, FileHash, LastOffset, AvgRecordSize, RecordsRead)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* insertStmt = nullptr;
    if (sqlite3_prepare_v2(db_, insertSql, -1, &insertStmt, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare insert statement: %s\n", sqlite3_errmsg(db_));
        return;
    }

    std::ignore = sqlite3_bind_text(insertStmt,   1, info.filename.c_str(), -1, SQLITE_TRANSIENT);
    std::ignore = sqlite3_bind_int64(insertStmt,  2, info.inode);
    std::ignore = sqlite3_bind_text(insertStmt,   3, info.hash.c_str(), -1, SQLITE_TRANSIENT);
    std::ignore = sqlite3_bind_int64(insertStmt,  4, info.last_offset);
    std::ignore = sqlite3_bind_double(insertStmt, 5, info.avg_record_size);
    std::ignore = sqlite3_bind_int64(insertStmt,  6, info.records_read);

    if (sqlite3_step(insertStmt) != SQLITE_DONE) {
        dprintf(D_ERROR, "Insert (or ignore) failed: %s\n", sqlite3_errmsg(db_));
        std::ignore = sqlite3_finalize(insertStmt);
        return;
    }
    std::ignore = sqlite3_finalize(insertStmt);

    const char* selectSql = R"(
        SELECT FileId, LastOffset, AvgRecordSize, RecordsRead FROM Files WHERE FileInode = ? AND FileHash = ?
    )";

    sqlite3_stmt* selectStmt = nullptr;
    if (sqlite3_prepare_v2(db_, selectSql, -1, &selectStmt, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare select statement: %s\n", sqlite3_errmsg(db_));
        return;
    }

    std::ignore = sqlite3_bind_int64(selectStmt, 1, info.inode);
    std::ignore = sqlite3_bind_text(selectStmt,  2, info.hash.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(selectStmt) == SQLITE_ROW) {
        info.id              = sqlite3_column_int(selectStmt, 0);
        info.last_offset     = sqlite3_column_int64(selectStmt, 1);
        info.avg_record_size = sqlite3_column_double(selectStmt, 2);
        info.records_read    = sqlite3_column_int64(selectStmt, 3);
    } else {
        dprintf(D_ERROR, "Failed to fetch FileId/LastOffset after insert/ignore.\n");
    }
    std::ignore = sqlite3_finalize(selectStmt);
}

/**
 * Updates an existing file row — filename, rotation timestamp, offset, and fully_read —
 * keyed by FileId so the update succeeds even after a filename change.
 */
void DBHandler::updateFileInfo(const ArchiveFile& info) {
    const char* updateSQL = R"(
        UPDATE Files
        SET FileName = ?, DateOfRotation = ?, LastOffset = ?, FullyRead = ?,
            AvgRecordSize = ?, RecordsRead = ?
        WHERE FileId = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, updateSQL, -1, &stmt, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare update statement: %s\n", sqlite3_errmsg(db_));
        return;
    }

    int64_t rotationTimestamp = convertRotationStringToTimestamp(info.rotation_time);

    std::ignore = sqlite3_bind_text(stmt,   1, info.filename.c_str(), -1, SQLITE_TRANSIENT);
    std::ignore = sqlite3_bind_int64(stmt,  2, rotationTimestamp);
    std::ignore = sqlite3_bind_int64(stmt,  3, info.last_offset);
    std::ignore = sqlite3_bind_int(stmt,    4, info.fully_read ? 1 : 0);
    std::ignore = sqlite3_bind_double(stmt, 5, info.avg_record_size);
    std::ignore = sqlite3_bind_int64(stmt,  6, info.records_read);
    std::ignore = sqlite3_bind_int64(stmt,  7, info.id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        dprintf(D_ERROR, "Failed to update file info for FileId %" PRId64 " (%s): %s\n",
                info.id, info.filename.c_str(), sqlite3_errmsg(db_));
    }
    std::ignore = sqlite3_finalize(stmt);
}


void DBHandler::markFileDeleted(long fileId, int64_t deletionTime) {
    const char* sql = "UPDATE Files SET DateOfDeletion = ? WHERE FileId = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "markFileDeleted: prepare failed: %s\n", sqlite3_errmsg(db_));
        return;
    }
    std::ignore = sqlite3_bind_int64(stmt, 1, deletionTime);
    std::ignore = sqlite3_bind_int64(stmt, 2, fileId);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        dprintf(D_ERROR, "markFileDeleted: update failed for FileId %ld: %s\n",
                fileId, sqlite3_errmsg(db_));
    }
    std::ignore = sqlite3_finalize(stmt);
}


// -------------------------
// Status Tracking
// -------------------------

bool DBHandler::writeStatusAndData(const Status& status, const StatusData& statusData) {
    const char* statusSql = R"(
        INSERT INTO Status (
            TimeOfUpdate,
            FileIdLastRead,
            FileOffsetLastRead,
            TotalRecordsRead,
            DurationMs,
            JobBacklogEstimate,
            HitMaxIngestLimit,
            GarbageCollectionRun
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?);
    )";

    const char* statusDataSql = R"(
        INSERT OR REPLACE INTO StatusData (
            StatusDataId,
            AvgAdsIngestedPerCycle,
            AvgIngestDurationMs,
            MeanIngestHz,
            MeanArrivalHz,
            MeanBacklogEstimate,
            TotalCycles,
            TotalAdsIngested,
            HitMaxIngestLimitRate,
            LastRunLeftBacklog,
            TimeOfLastUpdate
        ) VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to begin transaction: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    bool success = false;

    if (sqlite3_prepare_v2(db_, statusSql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::ignore = sqlite3_bind_int64(stmt, 1, status.update_time);
        std::ignore = sqlite3_bind_int64(stmt, 2, status.last_file_id);
        std::ignore = sqlite3_bind_int64(stmt, 3, status.last_file_offset);
        std::ignore = sqlite3_bind_int64(stmt, 4, status.records_processed);
        std::ignore = sqlite3_bind_int64(stmt, 5, status.duration_ms);
        std::ignore = sqlite3_bind_int64(stmt, 6, status.backlog_estimate);
        std::ignore = sqlite3_bind_int(stmt,   7, status.hit_ingest_limit   ? 1 : 0);
        std::ignore = sqlite3_bind_int(stmt,   8, status.ran_garbage_collect ? 1 : 0);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            dprintf(D_ERROR, "Failed to insert into Status: %s\n", sqlite3_errmsg(db_));
        } else {
            success = true;
        }
    } else {
        dprintf(D_ERROR, "Failed to prepare Status insert: %s\n", sqlite3_errmsg(db_));
    }
    std::ignore = sqlite3_finalize(stmt);

    if (success) {
        if (sqlite3_prepare_v2(db_, statusDataSql, -1, &stmt, nullptr) == SQLITE_OK) {
            int idx = 1;
            std::ignore = sqlite3_bind_double(stmt, idx++, statusData.AvgAdsIngestedPerCycle);
            std::ignore = sqlite3_bind_double(stmt, idx++, statusData.AvgIngestDurationMs);
            std::ignore = sqlite3_bind_double(stmt, idx++, statusData.MeanIngestHz);
            std::ignore = sqlite3_bind_double(stmt, idx++, statusData.MeanArrivalHz);
            std::ignore = sqlite3_bind_double(stmt, idx++, statusData.MeanBacklogEstimate);
            std::ignore = sqlite3_bind_int64(stmt,  idx++, statusData.TotalCycles);
            std::ignore = sqlite3_bind_int64(stmt,  idx++, statusData.TotalAdsIngested);
            std::ignore = sqlite3_bind_double(stmt, idx++, statusData.HitMaxIngestLimitRate);
            std::ignore = sqlite3_bind_int(stmt,    idx++, statusData.LastRunLeftBacklog ? 1 : 0);
            std::ignore = sqlite3_bind_int64(stmt,  idx++, statusData.TimeOfLastUpdate);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                dprintf(D_ERROR, "Failed to insert/update StatusData: %s\n", sqlite3_errmsg(db_));
                success = false;
            }
        } else {
            dprintf(D_ERROR, "Failed to prepare StatusData insert/update: %s\n", sqlite3_errmsg(db_));
            success = false;
        }
        std::ignore = sqlite3_finalize(stmt);
    }

    if (success) {
        pruneStatusTable(config[conf::i::StatusRetentionSeconds]);
        if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            dprintf(D_ERROR, "Failed to commit transaction: %s\n", errMsg);
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    } else {
        ROLLBACK_AND_RETURN();
    }
}

void DBHandler::pruneStatusTable(int64_t retentionSeconds) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, SavedQueries::PRUNE_STATUS_SQL.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "pruneStatusTable: prepare failed: %s\n", sqlite3_errmsg(db_));
        return;
    }
    std::ignore = sqlite3_bind_int64(stmt, 1, retentionSeconds);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        dprintf(D_ERROR, "pruneStatusTable: delete failed: %s\n", sqlite3_errmsg(db_));
    }
    std::ignore = sqlite3_finalize(stmt);
}

/**
 * Recovers in-memory state from the database after a daemon restart.
 * Populates archiveFiles (keyed by full path = directory / filename) and statusData.
 */
bool DBHandler::maybeRecoverStatusAndFiles(std::map<std::string, ArchiveFile>& archiveFiles,
                                           StatusData& statusData,
                                           const std::string& directory)
{
    const char* checkSql = "SELECT COUNT(*) FROM (SELECT 1 FROM Files LIMIT 1);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, checkSql, -1, &stmt, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to check database emptiness\n");
        return false;
    }
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        dprintf(D_ERROR, "Failed to query database: %s\n", sqlite3_errmsg(db_));
        sqlite3_finalize(stmt);
        return false;
    }
    int count = sqlite3_column_int(stmt, 0);
    std::ignore = sqlite3_finalize(stmt);

    if (count == 0) {
        dprintf(D_FULLDEBUG, "No existing data found; skipping recovery.\n");
        return false;
    }

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to begin recovery transaction: %s\n", errMsg ? errMsg : "unknown");
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }

    // 1. Recover StatusData
    const char* statusDataSql =
        "SELECT AvgAdsIngestedPerCycle, AvgIngestDurationMs, MeanIngestHz, MeanArrivalHz, "
        "MeanBacklogEstimate, TotalCycles, TotalAdsIngested, HitMaxIngestLimitRate, "
        "LastRunLeftBacklog, TimeOfLastUpdate FROM StatusData WHERE StatusDataId = 1;";

    sqlite3_stmt* stmtSD = nullptr;
    rc = sqlite3_prepare_v2(db_, statusDataSql, -1, &stmtSD, nullptr);
    if (rc != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare StatusData query\n");
        ROLLBACK_AND_RETURN();
    }
    if (sqlite3_step(stmtSD) == SQLITE_ROW) {
        statusData.AvgAdsIngestedPerCycle = sqlite3_column_double(stmtSD, 0);
        statusData.AvgIngestDurationMs    = sqlite3_column_double(stmtSD, 1);
        statusData.MeanIngestHz           = sqlite3_column_double(stmtSD, 2);
        statusData.MeanArrivalHz          = sqlite3_column_double(stmtSD, 3);
        statusData.MeanBacklogEstimate    = sqlite3_column_double(stmtSD, 4);
        statusData.TotalCycles            = sqlite3_column_int64(stmtSD,  5);
        statusData.TotalAdsIngested       = sqlite3_column_int64(stmtSD,  6);
        statusData.HitMaxIngestLimitRate  = sqlite3_column_double(stmtSD, 7);
        statusData.LastRunLeftBacklog     = sqlite3_column_int(stmtSD, 8) != 0;
        statusData.TimeOfLastUpdate       = sqlite3_column_int64(stmtSD,  9);
    }
    sqlite3_finalize(stmtSD);

    // 2. Cross-check TimeOfLastUpdate against the most recent Status row
    const char* statusSql =
        "SELECT TimeOfUpdate FROM Status ORDER BY TimeOfUpdate DESC LIMIT 1;";
    sqlite3_stmt* stmtS = nullptr;
    rc = sqlite3_prepare_v2(db_, statusSql, -1, &stmtS, nullptr);
    if (rc != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare Status query\n");
        ROLLBACK_AND_RETURN();
    }
    if (sqlite3_step(stmtS) == SQLITE_ROW) {
        int64_t timeOfUpdate = sqlite3_column_int64(stmtS, 0);
        if (timeOfUpdate > statusData.TimeOfLastUpdate) {
            statusData.TimeOfLastUpdate = timeOfUpdate;
        }
    }
    std::ignore = sqlite3_finalize(stmtS);

    // 3. Recover Files — only non-deleted, non-rotated files (active tracking set)
    const char* filesSql =
        "SELECT FileId, FileName, FileInode, FileHash, LastOffset, FullyRead, "
        "AvgRecordSize, RecordsRead "
        "FROM Files WHERE DateOfDeletion IS NULL;";

    sqlite3_stmt* stmtF = nullptr;
    rc = sqlite3_prepare_v2(db_, filesSql, -1, &stmtF, nullptr);
    if (rc != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to prepare Files query\n");
        ROLLBACK_AND_RETURN();
    }
    while ((rc = sqlite3_step(stmtF)) == SQLITE_ROW) {
        ArchiveFile file{};
        file.id              = sqlite3_column_int(stmtF, 0);
        file.filename        = reinterpret_cast<const char*>(sqlite3_column_text(stmtF, 1));
        file.inode           = sqlite3_column_int64(stmtF, 2);
        file.hash            = reinterpret_cast<const char*>(sqlite3_column_text(stmtF, 3));
        file.last_offset     = sqlite3_column_int64(stmtF, 4);
        file.fully_read      = sqlite3_column_int(stmtF, 5) != 0;
        file.avg_record_size = sqlite3_column_double(stmtF, 6);
        file.records_read    = sqlite3_column_int64(stmtF, 7);
        file.size            = -1;
        file.last_modified   = 0;

        std::string path = (fs::path(directory) / file.filename).string();
        archiveFiles[path] = std::move(file);
    }
    std::ignore = sqlite3_finalize(stmtF);

    rc = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        dprintf(D_ERROR, "Failed to commit recovery transaction: %s\n", errMsg ? errMsg : "unknown");
        if (errMsg) sqlite3_free(errMsg);
        ROLLBACK_AND_RETURN();
    }

    return true;
}


// -------------------------
// Garbage Collection
// -------------------------

bool DBHandler::runGarbageCollection(const std::string& gcQuerySQL, int fileLimit) {
    if ( ! db_) return false;

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        dprintf(D_ERROR, "Garbage collection begin transaction failed: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    size_t pos = gcQuerySQL.find(';');
    if (pos == std::string::npos) {
        dprintf(D_ERROR, "Invalid Garbage Collection SQL — no statement terminator\n");
        ROLLBACK_AND_RETURN();
    }

    std::string step1Sql = gcQuerySQL.substr(0, pos + 1);
    std::string step2Sql = gcQuerySQL.substr(pos + 1);

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, step1Sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        dprintf(D_ERROR, "Garbage collection prepare step1 failed: %s\n", sqlite3_errmsg(db_));
        ROLLBACK_AND_RETURN();
    }
    if (sqlite3_bind_int(stmt, 1, fileLimit) != SQLITE_OK) {
        dprintf(D_ERROR, "Garbage collection bind failed: %s\n", sqlite3_errmsg(db_));
        std::ignore = sqlite3_finalize(stmt);
        ROLLBACK_AND_RETURN();
    }
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        dprintf(D_ERROR, "Garbage collection execute step1 failed: %s\n", sqlite3_errmsg(db_));
        std::ignore = sqlite3_finalize(stmt);
        ROLLBACK_AND_RETURN();
    }
    sqlite3_finalize(stmt);

    if (sqlite3_exec(db_, step2Sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        dprintf(D_ERROR, "Garbage collection execute step2 failed: %s\n", errMsg);
        sqlite3_free(errMsg);
        ROLLBACK_AND_RETURN();
    }

    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        dprintf(D_ERROR, "Garbage collection commit failed: %s\n", errMsg);
        sqlite3_free(errMsg);
        ROLLBACK_AND_RETURN();
    }

    dprintf(D_FULLDEBUG, "Garbage collection successful.\n");
    return true;
}
