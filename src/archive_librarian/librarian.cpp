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
#include <fstream>

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

// Finds a history file matching the config name pattern and calculates 
// EstimatedBytesPerJob_ as the byte size from file start to first "***" line
bool Librarian::calculateEstimatedBytesPerJob() {
    std::error_code ec;
    std::filesystem::path historyDir(historyFileSet_.historyDirectoryPath);
    
    // Check if directory exists
    if (!std::filesystem::exists(historyDir, ec) || ec) {
        return false;
    }
    
    // Find a file that starts with historyConfigName and has additional content
    for (const auto& entry : std::filesystem::directory_iterator(historyDir, ec)) {
        if (ec) {
            return false;
        }
        
        if (entry.is_regular_file(ec) && !ec) {
            std::string filename = entry.path().filename().string();
            
            // Check if filename starts with historyConfigName and has more content after it
            if (filename.length() > historyFileSet_.historyNameConfig.length() &&
                filename.substr(0, historyFileSet_.historyNameConfig.length()) == historyFileSet_.historyNameConfig) {
                
                std::ifstream file(entry.path());
                if (!file.is_open()) {
                    continue; // Try next file
                }
                
                std::string line;
                std::streampos startPos = file.tellg();
                
                // Check if tellg() failed
                if (startPos == std::streampos(-1)) {
                    file.close();
                    continue;
                }
                
                // Read until we find a line starting with "***"
                while (std::getline(file, line)) {
                    if (line.length() >= 3 && line.substr(0, 3) == "***") {
                        std::streampos endPos = file.tellg();
                        if (endPos == std::streampos(-1)) {
                            file.close();
                            continue;
                        }
                        
                        std::streamsize chunkSize = endPos - startPos;
                        EstimatedBytesPerJob_ = static_cast<double>(chunkSize);
                        file.close();
                        return true;
                    }
                }
                
                file.close();
                // If we reach here, no "***" line was found in this file
                // Continue to try other files
            }
        }
    }
    
    // No suitable file found or no "***" line found
    return false;
}

/**
 * @brief Estimates remaining job records by calculating unread bytes in history files
 * 
 * Calculates bytes remaining in the last partially-read file plus all bytes in
 * unread files (LastOffset == 0), then estimates job count using estimatedBytesPerJob_.
 */
int Librarian::calculateBacklogFromBytes(const Status& status) {
    if (EstimatedBytesPerJob_ <= 0) return 0;

    int64_t totalUnreadBytes = 0;

    // Find the last read file info
    auto lastFileIt = historyFileSet_.fileMap.find(status.HistoryFileIdLastRead);
    if (lastFileIt != historyFileSet_.fileMap.end()) {
        const FileInfo& lastFileInfo = lastFileIt->second;
        
        try {
            // Get the size of the last read file
            int64_t lastFileSizeBytes = std::filesystem::file_size(lastFileInfo.FileName);
            
            // Calculate bytes left in the partially read file
            int64_t unreadInLastFile = std::max<int64_t>(0, lastFileSizeBytes - status.HistoryFileOffsetLastRead);
            totalUnreadBytes += unreadInLastFile;
            
        } catch (const std::filesystem::filesystem_error& e) {
            printf("[Librarian] Warning: Could not get file size for %s: %s\n", 
                   lastFileInfo.FileName.c_str(), e.what());
            // Continue without counting this file
        }
    } else {
        printf("[Librarian] Warning: historyFileIdLastRead (%d) not found in historyFileSet_.fileMap\n", 
               status.HistoryFileIdLastRead);
    }

    // Find all unread files (LastOffset == 0) and sum their sizes
    for (const auto& [fileId, fileInfo] : historyFileSet_.fileMap) {
        if (fileInfo.LastOffset == 0) {
            try {
                int64_t fileSize = std::filesystem::file_size(fileInfo.FileName);
                totalUnreadBytes += fileSize;
                
            } catch (const std::filesystem::filesystem_error& e) {
                printf("[Librarian] Warning: Could not get file size for %s: %s\n", 
                       fileInfo.FileName.c_str(), e.what());
                // Continue without counting this file
            }
        }
    }

    // Calculate estimated backlog
    int estimatedBacklog = static_cast<int>(std::round(static_cast<double>(totalUnreadBytes) / EstimatedBytesPerJob_));
    
    printf("[Librarian] Backlog calculation: %lld total unread bytes, estimated %d jobs remaining\n", 
           totalUnreadBytes, estimatedBacklog);
    
    return estimatedBacklog;
}

void Librarian::estimateArrivalRateWhileAsleep() {
    int64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Calculate arrival rate if we didn't leave backlog last time and have previous timestamp
    // Create dummy status for backlog calculation
    Status dummyStatus = {};
    dummyStatus.HistoryFileIdLastRead = historyFileSet_.lastFileReadId;

    auto lastFileIt = historyFileSet_.fileMap.find(dummyStatus.HistoryFileIdLastRead);
    if (lastFileIt != historyFileSet_.fileMap.end()) {
        dummyStatus.HistoryFileOffsetLastRead = lastFileIt->second.LastOffset;

        int backlogAds = calculateBacklogFromBytes(dummyStatus);
        int64_t timeDeltaMs = currentTime - statusData_.TimeOfLastUpdate;

        if (timeDeltaMs > 0) {
            double arrivalHz = (backlogAds * 1000.0) / timeDeltaMs;
            double n = static_cast<double>(statusData_.TotalCycles + 1); // +1 for the cycle about to start
            statusData_.MeanArrivalHz += (arrivalHz - statusData_.MeanArrivalHz) / n;
        }
    }
}


