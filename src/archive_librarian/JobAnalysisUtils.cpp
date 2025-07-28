#include "JobQueryStructures.h"
#include "JobAnalysisUtils.h"

#include <cstdio>
#include <optional>
#include <unordered_map>
#include <sstream>

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

    CommandOptions parseArguments(int argc, char* argv[]) {
        CommandOptions options;
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "-user" && i + 1 < argc) {
                options.username = argv[++i];
            } else if (arg == "-clusterId" && i + 1 < argc) {
                options.clusterId = std::stoi(argv[++i]);
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
            } else if (arg == "cache" && i + 1 < argc) {
                std::string cacheCmd = argv[++i];
                if (cacheCmd == "stats") {
                    options.showCacheStats = true;
                } else if (cacheCmd == "clear") {
                    options.clearCache = true;
                }
            }
        }
        
        return options;
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

    void printAnalysisOutput(const std::vector<ParsedJobRecord>& jobs, 
                        const AnalysisFlags& flags,
                        const std::string& username, int clusterId) {
        // TODO: Implement unified table output with statistics
        printf("Analysis output for user %s, cluster %d (not yet implemented)\n", 
            username.c_str(), clusterId);
        printf("Found %zu jobs\n", jobs.size());
        
        if (flags.usage) {
            printf("Usage analysis requested\n");
        }
        if (flags.files) {
            printf("File analysis requested\n");
        }
        if (flags.batch) {
            printf("Batch analysis requested\n");
        }
        if (flags.dag) {
            printf("DAG analysis requested\n");
        }
        if (flags.when) {
            printf("Timing analysis requested\n");
        }
        if (flags.where) {
            printf("Location analysis requested\n");
        }
    }

    void printManual() {
        printf("Usage: myList -user <username> -clusterId <clusterid> [analysis_flags]\n");
        printf("Analysis flags: -usage -batch -dag -files -when -where\n");
        printf("Cache commands: myList cache stats | myList cache clear\n");
    }

    void printError(const std::string& errorMessage) {
        printf("Error: %s\n", errorMessage.c_str());
    }

    void printNoJobsFound(const std::string& username, int clusterId) {
        printf("No jobs found for user %s, cluster %d\n", 
            username.c_str(), clusterId);
    }

} 

