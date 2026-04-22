#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "historyFileFinder.h"
#include "stl_string_utils.h"

#include "librarian.h"
#include "SavedQueries.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

#ifndef _WIN32
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

namespace conf = LibrarianConfigOptions;


// ================================
// FILE FINGERPRINTING (PRIVATE STATIC HELPERS)
// ================================

static uint64_t fnv1a_64(const std::string& s) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char c : s) {
        hash ^= c;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

// Reads the first record from path and returns an FNV-1a hex string of its raw text.
// Returns empty string on failure.
static std::string computeFileHash(const std::string& path) {
    ArchiveReader reader(path, ArchiveReader::Direction::Forward);
    if ( ! reader.IsOpen()) {
        dprintf(D_ERROR, "computeFileHash: failed to open '%s'\n", path.c_str());
        return {};
    }
    ArchiveRecord rec;
    reader.Next(rec);
    std::string hash;
    formatstr(hash, "%llx", (unsigned long long)fnv1a_64(rec.GetRawRecord()));
    return hash;
}

// Extracts the YYYYMMDDTHHMMSS rotation timestamp from a filename suffix.
// Returns nullopt when the filename has no rotation suffix.
// NOTE: findHistoryFiles() gets all archive files and verifies that each
//       is a rotated archive file in <basename>.YYYYMMDDTHHMMSS format
// NOTE: rfind in case history file basename contains '.' e.g. schedd1.history
static std::optional<std::string> extractRotationTime(const std::string& filename) {
    auto pos = filename.rfind(".");
    return (pos == std::string::npos || pos + 1 >= filename.length()) ? std::nullopt : std::make_optional(filename.substr(pos + 1));
}

// Collects disk metadata for path and returns a populated ArchiveFile.
// Returns nullopt if any required stat / hash step fails.
static std::optional<ArchiveFile> makeArchiveFile(const std::string& path) {
    ArchiveFile result;
    std::error_code ec;
    std::filesystem::path filePath(path);

    result.filename = filePath.filename().string();

    result.size = static_cast<int64_t>(std::filesystem::file_size(filePath, ec));
    if (ec) {
        dprintf(D_ERROR, "makeArchiveFile: could not get size for '%s': %s\n",
                path.c_str(), ec.message().c_str());
        return std::nullopt;
    }

    auto ftime = std::filesystem::last_write_time(filePath, ec);
    if (ec) {
        dprintf(D_ERROR, "makeArchiveFile: could not get mtime for '%s': %s\n",
                path.c_str(), ec.message().c_str());
        return std::nullopt;
    }
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    result.last_modified = std::chrono::system_clock::to_time_t(sctp);

#ifdef _WIN32
    result.inode = 0;
#else
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        dprintf(D_ERROR, "makeArchiveFile: stat failed for '%s'\n", path.c_str());
        return std::nullopt;
    }
    result.inode = static_cast<int64_t>(st.st_ino);
#endif

    result.hash = computeFileHash(path);
    if (result.hash.empty()) {
        dprintf(D_ERROR, "makeArchiveFile: failed to compute hash for '%s'\n", path.c_str());
        return std::nullopt;
    }

    auto rotTime = extractRotationTime(result.filename);
    if (rotTime) {
        result.rotation_time = *rotTime;
    }

    return result;
}


// ================================
// FILE READS AND WRITES
// ================================

/**
 * @brief Reads new job records from path using the persistent reader embedded in info.
 *
 * On the first call for a given ArchiveFile the reader is created.  If last_offset > 0
 * (daemon restart recovery) the reader seeks past already-processed records.  On
 * subsequent calls the existing reader has its EOF cleared so newly appended records
 * are picked up without reopening the file.
 *
 * When a rotated file reaches clean EOF the reader is released and fully_read is set.
 */
