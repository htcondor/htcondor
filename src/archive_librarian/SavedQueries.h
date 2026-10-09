#ifndef SAVED_QUERIES_H
#define SAVED_QUERIES_H

#include <string>

// TODO: Remove Namespace
// TODO: Move DBHandler statments here

namespace SavedQueries {
    
    // Database schema initialization query
    const std::string SCHEMA_SQL = R"(
CREATE TABLE IF NOT EXISTS Files (
    FileId INTEGER PRIMARY KEY AUTOINCREMENT,
    FileName TEXT,
    FileInode INTEGER,
    FileHash TEXT,
    LastOffset INTEGER,
    DateOfRotation INTEGER,
    DateOfDeletion INTEGER,
    FullyRead INTEGER DEFAULT 0,
    AvgRecordSize REAL DEFAULT 0.0,
    RecordsRead INTEGER DEFAULT 0
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_unique_inode_hash ON Files(FileInode, FileHash);
CREATE INDEX IF NOT EXISTS idx_date_of_deletion ON Files(DateOfDeletion);

CREATE TABLE IF NOT EXISTS Users (
    UserId INTEGER PRIMARY KEY, 
    UserName TEXT, 
    DateOfLastJob INTEGER,    -- NULL while the user has indexed jobs; else time GC removed the user's last job (see GC_MARK_USERS_SQL)
    UNIQUE (UserName)
);

CREATE TABLE IF NOT EXISTS JobLists (
    JobListId INTEGER PRIMARY KEY,  
    ClusterId INTEGER NOT NULL, 
    UserId INTEGER, 
    FOREIGN KEY (UserId) REFERENCES Users(UserId)
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_unique_cluster_user ON JobLists(ClusterId, UserId);
CREATE INDEX IF NOT EXISTS idx_UserIdInJobLists ON JobLists(UserId);

CREATE TABLE IF NOT EXISTS Jobs (    -- Info from Spawn Ads
    JobId INTEGER PRIMARY KEY AUTOINCREMENT, 
    ClusterId INTEGER NOT NULL,
    ProcId INTEGER NOT NULL,
    UserId INTEGER,
    TimeOfCreation INTEGER, 
    JobListId INTEGER,
    UNIQUE (ClusterId, ProcId),
    FOREIGN KEY (JobListId) REFERENCES JobLists(JobListId),
    FOREIGN KEY (UserId)    REFERENCES Users(UserId)
);
CREATE INDEX IF NOT EXISTS idx_OwnerInJobs ON Jobs(UserId);
CREATE UNIQUE INDEX IF NOT EXISTS idx_unique_cluster_proc_jobs ON Jobs(ClusterId, ProcId);
CREATE INDEX IF NOT EXISTS idx_JobListIdInJobs ON Jobs(JobListId);

CREATE TABLE IF NOT EXISTS JobRecords (
    JobRecordId INTEGER PRIMARY KEY AUTOINCREMENT,
    Offset INTEGER,
    CompletionDate INTEGER,
    JobId INTEGER,
    FileId INTEGER,
    JobListId INTEGER,
    DAGManJobId INTEGER,
    JobBatchId TEXT,
    JobBatchName TEXT,
    FOREIGN KEY(JobId) REFERENCES Jobs(JobId),
    FOREIGN KEY(FileId) REFERENCES Files(FileId),
    FOREIGN KEY (JobListId) REFERENCES JobLists(JobListId)
);
CREATE INDEX IF NOT EXISTS idx_JobIdInJobRecords ON JobRecords(JobId);
CREATE INDEX IF NOT EXISTS idx_JobListIdInJobRecords ON JobRecords(JobListId);
CREATE INDEX IF NOT EXISTS idx_FileIdInJobRecords ON JobRecords(FileId);

CREATE TABLE IF NOT EXISTS Status (
    StatusId INTEGER PRIMARY KEY AUTOINCREMENT,
    TimeOfUpdate INTEGER NOT NULL,                     -- Timestamp for this status record

    FileIdLastRead INTEGER,                            -- FK to Files table (history)
    FileOffsetLastRead INTEGER,                        -- Byte offset in that history file

    TotalRecordsRead INTEGER DEFAULT 0,                -- Records processed
    DurationMs INTEGER DEFAULT 0,                      -- Duration of update cycle
    JobBacklogEstimate INTEGER DEFAULT 0,              -- Estimated number of unprocessed ads
    HitMaxIngestLimit BOOLEAN DEFAULT 0,               -- Whether this ingestion cycle hit the max ingest limit
    GarbageCollectionRun BOOLEAN DEFAULT 0,            -- Whether garbage collection ran during this ingestion cycle
    RecordsLostOnInsertFailure INTEGER DEFAULT 0       -- Records read but dropped due to a failed DB insert
);
CREATE INDEX IF NOT EXISTS idx_TimeOfUpdateInStatus ON Status(TimeOfUpdate);

CREATE TABLE IF NOT EXISTS StatusData (
    StatusDataId INTEGER PRIMARY KEY CHECK (StatusDataId = 1 OR StatusDataId = 2),

    AvgAdsIngestedPerCycle REAL,          -- Mean ads processed per ingest cycle
    AvgIngestDurationMs REAL,             -- Mean duration (ms) per ingest cycle
    MeanIngestHz REAL,                    -- Mean ingest rate (ads/sec)
    MeanArrivalHz REAL,                   -- Mean arrival rate of new ads (ads/sec)
    MeanBacklogEstimate REAL,             -- Average estimated backlog size

    TotalCycles INTEGER,                  -- Total number of timeout cycles
    TotalAdsIngested INTEGER,             -- Cumulative count of ads ingested
    TotalRecordsLost INTEGER DEFAULT 0,   -- Cumulative count of records dropped due to insert failures

    HitMaxIngestLimitRate REAL,           -- Proportion of cycles that hit ingest cap
    LastRunLeftBacklog BOOLEAN DEFAULT 0, -- Whether the previous run left backlog
    TimeOfLastUpdate INTEGER              -- Timestamp of most recent update
);

)";

// TODO: Break this up and do logic/steps in librarian code
    // Garbage collection query
    const std::string GC_QUERY_SQL = R"(
-- 1. Find files to delete (ordered by deletion date, limited by job count target that we calculate)
      -- Creates a temporary table with the FileIds of only the Files that we want to delete
CREATE TEMP TABLE IF NOT EXISTS FilesToDelete AS 
SELECT FileId FROM Files 
WHERE DateOfDeletion IS NOT NULL 
ORDER BY DateOfDeletion ASC, FileId ASC  -- must match DBHandler::countFilesToCollect()
LIMIT ?; -- calculated based on job count needed

-- 2. Collect JobIds 
      -- Finds all JobIds that are in those selected Files 
CREATE TEMP TABLE IF NOT EXISTS JobsToDelete AS
SELECT DISTINCT JobId 
FROM JobRecords 
WHERE FileId IN (SELECT FileId FROM FilesToDelete);

-- 3. Collect JobListIds that might become empty
     -- Finds any associated JobListIds to the jobs we're about to delete and saves them to check later
CREATE TEMP TABLE IF NOT EXISTS JobListsToCheck AS
SELECT DISTINCT JobListId 
FROM JobRecords 
WHERE JobId IN (SELECT JobId FROM JobsToDelete);

-- 3.5. Collect UserIds that might lose their last indexed job
     -- Must happen before the Jobs rows are deleted; checked by GC_MARK_USERS_SQL after the deletes
CREATE TEMP TABLE IF NOT EXISTS UsersToCheck AS
SELECT DISTINCT UserId
FROM Jobs
WHERE JobId IN (SELECT JobId FROM JobsToDelete) AND UserId IS NOT NULL;

-- 4. Fast deletes using existing indexes
    -- Deletes the marked JobRecords and Jobs from their respective tables
DELETE FROM JobRecords WHERE JobId IN (SELECT JobId FROM JobsToDelete);
DELETE FROM Jobs WHERE JobId IN (SELECT JobId FROM JobsToDelete);

-- 5. Delete empty JobLists
    -- Check the marked JobListIds and see if any of them now have no associated jobs, delete if so
DELETE FROM JobLists 
WHERE JobListId IN (SELECT JobListId FROM JobListsToCheck)
AND JobListId NOT IN (SELECT DISTINCT JobListId FROM Jobs WHERE JobListId IS NOT NULL);

-- 6. Delete files
    -- Delete the File entries that we had previously marked
DELETE FROM Files WHERE FileId IN (SELECT FileId FROM FilesToDelete);

-- 7. Drop temporary tables
     -- NOTE: UsersToCheck is intentionally kept; it is consumed and dropped by GC_MARK_USERS_SQL
DROP TABLE IF EXISTS FilesToDelete;
DROP TABLE IF EXISTS JobsToDelete;
DROP TABLE IF EXISTS JobListsToCheck;
)";

