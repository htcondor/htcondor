#pragma once

#include <vector>
#include "JobRecord.h"
#include "dbHandler.h"

namespace DBHandlerTestUtils {

    std::vector<JobRecord> readAllJobs(const DBHandler& handler);

    std::vector<FileInfo> readAllFileInfos(const DBHandler& handler);

    bool clearAllTables(const DBHandler& handler);

    void clearCache(const DBHandler& handler);

    int countTable(const DBHandler& handler, const std::string& table);

    void writeTesterSpawnAd(DBHandler& handler, int clusterId, int procId, const std::string& owner, long completionDate);
}