bool Librarian::readJobRecords(std::vector<ArchiveRecord>& records,
                               const std::string& path,
                               ArchiveFile& info,
                               int64_t limit) {
    if ( ! info.reader) {
        auto reader = std::make_unique<ArchiveReader>(path, ArchiveReader::Direction::Forward);
        if ( ! reader->IsOpen()) {
            dprintf(D_ERROR, "readJobRecords: failed to open '%s'\n", path.c_str());
            return false;
        }

        if (info.last_offset > 0) {
            // last_offset is the byte position *after* the last processed record.
            if ( ! reader->SeekForward(info.last_offset)) {
                dprintf(D_ERROR, "readJobRecords: seek to offset %lld failed for '%s'\n",
                        (long long)info.last_offset, path.c_str());
                return false;
            }
        }

        info.reader = std::move(reader);
    } else {
        info.reader->ClearEOF();
    }

    ArchiveRecord rec;
    int64_t prevPos = info.last_offset;
    while (info.reader->Next(rec)) {
        int64_t curPos = info.reader->Tellp();
        int64_t recordBytes = curPos - prevPos;
        if (recordBytes > 0) {
            info.records_read++;
            info.avg_record_size += (static_cast<double>(recordBytes) - info.avg_record_size)
                                  / static_cast<double>(info.records_read);
        }
        prevPos = curPos;
        records.push_back(rec);
        if (limit > 0 && static_cast<int64_t>(records.size()) >= limit) { break; }
    }

    // Store the position *after* the last processed byte so that
    // calculateBacklogFromBytes sees fileSize - last_offset == 0 when fully
    // caught up, and recovery seeks directly to the next unread record.
    info.last_offset = info.reader->Tellp();

    // A rotated file that reached clean EOF is done; release the file descriptor.
    if ( ! info.rotation_time.empty() && info.reader->LastError() == 0) {
        info.fully_read = true;
        info.reader.reset();
    }

    return true;
}


// ================================
// STATUS TRACKING UTILITIES
// ================================

// Computes EstimatedBytesPerJobInArchive_ as a weighted mean across all files
// that have seen at least one record. Called after each read cycle and on restart
// (when per-file data is recovered from the DB) to replace cold-start sampling.
void Librarian::updateBytesPerJobEstimate() {
    int64_t totalRecords = 0;
    double  weightedSum  = 0.0;

    for (const auto& [path, info] : m_archive_files) {
        if (info.records_read > 0) {
            weightedSum  += info.avg_record_size * static_cast<double>(info.records_read);
            totalRecords += info.records_read;
        }
    }

    if (totalRecords > 0) {
        EstimatedBytesPerJobInArchive_ = weightedSum / static_cast<double>(totalRecords);
        dprintf(D_FULLDEBUG, "Estimated bytes per record updated: %.1f bytes (%lld records)\n",
                EstimatedBytesPerJobInArchive_, (long long)totalRecords);
    }
}

/**
 * @brief Estimates remaining job records by summing unread bytes across all tracked files.
 *
 * For the most-recently-touched file uses status.last_file_offset (current cycle's value).
 * For all other not-fully-read files uses their stored last_offset.
 */
int Librarian::calculateBacklogFromBytes(const Status& status) {
    if (EstimatedBytesPerJobInArchive_ <= 0) return 0;

    int64_t totalUnreadBytes = 0;
    std::error_code ec;

    for (const auto& [path, info] : m_archive_files) {
        if (info.fully_read) continue;

        int64_t fileSize = static_cast<int64_t>(std::filesystem::file_size(path, ec));
        if (ec) {
            dprintf(D_ERROR, "Warning: Could not get file size for %s: %s\n",
                    path.c_str(), ec.message().c_str());
            continue;
        }

        int64_t offset = (info.id == status.last_file_id)
                       ? status.last_file_offset
                       : info.last_offset;

        totalUnreadBytes += std::max<int64_t>(0, fileSize - offset);
    }

    int estimatedBacklog = static_cast<int>(
        std::round(static_cast<double>(totalUnreadBytes) / EstimatedBytesPerJobInArchive_));

    dprintf(D_ALWAYS, "Backlog calculation: %lld total unread bytes, estimated %d jobs remaining\n",
            (long long)totalUnreadBytes, estimatedBacklog);

    return estimatedBacklog;
}

void Librarian::estimateArrivalRateWhileAsleep() {
    int64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Find the file with the highest id that has been partially read.
    long    lastId     = 0;
    int64_t lastOffset = 0;
    for (const auto& [path, info] : m_archive_files) {
        if (info.last_offset > 0 && info.id > lastId) {
            lastId     = info.id;
            lastOffset = info.last_offset;
        }
    }
    if (lastId == 0) return;

    Status dummyStatus;
    dummyStatus.last_file_id     = lastId;
    dummyStatus.last_file_offset = lastOffset;

    int     backlogAds  = calculateBacklogFromBytes(dummyStatus);
    int64_t timeDeltaMs = currentTime - statusData_.TimeOfLastUpdate;

    if (timeDeltaMs > 0) {
        double arrivalHz = (backlogAds * 1000.0) / timeDeltaMs;
        double n = static_cast<double>(statusData_.TotalCycles + 1);
        statusData_.MeanArrivalHz += (arrivalHz - statusData_.MeanArrivalHz) / n;
    }
}

