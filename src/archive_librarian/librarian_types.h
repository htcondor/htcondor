#pragma once

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>

#include "archive_reader.h"

struct ArchiveFile {
    std::unique_ptr<ArchiveReader> reader{};   // Forward reader; open until fully read then reset
    std::string filename{};                    // Basename only (e.g. "history" or "history.20241215T143022")
    std::string hash{};                        // FNV-1a hash of first record — file identity fingerprint
    std::string rotation_time{};               // Rotation timestamp string (empty when file is still active)
    int64_t     size{-1};                      // File size in bytes at discovery time
    int64_t     last_offset{0};                // Byte offset of the last processed record
    std::time_t last_modified{};               // Last write time at discovery
    int64_t     id{0};                         // Database FileId
    int64_t     inode{0};                      // Inode number; always 0 on Windows
    bool        fully_read{false};             // True when a rotated file's reader reached clean EOF
    double      avg_record_size{0.0};          // Welford online mean: bytes per record seen so far
    int64_t     records_read{0};               // Count of records read (Welford denominator)

    ArchiveFile() = default;
    ArchiveFile(ArchiveFile&&) = default;
    ArchiveFile& operator=(ArchiveFile&&) = default;

    // Copy leaves reader null — the reader is not logically copyable
    ArchiveFile(const ArchiveFile& o)
        : reader(nullptr)
        , filename(o.filename)
        , hash(o.hash)
        , rotation_time(o.rotation_time)
        , size(o.size)
        , last_offset(o.last_offset)
        , last_modified(o.last_modified)
        , id(o.id)
        , inode(o.inode)
        , fully_read(o.fully_read)
        , avg_record_size(o.avg_record_size)
        , records_read(o.records_read) {}

    ArchiveFile& operator=(const ArchiveFile& o) {
        if (this != &o) {
            reader         = nullptr;
            filename       = o.filename;
            hash           = o.hash;
            rotation_time  = o.rotation_time;
            size           = o.size;
            last_offset    = o.last_offset;
            last_modified  = o.last_modified;
            id             = o.id;
            inode          = o.inode;
            fully_read     = o.fully_read;
            avg_record_size = o.avg_record_size;
            records_read   = o.records_read;
        }
        return *this;
    }
};

struct Status {
    int64_t update_time{0};         // Timestamp of this cycle's end (seconds since epoch)
    int64_t last_file_offset{0};    // Byte offset of the last record processed
    int64_t backlog_estimate{0};    // Estimated number of unprocessed records

    int64_t last_file_id{0};        // DB FileId of the last file touched this cycle
    int64_t duration_ms{0};         // Wall-clock duration of the update cycle in milliseconds

    int64_t records_processed{0};   // Records ingested this cycle
    int64_t records_lost{0};        // Records read but dropped due to insert failure

    bool hit_ingest_limit{false};   // True when MaxRecordsPerUpdate cap was reached
    bool ran_garbage_collect{false};
};

struct StatusData {
    double  AvgAdsIngestedPerCycle{0.0};
    double  AvgIngestDurationMs{0.0};
    double  MeanIngestHz{0.0};
    double  MeanArrivalHz{0.0};
    double  MeanBacklogEstimate{0.0};
    int64_t TotalCycles{0};
    int64_t TotalAdsIngested{0};
    int64_t TotalRecordsLost{0};
    double  HitMaxIngestLimitRate{0.0};
    int64_t TimeOfLastUpdate{0};
    bool    LastRunLeftBacklog{false};
};
