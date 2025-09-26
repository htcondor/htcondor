#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"

#include "librarian.h"
#include "readHistory.h"
#include "archiveMonitor.h"
#include "SavedQueries.h"

#include <filesystem>
#include <cstdio>
#include <chrono>
#include <memory>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace conf = LibrarianConfigOptions;
namespace fs = std::filesystem;

// ================================
// FILE READS AND WRITES
// ================================  


/**
 * @brief Reads new job records from the history file and updates the file offset
 * @param newJobRecords Output vector to store newly read job records
 * @param fileInfo Input/output file information containing current offset, updated with new offset
 * @return true if successful, false on failure
 */
bool Librarian::readJobRecords(std::vector<JobRecord>& newJobRecords, FileInfo& fileInfo) {

    fs::path fullPath = historyFileSet_.GetDirectory() / fileInfo.FileName;
    long newOffset = readHistoryIncremental(fullPath.string().c_str(), newJobRecords, fileInfo);
    if (newOffset < 0) {
        dprintf(D_ERROR, "Failed to read job records.\n");
        return false;
    }
    fileInfo.LastOffset = newOffset;

    // Check if this rotated file is now fully read
    if (!fileInfo.DateOfRotation.empty() && fileInfo.LastOffset >= fileInfo.FileSize) {
        fileInfo.FullyRead = true;
    }

    return true;
}

// ================================
// STATUS TRACKING UTILITIES
// ================================

// Finds a history file matching the config name pattern and calculates 
// EstimatedBytesPerJobInArchive_ as the byte size from file start to first "***" line
bool Librarian::calculateEstimatedBytesPerJob() {
    std::error_code ec;
    std::filesystem::path historyDir = historyFileSet_.GetDirectory();

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

            auto setFileNameLen = historyFileSet_.GetFileName().length();

            // Check if filename starts with archive file root filename and has more content after it
            if (filename.length() > setFileNameLen && filename.substr(0, setFileNameLen) == historyFileSet_.GetFileName()) {

                std::ifstream file(entry.path(), std::ios::binary);
                if (!file.is_open()) {
                    continue; // Try next file
                }

                std::string line;
                std::streampos startPos = file.tellg();
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
                        EstimatedBytesPerJobInArchive_ = static_cast<double>(chunkSize);

                        file.close();

                        // Now get the size of the whole file
                        std::uintmax_t fileSize = std::filesystem::file_size(entry.path(), ec);
                        if (ec || EstimatedBytesPerJobInArchive_ <= 0.0) {
                            return false;
                        }

                        EstimatedJobsPerFileInArchive_ = static_cast<int>(
                            static_cast<double>(fileSize) / EstimatedBytesPerJobInArchive_
                        );

                        return true;
                    }
                }

                file.close(); // No "***" line found
            }
        }
    }

    return false;
}


/**
 * @brief Estimates remaining job records by calculating unread bytes in history files
 * 
 * Calculates bytes remaining in the last partially-read file plus all bytes in
 * unread files (LastOffset == 0), then estimates job count using EstimatedBytesPerJobInArchive_.
 */