void Librarian::updateStatusData(Status status) {
    int64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    statusData_.TotalCycles++;
    statusData_.TotalAdsIngested += status.records_processed;

    double currentAdsPerCycle    = static_cast<double>(status.records_processed);
    double currentDurationMs     = static_cast<double>(status.duration_ms);
    double currentIngestHz       = (currentDurationMs > 0)
                                 ? (currentAdsPerCycle * 1000.0 / currentDurationMs) : 0.0;
    double currentBacklogEstimate = static_cast<double>(status.backlog_estimate);

    double n = static_cast<double>(statusData_.TotalCycles);

    statusData_.AvgAdsIngestedPerCycle += (currentAdsPerCycle     - statusData_.AvgAdsIngestedPerCycle) / n;
    statusData_.AvgIngestDurationMs    += (currentDurationMs      - statusData_.AvgIngestDurationMs)    / n;
    statusData_.MeanIngestHz           += (currentIngestHz        - statusData_.MeanIngestHz)           / n;
    statusData_.MeanBacklogEstimate    += (currentBacklogEstimate - statusData_.MeanBacklogEstimate)    / n;

    if (status.hit_ingest_limit) {
        statusData_.HitMaxIngestLimitRate = (statusData_.HitMaxIngestLimitRate * (n - 1) + 1.0) / n;
    } else {
        statusData_.HitMaxIngestLimitRate = (statusData_.HitMaxIngestLimitRate * (n - 1)) / n;
    }

    statusData_.TimeOfLastUpdate  = currentTime;
    statusData_.LastRunLeftBacklog = status.hit_ingest_limit;
}


// ================================
// ARCHIVE MANAGEMENT
// ================================

/**
 * @brief Registers new archive paths and resolves file rotations.
 *
 * For each path returned by findHistoryFiles() that is not yet in m_archive_files:
 *   - If the path has a rotation timestamp AND its inode (Unix) or hash (Windows)
 *     matches the currently-active ArchiveFile, a rotation is recorded:
 *       * The active entry is re-keyed to the rotated path and its rotation_time set.
 *       * A fresh ArchiveFile is created for the original (now active) path.
 *   - Otherwise the path is treated as a brand-new file.
 *
 * All new entries are inserted into the database via writeFileInfo().
 */
void Librarian::reconcileArchiveFiles(const std::vector<std::string>& archive_files) {
    for (const auto& path : archive_files) {
        if (m_archive_files.contains(path)) continue;

        // Locate the active entry (the one file not yet rotated).
        std::string activePath;
        for (const auto& [key, entry] : m_archive_files) {
            if (entry.rotation_time.empty() && ! entry.fully_read) {
                activePath = key;
                break;
            }
        }

        bool isRotation = false;
        if ( ! activePath.empty()) {
            const ArchiveFile& active = m_archive_files[activePath];
            std::string basename = std::filesystem::path(path).filename().string();
            if (extractRotationTime(basename)) {
                // Windows (inode == 0): fall back to hash comparison.
                if (active.inode) {
                #ifndef _WIN32
                    struct stat st;
                    if (stat(path.c_str(), &st) == 0) {
                        isRotation = (static_cast<int64_t>(st.st_ino) == active.inode);
                    }
                #endif
                } else {
                    isRotation = ( ! active.hash.empty() && computeFileHash(path) == active.hash);
                }
            }
        }

        if (isRotation) {
            dprintf(D_STATUS, "reconcileArchiveFiles: rotation detected — '%s' → '%s'\n",
                    activePath.c_str(), path.c_str());

            // Re-key the active entry to the rotated path.
            auto node = m_archive_files.extract(activePath);
            node.key()                = path;
            node.mapped().filename    = std::filesystem::path(path).filename().string();
            node.mapped().rotation_time =
                *extractRotationTime(node.mapped().filename);
            m_archive_files.insert(std::move(node));

            dbHandler_.updateFileInfo(m_archive_files[path]);

            // Register the fresh active file.
            auto newActive = makeArchiveFile(activePath);
            if ( ! newActive) {
                dprintf(D_ERROR, "reconcileArchiveFiles: failed to build ArchiveFile for '%s'\n",
                        activePath.c_str());
            } else {
                dbHandler_.writeFileInfo(*newActive);
                m_archive_files[activePath] = std::move(*newActive);
            }
        } else {
            auto newFile = makeArchiveFile(path);
            if ( ! newFile) {
                dprintf(D_ERROR, "reconcileArchiveFiles: failed to build ArchiveFile for '%s'\n",
                        path.c_str());
            } else {
                dbHandler_.writeFileInfo(*newFile);
                m_archive_files[path] = std::move(*newFile);
            }
        }
    }
}


