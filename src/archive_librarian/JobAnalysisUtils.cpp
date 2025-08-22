#include "JobQueryStructures.h"
#include "JobAnalysisUtils.h"

#include <cstdio>
#include <optional>
#include <unordered_map>
#include <sstream>
#include <unordered_map>
#include <set>
#include <map>
#include <numeric>

namespace JobAnalysisUtils {

    std::vector<ParsedJobRecord> parseClassAds(const std::vector<std::string>& rawClassAds) {
        std::vector<ParsedJobRecord> parsedJobs;
        int parseFailures = 0;
        
        for (const auto& classAdText : rawClassAds) {
            if (auto parsed = parseClassAd(classAdText)) {
                parsedJobs.push_back(*parsed);
            } else {
                parseFailures++;
            }
        }
        
        if (parseFailures > 0) {
            printf("Warning: Failed to parse %d ClassAd records\n", parseFailures);
        }
        
        printf("Successfully parsed %zu job records\n", parsedJobs.size());
        
        return parsedJobs;
    }

    std::optional<ParsedJobRecord> parseClassAd(const std::string& classAdText) {
        ParsedJobRecord record;
        record.raw_classad_text = classAdText;
        
        std::istringstream stream(classAdText);
        std::string line;
        
        // Parse key-value pairs
        std::unordered_map<std::string, std::string> fields;
        
        while (std::getline(stream, line)) {
            // Skip empty lines
            if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
                continue;
            }
            
            // Find first '=' to split key-value
            size_t equalPos = line.find('=');
            if (equalPos == std::string::npos) {
                continue; // Skip malformed lines
            }
            
            std::string key = line.substr(0, equalPos);
            std::string value = line.substr(equalPos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            // Remove surrounding quotes if present
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            
            fields[key] = value;
        }
        
        // Extract direct fields
        record.job_batch_id = fields["JobBatchId"];
        record.job_batch_name = fields["JobBatchName"];
        record.dagman_job_id = fields["DAGManJobId"];
        record.dag_node_name = fields["DAGNodeName"];
        record.job_submission_file = fields["JobSubmitFile"];
        record.job_status = fields["JobStatus"];
        record.last_remote_host = fields["LastRemoteHost"];
        record.out_file = fields["Out"];
        record.err_file = fields["Err"];
        record.user_log = fields["UserLog"];
        
        // Construct job_id from ClusterId.ProcId
        std::string clusterId = fields["ClusterId"];
        std::string procId = fields["ProcId"];
        if (!clusterId.empty() && !procId.empty()) {
            record.job_id = clusterId + "." + procId;
        }
        
        // Parse numeric fields with error handling
        if (!fields["ExitCode"].empty()) {
            char* endptr;
            long val = std::strtol(fields["ExitCode"].c_str(), &endptr, 10);
            if (*endptr == '\0') {
                record.exit_code = static_cast<int>(val);
            }
        }
        
        if (!fields["ExitStatus"].empty()) {
            char* endptr;
            long val = std::strtol(fields["ExitStatus"].c_str(), &endptr, 10);
            if (*endptr == '\0') {
                record.exit_status = static_cast<int>(val);
            }
        }
        
        if (!fields["JobStartDate"].empty()) {
            char* endptr;
            long val = std::strtol(fields["JobStartDate"].c_str(), &endptr, 10);
            if (*endptr == '\0') {
                record.job_start_date = val;
            }
        }
        
        // Compute memory usage from ResidentSetSize
        if (!fields["ResidentSetSize"].empty()) {
            char* endptr;
            long residentSetSize = std::strtol(fields["ResidentSetSize"].c_str(), &endptr, 10);
            if (*endptr == '\0' && residentSetSize >= 0) {
                record.memory_usage_mb = static_cast<double>((residentSetSize + 1023) / 1024);
            }
        }
        
        // Extract RemoteWallClockTime
        if (!fields["RemoteWallClockTime"].empty()) {
            char* endptr;
            double val = std::strtod(fields["RemoteWallClockTime"].c_str(), &endptr);
            if (*endptr == '\0') {
                record.remote_wall_clock_time = val;
            }
        }
        
        return record;
    }

