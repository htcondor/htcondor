/**
 * @file readHistory.cpp
 * @brief Handles reading and parsing HTCondor history files.
 *
 * Responsibilities:
 * - Read history files starting from a provided byte offset.
 * - Read EPOCHHISTORY files starting from a provided byte offset, collecting only SpawnAds. 
 * - Parse raw JobAd text into structured JobRecord objects.
 * - Return a vector of parsed jobs and the final offset reached.
 *
 * Notes:
 * - Does not update the database; returns offset to be used by librarian.
 * - Handles both file I/O and parsing logic.
 *
 * Key Function:
 * - long readHistoryIncremental(const char* file, std::vector<JobRecord>& out, FileInfo& fileInfoOut)
 * - long readEpochIncremental(const char* file, std::vector<EpochRecord>& out, FileInfo& fileInfoOut)
 */

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <regex>
#include <iostream>
#include <iomanip>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include "JobRecord.h"  
#include "archiveMonitor.h"
extern "C" {
    #include "sha256.h"
}

#define SHA256_BLOCK_SIZE 32

// -------------------------
// Lightweight backward file reader – optimized with chunked reads
// -------------------------

class BackwardFileReader {
public:
    explicit BackwardFileReader(const std::string &filename) : file(filename) {
        if (!file)
            err = 1;
        else {
            file.seekg(0, std::ios::end);
            pos = file.tellg();
        }
    }

    ~BackwardFileReader(){
        Close();
    }

    bool readChunk(std::string &chunk) {    
        const size_t BUFFER_SIZE = 8192;
        if (pos == 0) return false;

        std::streamoff readSize = (pos < static_cast<std::streamoff>(BUFFER_SIZE))
                          ? static_cast<std::streamoff>(pos)
                          : static_cast<std::streamoff>(BUFFER_SIZE);
        std::streamoff newPos = pos - readSize;

        file.seekg(newPos, std::ios::beg);
        std::vector<char> buffer(readSize);
        file.read(buffer.data(), readSize);

        if (!file.good() && !file.eof()) return false;

        chunk.assign(buffer.data(), readSize);
        pos = newPos;
        return true;
    }


    [[nodiscard]] int LastError() const { return err; }
    void Close() { file.close(); }

private:
    std::ifstream file;
    std::streampos pos{};
    int err{0};
};


//  For reading Epoch files (need to go forward to easily calculate offset)
class ForwardFileReader {
public:
    explicit ForwardFileReader(const std::string &filename) : file(filename) {
        if (!file)
            err = 1;
    }

    ~ForwardFileReader(){
        file.close();
    }

    bool readChunk(std::string &chunk) {
        const size_t BUFFER_SIZE = 8192;
        if (!file || file.eof()) return false;

        std::vector<char> buffer(BUFFER_SIZE);
        file.read(buffer.data(), BUFFER_SIZE);

        std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0) return false;

        chunk.assign(buffer.data(), static_cast<size_t>(bytesRead));
        return true;
    }

    [[nodiscard]] int LastError() const { return err; }
    void Close() { file.close(); }
private:
    std::ifstream file;
    int err{0};
};


// -------------------------
// Parsing helpers
// -------------------------
static const std::regex offsetRegex(
    R"(\*\*\* Offset = (\d+) ClusterId = (\d+) ProcId = (\d+) Owner = \"([^\"]+)\" CompletionDate = ([-]?\d+))");
static const std::regex epochHistoryRegex(
    R"(\*\*\*\s+(\w+)\s+ClusterId=(\d+)\s+ProcId=(\d+)\s+RunInstanceId=(\d+)\s+Owner=\"([^\"]+)\"\s+CurrentTime=(\d+))"
);


bool parseEpochHistoryBanner(const std::string &line, EpochRecord &out) {
    std::smatch m;
    if (std::regex_search(line, m, epochHistoryRegex)) {
        out.RecordType         = m[1];
        out.ClusterId    = std::stoi(m[2]);
        out.ProcId       = std::stoi(m[3]);
        out.RunInstanceId = std::stoi(m[4]);
        out.Owner        = m[5];
        out.CurrentTime  = std::stol(m[6]);
        return true;
    }
    return false;
}