int Librarian::calculateBacklogFromBytes(const Status& status) {
    if (EstimatedBytesPerJobInArchive_ <= 0) return 0;

    int64_t totalUnreadBytes = 0;

    // Find the last read file info
    auto lastFileIt = historyFileSet_.fileMap.find(status.HistoryFileIdLastRead);
    if (lastFileIt != historyFileSet_.fileMap.end()) {
        const FileInfo& lastFileInfo = lastFileIt->second;

        // Construct full path to the history file
        std::filesystem::path fullFilePath = historyFileSet_.GetDirectory() / lastFileInfo.FileName;

        // Check if file exists and get its size without exceptions
        std::error_code ec;
        int64_t lastFileSizeBytes = std::filesystem::file_size(fullFilePath, ec);

        if (!ec) {
            // File size retrieved successfully
            // Calculate bytes left in the partially read file
            int64_t unreadInLastFile = std::max<int64_t>(0, lastFileSizeBytes - status.HistoryFileOffsetLastRead);
            totalUnreadBytes += unreadInLastFile;
        } else {
            dprintf(D_ERROR, "Warning: Could not get file size for %s: %s\n",
                    fullFilePath.string().c_str(), ec.message().c_str());
            // Continue without counting this file
        }
    } else {
        dprintf(D_ERROR, "Warning: Last history file ID (%d) not found.\n",
                status.HistoryFileIdLastRead);
    }

    // Find all unread files (LastOffset == 0) and sum their sizes
    for (const auto& [fileId, fileInfo] : historyFileSet_.fileMap) {
        if (fileInfo.LastOffset == 0) {
            // Construct full path to the history file
            std::filesystem::path fullFilePath = historyFileSet_.GetDirectory() / fileInfo.FileName;

            // Check if file exists and get its size without exceptions
            std::error_code ec;
            int64_t fileSize = std::filesystem::file_size(fullFilePath, ec);

            if (!ec) {
                // File size retrieved successfully
                totalUnreadBytes += fileSize;
            } else {
                dprintf(D_ERROR, "Warning: Could not get file size for %s: %s\n",
                        fullFilePath.string().c_str(), ec.message().c_str());
                // Continue without counting this file
            }
        }
    }

    // Calculate estimated backlog
    int estimatedBacklog = static_cast<int>(std::round(static_cast<double>(totalUnreadBytes) / EstimatedBytesPerJobInArchive_));

    dprintf(D_ALWAYS, "Backlog calculation: %lld total unread bytes, estimated %d jobs remaining\n",
            (long long) totalUnreadBytes, estimatedBacklog);

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
        const long lastFileReadId = change.rotatedFile.first;
        const std::string& newName = change.rotatedFile.second;

        auto it = fileSet.fileMap.find(lastFileReadId);
        if (it != fileSet.fileMap.end()) {
            it->second.FileName = newName;

            // Extract and set DateOfRotation from the new rotated filename
            if (auto dateOfRotation = ArchiveMonitor::extractDateOfRotation(newName)) {
                it->second.DateOfRotation = *dateOfRotation;
            }
        } else {
            dprintf(D_ERROR, "Warning: Rotated file not found in fileMap. FileId=%ld\n", lastFileReadId);
        }
    }

    // 2. Add new files to FileSet's FileMap
    for (const auto& newFile : change.newFiles) {
        if (newFile.FileId != 0) {
            fileSet.fileMap[newFile.FileId] = newFile;
        } else {
            dprintf(D_ERROR, "Warning: New file has invalid FileId. Skipping.\n");
        }
    }

    // 3. Remove deleted files from FileSet's FileMap
    for (long deletedFileId : change.deletedFileIds) {
        size_t erased = fileSet.fileMap.erase(deletedFileId);
        if (erased == 0) {
            dprintf(D_ERROR, "Warning: Tried to delete file not found in fileMap. FileId=%ld\n", deletedFileId);
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
    dbHandler_.insertNewFilesAndMarkOldOnes(fileSetChange);

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
            dprintf(D_FULLDEBUG, "Added lastFileRead to queue: %s (FileId: %ld)\n",
                    it->second.FileName.c_str(), it->second.FileId);
        } else {
            dprintf(D_ERROR, "Error: lastFileReadId %ld not found in fileMap\n", fileSet.lastFileReadId);
            return false;
        }
    }

    // SECOND PRIORITY: Add new files from archive changes in order 
    for (const FileInfo& newFile : changes.newFiles) {
        queue.push_back(newFile);
        dprintf(D_ALWAYS, "Added new file to queue: %s (FileId: %ld)\n",
               newFile.FileName.c_str(), newFile.FileId);
    }

    return true;
}



// ================================
// GARBAGE COLLECTION
// ================================


/**
 * Get the file system size of the database in bytes.
 * Portable across all file systems.
 * 
 * @param db_path Path to the database file
 * @return Size in bytes, or 0 if file doesn't exist
 */
std::uintmax_t get_database_size(const std::string& db_path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(db_path, ec);

    if (ec) {
        return 0;  // File doesn't exist or other error
    }

    return size;
}


