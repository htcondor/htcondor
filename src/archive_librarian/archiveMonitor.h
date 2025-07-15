#pragma once
#include <string>
#include <vector>
#include "JobRecord.h"
#include "dbHandler.h"

std::string getHash(const char* filepath);

FileInfo collectNewFileInfo(const std::string& filePath, long defaultOffset = 0);

std::pair <bool, bool> checkFileEquals(const std::string& historyFilePath, const FileInfo& fileInfo);

std::vector<FileInfo> trackUntrackedFiles(const std::string& directory, DBHandler *handler);