// ================================
// GARBAGE COLLECTION
// ================================

static std::uintmax_t get_database_size(const std::string& db_path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(db_path, ec);
    return ec ? 0 : size;
}

bool Librarian::cleanupDatabaseIfNeeded() {
    bool garbageCollected = false;

    size_t currentSize  = get_database_size(config[conf::str::DBPath]);
    long long sizeLimit = config[conf::ll::DBMaxSizeBytes];
    size_t highWatermark = static_cast<size_t>(sizeLimit * config[conf::dbl::DBHighWaterMark]);

    if (currentSize > highWatermark) {
        size_t lowWatermark   = static_cast<size_t>(sizeLimit * config[conf::dbl::DBLowWaterMark]);
        size_t bytesToDelete  = currentSize - lowWatermark;
        int numJobsToDelete   = static_cast<int>(
            std::ceil(static_cast<double>(bytesToDelete) / EstimatedBytesPerJobInDatabase_));
        int numFilesToDelete = 1;
        if (EstimatedJobsPerFileInArchive_ > 0) {
            numFilesToDelete = static_cast<int>(
                std::ceil(static_cast<double>(numJobsToDelete) / EstimatedJobsPerFileInArchive_));
        }

        garbageCollected = dbHandler_.runGarbageCollection(SavedQueries::GC_QUERY_SQL, numFilesToDelete);
        if ( ! garbageCollected) {
            dprintf(D_ERROR, "Garbage collection attempted but failed.\n");
        }
    }

    return garbageCollected;
}


// ================================
// CORE PUBLIC INTERFACE
// ================================

bool Librarian::initialize() {
    const std::string& archive = config[conf::str::ArchiveFile];
    if (archive.empty()) {
        dprintf(D_ERROR, "Empty archive file provided to librarian\n");
        return false;
    }

    dprintf(D_FULLDEBUG, "Initializing DBHandler.\n");
    dprintf(D_STATUS, "Tracking archive file: %s\n", archive.c_str());

    if ( ! dbHandler_.initialize()) {
        return false;
    }
    if ( ! dbHandler_.testDatabaseConnection()) {
        dprintf(D_ERROR, "DBHandler connection test failed\n");
        return false;
    }
    if ( ! dbHandler_.verifyDatabaseSchema(SavedQueries::SCHEMA_SQL)) {
        dprintf(D_ERROR, "DBHandler schema is not as expected\n");
        return false;
    }

    std::string directory = std::filesystem::path(archive).parent_path().string();
    dbHandler_.maybeRecoverStatusAndFiles(m_archive_files, statusData_, directory);

    dprintf(D_FULLDEBUG, "DBHandler initialized.\n");

    updateBytesPerJobEstimate();

    return true;
}

/**
 * @brief Executes one complete update cycle.
 *
 * Phase 1:   Discover current archive files on disk.
 * Phase 1.5: Reconcile new/rotated files into m_archive_files and the DB.
 * Phase 2:   Evict stale entries (files removed from disk since last cycle).
 * Phase 3:   Read new records from each file and insert them into the DB.
 * Phase 4:   Run garbage collection if the DB exceeds its size limit.
 * Phase 5:   Record status metrics for this cycle.
 */