    void parseArguments(int argc, char* argv[], CommandOptions& options) {
        fflush(stdout);
        
        // Basic safety check
        if (argv == nullptr) {
            printf("[PARSE] ERROR: argv is nullptr\n");
            return;
        }

        
        for (int i = 1; i < argc; ++i) {
            
            std::string arg(argv[i]);
            
            if (arg == "-user" && i + 1 < argc) {
                i++;
                options.username.assign(argv[i]);
            } else if (arg == "-clusterId" && i + 1 < argc) {
                i++;
                options.clusterId = std::stoi(argv[i]);
            } else if (arg == "-usage") {
                options.flags.usage = true;
            } else if (arg == "-batch") {
                options.flags.batch = true;
            } else if (arg == "-dag") {
                options.flags.dag = true;
            } else if (arg == "-files") {
                options.flags.files = true;
            } else if (arg == "-when") {
                options.flags.when = true;
            } else if (arg == "-where") {
                options.flags.where = true;
            } else if (arg == "-status") {
            options.flags.status = true;
            } else {
                printf("[PARSE] Unknown argument: '%s'\n", arg.c_str());
                fflush(stdout);
            }
        }
    }

    void printBaseOutput(const std::vector<ParsedJobRecord>& jobs, 
                        const std::string& username, int clusterId) {
        printf("Found %zu jobs for user %s, cluster %d:\n\n", 
            jobs.size(), username.c_str(), clusterId);
        
        for (const auto& job : jobs) {
            printf("Job %s:\n", job.job_id.c_str());
            printf("%s\n***\n\n", job.raw_classad_text.c_str());
        }
    }

    // Calculate statistics for a vector of numeric values
    AnalysisStats calculateStats(std::vector<double> values) {
        AnalysisStats stats;
        if (values.empty()) return stats;
        
        // Remove any invalid values (negative or zero where inappropriate)
        values.erase(std::remove_if(values.begin(), values.end(), 
                    [](double v) { return v < 0; }), values.end());
        
        if (values.empty()) return stats;
        
        stats.count = values.size();
        stats.total = std::accumulate(values.begin(), values.end(), 0.0);
        stats.mean = stats.total / stats.count;
        
        auto minmax = std::minmax_element(values.begin(), values.end());
        stats.min = *minmax.first;
        stats.max = *minmax.second;
        
        // Calculate median
        std::sort(values.begin(), values.end());
        size_t mid = values.size() / 2;
        if (values.size() % 2 == 0) {
            stats.median = (values[mid - 1] + values[mid]) / 2.0;
        } else {
            stats.median = values[mid];
        }
        
        return stats;
    }

    void printUsageAnalysis(const std::vector<ParsedJobRecord>& jobs, 
                           const std::string& username, int clusterId) {
        printf("\n\n=== Usage Analysis for user %s, cluster %d ===\n\n", 
               username.c_str(), clusterId);
        
        // Print table header
        printf("%-15s | %-15s | %-18s\n", "JobID", "Memory(MB)", "Wall Clock Time(s)");
        printf("%-15s-+-%-15s-+-%-18s\n", "---------------", "---------------", "------------------");
        
        std::vector<double> memoryValues, wallClockValues;
        
        // Print job data and collect values for statistics
        for (const auto& job : jobs) {
            printf("%-15s | %-15.1f | %-18.1f\n", 
                   job.job_id.c_str(), 
                   job.memory_usage_mb, 
                   job.remote_wall_clock_time);
            
            if (job.memory_usage_mb > 0) {
                memoryValues.push_back(job.memory_usage_mb);
            }
            if (job.remote_wall_clock_time > 0) {
                wallClockValues.push_back(job.remote_wall_clock_time);
            }
        }
        
        // Print statistics in table format
        printf("\n=== Usage Statistics ===\n");
        printf("%-18s | %-10s | %-10s | %-10s | %-10s | %-10s\n", 
               "Metric", "Min", "Max", "Mean", "Median", "Total");
        printf("%-18s-+-%-10s-+-%-10s-+-%-10s-+-%-10s-+-%-10s\n", 
               "------------------", "----------", "----------", "----------", "----------", "----------");
        
        if (!memoryValues.empty()) {
            auto memStats = calculateStats(memoryValues);
            printf("%-18s | %-10.1f | %-10.1f | %-10.1f | %-10.1f | %-10s\n",
                   "Memory (MB)", memStats.min, memStats.max, memStats.mean, memStats.median, "N/A");
        }
        
        if (!wallClockValues.empty()) {
            auto wallStats = calculateStats(wallClockValues);
            printf("%-18s | %-10.1f | %-10.1f | %-10.1f | %-10.1f | %-10.1f\n",
                   "Wall Clock (s)", wallStats.min, wallStats.max, wallStats.mean, wallStats.median, wallStats.total);
        }
        
        printf("\n");
    }