    // Garbage collection: timestamp users whose last indexed job was just removed.
    // Run after GC_QUERY_SQL within the same transaction. Parameter 1 is the current time.
    const std::string GC_MARK_USERS_SQL = R"(
UPDATE Users SET DateOfLastJob = ?1
WHERE UserId IN (SELECT UserId FROM UsersToCheck)
  AND DateOfLastJob IS NULL
  AND NOT EXISTS (SELECT 1 FROM Jobs j      WHERE j.UserId  = Users.UserId)
  AND NOT EXISTS (SELECT 1 FROM JobLists jl WHERE jl.UserId = Users.UserId);
)";

    const std::string GC_DROP_USERS_TO_CHECK_SQL = R"(DROP TABLE IF EXISTS UsersToCheck;)";

    // Garbage collection: remove users with no indexed jobs whose DateOfLastJob is at
    // or before the retention cutoff (parameter 1 = now - retention seconds).
    const std::string GC_PRUNE_USERS_SQL = R"(
DELETE FROM Users
WHERE DateOfLastJob IS NOT NULL
  AND DateOfLastJob <= ?1
  AND NOT EXISTS (SELECT 1 FROM Jobs j      WHERE j.UserId  = Users.UserId)
  AND NOT EXISTS (SELECT 1 FROM JobLists jl WHERE jl.UserId = Users.UserId);
)";

    // Clear a user's DateOfLastJob once they have an indexed job again
    const std::string USER_CLEAR_LAST_JOB_SQL = R"(UPDATE Users SET DateOfLastJob = NULL WHERE UserId = ? AND DateOfLastJob IS NOT NULL;)";

    // Prune Status rows older than a configurable retention window (parameterized in seconds)
    const std::string PRUNE_STATUS_SQL = R"(DELETE FROM Status WHERE TimeOfUpdate < strftime('%s','now') - ?;)";
}

#endif // SAVED_QUERIES_H