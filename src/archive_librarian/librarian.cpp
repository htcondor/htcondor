#include "librarian.h"
#include "readHistory.h"
#include "archiveMonitor.h"
#include "FileSet.h"

#include <filesystem>
#include <cstdio>
#include <chrono>
#include <memory>
#include <algorithm>
#include <cmath>

namespace fs = std::filesystem;

// ================================
// CONSTRUCTOR
// ================================

Librarian::Librarian(const std::string& schemaPath,
                     const std::string& dbPath,
                     const std::string& historyFilePath,
                     const std::string& epochHistoryFilePath,
                     size_t jobCacheSize)
    : schemaPath_(schemaPath),
      dbPath_(dbPath),
      historyFilePath_(historyFilePath),
      epochHistoryFilePath_(epochHistoryFilePath),
      jobCacheSize_(jobCacheSize),
      historyFileSet_(historyFilePath),
      epochHistoryFileSet_(epochHistoryFilePath) {}


// ================================
// FILE READS AND WRITES
// ================================  

/**
 * @brief Reads new epoch records from the epoch history file and updates the file offset
 * @param newEpochRecords Output vector to store newly read epoch records
 * @param fileInfo Input/output file information containing current offset, updated with new offset
 * @return true if successful, false on failure
 */
bool Librarian::readEpochRecords(std::vector<EpochRecord>& newEpochRecords, FileInfo& fileInfo) {
    long newOffset = readEpochIncremental(epochHistoryFilePath_.c_str(), newEpochRecords, fileInfo);
    if (newOffset < 0) {
        fprintf(stderr, "[Librarian] Failed to read epoch records.\n");
        return false;
    }
    fileInfo.LastOffset = newOffset;
    return true;
}

/**
 * @brief Reads new job records from the history file and updates the file offset
 * @param newJobRecords Output vector to store newly read job records
 * @param fileInfo Input/output file information containing current offset, updated with new offset
 * @return true if successful, false on failure
 */
bool Librarian::readJobRecords(std::vector<JobRecord>& newJobRecords, FileInfo& fileInfo) {
    long newOffset = readHistoryIncremental(historyFilePath_.c_str(), newJobRecords, fileInfo);
    if (newOffset < 0) {
        fprintf(stderr, "[Librarian] Failed to read job records.\n");
        return false;
    }
    fileInfo.LastOffset = newOffset;
    return true;
}


// ================================
// STATUS TRACKING UTILITIES
// ================================

// Calculates an estimated backlog of unprocessed jobs based on file sizes and bytes per job
int calculateBacklogFromBytes(
    int64_t lastFileSizeBytes,
    int64_t lastOffsetRead,
    const std::vector<std::pair<std::string, int64_t>>& unreadFiles,
    double estimatedBytesPerJob
) {
    if (estimatedBytesPerJob <= 0) return 0;

    // Bytes left in the partially read file
    int64_t unreadInLastFile = std::max<int64_t>(0, lastFileSizeBytes - lastOffsetRead);

    // Bytes in fully unread files
    int64_t unreadInFullFiles = 0;
    for (const auto& [filename, fileSize] : unreadFiles) {
        unreadInFullFiles += fileSize;
    }

    int64_t totalUnreadBytes = unreadInLastFile + unreadInFullFiles;

    // Estimated number of ads still unread
    int estimatedBacklog = static_cast<int>(std::round(totalUnreadBytes / estimatedBytesPerJob));
    return estimatedBacklog;
}


// ================================
// ARCHIVE MANAGEMENT (PRIVATE HELPERS)
// ================================

/**
 * @brief Applies detected archive changes to the in-memory FileSet
 * This function synchronizes the FileSet with filesystem changes detected by ArchiveMonitor
 * @param change Archive changes detected (rotations, additions, deletions)
 * @param fileSet Target FileSet to update
 */
void applyArchiveChangeToFileSet(const ArchiveChange& change, FileSet& fileSet) {

    // 1. Handle rotated file
    if (!change.rotatedFile.second.empty()) {
        const int lastFileReadId = change.rotatedFile.first;
        const std::string& newName = change.rotatedFile.second;

        auto it = fileSet.fileMap.find(lastFileReadId);
        if (it != fileSet.fileMap.end()) {
            it->second.FileName = newName;
        } else {
            fprintf(stderr, "[Librarian] Warning: Rotated file not found in fileMap. FileId=%d\n", lastFileReadId);
        }
    }

    // 2. Add new files to FileSet's FileMap
    for (const auto& newFile : change.newFiles) {
        if (newFile.FileId != 0) {
            fileSet.fileMap[newFile.FileId] = newFile;
        } else {
            fprintf(stderr, "[Librarian] Warning: New file has invalid FileId. Skipping.\n");
        }
    }

    // 3. Remove deleted files from FileSEt's FileMap
    for (int deletedFileId : change.deletedFileIds) {
        size_t erased = fileSet.fileMap.erase(deletedFileId);
        if (erased == 0) {
            fprintf(stderr, "[Librarian] Warning: Tried to delete file not found in fileMap. FileId=%d\n", deletedFileId);
        }
    }
}

