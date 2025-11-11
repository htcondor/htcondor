#include "librarian.h"
#include "readHistory.h"
#include "archiveMonitor.h"
#include "FileSet.h"
#include "JobAnalysisUtils.h" 
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

namespace fs = std::filesystem;

// ================================
// CONSTRUCTOR AND INITIALIZATION
// ================================

Librarian::Librarian(const std::string& dbPath,
                     const std::string& historyFilePath,
                     const std::string& epochHistoryFilePath,
                     size_t jobCacheSize, 
                     size_t databaseSize, 
                     double schemaVersionNumber)
    : dbPath_(dbPath),
      historyFilePath_(historyFilePath),
      epochHistoryFilePath_(epochHistoryFilePath),
      jobCacheSize_(jobCacheSize),
      databaseSizeLimit_(databaseSize),
      historyFileSet_(historyFilePath),
      epochHistoryFileSet_(epochHistoryFilePath), 
      schemaVersionNumber_(schemaVersionNumber) {}


std::string Librarian::loadSchemaSQL() {
    return SavedQueries::SCHEMA_SQL;
}

// Simplified - no file I/O needed
bool Librarian::loadGCSQL() {
    gcQuerySQL_ = SavedQueries::GC_QUERY_SQL;
    
    if (gcQuerySQL_.empty()) {
        printf("[ERROR] GC query is empty\n");
        return false;
    }
    
    return true;
}

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
    
    std::string fullPath = epochHistoryFileSet_.historyDirectoryPath + "/" + fileInfo.FileName;
    long newOffset = readEpochIncremental(fullPath.c_str(), newEpochRecords, fileInfo);
    if (newOffset < 0) {
        fprintf(stderr, "[Librarian] Failed to read epoch records.\n");
        return false;
    }
    fileInfo.LastOffset = newOffset;
    
    // Check if this rotated file is now fully read
    if (!fileInfo.DateOfRotation.empty() && fileInfo.LastOffset >= fileInfo.FileSize) {
        fileInfo.FullyRead = true;
    }
    
    return true;
}

/**
 * @brief Reads new job records from the history file and updates the file offset
 * @param newJobRecords Output vector to store newly read job records
 * @param fileInfo Input/output file information containing current offset, updated with new offset
 * @return true if successful, false on failure
 */