bool parseBanner(BannerInfo &out, const std::string &line) {
    std::smatch m;
    if (std::regex_search(line, m, offsetRegex)) {
        out.offset = std::stol(m[1]);
        out.cluster = std::stoi(m[2]);
        out.proc = std::stoi(m[3]);
        out.owner = m[4];
        out.completionDate = std::stol(m[5]);
        return true;
    } else {
    std::cerr << "[WARN] Failed to match banner line: " << line << "\n";
    return false;
    }   
}

// Check whether line starts with *** to find offset banners
static inline bool starts_with(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

/*
* A helper function to populate a JobRecord struct and put it into the output vector
*/
void collectJob(
    const std::vector<std::string> &exprs, // right now unused because we're looking purely at the banner info
    const BannerInfo &banner,
    std::vector<JobRecord> &jobs,
    long fileId) {

    JobRecord job{};
    job.Offset = banner.offset;
    job.ClusterId = banner.cluster;
    job.ProcId = banner.proc;
    job.Owner = banner.owner;
    job.CompletionDate = banner.completionDate;
    job.FileId = fileId;

    jobs.emplace_back(std::move(job));
}


/**
 * A helper function to make sure we're looking at the file we expect
*/
bool verifyFileMatch(const char *historyFilePath, FileInfo &fileInfo) {
    auto [inodeMatch, hashMatch] = ArchiveMonitor::checkFileEquals(historyFilePath, fileInfo);

    if(!inodeMatch) printf("Inode doesn't match expected for file at %s \n", historyFilePath);
    if(!hashMatch) printf("Hash doesn't match expected for file at %s \n", historyFilePath);

    return inodeMatch && hashMatch;
}

/**
 * Reads new job ads from a history file incrementally (backward).
 * Stops when reaching the last processed offset.
 * 
 * @param historyFilePath Path to the history file.
 * @param out Vector to store parsed JobRecords.
 * @param fileInfo File metadata (last offset, FileId).
 * @return Last read offset or error code.
 */
long readHistoryIncremental(const char *historyFilePath, std::vector<JobRecord> &out, FileInfo &fileInfo) {
    BackwardFileReader reader(historyFilePath);
    if (reader.LastError()) {
        std::cerr << "Error opening history file: " << historyFilePath << "\n";
        return 0;
    }

    // Verify that filepath still points to a file w/ all the expected FileInfo
    if (!verifyFileMatch(historyFilePath, fileInfo)) {
        std::cerr << "File has changed or rotated.\n";
        return -1;
    }

    // Time to read!
    std::string chunk;
    std::string carry;
    std::vector<std::string> lines;
    bool done = false;
    long lastOffset = -1;
    long stopOffset = fileInfo.LastOffset;
    std::vector<std::string> exprs; // TODO: Will hold parsed info from the body of JobAd, right now its empty

    bool firstLineRead = false;  // Flag to check if it's the first line being read

    while (!done && reader.readChunk(chunk)) {
        chunk = chunk + carry;  // Append leftover from previous chunk
        carry.clear();

        // Split into lines
        std::size_t start = chunk.rfind('\n');
        std::size_t end = chunk.size();
        while (start != std::string::npos) {
            std::string line = chunk.substr(start + 1, end - start - 1);
            if (!line.empty())
                lines.push_back(line);
            end = start;
            start = (start == 0) ? std::string::npos : chunk.rfind('\n', start - 1);
        }

        // Whatever is left at the top of the chunk is a partial line
        if (end > 0) {
            carry = chunk.substr(0, end);
        }

        // Output the first line of the chunk in the first read to check whether file has updated
        if (!firstLineRead && !lines.empty()) {
                // Regex to extract the offset number
                std::smatch match;
                if (std::regex_search(lines.front(), match, offsetRegex)) {
                    lastOffset = std::stol(match[1].str());  // Store the offset number
                } else {
                    std::cerr << "[WARN] Failed to extract offset from the first line." << std::endl;
                    std::cerr << "Line is: " << lines.front() <<std::endl;
                }

                firstLineRead = true;  // Set the flag to prevent further checks 
        }

        if(lastOffset == stopOffset) {
            done = true; //We've already processed up to this point
            std::cerr << "No new entries have been added to this file." << std::endl;
            continue;
        }

        // Now process lines in reverse (bottom to top)
        for (auto it = lines.begin(); it != lines.end(); ++it) {
            const std::string &l = *it;

            if (starts_with(l.c_str(), "***")) {
                // No check for empty exprs bc right now we're just using banners
                BannerInfo banner{};
                if (parseBanner(banner, l)) {
                    if (stopOffset && banner.offset == stopOffset) {
                        done = true;
                    } else {
                        collectJob(exprs, banner, out, fileInfo.FileId);
                    }
                } else {
                    std::cerr << "[WARN] Failed to parse banner: " << l << "\n";
                }
                exprs.clear(); // Clear exprs after processing the banner
            } else {
                exprs.push_back(l); // Fill exprs with non-banner lines
            }

        }

        lines.clear();  // Clear lines for next chunk
    }
    reader.Close();
    return lastOffset;
}


/**
 * Reads new epoch records from the epoch history file incrementally (forward).
 * Finds the last banner line and reads from last offset to there.
 * Currently only records Spawn ads from the epoch file 
 *
 * @param historyFilePath Path to the epoch history file.
 * @param out Vector to store parsed EpochRecords.
 * @param fileInfo File metadata (last offset, FileId).
 * @return Last read offset or error code.
*/
long readEpochIncremental(const char *historyFilePath, std::vector<EpochRecord> &out, FileInfo &fileInfo) {
    std::ifstream file(historyFilePath, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "[ERROR] Failed to open file: " << historyFilePath << "\n";
        return -1;
    }

    if (!verifyFileMatch(historyFilePath, fileInfo)) {
        std::cerr << "[ERROR] File verification failed (rotated or modified).\n";
        return -1;
    }

    long fileSize = file.tellg();
    if (fileSize <= 0) {
        file.close();
        return 0;
    }

    // Step 1: Scan backward to find the last "***" banner
    long lastBannerOffset = -1;
    const long scanWindow = 4096;
    std::string buffer;
    std::string remainder;
    long cursor = fileSize;

    while (cursor > 0 && lastBannerOffset == -1) {
        long readSize = std::min(scanWindow, cursor);
        cursor -= readSize;
        file.seekg(cursor);
        std::string chunk(readSize, '\0');
        file.read(&chunk[0], readSize);
        chunk += remainder;

        std::istringstream stream(chunk);
        std::string line;
        long localOffset = cursor;

        while (std::getline(stream, line)) {
            localOffset += line.size() + 1;
            if (starts_with(line.c_str(), "***")) {
                lastBannerOffset = localOffset;
            }
        }

        remainder = chunk.substr(chunk.rfind('\n') + 1);
    }

    if (lastBannerOffset == -1 || lastBannerOffset <= fileInfo.LastOffset) {
        file.close();
        return fileInfo.LastOffset;
    }

    // Step 2: Forward parse from LastOffset to lastBannerOffset
    file.clear();
    file.seekg(fileInfo.LastOffset);
    long offset = fileInfo.LastOffset;
    std::string line;

    while (std::getline(file, line)) {
        long lineStart = offset;
        offset += line.size() + 1;

        if (lineStart >= lastBannerOffset) {
            break;
        }

        if (starts_with(line.c_str(), "*** SPAWN")) { // For now, only looks at Spawn ads from the epoch file
            std::istringstream iss(line);
            std::string stars, recordType;
            iss >> stars >> recordType;

            EpochRecord record;
            record.RecordType = recordType;  // EPOCH, SPAWN, etc.
            record.Offset = lineStart;
            record.FileId = fileInfo.FileId;

            if (parseEpochHistoryBanner(line, record)) {
                out.push_back(record);
            } else {
                std::cerr << "[WARNING] Failed to parse " << recordType << " banner at offset: " << lineStart << "\n";
            }
        }
    }

    file.close();
    std::cerr << "[HistoryReader] Done. Returning new offset: " << lastBannerOffset << "\n";
    return lastBannerOffset;
}

  