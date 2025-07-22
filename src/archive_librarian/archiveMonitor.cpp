/**
 * @file archiveMonitor.cpp
 * @brief Monitors history archive for changes or file rotation.
 *
 * Responsibilities:
 * - Identify file updates using inode and hash comparisons.
 * - Detect when a file has been rotated or modified.
 * - Signal changes to the librarian module (no DB writes).
 *
 * Notes:
 * - This module is stateless; it only reads from disk and reports conditions.
 * - Actual response to changes (e.g., DB updates) is handled by librarian.cpp.
 *
 * Key Functions:
 * - bool hasFileChanged(const std::string& path)
 * - bool isRotated(const FileInfo& oldInfo, const FileInfo& newInfo)
 * - std::vector<std::string> listHistoryFiles(const std::string& directory)
 */

#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <optional>

#include "sha256.h"
#include "JobRecord.h"  
#include "readHistory.h"  
#include "dbHandler.h"
#include "archiveMonitor.h"
#include "FileSet.h"


namespace fs = std::filesystem; // where am i?
 
namespace { // Helper functions for ArchiveMonitor utility functions

    // Helper Function: Convert hash into hex string to log it 
    std::string hashToHex(const uint8_t hash[32]) {
        char buf[65];
        for (int i = 0; i < 32; ++i)
            snprintf(buf + (i * 2), 3, "%02x", hash[i]);
        buf[64] = 0;
        return std::string(buf);
    }

