CREATE TABLE IF NOT EXISTS Files (
    FileId INTEGER PRIMARY KEY AUTOINCREMENT,
    FileName TEXT,
    FileInode INTEGER,
    FileHash TEXT,
    LastOffset INTEGER,
    DateOfRotation INTEGER
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_unique_inode_hash ON Files(FileInode, FileHash);

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
CREATE INDEX IF NOT EXISTS idx_Owner ON Jobs(UserId);
CREATE UNIQUE INDEX IF NOT EXISTS idx_unique_cluster_proc ON Jobs(ClusterId, ProcId);

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
    HitMaxIngestLimit BOOLEAN DEFAULT 0                -- Whether this ingestion cycle hit the max ingest limit
);

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
