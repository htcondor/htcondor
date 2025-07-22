#pragma once
#include <string>

struct Job {
    int ClusterId{};
    int ProcId{};
    long TimeOfCreation{};
    long Offset{}; // To set up functionality for parsing EPOCH records in the future
    long FileId{};
};

struct JobRecord {
    long Offset{};
    int ClusterId{}; //This might be redundant, ultimately
    int ProcId{};
    std::string Owner;
    long CompletionDate{};
    long FileId{};
    int JobId = -1;
};

struct BannerInfo {
    long offset{};
    int cluster{};
    int proc{};
    std::string owner;
    long completionDate{};
};

struct FileInfo {
    long FileId{};
    long FileInode{};
    std::string FileHash;
    std::string FileName;
    long LastOffset{};
    int64_t FileSize = -1; 
    std::time_t LastModified;
};

struct Status {
    int64_t TimeOfUpdate;

    int HistoryFileIdLastRead;
    int64_t HistoryFileOffsetLastRead;

    int EpochFileIdLastRead;
    int64_t EpochFileOffsetLastRead;

    long TotalJobsRead;
    long TotalEpochsRead;
    long DurationMs;
    long BacklogEstimate = 0; // Not calculating this for now 
};

struct StatusData {
    double AvgAdsIngestedPerCycle;    // Mean ads processed per ingest cycle
    double AvgIngestDurationMs;       // Mean duration (ms) per ingest cycle
    double MeanIngestHz;              // Mean ingest rate (ads/sec)
    double MeanArrivalHz;             // Mean arrival rate of new ads (ads/sec)
    double MeanBacklogEstimate;       // Average estimated backlog size
    int64_t TotalCycles;              // Total number of timeout cycles
    int64_t TotalAdsIngested;         // Cumulative count of ads ingested
    // TO ADD: HitMaxIngestLimitRate   - Proportion of cycles that hit ingest cap
    //         ErrorRate (?)           - Proportion of cycles with errors
};

struct EpochRecord {
    int ClusterId;
    int ProcId;
    int RunInstanceId;
    std::string Owner;
    long CurrentTime;
    long Offset;    // We'll calculate this offset manually
    long FileId;
    std::string RecordType;
};