bool Librarian::cleanupDatabaseIfNeeded() {
    bool garbageCollected =  false;

    // Get current database size
    size_t currentSize = get_database_size(config[conf::str::DBPath]);

    long long size_limit = config[conf::ll::DBMaxSizeBytes];

    // Calculate high watermark (97% of capacity)
    size_t highWatermark = static_cast<size_t>(size_limit * 0.97);

    // Check if we've exceeded the high watermark
    if (currentSize > highWatermark) {
        // Calculate low watermark (85% of capacity)
        size_t lowWatermark = static_cast<size_t>(size_limit * 0.85);

        // Calculate how many bytes we need to free up
        size_t bytesToDelete = currentSize - lowWatermark;

        // Calculate number of jobs to delete (round up to ensure we delete enough)
        int numJobsToDelete = static_cast<int>(std::ceil(static_cast<double>(bytesToDelete) / EstimatedBytesPerJobInDatabase_));

        // Convert jobs to files: calculate how many files we need to delete to satisfy numJobsToDelete
        // Round up to ensure we delete enough files to cover the required number of jobs
        int numFilesToDelete = static_cast<int>(std::ceil(static_cast<double>(numJobsToDelete) / EstimatedJobsPerFileInArchive_));

        // Next step: run the query to delete jobs
        garbageCollected = dbHandler_.runGarbageCollection(SavedQueries::GC_QUERY_SQL, numFilesToDelete);
        if(!garbageCollected){
            dprintf(D_ERROR, "Garbage collection attempted but failed.\n");
        }
    }

    return garbageCollected;
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
    historyFileSet_.Init(config[conf::str::ArchiveFile]);

    dprintf(D_FULLDEBUG, "Initializing DBHandler.\n");
    // Construct the DBHandler with provided schema, db path, and cache size.
    if ( ! dbHandler_.initialize()) {
        return false;
    }

    // Check whether database is connected and has correct expect tables
    if (!dbHandler_.testDatabaseConnection()) {
        dprintf(D_ERROR, "DBHandler connection test failed\n");
        return false;
    }
    if(!dbHandler_.verifyDatabaseSchema(SavedQueries::SCHEMA_SQL)) {
        dprintf(D_ERROR, "DBHandler schema is not as expected\n");
        return false;
    }

    dprintf(D_FULLDEBUG, "DBHandler initialized.\n");

    // Fill in info to be used for Status estimates later
    if (!calculateEstimatedBytesPerJob()) {
        EstimatedBytesPerJobInArchive_ = 5814.0;
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
    dprintf(D_FULLDEBUG, "Starting update protocol.\n");

    // PhHASE  0: Status Tracking and Data Recovery
    // Initialize status tracking for this update cycle

    auto startTime = std::chrono::system_clock::now();

    // Recovery: Populate statusData_ and FileSet structs if memory is empty
    dbHandler_.maybeRecoverStatusAndFiles(historyFileSet_, statusData_);
    Status status = {};

    // Estimate arrivalHz while asleep if there was no backlog left last cycle
    if (!statusData_.LastRunLeftBacklog && statusData_.TimeOfLastUpdate > 0) estimateArrivalRateWhileAsleep ();

    // PHASE 1: Directory Scanning and File Tracking
    ArchiveChange historyChange = trackAndUpdateFileSet(historyFileSet_);

    // PHASE 2: Queue Construction
    // TODO: use lastFileReadId in queue construction so that we don't reread tracked but unread files
    std::vector<FileInfo> historyQueue;

    if (!buildProcessingQueue(historyFileSet_, historyChange, historyQueue)) {
        dprintf(D_ERROR, "Failed to build history processing queue\n");
        return false;
    }

    // PHASE 3: Record Processing
    // Process History Queue
    size_t totalJobRecordsProcessed = 0;
    size_t historyFilesProcessed = 0;
    dprintf(D_FULLDEBUG, "Processing history file queue.\n");

    for (FileInfo& fileInfo : historyQueue) {

        // Check if we've already exceeded the limit
        if (totalJobRecordsProcessed >= (size_t)config[conf::i::MaxRecordsPerUpdate]) {
            dprintf(D_STATUS, "Reached job record limit (%d), stopping history processing. Processed %zu files, %zu remaining.\n",
                    config[conf::i::MaxRecordsPerUpdate], historyFilesProcessed, historyQueue.size() - historyFilesProcessed);
            status.HitMaxIngestLimit = true; // Note that we hit the limit during this ingestion cycle
            break;
        }

        dprintf(D_STATUS, "Processing history file: %s (offset: %ld)\n",
                fileInfo.FileName.c_str(), fileInfo.LastOffset);
        
        std::vector<JobRecord> fileRecords;
        if (!readJobRecords(fileRecords, fileInfo)) {
            dprintf(D_ERROR, "Failed to read job records from %s\n", fileInfo.FileName.c_str());
            return false;
        }

        // If nothing new to read, move on to the next file
        if(fileRecords.empty()){
            dprintf(D_FULLDEBUG, "No new job records in %s, skipping this file\n", fileInfo.FileName.c_str());
                status.HistoryFileIdLastRead = fileInfo.FileId;
                status.HistoryFileOffsetLastRead = fileInfo.LastOffset;
            continue;
        }

        // Atomic transaction: Insert records AND update file offset
        if (!dbHandler_.insertJobFileRecords(fileRecords, fileInfo)) {
                dprintf(D_ERROR, "Failed to atomically process history file %s\n", fileInfo.FileName.c_str());
                continue;
        }
        
        // Update in-memory FileSet with new offset
        auto it = historyFileSet_.fileMap.find(fileInfo.FileId);
        if (it != historyFileSet_.fileMap.end()) {
            it->second.LastOffset = fileInfo.LastOffset;
        } else {
            dprintf(D_ERROR, "Warning: FileId %ld not found in historyFileSet.fileMap\n", fileInfo.FileId);
        }

        // Accumulate stats for status reporting
        totalJobRecordsProcessed += fileRecords.size(); 
        status.TotalJobsRead += fileRecords.size();

        // Record the last ID read
        status.HistoryFileIdLastRead = fileInfo.FileId;
        status.HistoryFileOffsetLastRead = fileInfo.LastOffset;
    }

    // PHASE 4: Garbage Collection
    bool garbageCollected = cleanupDatabaseIfNeeded();

    // PHASE 5: Status Update and Finalization
    auto endTime = std::chrono::system_clock::now();
    status.TimeOfUpdate = std::chrono::system_clock::to_time_t(endTime); 
    status.DurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    status.JobBacklogEstimate = calculateBacklogFromBytes(status);
    status.GarbageCollectionRun = garbageCollected;
    updateStatusData(status);

    // Persist status to database
    if (!dbHandler_.writeStatusAndData(status, statusData_)) {
        dprintf(D_ERROR, "Warning: Failed to write status to database\n");
        // Don't want to fail the entire update cycle for this, just logging it
    }

    dprintf(D_FULLDEBUG, "Update protocol completed successfully. Inserted %zu job records, %zu epoch records in %ld ms.\n",
            status.TotalJobsRead, status.TotalEpochsRead, status.DurationMs);
    return true;
}
