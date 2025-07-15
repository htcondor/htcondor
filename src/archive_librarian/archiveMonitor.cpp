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
// #include "archiveMonitor.h"


namespace fs = std::filesystem; // where am i?


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
        exit(EXIT_FAILURE);
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

    // Get .fileName from full path
    std::string fullPath(historyFilePath);
    size_t pos = fullPath.find_last_of("/\\");
    fileInfo.FileName = (pos == std::string::npos) ? fullPath : fullPath.substr(pos + 1);

    // Get .fileInode
    struct stat file_stat;
    if (stat(historyFilePath, &file_stat) == 0) {
        ino_t inode_number = file_stat.st_ino;
        fileInfo.FileInode = inode_number;
    } else {
        printf("[ERROR] Could not get file stats for '%s'\n", historyFilePath);
    }

    // Get file hash
    try {
        fileInfo.FileHash = getHash(historyFilePath);
    } catch (const std::exception &e) {
        printf("[ERROR] Failed to compute hash for file '%s': %s\n", historyFilePath, e.what());
        return std::nullopt;
    }

    return fileInfo;
}

FileInfo collectNewFileInfo(const std::string& path, long defaultOffset = 0) {
    auto maybe = maybeCollectNewFileInfo(path.c_str());
    if (!maybe) {
        printf("[ERROR] Couldn't collect file info for: %s\n", path.c_str());
        FileInfo dummy;
        dummy.FileName = path.substr(path.find_last_of("/\\") + 1);
        dummy.LastOffset = defaultOffset;
        dummy.FileHash = "UNKNOWN";
        dummy.FileInode = 0;
        return dummy;
    }

    FileInfo fileInfo = *maybe;
    fileInfo.LastOffset = defaultOffset; 
    return fileInfo;
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

bool checkRotation(const FileInfo& lastFileRead, const std::string& currHistoryFilePath){
    auto [inodeMatch, hashMatch] = checkFileEquals(currHistoryFilePath, lastFileRead);
    return inodeMatch && hashMatch;
}


// Helper function for getFilesCreated() to get file info
std::time_t to_time_t(std::filesystem::file_time_type ftime) {
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(ftime - fs::file_time_type::clock::now()
        + system_clock::now());
    return system_clock::to_time_t(sctp);
}

// Helper function for getUntrackedFiles() -> gets files made after lastStatusRead.TimeOfUpdate
std::vector<fs::path> getFilesModifiedAfter(const std::string& directory, std::time_t minTimestamp) {
    std::vector<std::pair<fs::path, std::time_t>> matching;

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!fs::is_regular_file(entry)) continue;

        auto ftime = fs::last_write_time(entry);
        std::time_t ctime = to_time_t(ftime);

        if (ctime > minTimestamp) {
            matching.emplace_back(entry.path(), ctime);
        }
    }

    std::sort(matching.begin(), matching.end(),
        [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

    std::vector<fs::path> result;
    for (const auto& [path, _] : matching) {
        result.push_back(path);
    }

    return result;
}


std::vector<FileInfo> trackUntrackedFiles(const std::string& directory, DBHandler *handler){
    // Get status and last file read
    Status lastStatus = handler->readLastStatus();
    FileInfo lastReadFile = handler->lastFileRead("history");

    printf("[ArchiveMonitor] lastStatus.TimeOfUpdate: %lld  | lastFileRead: %s\n",
       lastStatus.TimeOfUpdate, lastReadFile.FileName.c_str());

    std::vector<fs::path> untrackedFilePaths = getFilesModifiedAfter(directory, lastStatus.TimeOfUpdate);

    // Step 1: Sort from oldest to newest
    std::sort(untrackedFilePaths.begin(), untrackedFilePaths.end(),
        [](const fs::path& a, const fs::path& b) {
            return fs::last_write_time(a) < fs::last_write_time(b);
        });

    // Step 2: Check if the least recently modified matches lastReadFile
    if (!untrackedFilePaths.empty()) {
        const fs::path& oldest = untrackedFilePaths.front();
        if (checkRotation(lastReadFile, oldest.c_str())) {
            untrackedFilePaths.erase(untrackedFilePaths.begin());  // remove the match
        }
    }

    // Step 3: Write info for remaining untracked files
    std::vector<FileInfo> untrackedFiles;
    for (const fs::path& filePath : untrackedFilePaths) {
        FileInfo newFile = collectNewFileInfo(filePath.c_str(), 0);
        untrackedFiles.push_back(newFile); // save to give to manager
        handler->writeFileInfo(newFile); // push into database
    }

    return untrackedFiles; // should be from oldest to newest 
}


