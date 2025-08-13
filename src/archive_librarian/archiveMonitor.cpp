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
#include <functional>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <ranges>
#include <optional>
#include <regex>
#include <system_error>

#include "JobRecord.h"  
#include "readHistory.h"  
#include "dbHandler.h"
#include "archiveMonitor.h"


namespace fs = std::filesystem; // where am i?
 
namespace { // Helper functions for ArchiveMonitor utility functions

    std::string getHash(const char* historyFilePath) {
        FILE* file = fopen(historyFilePath, "rb");
        if (!file) {
            perror("fopen");
            return std::string{};
        }

        std::string buffer;
        char line[4096];

        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "***", 3) == 0) {
                break;
            }
            buffer += line;
        }

        fclose(file);

        std::stringstream ss;
        ss << std::hex << std::hash<std::string>{}(buffer);
        return ss.str();
    }


    // Collect information about a new file: Name, Inode, Hash
    std::optional<FileInfo> maybeCollectNewFileInfo(const std::string& historyFilePath) {
        FileInfo fileInfo;
        std::error_code ec;
        
        std::filesystem::path filePath(historyFilePath);
        
        // Get .FileName from full path - cross-platform safe
        fileInfo.FileName = filePath.filename().string();

        // Check for rotated file pattern: ends with YYYYMMDDTHHMMSS (potentially more S digits)
        auto dateOfRotation = ArchiveMonitor::extractDateOfRotation(fileInfo.FileName);
        if (dateOfRotation.has_value()) {
            fileInfo.DateOfRotation = dateOfRotation.value();
        }

        // Check if file exists before getting stats
        if (!std::filesystem::exists(filePath, ec)) {
            printf("[ERROR] File does not exist: '%s'\n", historyFilePath.c_str());
            return std::nullopt;
        } else if (ec) {
            printf("[ERROR] Error checking file existence for '%s': %s\n", 
                historyFilePath.c_str(), ec.message().c_str());
            return std::nullopt;
        }
        
        // Get file size using std::filesystem
        fileInfo.FileSize = std::filesystem::file_size(filePath, ec);
        if (ec) {
            printf("[ERROR] Could not get file size for '%s': %s\n", 
                historyFilePath.c_str(), ec.message().c_str());
            return std::nullopt;
        }
        
        // Get last write time
        auto ftime = std::filesystem::last_write_time(filePath, ec);
        if (ec) {
            printf("[ERROR] Could not get last write time for '%s': %s\n", 
                historyFilePath.c_str(), ec.message().c_str());
            return std::nullopt;
        }
        
        // Convert to time_t (may need adjustment based on your needs)
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        fileInfo.LastModified = std::chrono::system_clock::to_time_t(sctp);
        
        // Windows doesn't have an Inode nor a direct equivalent
        #ifdef _WIN32
            fileInfo.FileInode = 0; // Just using the same value for all files if on Windows
        #else
            // Unix-like systems with inode
            struct stat file_stat;
            if (stat(historyFilePath.c_str(), &file_stat) == 0) {
                fileInfo.FileInode = static_cast<long>(file_stat.st_ino);
            } else {
                printf("[ERROR] Could not get inode for '%s'\n", historyFilePath.c_str());
                return std::nullopt;
            }
        #endif
        
        // Get .FileHash
        fileInfo.FileHash = getHash(historyFilePath.c_str());
        if (fileInfo.FileHash.empty()) {
            printf("[ERROR] Failed to compute hash for file '%s'\n", historyFilePath.c_str());
            return std::nullopt;
        }

        //printf("FileInode is: %ld, and File Hash is: %s \n", fileInfo.FileInode, fileInfo.FileHash.c_str());
        
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
        std::ranges::sort(filesAfterUpdate,
            [](const fs::path& a, const fs::path& b) {
                return fs::last_write_time(a) < fs::last_write_time(b);
            });

        if (!filesAfterUpdate.empty()) {
            const fs::path& oldestFile = filesAfterUpdate.front();

            // Check if oldest file matches lastFileRead (rotation detection)
            if (checkRotation(lastFileRead, oldestFile.string())) {
                // We found a rotation
                changes.rotatedFile = std::make_pair(lastFileRead.FileId, oldestFile.filename().string());

                // Remove oldest file from untracked list (since it's lastFileRead rotated)
                filesAfterUpdate.erase(filesAfterUpdate.begin());
            }
        }

        // Return remaining untracked files for next steps (new files)
        return filesAfterUpdate;
    }

    // Step 2: Collect FileInfo for all untracked files, update ArchiveChange
    // TODO: remove exception try-catch from this
    void trackUntrackedFiles(const std::vector<fs::path>& untrackedFiles,
                          ArchiveChange& changes) {
        for (const auto& filePath : untrackedFiles) {
            try {
                // Collect FileInfo with size, offset=0 by default
                FileInfo newFileInfo = ArchiveMonitor::collectNewFileInfo(filePath.string(), 0);
                newFileInfo.FileSize = static_cast<int64_t>(fs::file_size(filePath));

                // Simply push the copy of FileInfo into the vector
                changes.newFiles.push_back(newFileInfo);

            } catch (const std::exception& e) {
                std::cerr << "[ArchiveMonitor] Failed to process file " << filePath
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
        std::ranges::sort(sortedCache, std::ranges::less(), &FileInfo::LastModified);

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
        std::ranges::sort(historyFiles,
            [](const fs::path& a, const fs::path& b) {
                return fs::last_write_time(a) < fs::last_write_time(b);
            });
        
        return historyFiles;
    }
    

}


namespace ArchiveMonitor{

    /**
    * Extracts YYYYMMDDTHHMMSS timestamp from rotated filenames ending with that pattern.
    * Returns std::nullopt if no rotation timestamp is found.
    */
    std::optional<std::string> extractDateOfRotation(const std::string& filename) {
        std::regex rotatedPattern(R"(.*(\d{4}\d{2}\d{2}T\d{2}\d{2}\d{2})\d*$)");
        std::smatch match;

        if (std::regex_match(filename, match, rotatedPattern)) {
            // Extract the YYYYMMDDTHHMMSS part (first 15 characters of the timestamp)
            return match[1].str();
        }
        
        return std::nullopt;  // No match found
    }

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
        auto maybe = maybeCollectNewFileInfo(path);
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
        
            printf("[ArchiveMonitor] First startup: discovered %zu files with prefix '%s'\n", 
                allHistoryFiles.size(), historyFileSet.historyNameConfig.c_str());
            
            // Use the existing trackUntrackedFiles function to properly process all discovered files
            trackUntrackedFiles(allHistoryFiles, changes);
            
            return changes;
        }


        // Lookup lastFileRead FileInfo from fileMap using lastFileReadId
        auto it = historyFileSet.fileMap.find(historyFileSet.lastFileReadId);
        if (it == historyFileSet.fileMap.end()) {
            std::cerr << "[ArchiveMonitor] Error: lastFileReadId " << historyFileSet.lastFileReadId
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







