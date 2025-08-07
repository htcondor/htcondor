#pragma once

#include <string>
#include <vector>

/**
 * @file JobQueryStructures.h
 * @brief Data structures for CLI job query and analysis operations
 * 
 * This file contains all the data structures used by the CLI tool for querying
 * and analyzing HTCondor job data. Separated from Librarian to keep ingestion
 * and query concerns distinct.
 */



/**
 * @struct ParsedJobRecord
 * @brief Fully parsed and processed job record ready for analysis and display
 * 
 * Contains both direct fields extracted from ClassAd format and computed fields
 * derived from raw ClassAd data. Used as the final output of the parsing pipeline.
 */
struct ParsedJobRecord {
    // Direct fields from ClassAd - no computation needed
    std::string job_id;                 // ClusterId.ProcId (e.g., "4327172.13")
    std::string job_batch_id;           // JobBatchId - id for job's batch (analysis flag: -batch)
    std::string job_batch_name;         // JobBatchName - name of job's batch (analysis flag: -batch)
    std::string dagman_job_id;          // DAGManJobId - id for job's DAGMan (analysis flag: -dag)
    std::string dag_node_name;          // DAGNodeName - name for job's DAGMan (analysis flag: -dag)
    std::string job_submission_file;    // JobSubmissionFile - file used to submit job (analysis flag: -files)
    std::string job_status;             // JobStatus - whether job was completed, held, removed, etc (analysis flag: -status)
    std::string last_remote_host;       // LastRemoteHost - slot of machine where job last executed (analysis flag: -where)
    std::string out_file;               // Out - output from the job (analysis flag: -files)
    std::string err_file;               // Err - error file for the job (analysis flag: -files)
    std::string user_log;               // UserLog - log file for the job (analysis flag: -files)
    int exit_code = 0;                  // ExitCode - how the code itself exited (analysis flag: -status)
    int exit_status = 0;                // ExitStatus - how condor thinks the job went (analysis flag: -status)
    long job_start_date = 0;            // JobStartDate - date job was started (analysis flag: -when)
    
    // Computed fields - derived from raw ClassAd data
    double memory_usage_mb = 0.0;       // Computed from ResidentSetSize - how much RAM was used (analysis flag: -usage)
    double remote_wall_clock_time = 0.0; // RemoteWallClockTime - how long job actually ran on machine (analysis flag: -usage)
    
    // Raw classad for base command (when no analysis flags are specified)
    std::string raw_classad_text;       // Complete unparsed ClassAd text from archive file
};



/**
 * @struct QueryResult
 * @brief Result container for database query operations
 * 
 * Encapsulates the success/failure status of database queries along with
 * either the retrieved job records or an error message. Enables graceful
 * error handling without exceptions.
 */
struct QueryResult {
    bool success;                       // True if query succeeded, false on any error
    std::string errorMessage;           // Human-readable error description (empty on success)
    std::vector<ParsedJobRecord> jobs; // Retrieved job records (empty on failure)
};



/**
 * @struct QueriedJobRecord  
 * @brief Raw job record data retrieved from database queries
 * 
 * Contains the minimal information needed to locate and read a job's ClassAd
 * from archive files. This is the intermediate format between database query
 * results and file reading operations.
 */
struct QueriedJobRecord {
    long offset;                        // Byte offset in archive file where job's ClassAd begins
    long fileId;                        // Database ID referencing the Files table
    std::string fileName;               // Archive file name (relative path)
    long fileInode;                     // File system inode for verification
    std::string fileHash;               // File hash for integrity checking
    int completionDate;                 // Job completion timestamp
};


// Structure to hold analysis statistics if analysis flags are set
 struct AnalysisStats {
    double min = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double median = 0.0;
    size_t count = 0;
    double total = 0.0;  // For summing up times, etc.
};