void Librarian::updateStatusData(Status status) {
    int64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Increment total cycles
    statusData_.TotalCycles++;
    
    // Update total ads ingested
    statusData_.TotalAdsIngested += status.TotalJobsRead;
    
    // Calculate current cycle values
    double currentAdsPerCycle = static_cast<double>(status.TotalJobsRead);
    double currentDurationMs = static_cast<double>(status.DurationMs);
    double currentIngestHz = (currentDurationMs > 0) ? 
        (currentAdsPerCycle * 1000.0 / currentDurationMs) : 0.0;
    double currentBacklogEstimate = static_cast<double>(status.JobBacklogEstimate);
    
    // Update averages using incremental formula: new_avg = old_avg + (new_value - old_avg) / n
    double n = static_cast<double>(statusData_.TotalCycles);
    
    statusData_.AvgAdsIngestedPerCycle += 
        (currentAdsPerCycle - statusData_.AvgAdsIngestedPerCycle) / n;
    
    statusData_.AvgIngestDurationMs += 
        (currentDurationMs - statusData_.AvgIngestDurationMs) / n;
    
    statusData_.MeanIngestHz += 
        (currentIngestHz - statusData_.MeanIngestHz) / n;
    
    statusData_.MeanBacklogEstimate += 
        (currentBacklogEstimate - statusData_.MeanBacklogEstimate) / n;
    
    // Update hit max ingest limit rate
    if (status.HitMaxIngestLimit) {
        // Count of cycles that hit limit is now: (old_rate * old_n) + 1
        // New rate is: ((old_rate * old_n) + 1) / new_n
        statusData_.HitMaxIngestLimitRate = 
            (statusData_.HitMaxIngestLimitRate * (n - 1) + 1.0) / n;
    } else {
        // No hit this cycle, just update denominator
        statusData_.HitMaxIngestLimitRate = 
            (statusData_.HitMaxIngestLimitRate * (n - 1)) / n;
    }
    
    // Update tracking variables
    statusData_.TimeOfLastUpdate = currentTime;
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

    // Fill in info to be used for Status estimates later
    if (!calculateEstimatedBytesPerJob()) {
        EstimatedBytesPerJob_ = 5814.0;
    }
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

    // PhHASE  0: Status Tracking and Recovery
    // Initialize status tracking for this update cycle

    auto startTime = std::chrono::system_clock::now();
    Status status;

    // Estimate arrivalHz while asleep if there was no backlog left last cycle
    if (!statusData_.LastRunLeftBacklog && statusData_.TimeOfLastUpdate > 0) estimateArrivalRateWhileAsleep ();

    // Recovery: Populate statusData_ and FileSet structs if memory is empty
    

    // PHASE 1: Directory Scanning and File Tracking
    ArchiveChange epochChange = trackAndUpdateFileSet(epochHistoryFileSet_);
    ArchiveChange historyChange = trackAndUpdateFileSet(historyFileSet_);

    // PHASE 2: Queue Construction
    // TODO: use lastFileReadId in queue construction so that we don't reread tracked but unread files
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
    size_t totalEpochRecordsProcessed = 0;
    size_t epochFilesProcessed = 0;

    for (FileInfo& fileInfo : epochQueue) {

        // Check if we've already exceeded the limit - if so stop processing
        if (totalEpochRecordsProcessed >= MAX_EPOCH_RECORDS_PER_UPDATE) {
            printf("[Librarian] Reached epoch record limit (%zu), stopping epoch processing. Processed %zu files, %zu remaining.\n", 
                   MAX_EPOCH_RECORDS_PER_UPDATE, epochFilesProcessed, epochQueue.size() - epochFilesProcessed);
            status.HitMaxIngestLimit = true; // Note that we hit the limit during this ingestion cycle
            break;
        }

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
        
        // Accumulate stats for status reporting'
        totalEpochRecordsProcessed += fileRecords.size(); // TODO: currently duplicating this but theoretically we could just use status
        status.TotalEpochsRead += fileRecords.size();

        // Record the last ID read
        status.EpochFileIdLastRead = fileInfo.FileId;
        status.EpochFileOffsetLastRead = fileInfo.LastOffset;
    }
    
    // Process History Queue
    size_t totalJobRecordsProcessed = 0;
    size_t historyFilesProcessed = 0;

    for (FileInfo& fileInfo : historyQueue) {

        // Check if we've already exceeded the limit
        if (totalJobRecordsProcessed >= MAX_JOB_RECORDS_PER_UPDATE) {
            printf("[Librarian] Reached job record limit (%zu), stopping history processing. Processed %zu files, %zu remaining.\n", 
                   MAX_JOB_RECORDS_PER_UPDATE, historyFilesProcessed, historyQueue.size() - historyFilesProcessed);
            status.HitMaxIngestLimit = true; // Note that we hit the limit during this ingestion cycle
            break;
        }

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
        totalJobRecordsProcessed += fileRecords.size(); 
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
    status.JobBacklogEstimate = calculateBacklogFromBytes(status);
    updateStatusData(status);

    // Persist status to database
    if (!dbHandler_->writeStatusAndData(status, statusData_)) {
        fprintf(stderr, "[Librarian] Warning: Failed to write status to database\n");
        // Don't want to fail the entire update cycle for this, just logging it
    }

    printf("[Librarian] Update protocol completed successfully. Inserted %zu job records, %zu epoch records in %ld ms.\n",
        status.TotalJobsRead, status.TotalEpochsRead, status.DurationMs);
    return true;
}