bool Librarian::readJobRecords(std::vector<JobRecord>& newJobRecords, FileInfo& fileInfo) {

    std::string fullPath = historyFileSet_.historyDirectoryPath + "/" + fileInfo.FileName;
    long newOffset = readHistoryIncremental(fullPath.c_str(), newJobRecords, fileInfo);
    if (newOffset < 0) {
        fprintf(stderr, "[Librarian] Failed to read job records.\n");
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
            
            // Check if filename starts with historyNameConfig and has more content after it
            if (filename.length() > historyFileSet_.historyNameConfig.length() &&
                filename.substr(0, historyFileSet_.historyNameConfig.length()) == historyFileSet_.historyNameConfig) {
                
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
        std::filesystem::path fullFilePath = std::filesystem::path(historyFileSet_.historyDirectoryPath) / lastFileInfo.FileName;
        
        // Check if file exists and get its size without exceptions
        std::error_code ec;
        int64_t lastFileSizeBytes = std::filesystem::file_size(fullFilePath, ec);
        
        if (!ec) {
            // File size retrieved successfully
            // Calculate bytes left in the partially read file
            int64_t unreadInLastFile = std::max<int64_t>(0, lastFileSizeBytes - status.HistoryFileOffsetLastRead);
            totalUnreadBytes += unreadInLastFile;
        } else {
            printf("[Librarian] Warning: Could not get file size for %s: %s\n", 
                fullFilePath.string().c_str(), ec.message().c_str());
            // Continue without counting this file
        }
    } else {
        printf("[Librarian] Warning: historyFileIdLastRead (%d) not found in historyFileSet_.fileMap\n", 
            status.HistoryFileIdLastRead);
    }

    // Find all unread files (LastOffset == 0) and sum their sizes
    for (const auto& [fileId, fileInfo] : historyFileSet_.fileMap) {
        if (fileInfo.LastOffset == 0) {
            // Construct full path to the history file
            std::filesystem::path fullFilePath = std::filesystem::path(historyFileSet_.historyDirectoryPath) / fileInfo.FileName;
            
            // Check if file exists and get its size without exceptions
            std::error_code ec;
            int64_t fileSize = std::filesystem::file_size(fullFilePath, ec);
            
            if (!ec) {
                // File size retrieved successfully
                totalUnreadBytes += fileSize;
            } else {
                printf("[Librarian] Warning: Could not get file size for %s: %s\n", 
                    fullFilePath.string().c_str(), ec.message().c_str());
                // Continue without counting this file
            }
        }
    }

    // Calculate estimated backlog
    int estimatedBacklog = static_cast<int>(std::round(static_cast<double>(totalUnreadBytes) / EstimatedBytesPerJobInArchive_));
    
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

// Testing function to see what has actually been saved to a FileSet struct
void PrintFileSetInfo(const FileSet& fs) {
    std::cout << "\n[Librarian] Printing FileSet Info: \n" ;
    std::cout << "------------------------------------------------- \n" ;
    std::cout << "History Name Config: " << fs.historyNameConfig << "\n";
    std::cout << "History Directory Path: " << fs.historyDirectoryPath << "\n";
    std::cout << "Last Status Time: " << fs.lastStatusTime << "\n";
    std::cout << "Last File Read ID: " << fs.lastFileReadId << "\n";

    std::cout << "\nFile Map:\n";
    std::cout << std::left << std::setw(10) << "FileId"
              << std::setw(20) << "FileName"
              << std::setw(15) << "Inode"
              << std::setw(40) << "Hash" << "\n";
    std::cout << std::string(85, '-') << "\n";

    for (const auto& [id, info] : fs.fileMap) {
        std::cout << std::left << std::setw(10) << id
                  << std::setw(20) << info.FileName
                  << std::setw(15) << info.FileInode
                  << std::setw(40) << info.FileHash << "\n";
    }
    std::cout << "------------------------------------------------- \n" ;
}

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
            fprintf(stderr, "[Librarian] Warning: Rotated file not found in fileMap. FileId=%ld\n", lastFileReadId);
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

    // 3. Remove deleted files from FileSet's FileMap
    for (long deletedFileId : change.deletedFileIds) {
        size_t erased = fileSet.fileMap.erase(deletedFileId);
        if (erased == 0) {
            fprintf(stderr, "[Librarian] Warning: Tried to delete file not found in fileMap. FileId=%ld\n", deletedFileId);
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
    dbHandler_->insertNewFilesAndMarkOldOnes(fileSetChange);

    // Step 3: Apply the same changes to the in-memory FileSet,
    // so that the system’s internal state reflects what’s on disk.
    applyArchiveChangeToFileSet(fileSetChange, fileSet);

    return fileSetChange;
}\

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
            printf("[Librarian] Added lastFileRead to queue: %s (FileId: %ld)\n", 
                   it->second.FileName.c_str(), it->second.FileId);
        } else {
            printf("[Librarian] Error: lastFileReadId %ld not found in fileMap\n", fileSet.lastFileReadId);
            return false;
        }
    }

    // SECOND PRIORITY: Add new files from archive changes in order 
    for (const FileInfo& newFile : changes.newFiles) {
        queue.push_back(newFile);
        printf("[Librarian] Added new file to queue: %s (FileId: %ld)\n", 
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
    size_t currentSize = get_database_size(dbPath_);
    
    // Calculate high watermark (97% of capacity)
    size_t highWatermark = static_cast<size_t>(databaseSizeLimit_ * 0.97);
    
    // Check if we've exceeded the high watermark
    if (currentSize > highWatermark) {
        // Calculate low watermark (85% of capacity)
        size_t lowWatermark = static_cast<size_t>(databaseSizeLimit_ * 0.85);
        
        // Calculate how many bytes we need to free up
        size_t bytesToDelete = currentSize - lowWatermark;
        
        // Calculate number of jobs to delete (round up to ensure we delete enough)
        int numJobsToDelete = static_cast<int>(std::ceil(static_cast<double>(bytesToDelete) / EstimatedBytesPerJobInDatabase_));

        // Convert jobs to files: calculate how many files we need to delete to satisfy numJobsToDelete
        // Round up to ensure we delete enough files to cover the required number of jobs
        int numFilesToDelete = static_cast<int>(std::ceil(static_cast<double>(numJobsToDelete) / EstimatedJobsPerFileInArchive_));

        // Next step: run the query to delete jobs
        garbageCollected = dbHandler_->runGarbageCollection(gcQuerySQL_, numFilesToDelete);
        if(!garbageCollected){
            printf("[Librarian] Garbage collection attempted but failed.");
        }
    }

    return garbageCollected;
}


// ================================
// USER QUERY INTERFACE
// ================================

// move down into public utilities! but later
int Librarian::query(int argc, char* argv[]) {
    
    // Parse command line arguments using internal utilities
    JobAnalysisUtils::CommandOptions options;
    JobAnalysisUtils::parseArguments(argc, argv, options);

    // Validate arguments
    if (options.username.empty() || options.clusterId == -1) {
        JobAnalysisUtils::printManual();
        return 1;
    }
    
    // Build query string for logging
    std::string queryString = "myList -user " + options.username + " -clusterId " + std::to_string(options.clusterId);
    if (options.flags.usage) queryString += " -usage";
    if (options.flags.batch) queryString += " -batch";
    if (options.flags.dag) queryString += " -dag";
    if (options.flags.files) queryString += " -files";
    if (options.flags.when) queryString += " -when";
    if (options.flags.where) queryString += " -where";
    if (options.flags.status) queryString += " -status";
    
    // Execute the query using helper method
    auto startTime = std::chrono::high_resolution_clock::now(); // Check whether offsets actually make this process faster
    auto result = executeQuery(options.username, options.clusterId, historyFileSet_.historyDirectoryPath);
    
    if (!result.success) {
        JobAnalysisUtils::printError(result.errorMessage);
        return 1;
    }
    
    if (result.jobs.empty()) {
        JobAnalysisUtils::printNoJobsFound(options.username, options.clusterId);
        return 0;
    }

    // End timing
    auto endTime = std::chrono::high_resolution_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // Report timing to console
    std::cout << "\n\nQuery execution took " << durationMs << " ms\n";
    
    // Write timing data to file
    std::ofstream timingFile("librarian_query_times.txt", std::ios::app);
    if (timingFile.is_open()) {
        timingFile << queryString << "\n";
        timingFile << durationMs << " ms\n";
        timingFile << "\n\n";
        timingFile.close();
    } else {
        printf("Warning: Could not write to librarian_query_times.txt\n");
    }
    
    // Format and display output using utilities
    if (!options.flags.hasAnyFlag()) {
        JobAnalysisUtils::printBaseOutput(result.jobs, options.username, options.clusterId);
    } else {
        JobAnalysisUtils::printAnalysisOutput(result.jobs, options.flags, options.username, options.clusterId);
    }
    
    return 0;
}

QueryResult Librarian::executeQuery(const std::string& username, int clusterId, std::string historyDirectoryPath) {
    // Execute the actual query
    std::vector<QueriedJobRecord> jobRecords;
    
    if (!dbHandler_->queryJobRecordsForJobList(username, clusterId, jobRecords)) {
        return {false, "No jobs found for user '" + username + "', cluster " + std::to_string(clusterId), {}};
    }
    
    if (jobRecords.empty()) {
        return {false, "No jobs found for user '" + username + "', cluster " + std::to_string(clusterId), {}};
    }
    
    printf("[Librarian] JobList Id found, reading jobs grouped by file...");
    std::vector<std::string> rawClassAds = readJobsGroupedByFile(jobRecords, historyDirectoryPath);
    if (rawClassAds.empty()) {
        return {false, "Could not read any job data from archive files", {}};
    }
    
    // Parse ClassAds using utility function
    std::vector<ParsedJobRecord> parsedJobs = JobAnalysisUtils::parseClassAds(rawClassAds);
    
    if (parsedJobs.empty()) {
        return {false, "Failed to parse any job records", {}};
    }
    
    return {true, "", parsedJobs};
}

std::vector<std::string> Librarian::readJobsGroupedByFile(const std::vector<QueriedJobRecord>& jobRecords, std::string historyDirectoryPath) {
    // Group by fileName to minimize file operations
    std::map<std::string, std::vector<std::pair<long, FileInfo>>> fileGroups;
    
    for (const auto& record : jobRecords) {
        FileInfo fileInfo;
        fileInfo.FileId = record.fileId;
        fileInfo.FileInode = record.fileInode;
        fileInfo.FileHash = record.fileHash;
        fileInfo.FileName = record.fileName;
        
        fileGroups[record.fileName].emplace_back(record.offset, fileInfo);
    }
    
    std::vector<std::string> allJobs;
    int skippedFiles = 0;
    int skippedJobs = 0;
    
    for (const auto& [fileName, offsetsAndInfo] : fileGroups) {
        printf("[Librarian] Reading from file %s\n", fileName.c_str());

        // Construct full file path using historyDirectoryPath
        std::string filePath = historyDirectoryPath + "/" + fileName;
        
        // Verify file integrity using the first FileInfo (they should all be the same for same file)
        auto [fileExists, fileMatches] = ArchiveMonitor::checkFileEquals(filePath, offsetsAndInfo[0].second);
        if (!fileExists || !fileMatches) {
            skippedFiles++;
            skippedJobs += offsetsAndInfo.size();
            printf("Warning: Skipping %zu jobs from file %s (file verification failed)\n", 
                   offsetsAndInfo.size(), fileName.c_str());
            continue;
        }
        
        // Read all jobs from this file
        for (const auto& [offset, fileInfo] : offsetsAndInfo) {
            if (auto jobData = readJobAtOffset(filePath, offset)) {
                allJobs.push_back(*jobData);
            } else {
                printf("Warning: Could not read job at offset %ld in file %s\n", 
                       offset, fileName.c_str());
            }
        }
    }
    
    if (skippedFiles > 0) {
        printf("Summary: Skipped %d jobs from %d inaccessible files\n", 
               skippedJobs, skippedFiles);
    }
    
    return allJobs;
}

std::optional<std::string> Librarian::readJobAtOffset(const std::string& filePath, long offset) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    file.seekg(offset);
    if (!file.good()) {
        return std::nullopt;
    }
    
    // Skip to the start of the next line
    std::string dummy;
    std::getline(file, dummy);  // Read and discard the partial/complete line
    
    std::string jobData;
    std::string line;
    
    // Read until we hit the *** delimiter
    while (std::getline(file, line)) {
        if (line.find("***") == 0) {
            break;
        }
        jobData += line + "\n";
    }
    
    return jobData.empty() ? std::nullopt : std::make_optional(jobData);
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

    // Load SQL query strings
    std::string schemaSQL = this->loadSchemaSQL();
    if(schemaSQL.empty()) {
        printf("[Librarian] Schema initialization has failed! Could not get schema.sql from file\n");
        return false;
    }
    if (!loadGCSQL()) {
        printf("[Librarian] Failed to load garbage collection query.\n");
        return false;
    }

    // Construct the DBHandler with provided schema, db path, and cache size.
    dbHandler_ = std::make_unique<DBHandler>(schemaSQL, dbPath_, jobCacheSize_);

    // Check whether database is connected and has correct expect tables
    if (!dbHandler_->testDatabaseConnection()) {
        fprintf(stderr, "[Librarian] DBHandler connection test failed\n");
        return false;
    }
    if(!dbHandler_->verifyDatabaseSchema(schemaSQL, schemaVersionNumber_)) {
        fprintf(stderr, "[Librarian] DBHandler schema is not as expected\n");
        return false;
    }

    printf("[Librarian] DBHandler initialized.\n");

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
    printf("[Librarian] Starting update protocol...\n");

    // PhHASE  0: Status Tracking and Data Recovery
    // Initialize status tracking for this update cycle

    auto startTime = std::chrono::system_clock::now();

    // Recovery: Populate statusData_ and FileSet structs if memory is empty
    dbHandler_->maybeRecoverStatusAndFiles(historyFileSet_, epochHistoryFileSet_, statusData_);
    Status status = {};

    // Estimate arrivalHz while asleep if there was no backlog left last cycle
    if (!statusData_.LastRunLeftBacklog && statusData_.TimeOfLastUpdate > 0) estimateArrivalRateWhileAsleep ();

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
    printf("[Librarian] Processing epoch file queue...\n");

    for (FileInfo& fileInfo : epochQueue) {

        // Check if we've already exceeded the limit - if so stop processing
        if (totalEpochRecordsProcessed >= MAX_EPOCH_RECORDS_PER_UPDATE) {
            printf("[Librarian] Reached epoch record limit (%zu), stopping epoch processing. Processed %zu files, %zu remaining.\n", 
                   MAX_EPOCH_RECORDS_PER_UPDATE, epochFilesProcessed, epochQueue.size() - epochFilesProcessed);
            status.HitMaxIngestLimit = true; // Note that we hit the limit during this ingestion cycle
            break;
        }

        printf("[Librarian] Processing epoch file: %s (offset: %ld)\n", 
               fileInfo.FileName.c_str(), fileInfo.LastOffset);
        
        std::vector<EpochRecord> fileRecords;
        if (!readEpochRecords(fileRecords, fileInfo)) {
            printf("[Librarian] Failed to read epoch records from %s \n", fileInfo.FileName.c_str());
            return false;
        }
        
        // If nothing new to read, move on to the next file
        if(fileRecords.empty()){
            printf("[Librarian] No new epoch records in %s, skipping this file \n", fileInfo.FileName.c_str());
            status.EpochFileIdLastRead = fileInfo.FileId;
            status.EpochFileOffsetLastRead = fileInfo.LastOffset;
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
            fprintf(stderr, "[Librarian] Warning: FileId %ld not found in epochHistoryFileSet.fileMap\n", fileInfo.FileId);
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
    printf("[Librarian] Processing history file queue...\n");

    for (FileInfo& fileInfo : historyQueue) {

        // Check if we've already exceeded the limit
        if (totalJobRecordsProcessed >= MAX_JOB_RECORDS_PER_UPDATE) {
            printf("[Librarian] Reached job record limit (%zu), stopping history processing. Processed %zu files, %zu remaining.\n", 
                   MAX_JOB_RECORDS_PER_UPDATE, historyFilesProcessed, historyQueue.size() - historyFilesProcessed);
            status.HitMaxIngestLimit = true; // Note that we hit the limit during this ingestion cycle
            break;
        }

        printf("[Librarian] Processing history file: %sf (offset: %ld)\n", 
               fileInfo.FileName.c_str(), fileInfo.LastOffset);
        
        std::vector<JobRecord> fileRecords;
        if (!readJobRecords(fileRecords, fileInfo)) {
            printf("[Librarian] Failed to read job records from %s \n", fileInfo.FileName.c_str());
            return false;
        }

        // If nothing new to read, move on to the next file
        if(fileRecords.empty()){
            printf("[Librarian] No new job records in %s, skipping this file \n", fileInfo.FileName.c_str());
                status.HistoryFileIdLastRead = fileInfo.FileId;
                status.HistoryFileOffsetLastRead = fileInfo.LastOffset;
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
            fprintf(stderr, "[Librarian] Warning: FileId %ld not found in historyFileSet.fileMap\n", fileInfo.FileId);
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
    if (!dbHandler_->writeStatusAndData(status, statusData_)) {
        fprintf(stderr, "[Librarian] Warning: Failed to write status to database\n");
        // Don't want to fail the entire update cycle for this, just logging it
    }

    printf("[Librarian] Update protocol completed successfully. Inserted %zu job records, %zu epoch records in %ld ms.\n",
        status.TotalJobsRead, status.TotalEpochsRead, status.DurationMs);
    return true;
}