/**
 * @brief Scans directory for changes and updates both database and in-memory FileSet
 * This is the main coordination function for file tracking
 * @param fileSet FileSet to scan and update
 * @return ArchiveChange struct containing all detected changes
 */
ArchiveChange Librarian::trackAndUpdateFileSet(FileSet& fileSet) {
    // Step 1: Scan the directory to detect changes since the last check.
    // This includes:
    //   - detecting rotated files
    //   - identifying newly added files
    //   - finding deleted files
    ArchiveChange fileSetChange = ArchiveMonitor::trackHistoryFileSet(fileSet);

    // Step 2: Apply SOME of those changes to the persistent database.
    // Change the name of the rotated file, add new files to directory and populate their FileIds. 
    // Does NOT handle marking deleted files as deleted, as will be added either here or later when
    // garbage collection is implemented. 
    dbHandler_->insertNewFiles(fileSetChange.rotatedFile, fileSetChange.newFiles);

    // Step 3: Apply the same changes to the in-memory FileSet,
    // so that the system’s internal state reflects what’s on disk.
    applyArchiveChangeToFileSet(fileSetChange, fileSet);

    return fileSetChange;
}



/**
 * @brief Builds a processing queue from file set and archive changes
 * @param fileSet The history file set containing file cache
 * @param changes Archive changes detected during scanning
 * @param queue Output queue of FileInfo structs to process
 * @return true if successful, false on error
 */
bool Librarian::buildProcessingQueue(const FileSet& fileSet, 
                                    const ArchiveChange& changes,
                                    std::vector<FileInfo>& queue) {
    queue.clear();

    // FIRST PRIORITY: Continue reading the last file we were working on
    if (fileSet.lastFileReadId != -1) {
        auto it = fileSet.fileMap.find(fileSet.lastFileReadId);
        if (it != fileSet.fileMap.end()) {
            queue.push_back(it->second);
            printf("[Librarian] Added lastFileRead to queue: %s (FileId: %d)\n", 
                   it->second.FileName.c_str(), it->second.FileId);
        } else {
            printf("[Librarian] Error: lastFileReadId %d not found in fileMap\n", fileSet.lastFileReadId);
            return false;
        }
    }

    // SECOND PRIORITY: Add new files from archive changes in order 
    for (const FileInfo& newFile : changes.newFiles) {
        queue.push_back(newFile);
        printf("[Librarian] Added new file to queue: %s (FileId: %d)\n", 
               newFile.FileName.c_str(), newFile.FileId);
    }

    return true;
}



// ================================
// CORE PUBLIC INTERFACE
// ================================

/**
 * @brief Initializes the Librarian service
 * This should only be called once during daemon startup
 * @return true if initialization successful, false on failure
 */
bool Librarian::initialize() { 
    printf("[Librarian] Initializing DBHandler...\n");

    // Construct the DBHandler with provided schema, db path, and cache size.
    dbHandler_ = std::make_unique<DBHandler>(schemaPath_, dbPath_, jobCacheSize_);

    // Check whether database is connected and has correct expect tables
    if (!dbHandler_->testConnection()) {
        fprintf(stderr, "[Librarian] DBHandler connection test failed\n");
        return false;
    }

    printf("[Librarian] DBHandler initialized.\n");
    return true;
}

/**
 * @brief Executes one complete update cycle
 * 
 * This is the main processing function that:
 * 1. Scans directories for file changes (rotations, additions, deletions)
 * 2. Builds processing queues for epoch and history files
 * 3. Reads new records from files and inserts them into the database
 * 4. Updates file offsets and internal state
 * 5. Calculates and stores status information
 * 
 * @return true if update cycle completed successfully, false on error
 */