    // Get hash by reading until first banner and hashing the first JobAd
    std::string getHash(const char* historyFilePath) {
        FILE* file = fopen(historyFilePath, "rb");
        if (!file) {
            perror("fopen");
            return std::string{};
        }

        const char* bannerPrefix = "*** Offset =";
        std::string buffer;
        char line[4096];

        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, bannerPrefix, strlen(bannerPrefix)) == 0) {
                break;
            }
            buffer += line;
        }

        fclose(file);

        uint8_t outputHash[32];
        SHA256_CTX ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, reinterpret_cast<const uint8_t*>(buffer.c_str()), buffer.size());
        sha256_final(&ctx, outputHash);

        std::string hexString = hashToHex(outputHash);
        return hexString;
    }

    // Collect information about a new file: Name, Inode, Hash
    std::optional<FileInfo> maybeCollectNewFileInfo(const char *historyFilePath) {
        FileInfo fileInfo;

        // Get .FileName from full path
        std::string fullPath(historyFilePath);
        size_t pos = fullPath.find_last_of("/\\");
        fileInfo.FileName = (pos == std::string::npos) ? fullPath : fullPath.substr(pos + 1);

        // Get .FileInode and .FileSize using stat
        struct stat file_stat;
        if (stat(historyFilePath, &file_stat) == 0) {
            fileInfo.FileInode = static_cast<long>(file_stat.st_ino);
            fileInfo.FileSize = static_cast<int64_t>(file_stat.st_size);
            fileInfo.LastModified = file_stat.st_mtime;
        } else {
            printf("[ERROR] Could not get file stats for '%s'\n", historyFilePath);
            return std::nullopt;
        }

        // Get .FileHash
        fileInfo.FileHash = getHash(historyFilePath);
        if (fileInfo.FileHash.empty()) {
            printf("[ERROR] Failed to compute hash for file '%s'\n", historyFilePath);
            return std::nullopt;
        }

        return fileInfo;
    }

    // Used to check whether the previous "current" file has changed
    bool checkRotation(const FileInfo& lastFileRead, const std::string& currHistoryFilePath){
        auto [inodeMatch, hashMatch] = ArchiveMonitor::checkFileEquals(currHistoryFilePath, lastFileRead);
        return inodeMatch && hashMatch;
    }

    // Helper function for getFilesCreated() to get file info
    std::time_t to_time_t(std::filesystem::file_time_type ftime) {
        using namespace std::chrono;
        auto sctp = time_point_cast<system_clock::duration>(ftime - fs::file_time_type::clock::now()
            + system_clock::now());
        return system_clock::to_time_t(sctp);
    }

    // Helper function to check if a file belongs to this history set
    bool isHistoryFile(const fs::path& filePath, const std::string& historyPrefix) {
        std::string filename = filePath.filename().string();
        return filename.substr(0, historyPrefix.length()) == historyPrefix;
    }

    // Helper function to get all history files modified after a certain time
    std::vector<fs::path> getHistoryFilesModifiedAfter(const std::string& directory,
                                                  const std::string& historyPrefix,
                                                  std::time_t afterTime) {
        std::vector<fs::path> historyFiles;
        
        try {
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    const fs::path& filePath = entry.path();
                    
                    // Check if this file belongs to our history set
                    if (isHistoryFile(filePath, historyPrefix)) {
                        auto writeTime = fs::last_write_time(filePath);
                        auto writeTime_t = to_time_t(writeTime);  // Use your helper function
                        
                        if (writeTime_t > afterTime) {
                            historyFiles.push_back(filePath);
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[getHistoryFilesModifiedAfter] Error reading directory " 
                    << directory << ": " << e.what() << "\n";
        }
        
        return historyFiles;
    }

    // Function to perform Step 1 for one history file set:
    // Find last file we were working on and identifies untracked files
    std::vector<fs::path> detectRotationAndGetUntrackedFiles(const std::string& directory,
                                            const std::string& historyPrefix,
                                            std::time_t lastStatusTime,
                                            const FileInfo& lastFileRead,
                                            ArchiveChange& changes) {
        // Get all history files modified after lastStatusTime
        std::vector<fs::path> filesAfterUpdate = getHistoryFilesModifiedAfter(directory, historyPrefix, lastStatusTime);

        // Sort from oldest to newest
        std::sort(filesAfterUpdate.begin(), filesAfterUpdate.end(),
            [](const fs::path& a, const fs::path& b) {
                return fs::last_write_time(a) < fs::last_write_time(b);
            });

        if (!filesAfterUpdate.empty()) {
            const fs::path& oldestFile = filesAfterUpdate.front();

            // Check if oldest file matches lastFileRead (rotation detection)
            if (checkRotation(lastFileRead, oldestFile.string())) {
                // We found a rotation
                std::cout << "[detectRotationAndGetUntrackedFiles] Rotation detected: "
                        << lastFileRead.FileName << " -> " << oldestFile.filename().string() << "\n";

                changes.rotatedFile = std::make_pair(lastFileRead.FileId, oldestFile.filename().string());

                // Remove oldest file from untracked list (since it's lastFileRead rotated)
                filesAfterUpdate.erase(filesAfterUpdate.begin());
            }
        }

        // Return remaining untracked files for next steps (new files)
        return filesAfterUpdate;
    }

    // Step 2: Collect FileInfo for all untracked files, update ArchiveChange
    void trackUntrackedFiles(const std::vector<fs::path>& untrackedFiles,
                          ArchiveChange& changes) {
        for (const auto& filePath : untrackedFiles) {
            try {
                // Collect FileInfo with size, offset=0 by default
                FileInfo newFileInfo = ArchiveMonitor::collectNewFileInfo(filePath.string(), 0);
                newFileInfo.FileSize = static_cast<int64_t>(fs::file_size(filePath));

                // Simply push the copy of FileInfo into the vector
                changes.newFiles.push_back(newFileInfo);

                std::cout << "[trackUntrackedFiles] Added new file: " << newFileInfo.FileName << "\n";

            } catch (const std::exception& e) {
                std::cerr << "[trackUntrackedFiles] Failed to process file " << filePath
                        << ": " << e.what() << "\n";
            }
        }
    }


    // Step 3: Look for deleted files that are in the cache but no longer in the directory
    void checkDeletedFiles(const std::string& directory,
                        const std::string& historyPrefix,
                        const std::unordered_map<int, FileInfo>& fileCache,
                        ArchiveChange& changes) {
        // Extract values from unordered_map into vector
        std::vector<FileInfo> sortedCache;
        sortedCache.reserve(fileCache.size());
        for (const auto& [fileId, fi] : fileCache) {
            sortedCache.push_back(fi);
        }

        // Sort by LastModified (oldest to newest)
        std::sort(sortedCache.begin(), sortedCache.end(),
            [](const FileInfo& a, const FileInfo& b) {
                return a.LastModified < b.LastModified;
            });

        for (const FileInfo& fi : sortedCache) {
            std::string fullPath = directory + "/" + fi.FileName;

            // Double-check that this cached file should belong to our history set
            // (defensive programming in case cache gets corrupted)
            if (!isHistoryFile(fs::path(fi.FileName), historyPrefix)) {
                std::cout << "[checkDeletedFiles] Skipping cached file that doesn't match prefix: " 
                         << fi.FileName << "\n";
                continue;
            }

            if (!fs::exists(fullPath)) {
                std::cout << "[checkDeletedFiles] File missing: " << fi.FileName << "\n";
                changes.deletedFileIds.push_back(fi.FileId);  // Push only the FileId
            } else {
                std::cout << "[checkDeletedFiles] Found file: " << fi.FileName << " — stopping scan.\n";
                break;  // Stop at first file that still exists
            }
        }
    }

    // Helper function for very first startup
    std::vector<fs::path> getAllHistoryFiles(const std::string& directory, const std::string& historyPrefix) {
        std::vector<fs::path> historyFiles;
        
        try {
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    const fs::path& filePath = entry.path();
                    if (isHistoryFile(filePath, historyPrefix)) {
                        historyFiles.push_back(filePath);
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[getAllHistoryFiles] Error reading directory " << directory << ": " << e.what() << "\n";
        }
        
        // Sort by modification time (oldest first)
        std::sort(historyFiles.begin(), historyFiles.end(),
            [](const fs::path& a, const fs::path& b) {
                return fs::last_write_time(a) < fs::last_write_time(b);
            });
        
        return historyFiles;
    }
    

}


namespace ArchiveMonitor{

    // Given a filepath, we check if its equal to the fileInfo struct using hash and inode
    std::pair<bool, bool> checkFileEquals(const std::string& historyFilePath, const FileInfo& fileInfo) {
        // Get the inode of the file at the filepath
        struct stat file_stat_check;
        if (stat(historyFilePath.c_str(), &file_stat_check) != 0) {
            printf("[ERROR] Failed to stat file: %s\n", historyFilePath.c_str());
            return {false, false}; // Can't stat file; both checks fail
        }

        // Get the hash of the file at the path
        std::string hashToCheck = getHash(historyFilePath.c_str());

        // Perform both checks
        bool inodeMatch = (file_stat_check.st_ino == fileInfo.FileInode);
        bool hashMatch = (hashToCheck == fileInfo.FileHash);

        return {inodeMatch, hashMatch};
    }

    FileInfo collectNewFileInfo(const std::string& path, long defaultOffset) {
        auto maybe = maybeCollectNewFileInfo(path.c_str());
        if (!maybe) {
            printf("[ERROR] Couldn't collect file info for: %s\n", path.c_str());
            FileInfo dummy;
            dummy.FileName = path.substr(path.find_last_of("/\\") + 1);
            dummy.LastOffset = defaultOffset;
            dummy.FileHash = "UNKNOWN";
            dummy.FileInode = 0;
            dummy.FileSize = -1;
            dummy.LastModified = 0;
            return dummy;
        }

        FileInfo fileInfo = *maybe;
        fileInfo.LastOffset = defaultOffset;
        return fileInfo;
    }

    /**
     * @brief Performs all 3 archive-monitoring steps for a single history file set.
     * 
     * Steps:
     * 1. Detect rotation
     * 2. Track new files
     * 3. Detect deleted files
     *
     * @param historyFileSet Metadata and cache info for this history file set
     * @param directory The directory path containing the mixed files
     * @return ArchiveChange describing all detected modifications
     */
    ArchiveChange trackHistoryFileSet(FileSet& historyFileSet) {
        ArchiveChange changes;
        std::string directory = historyFileSet.historyDirectoryPath;
        
        // Is this our first startup? 
        if (historyFileSet.lastFileReadId == -1) {
            // This is our very first read! Discover all files and add them as new files

             std::vector<fs::path> allHistoryFiles = getAllHistoryFiles(directory, historyFileSet.historyNameConfig);
        
            // Convert paths to FileInfo and add to changes.newFiles
            for (const auto& filePath : allHistoryFiles) {
                FileInfo fileInfo;
                fileInfo.FileName = filePath.filename().string();
                fileInfo.LastOffset = 0;  // Start from beginning
                // FileId will be assigned by dbHandler_->insertNewFiles()
                changes.newFiles.push_back(fileInfo);
            }
            
            printf("[trackHistoryFileSet] First startup: discovered %zu files with prefix '%s'\n", 
                changes.newFiles.size(), historyFileSet.historyNameConfig.c_str());
            
            return changes;
        }


        // Lookup lastFileRead FileInfo from fileMap using lastFileReadId
        auto it = historyFileSet.fileMap.find(historyFileSet.lastFileReadId);
        if (it == historyFileSet.fileMap.end()) {
            std::cerr << "[trackHistoryFileSet] Error: lastFileReadId " << historyFileSet.lastFileReadId
                    << " not found in fileMap.\n";
            return changes;
        }
        const FileInfo& lastFileRead = it->second;

        // Step 1: Find untracked files and check for rotation
        std::vector<fs::path> untrackedPaths = detectRotationAndGetUntrackedFiles(
            directory,
            historyFileSet.historyNameConfig,
            historyFileSet.lastStatusTime,
            lastFileRead,
            changes
        );

        // Step 2: Collect info on untracked files and add to ArchiveChange
        trackUntrackedFiles(untrackedPaths, changes);

        // Step 3: Detect deleted files in cache that no longer exist
        checkDeletedFiles(directory, historyFileSet.historyNameConfig, historyFileSet.fileMap, changes);

        return changes;
    }

}







