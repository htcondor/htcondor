#include "librarian.h"
#include "readHistory.h"
#include "archiveMonitor.h"
#include <cstdio>
#include <chrono>

Librarian::Librarian(const std::string& schemaPath,
                     const std::string& dbPath,
                     const std::string& historyFilePath,
                     const std::string& epochHistoryFilePath,
                     size_t jobCacheSize)
    : schemaPath_(schemaPath),
      dbPath_(dbPath),
      historyFilePath_(historyFilePath),
      epochHistoryFilePath_(epochHistoryFilePath),
      jobCacheSize_(jobCacheSize) {}

bool Librarian::initialize() {
    // Constructy the DBHandler with provided schema, db path, and cache size. 
    printf("[Librarian] Initializing DBHandler...\n");
    dbHandler_ = std::make_unique<DBHandler>(schemaPath_, dbPath_, jobCacheSize_);
    printf("[Librarian] DBHandler initialized.\n");
    return true;
}

// Collects and records metadata for both epoch and history files before they're read
// Enters this information into the 'Files' table of the DB
bool Librarian::updateFileInfo(FileInfo& epochFileInfo, FileInfo& historyFileInfo) {
    epochFileInfo = collectNewFileInfo(epochHistoryFilePath_);
    historyFileInfo = collectNewFileInfo(historyFilePath_);
    dbHandler_->writeFileInfo(epochFileInfo);
    dbHandler_->writeFileInfo(historyFileInfo);
    return true;  // TODO: add error handling
}

// Reads new epoch records from the epoch history file and updates the offset
// Also fills the Status struct with metadata for ArchiveMonitor and service health tracking
bool Librarian::readEpochRecords(std::vector<EpochRecord>& newEpochRecords, FileInfo& fileInfo, Status& status) {
    long newOffset = readEpochIncremental(epochHistoryFilePath_.c_str(), newEpochRecords, fileInfo);
    if (newOffset < 0) {
        fprintf(stderr, "[Librarian] Failed to read epoch records.\n");
        return false;
    }
    fileInfo.LastOffset = newOffset;
    status.EpochFileOffsetLastRead = newOffset;
    status.EpochFileIdLastRead = fileInfo.FileId;
    status.TotalEpochsRead = newEpochRecords.size();
    return true;
}

// Reads new job records from the history file and updates the offset
// Also fills the Status struct with metadata for ArchiveMonitor and service health tracking
bool Librarian::readJobRecords(std::vector<JobRecord>& newJobRecords, FileInfo& fileInfo, Status& status) {
    long newOffset = readHistoryIncremental(historyFilePath_.c_str(), newJobRecords, fileInfo);
    if (newOffset < 0) {
        fprintf(stderr, "[Librarian] Failed to read job records.\n");
        return false;
    }
    fileInfo.LastOffset = newOffset;
    status.HistoryFileOffsetLastRead = newOffset;
    status.HistoryFileIdLastRead = fileInfo.FileId;
    status.TotalJobsRead = newJobRecords.size();
    return true;
}

// Inserts new epoch and job records into their respective DB tables ('Job' and 'JobRecord')
void Librarian::insertRecords(const std::vector<EpochRecord>& epochRecords, const std::vector<JobRecord>& jobRecords) {
    if (!epochRecords.empty()) dbHandler_->batchInsertEpochRecords(epochRecords);
    if (!jobRecords.empty()) dbHandler_->batchInsertJobRecords(jobRecords);
}

// Runs one full update cycle:
// 1. Gathers file metadata (inode/hash/offset)
// 2. Reads new epoch and job records from respective history files
// 3. Inserts records into DB
// 4. Updates offset metadata and writes timing info into 'Status' table
bool Librarian::update() {
    printf("[Librarian] Starting update protocol...\n");
    auto startTime = std::chrono::high_resolution_clock::now();
    Status status;

    // Step 1: Gather latest file metadata to populate FileInfo structs
    FileInfo epochFileInfo;
    FileInfo historyFileInfo;
    if (!updateFileInfo(epochFileInfo, historyFileInfo)) return false;

    // Step 2: Read new records from each file
    std::vector<EpochRecord> newEpochRecords;
    if (!readEpochRecords(newEpochRecords, epochFileInfo, status)) return false;

    std::vector<JobRecord> newJobRecords;
    if (!readJobRecords(newJobRecords, historyFileInfo, status)) return false;

    // Step 3: Insert all new records into DB
    insertRecords(newEpochRecords, newJobRecords);

    // Step 4: Update file info and write new status 
    dbHandler_->updateFileInfo(epochFileInfo, historyFileInfo);
    auto endTime = std::chrono::high_resolution_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // Add time values and update status: when and how long did this update fake
    status.TimeOfUpdate = timestamp;
    status.DurationMs = durationMs;
    dbHandler_->writeStatus(status);

    printf("[Librarian] Update protocol completed successfully. Inserted %zu job records, %zu epoch records in %ld ms.\n",
        status.TotalJobsRead, status.TotalEpochsRead, status.DurationMs);
    return true;
}
