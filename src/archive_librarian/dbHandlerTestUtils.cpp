#include "dbHandlerTestUtils.h"
#include <sqlite3.h>
#include <iostream>
#include <unordered_set>



// -------------------------
// Testing / Validation
// -------------------------

/**
 * Return all JobRecords currently in the database.
 */
std::vector<JobRecord> DBHandlerTestUtils::readAllJobs(const DBHandler& handler) { 
    std::vector<JobRecord> jobs;
    const char* sql = "SELECT JobId, CompletionDate, Offset, FileId FROM JobRecords;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler.getDB(); 

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
std::vector<FileInfo> DBHandlerTestUtils::readAllFileInfos(const DBHandler& handler) {
    std::vector<FileInfo> files;
    const char* sql = "SELECT FileId, FileName, FileInode, FileHash, LastOffset FROM Files;";
    sqlite3_stmt* stmt;
    sqlite3* db = handler.getDB(); 

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
bool DBHandlerTestUtils::clearAllTables(const DBHandler& handler) {
    sqlite3* db = handler.getDB(); 
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
void DBHandlerTestUtils::clearCache(DBHandler& handler){
    handler.clearCache();
}


/**
 * Return row count for a given table (if valid).
 */

int DBHandlerTestUtils::countTable(const DBHandler& handler, const std::string& table) {
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
    sqlite3* db = handler.getDB(); // Make sure getDB() is marked const

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
void DBHandlerTestUtils::writeTesterSpawnAd(DBHandler& handler, int clusterId, int procId, const std::string& owner, long completionDate) {
    if (!handler.insertUnseenJob(owner, clusterId, procId, completionDate)) {
        printf("[ERROR] Failed to write tester spawn ad for ClusterId=%d, ProcId=%d, Owner=%s\n",
               clusterId, procId, owner.c_str());
    }
}