    void printFilesAnalysis(const std::vector<ParsedJobRecord>& jobs, 
                           const std::string& username, int clusterId) {
        printf("\n\n=== Files Analysis for user %s, cluster %d ===\n\n", 
               username.c_str(), clusterId);
        
        // Print table header
        printf("%-15s | %-30s | %-30s | %-30s | %-30s\n", 
               "JobID", "OutFile", "ErrFile", "UserLog", "SubmitFile");
        printf("%-15s-+-%-30s-+-%-30s-+-%-30s-+-%-30s\n", 
               "---------------", "------------------------------", "------------------------------", 
               "------------------------------", "------------------------------");
        
        // Print job data
        for (const auto& job : jobs) {
            printf("%-15s | %-30s | %-30s | %-30s | %-30s\n", 
                   job.job_id.c_str(),
                   job.out_file.empty() ? "N/A" : job.out_file.c_str(),
                   job.err_file.empty() ? "N/A" : job.err_file.c_str(),
                   job.user_log.empty() ? "N/A" : job.user_log.c_str(),
                   job.job_submission_file.empty() ? "N/A" : job.job_submission_file.c_str());
        }
        
        printf("\n");
    }

    void printBatchAnalysis(const std::vector<ParsedJobRecord>& jobs, 
                           const std::string& username, int clusterId) {
        printf("\n\n=== Batch Analysis for user %s, cluster %d ===\n\n", 
               username.c_str(), clusterId);
        
        printf("%-15s | %-25s | %-35s\n", "JobID", "BatchID", "BatchName");
        printf("%-15s-+-%-25s-+-%-35s\n", "---------------", "-------------------------", "-----------------------------------");
        
        for (const auto& job : jobs) {
            printf("%-15s | %-25s | %-35s\n", 
                   job.job_id.c_str(),
                   job.job_batch_id.empty() ? "N/A" : job.job_batch_id.c_str(),
                   job.job_batch_name.empty() ? "N/A" : job.job_batch_name.c_str());
        }
        
        printf("\n");
    }

    void printDagAnalysis(const std::vector<ParsedJobRecord>& jobs, 
                         const std::string& username, int clusterId) {
        printf("\n\n=== DAG Analysis for user %s, cluster %d ===\n\n", 
               username.c_str(), clusterId);
        
        printf("%-15s | %-25s | %-35s\n", "JobID", "DAGMan JobID", "DAG NodeName");
        printf("%-15s-+-%-25s-+-%-35s\n", "---------------", "-------------------------", "-----------------------------------");
        
        for (const auto& job : jobs) {
            printf("%-15s | %-25s | %-35s\n", 
                   job.job_id.c_str(),
                   job.dagman_job_id.empty() ? "N/A" : job.dagman_job_id.c_str(),
                   job.dag_node_name.empty() ? "N/A" : job.dag_node_name.c_str());
        }
        
        printf("\n");
    }

     void printWhenAnalysis(const std::vector<ParsedJobRecord>& jobs, 
                          const std::string& username, int clusterId) {
        printf("\n\n=== Timing Analysis for user %s, cluster %d ===\n\n", 
               username.c_str(), clusterId);
        
        printf("%-15s | %-20s\n", "JobID", "StartDate");
        printf("%-15s-+-%-20s\n", "---------------", "--------------------");
        
        std::vector<long> startDates;
        
        for (const auto& job : jobs) {
            // Convert timestamp to readable format
            std::string dateStr = "N/A";
            if (job.job_start_date > 0) {
                time_t timestamp = job.job_start_date;
                struct tm* timeinfo = localtime(&timestamp);
                char buffer[80];
                strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
                dateStr = buffer;
                startDates.push_back(job.job_start_date);
            }
            
            printf("%-15s | %-20s\n", job.job_id.c_str(), dateStr.c_str());
        }
        
        printf("\n=== Timing Statistics ===\n");
        if (!startDates.empty()) {
            auto minmax = std::minmax_element(startDates.begin(), startDates.end());
            time_t earliest = *minmax.first;
            time_t latest = *minmax.second;
            
            struct tm* earliestTm = localtime(&earliest);
            struct tm* latestTm = localtime(&latest);
            
            char earliestStr[80], latestStr[80];
            strftime(earliestStr, sizeof(earliestStr), "%Y-%m-%d %H:%M:%S", earliestTm);
            strftime(latestStr, sizeof(latestStr), "%Y-%m-%d %H:%M:%S", latestTm);
            
            printf("Earliest Start: %s\n", earliestStr);
            printf("Latest Start: %s\n", latestStr);
            printf("Time Span: %ld seconds (%.1f hours)\n", 
                   latest - earliest, (latest - earliest) / 3600.0);
        }
        
        printf("\n");
    }

