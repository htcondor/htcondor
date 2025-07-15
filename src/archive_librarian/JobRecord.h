#pragma once
#include <string>

struct Job {
    int ClusterId{};
    int ProcId{};
    long TimeOfCreation{};
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
