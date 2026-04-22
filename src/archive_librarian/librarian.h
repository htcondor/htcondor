#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "archive_reader.h"
#include "librarian_types.h"
#include "dbHandler.h"
#include "config.hpp"

/**
 * @class Librarian
 * @brief Orchestrates history processing: reads HTCondor archive files and syncs them to a database.
 *
 * Responsibilities:
 * - Initialize DB schema and handlers.
 * - Run the update cycle each timer tick:
 *   - Discover current archive files via findHistoryFiles()
 *   - Detect file rotations and register new files (reconcileArchiveFiles)
 *   - Read new records incrementally with persistent per-file readers
 *   - Insert records and update file state in the database atomically
 *   - Garbage collect old data when the DB exceeds its size limit
 *   - Record per-cycle and rolling status metrics
 */
class Librarian {
public:
    bool initialize();
    bool update();

    void reconfig(bool startup = false);

    LibrarianConfig config{};

private:
    StatusData statusData_{};
    double EstimatedBytesPerJobInArchive_{0.0};
    int    EstimatedJobsPerFileInArchive_{0};
    double EstimatedBytesPerJobInDatabase_{1024};

    // update() helpers
    bool readJobRecords(std::vector<ArchiveRecord>& records, const std::string& path, ArchiveFile& info, int64_t limit = -1);
    void reconcileArchiveFiles(const std::vector<std::string>& archive_files);
    bool calculateEstimatedBytesPerJob();
    void updateBytesPerJobEstimate();
    int  calculateBacklogFromBytes(const Status& status);
    void updateStatusData(Status status);
    void estimateArrivalRateWhileAsleep();
    bool cleanupDatabaseIfNeeded();

    DBHandler dbHandler_{config};

    // In-memory archive file state, keyed by full path
    std::map<std::string, ArchiveFile> m_archive_files{};
};
