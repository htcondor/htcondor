#pragma once

#include <string>
#include <unordered_map>
#include <ctime>        
#include "JobRecord.h"    

struct FileSet {
   std::string historyNameConfig;     // The prefix/identifier for this set of history files
   std::string historyDirectoryPath;  // The directory path where these history files are located
   std::unordered_map<int, FileInfo> fileMap; // The int here is for FileId
   std::time_t lastStatusTime;
   int lastFileReadId;

   // Constructor
   FileSet(const std::string& filePath)
       : historyNameConfig(fs::path(filePath).stem().string()),
         historyDirectoryPath(fs::path(filePath).parent_path().string()),
         lastStatusTime(0),
         lastFileReadId(-1) {}
};