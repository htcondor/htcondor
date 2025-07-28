#ifndef SAVED_QUERIES_H
#define SAVED_QUERIES_H

#include <string>

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
    FullyRead INTEGER DEFAULT 0
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_unique_inode_hash ON Files(FileInode, FileHash);
CREATE INDEX IF NOT EXISTS idx_date_of_deletion ON Files(DateOfDeletion);

CREATE TABLE IF NOT EXISTS Users (
    UserId INTEGER PRIMARY KEY, 
    UserName TEXT, 
    UNIQUE (UserName)
);

CREATE TABLE IF NOT EXISTS JobLists (
    JobListId INTEGER PRIMARY KEY,  
    ClusterId INTEGER NOT NULL, 
    UserId INTEGER, 
    FOREIGN KEY (UserId) REFERENCES Users(UserId)
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_unique_cluster_user ON JobLists(ClusterId, UserId);

CREATE TABLE IF NOT EXISTS Jobs (    -- Info from Spawn Ads
    JobId INTEGER PRIMARY KEY AUTOINCREMENT, 
    ClusterId INTEGER NOT NULL,
    ProcId INTEGER NOT NULL,
    UserId INTEGER,
    TimeOfCreation INTEGER, 
    JobListId INTEGER,
    FileId INTEGER,       -- For possible EpochRecord tracking functionality in the future, currently not filled
    Offset INTEGER, 
    UNIQUE (ClusterId, ProcId),
    FOREIGN KEY (JobListId) REFERENCES JobLists(JobListId), 
    FOREIGN KEY (UserId) REFERENCES Users(UserId), 
    FOREIGN KEY (FileId) REFERENCES Files(FileId)
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

    HistoryFileIdLastRead INTEGER,                     -- FK to Files table (history)
    HistoryFileOffsetLastRead INTEGER,                 -- Byte offset in that history file

    EpochFileIdLastRead INTEGER,                       -- FK to Files table (epoch)
    EpochFileOffsetLastRead INTEGER,                   -- Byte offset in that epoch file

    TotalJobsRead INTEGER DEFAULT 0,                   -- From history files
    TotalEpochsRead INTEGER DEFAULT 0,                 -- From epoch files
    DurationMs INTEGER DEFAULT 0,                      -- Duration of update cycle
    JobBacklogEstimate INTEGER DEFAULT 0,              -- Estimated number of unprocessed ads
    HitMaxIngestLimit BOOLEAN DEFAULT 0,               -- Whether this ingestion cycle hit the max ingest limit
    GarbageCollectionRun BOOLEAN DEFAULT 0             -- Whether garbage collection ran during this ingestion cycle
);
CREATE INDEX IF NOT EXISTS idx_TimeOfUpdateInStatus ON Status(TimeOfUpdate);

CREATE TABLE IF NOT EXISTS StatusData (
    StatusDataId INTEGER PRIMARY KEY CHECK (StatusDataId = 1),     

    AvgAdsIngestedPerCycle REAL,          -- Mean ads processed per ingest cycle
    AvgIngestDurationMs REAL,             -- Mean duration (ms) per ingest cycle
    MeanIngestHz REAL,                    -- Mean ingest rate (ads/sec)
    MeanArrivalHz REAL,                   -- Mean arrival rate of new ads (ads/sec)
    MeanBacklogEstimate REAL,             -- Average estimated backlog size

    TotalCycles INTEGER,                  -- Total number of timeout cycles
    TotalAdsIngested INTEGER,             -- Cumulative count of ads ingested

    HitMaxIngestLimitRate REAL,           -- Proportion of cycles that hit ingest cap
    LastRunLeftBacklog BOOLEAN DEFAULT 0, -- Whether the previous run left backlog
    TimeOfLastUpdate INTEGER              -- Timestamp of most recent update
);
)";

    // Garbage collection query
    const std::string GC_QUERY_SQL = R"(
-- 1. Find files to delete (ordered by deletion date, limited by job count target that we calculate)
      -- Creates a temporary table with the FileIds of only the Files that we want to delete
CREATE TEMP TABLE FilesToDelete AS 
SELECT FileId FROM Files 
WHERE DateOfDeletion IS NOT NULL 
ORDER BY DateOfDeletion ASC 
LIMIT ?; -- calculated based on job count needed

-- 2. Collect JobIds 
      -- Finds all JobIds that are in those selected Files 
CREATE TEMP TABLE JobsToDelete AS
SELECT DISTINCT JobId 
FROM JobRecords 
WHERE FileId IN (SELECT FileId FROM FilesToDelete);

-- 3. Collect JobListIds that might become empty
     -- Finds any associated JobListIds to the jobs we're about to delete and saves them to check later
CREATE TEMP TABLE JobListsToCheck AS
SELECT DISTINCT JobListId 
FROM JobRecords 
WHERE JobId IN (SELECT JobId FROM JobsToDelete);

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

-- 7. Delete all Status updates that are more than 7 days old
DELETE FROM Status
WHERE TimeOfUpdate < strftime('%s','now') - (7 * 86400);
)";
}

#endif // SAVED_QUERIES_H