bool Librarian::update() {
    dprintf(D_FULLDEBUG, "Starting update protocol.\n");

    Status status;

    // PHASE 1: Discover all current archive files (oldest rotated first, active last).
    auto archive_files = findHistoryFiles(config[conf::str::ArchiveFile].c_str());

    // PHASE 1.5: Register new files and handle rotations.
    reconcileArchiveFiles(archive_files);

    // PHASE 2: Remove entries for files that no longer exist on disk.
    const std::unordered_set<std::string> archive_set(archive_files.begin(), archive_files.end());
    std::erase_if(m_archive_files, [&](const auto& item) {
        const auto& [path, info] = item;
        bool removed = (archive_set.count(path) == 0);
        if (removed) {
            int64_t now = static_cast<int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

            if (info.rotation_time.empty()) {
                std::string removedName = info.filename + ".REMOVED";
                dprintf(D_STATUS, "update: active archive '%s' disappeared without rotation; renaming to '%s' and marking deleted in DB.\n",
                        path.c_str(), removedName.c_str());
                ArchiveFile updated = info;
                updated.filename = removedName;
                dbHandler_.updateFileInfo(updated);
            } else if ( ! info.fully_read) {
                dprintf(D_STATUS, "update: rotated archive '%s' disappeared before being fully read; marking deleted in DB.\n",
                        path.c_str());
            }

            dbHandler_.markFileDeleted(info.id, now);
        }
        return removed;
    });

    if ( ! statusData_.LastRunLeftBacklog && statusData_.TimeOfLastUpdate > 0) {
        estimateArrivalRateWhileAsleep();
    }

    // PHASE 3: Read and ingest records from each file.
    size_t filesProcessed = 0;
    dprintf(D_FULLDEBUG, "Processing history file queue.\n");
    auto startTime = std::chrono::system_clock::now();

    for (const auto& path : archive_files) {
        if (status.records_processed >= config[conf::ll::MaxRecordsPerUpdate]) {
            dprintf(D_STATUS, "Reached record limit (%" PRId64 "); stopping after %zu files.\n",
                    config[conf::ll::MaxRecordsPerUpdate], filesProcessed);
            status.hit_ingest_limit = true;
            break;
        }

        auto it = m_archive_files.find(path);
        if (it == m_archive_files.end()) continue; // reconcile failed for this path
        if (it->second.fully_read) continue;

        filesProcessed++;
        ArchiveFile& info = it->second;

        dprintf(D_STATUS, "Processing history file: %s (offset: %lld)\n",
                path.c_str(), (long long)info.last_offset);

        std::vector<ArchiveRecord> records;
        int64_t remaining = config[conf::ll::MaxRecordsPerUpdate] - status.records_processed;
        if ( ! readJobRecords(records, path, info, remaining)) {
            dprintf(D_ERROR, "Failed to read job records from %s\n", path.c_str());
            return false;
        }

        if (records.empty()) {
            dprintf(D_FULLDEBUG, "No new job records in %s\n", path.c_str());
            if (info.fully_read) {
                dbHandler_.updateFileInfo(info);
            }
        } else if ( ! dbHandler_.insertJobFileRecords(records, info)) {
            dprintf(D_ERROR, "Failed to atomically process history file %s\n", path.c_str());
            // TODO: Seek back to previous position and restore last_offset.
        }

        status.last_file_id     = info.id;
        status.last_file_offset = info.last_offset;
        status.records_processed += records.size();
    }

    auto endTime = std::chrono::system_clock::now();

    if (status.records_processed > 0) {
        updateBytesPerJobEstimate();
    }

    // PHASE 4: Garbage Collection.
    status.ran_garbage_collect = cleanupDatabaseIfNeeded();

    // PHASE 5: Status Update and Finalization.
    status.update_time    = std::chrono::system_clock::to_time_t(endTime);
    status.duration_ms    = std::chrono::duration_cast<std::chrono::milliseconds>(
                                endTime - startTime).count();
    status.backlog_estimate = calculateBacklogFromBytes(status);

    updateStatusData(status);

    if ( ! dbHandler_.writeStatusAndData(status, statusData_)) {
        dprintf(D_ERROR, "WARNING: Failed to write status to database\n");
        return false;
    }

    if (status.records_processed > 0) {
        dprintf(D_STATUS, "Update completed. Inserted %" PRId64 " records in %" PRId64 " ms.\n",
                status.records_processed, status.duration_ms);
    }

    return true;
}

void Librarian::reconfig(bool startup) {
    using namespace LibrarianConfigOptions;

    if (startup) {
        config[i::UpdateInterval] = param_integer("LIBRARIAN_UPDATE_INTERVAL", 5);
    }

    param(config[str::ArchiveFile], "HISTORY");
    param(config[str::DBPath],      "LIBRARIAN_DATABASE");

    config[i::DBMaxJobCacheSize]          = param_integer("LIBRARIAN_MAX_JOBS_CACHED", 10'000);
    config[i::StatusRetentionSeconds]     = param_integer("LIBRARIAN_STATUS_RETENTION_SECONDS", 300);

    param_longlong("LIBRARIAN_MAX_UPDATES_PER_CYCLE", config[ll::MaxRecordsPerUpdate], true,
                   100'000);
    param_longlong("LIBRARIAN_MAX_DATABASE_SIZE", config[ll::DBMaxSizeBytes], true,
                   2LL * 1024 * 1024 * 1024);

    config[dbl::DBHighWaterMark] = param_double("LIBRARIAN_HIGH_WATER_MARK", 0.97, 0, 1);
    config[dbl::DBLowWaterMark] = param_double("LIBRARIAN_LOW_WATER_MARK", 0.80, 0, 1);
}