    void printWhereAnalysis(const std::vector<ParsedJobRecord>& jobs, 
                           const std::string& username, int clusterId) {
        printf("\n\n=== Location Analysis for user %s, cluster %d ===\n\n", 
               username.c_str(), clusterId);
        
        printf("%-15s | %-50s\n", "JobID", "LastRemoteHost");
        printf("%-15s-+-%-50s\n", "---------------", "--------------------------------------------------");
        
        std::map<std::string, int> hostCounts;
        
        for (const auto& job : jobs) {
            std::string host = job.last_remote_host.empty() ? "N/A" : job.last_remote_host;
            printf("%-15s | %-50s\n", job.job_id.c_str(), host.c_str());
            
            if (!job.last_remote_host.empty()) {
                hostCounts[job.last_remote_host]++;
            }
        }
        
        printf("\n=== Location Statistics ===\n");
        printf("Unique Hosts: %zu\n", hostCounts.size());
        if (!hostCounts.empty()) {
            printf("Host Distribution:\n");
            for (const auto& [host, count] : hostCounts) {
                printf("  %s: %d jobs\n", host.c_str(), count);
            }
        }
        
        printf("\n");
    }

    void printStatusAnalysis(const std::vector<ParsedJobRecord>& jobs, 
                            const std::string& username, int clusterId) {
        printf("\n\n=== Status Analysis for user %s, cluster %d ===\n\n", 
               username.c_str(), clusterId);
        
        printf("%-15s | %-12s | %-12s | %-12s\n", "JobID", "Status", "ExitCode", "ExitStatus");
        printf("%-15s-+-%-12s-+-%-12s-+-%-12s\n", "---------------", "------------", "------------", "------------");
        
        std::map<std::string, int> statusCounts;
        std::map<int, int> exitCodeCounts;
        int totalJobs = jobs.size();
        
        for (const auto& job : jobs) {
            printf("%-15s | %-12s | %-12d | %-12d\n", 
                   job.job_id.c_str(),
                   job.job_status.empty() ? "N/A" : job.job_status.c_str(),
                   job.exit_code,
                   job.exit_status);
            
            if (!job.job_status.empty()) {
                statusCounts[job.job_status]++;
            }
            exitCodeCounts[job.exit_code]++;
        }
        
        printf("\n=== Status Statistics ===\n");
        
        if (!statusCounts.empty()) {
            printf("\nJob Status Distribution:\n");
            printf("%-15s | %-8s | %-10s\n", "Status", "Count", "Percentage");
            printf("%-15s-+-%-8s-+-%-10s\n", "---------------", "--------", "----------");
            
            for (const auto& [status, count] : statusCounts) {
                double percentage = (static_cast<double>(count) / totalJobs) * 100.0;
                printf("%-15s | %-8d | %-9.1f%%\n", status.c_str(), count, percentage);
            }
        }
        
        if (!exitCodeCounts.empty()) {
            printf("\nExit Code Distribution:\n");
            printf("%-10s | %-8s | %-10s\n", "ExitCode", "Count", "Percentage");
            printf("%-10s-+-%-8s-+-%-10s\n", "----------", "--------", "----------");
            
            for (const auto& [code, count] : exitCodeCounts) {
                double percentage = (static_cast<double>(count) / totalJobs) * 100.0;
                printf("%-10d | %-8d | %-9.1f%%\n", code, count, percentage);
            }
        }
        
        printf("\n");
    }

    void printAnalysisOutput(const std::vector<ParsedJobRecord>& jobs, 
                            const AnalysisFlags& flags,
                            const std::string& username, int clusterId) {
        
        // Print each requested analysis section
        if (flags.usage) {
            printUsageAnalysis(jobs, username, clusterId);
        }
        
        if (flags.files) {
            printFilesAnalysis(jobs, username, clusterId);
        }
        
        if (flags.batch) {
            printBatchAnalysis(jobs, username, clusterId);
        }
        
        if (flags.dag) {
            printDagAnalysis(jobs, username, clusterId);
        }
        
        if (flags.when) {
            printWhenAnalysis(jobs, username, clusterId);
        }
        
        if (flags.where) {
            printWhereAnalysis(jobs, username, clusterId);
        }
        
        if (flags.status) {
            printStatusAnalysis(jobs, username, clusterId);
        }
    }


    void printManual() {
        printf("Usage: myHistoryList -user <username> -clusterId <clusterid> [analysis_flags]\n");
        printf("Analysis flags: -status -usage -batch -dag -files -when -where\n");
    }

    void printError(const std::string& errorMessage) {
        printf("Error: %s\n", errorMessage.c_str());
    }

    void printNoJobsFound(const std::string& username, int clusterId) {
        printf("No jobs found for user %s, cluster %d\n", 
            username.c_str(), clusterId);
    }

} 

