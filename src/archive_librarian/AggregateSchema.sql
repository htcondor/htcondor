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

CREATE TABLE IF NOT EXISTS Jobs (
    JobId INTEGER PRIMARY KEY AUTOINCREMENT, 
    ClusterId INTEGER NOT NULL,
    ProcId INTEGER NOT NULL,
    UserId INTEGER,
    TimeOfCreation INTEGER, 
    JobListId INTEGER,
    UNIQUE (ClusterId, ProcId),
    FOREIGN KEY (JobListId) REFERENCES JobLists(JobListId), 
    FOREIGN KEY (UserId) REFERENCES Users(UserId)
);
CREATE INDEX IF NOT EXISTS idx_Owner ON Jobs(UserId);

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
    DurationMs INTEGER DEFAULT 0                       -- Duration of update cycle
);