bool Librarian::update() {
    printf("[Librarian] Starting update protocol...\n");

    // Initialize status tracking for this update cycle
    auto startTime = std::chrono::system_clock::now();
    Status status;

    // PHASE 1: Directory Scanning and File Tracking
    ArchiveChange epochChange = trackAndUpdateFileSet(epochHistoryFileSet_);
    ArchiveChange historyChange = trackAndUpdateFileSet(historyFileSet_);

    // PHASE 2: Queue Construction
    std::vector<FileInfo> epochQueue;
    std::vector<FileInfo> historyQueue;

    if (!buildProcessingQueue(epochHistoryFileSet_, epochChange, epochQueue)) {
        printf("[Librarian] Failed to build epoch processing queue\n");
        return false;
    }
    
    if (!buildProcessingQueue(historyFileSet_, historyChange, historyQueue)) {
        printf("[Librarian] Failed to build history processing queue\n");
        return false;
    }
    

    // PHASE 3: Record Processing

    // Process Epoch Queue
    for (FileInfo& fileInfo : epochQueue) {
        printf("[Librarian] Processing epoch file: %s (offset: %lld)\n", 
               fileInfo.FileName.c_str(), fileInfo.LastOffset);
        
        std::vector<EpochRecord> fileRecords;
        if (!readEpochRecords(fileRecords, fileInfo)) {
            printf("[Librarian] Failed to read epoch records from %s \n", fileInfo.FileName);
            return false;
        }
        
        // If nothing new to read, move on to the next file
        if(fileRecords.empty()){
            printf("[Librarian] No new epoch records in %s, skipping this file \n", fileInfo.FileName.c_str());
            continue;
        }

        // Atomic transaction: Insert records AND update file offset
        if (!dbHandler_->insertEpochFileRecords(fileRecords, fileInfo)) {
                printf("[Librarian] Failed to atomically process epoch file %s\n", fileInfo.FileName.c_str());
                continue;
        }
        
        // Update in-memory FileSet with new offset
        auto it = epochHistoryFileSet_.fileMap.find(fileInfo.FileId);
        if (it != epochHistoryFileSet_.fileMap.end()) {
            it->second.LastOffset = fileInfo.LastOffset;
        } else {
            fprintf(stderr, "[Librarian] Warning: FileId %d not found in epochHistoryFileSet.fileMap\n", fileInfo.FileId);
        }
        
        // Accumulate stats for status reporting
        status.TotalEpochsRead += fileRecords.size();

        // Record the last ID read
        status.EpochFileIdLastRead = fileInfo.FileId;
        status.EpochFileOffsetLastRead = fileInfo.LastOffset;
    }
    
    // Process History Queue

    for (FileInfo& fileInfo : historyQueue) {
        printf("[Librarian] Processing history file: %s (offset: %lld)\n", 
               fileInfo.FileName.c_str(), fileInfo.LastOffset);
        
        std::vector<JobRecord> fileRecords;
        if (!readJobRecords(fileRecords, fileInfo)) {
            printf("[Librarian] Failed to read job records from %s \n", fileInfo.FileName);
            return false;
        }

        // If nothing new to read, move on to the next file
        if(fileRecords.empty()){
            printf("[Librarian] No new job records in %s, skipping this file \n", fileInfo.FileName.c_str());
            continue;
        }

        // Atomic transaction: Insert records AND update file offset
        if (!dbHandler_->insertJobFileRecords(fileRecords, fileInfo)) {
                printf("[Librarian] Failed to atomically process history file %s\n", fileInfo.FileName.c_str());
                continue;
        }
        
        // Update in-memory FileSet with new offset
        auto it = historyFileSet_.fileMap.find(fileInfo.FileId);
        if (it != historyFileSet_.fileMap.end()) {
            it->second.LastOffset = fileInfo.LastOffset;
        } else {
            fprintf(stderr, "[Librarian] Warning: FileId %d not found in historyFileSet.fileMap\n", fileInfo.FileId);
        }
        
        // Accumulate stats for status reporting
        status.TotalJobsRead += fileRecords.size();

        // Record the last ID read
        status.HistoryFileIdLastRead = fileInfo.FileId;
        status.HistoryFileOffsetLastRead = fileInfo.LastOffset;
        
    }

    // PHASE 4: Garbage collection will be implemented here!
    // TODO: Modify the database based on the files that have been marked deleted
    // Note that file deletions have already been applied to the in-memory FileMap

    // PHASE 5: Status Update and Finalization
    auto endTime = std::chrono::system_clock::now();
    status.TimeOfUpdate = std::chrono::system_clock::to_time_t(endTime); 
    status.DurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // Persist status to database
    if (!dbHandler_->writeStatus(status)) {
        fprintf(stderr, "[Librarian] Warning: Failed to write status to database\n");
        // Don't want to fail the entire update cycle for this, just logging it
    }

    printf("[Librarian] Update protocol completed successfully. Inserted %zu job records, %zu epoch records in %ld ms.\n",
        status.TotalJobsRead, status.TotalEpochsRead, status.DurationMs);
    return true;
}
