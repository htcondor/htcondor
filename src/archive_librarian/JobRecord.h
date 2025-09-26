#pragma once
#include <string>
#include <ctime>

struct Job {
    int ClusterId{0};
    int ProcId{0};
    long TimeOfCreation{0};
    long Offset{0}; // To set up functionality for parsing EPOCH records in the future
    long FileId{-1};
};

struct JobRecord {
    long Offset{0};
    int ClusterId{0}; //This might be redundant, ultimately
    int ProcId{0};
    std::string Owner{};
    long CompletionDate{0};
    long FileId{0};
    int JobId{-1};
};

struct BannerInfo {
    long offset{0};
    int cluster{0};
    int proc{0};
    std::string owner{};
    long completionDate{0};
};

struct FileInfo {
    long FileId{0};
    long FileInode{0};
    std::string FileHash{};
    std::string FileName{};
    long LastOffset{0};
    int64_t FileSize{-1};
    std::time_t LastModified{};
    std::string DateOfRotation{};
    bool FullyRead{false};
};

struct Status {
    int64_t TimeOfUpdate{0};

    int HistoryFileIdLastRead{0};
    int64_t HistoryFileOffsetLastRead{0};

    int EpochFileIdLastRead{0};
    int64_t EpochFileOffsetLastRead{0};

    long TotalJobsRead{0};
    long TotalEpochsRead{0};
    long DurationMs{0};
    long JobBacklogEstimate{0}; // Not calculating this for now
    bool HitMaxIngestLimit{false};
    bool GarbageCollectionRun{false};
};

struct StatusData {
    double AvgAdsIngestedPerCycle{0.0};  // Mean ads processed per ingest cycle
    double AvgIngestDurationMs{0.0};     // Mean duration (ms) per ingest cycle
    double MeanIngestHz{0.0};            // Mean ingest rate (ads/sec)
    double MeanArrivalHz{0.0};           // Mean arrival rate of new ads (ads/sec)
    double MeanBacklogEstimate{0.0};     // Average estimated backlog size
    int64_t TotalCycles{0};              // Total number of timeout cycles
    int64_t TotalAdsIngested{0};         // Cumulative count of ads ingested
    double HitMaxIngestLimitRate{0.0};   // Proportion of cycles that hit ingest cap

    int64_t TimeOfLastUpdate{0};         // Used for MeanArrivalHz calculations
    bool LastRunLeftBacklog{false};      // Used for MeanArrivalHz calculations
    //         ErrorRate (?)           - Proportion of cycles with errors
};


