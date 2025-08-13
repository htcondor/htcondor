#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <filesystem>
#include <ctime>
#include "JobRecord.h"

namespace fs = std::filesystem;

// TODO: Move/Improve file discovery and tracking code

struct FileSet {
   std::string historyNameConfig{};     // The prefix/identifier for this set of history files
   std::string historyDirectoryPath{};  // The directory path where these history files are located
   std::unordered_map<int, FileInfo> fileMap; // The int here is for FileId
   std::time_t lastStatusTime{0};
   long lastFileReadId{-1};

   void Init(const std::string& path) {
      fs::path p(path);
      historyNameConfig = p.stem().string();
      historyDirectoryPath = p.parent_path().string();
   }
};

struct ArchiveChange {
    // If a rotation was detected:
    // First: FileId of the old file that rotated
    // Second: New filename it was rotated to
    std::pair<int, std::string> rotatedFile;

    // New untracked files to add to the database (full FileInfo structs)
    std::vector<FileInfo> newFiles;

    // FileIds of files that no longer exist on disk and should be deleted
    std::vector<int> deletedFileIds;
};

namespace ArchiveMonitor {

    // A helper function to use regex and extract the DateOfRotation value from a rotated file's name
    std::optional<std::string> extractDateOfRotation(const std::string& filename);

    // Check if the file at the path matches the expected info in FileInfo
    std::pair<bool, bool> checkFileEquals(const std::string& historyFilePath, const FileInfo& fileInfo);

    /**
     * High-level function to track changes in a directory 
     * Returns ArchiveChange describing detected modifications.
     */
    ArchiveChange trackHistoryFileSet(FileSet& fileSet);

    /**
     * Collect detailed FileInfo from a file path, with optional offset
     */
    FileInfo collectNewFileInfo(const std::string& filePath, long defaultOffset = 0);
}
