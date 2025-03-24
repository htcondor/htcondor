/***************************************************************
 *
 * Copyright (C) 1990-2024, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef DAGMAN_MAIN_H
#define DAGMAN_MAIN_H

#include "dag.h"
#include "dagman_classad.h"
#include "dagman_stats.hpp"
#include "dagman_metrics.h"
#include "utc_time.h"
#include "../condor_utils/dagman_utils.h"

extern DagmanUtils dagmanUtils;

// Don't change these values!  Doing so would break some DAGs.
enum exit_value {
	EXIT_OKAY = 0,
	EXIT_ERROR = 1,
	EXIT_ABORT = 2,     // condor_rm'ed or hit special abort DAG exit code
	EXIT_RESTART = 3,   // exit but indicate that we should be restarted
};

const int MAX_SUBMITS_PER_INT_DEFAULT = 100;
const int LOG_SCAN_INT_DEFAULT = 5;

void main_shutdown_rescue(int exitVal, DagStatus dagStatus, bool removeCondorJobs = true);
void main_shutdown_graceful(void);
void main_shutdown_logerror(void);
void print_status(bool forceScheddUpdate = false);

namespace DagmanConfigOptions {
	enum class b { // DAGMan boolean config options
		DepthFirst = 0,                // Submit DAG depth first as opposed to breadth first
		DetectCycle,                   // Peform expensive cycle-detection at startup
		UseJoinNodes,                  // Use join nodes to reduce dependency counts
		AggressiveSubmit,              // Override timer interval allowing submit cycle to keep submitting until no more available jobs or max_submit_attempts exceeded
		RetrySubmitFirst,              // Retry a node that failed job submission before other nodes in ready queue
		RetryNodeFirst,                // Retry a failed node with retries before other nodes in the ready queue
		MungeNodeNames,                // Munge node names for multi-DAG runs to make unique node names
		AllowIllegalChars,             // Allow Node names to contain illegal characters
		PartialRescue,                 // Write partial rescue DAG
		RescueResetRetry,              // Reset Node Retries when writing rescue file
		GenerateSubdagSubmit,          // Generate the *.condor.sub file for sub-DAGs at run time
		RemoveTempSubFiles,            // Remove temporary inline desc node submit files created for shell condor_submit
		RemoveJobs,                    // DAGMan will condor_rm all submitted jobs when removed itself
		HoldFailedJobs,                // Put failed jobs on hold
		AbortDuplicates,               // Abort duplicates of DAGMan running the same DAG at the same time
		AbortOnScarySubmit,            // Abort on submit event with HTCondor ID that doesn't match expected value
		AppendVars,                    // Determine if VARS are naturally appended or prepended to job submit descriptions
		JobInsertRetry,                // Insert Node retry value to job ad at submission time
		SuppressJobLogs,               // Suppress specified job log files (see gittrac #4353)
		EnforceNewJobLimits,           // Have DAG enforce the a newly set MaxJobs limit by removing node batch jobs
		ProduceJobCreds,               // Have DAGMan direct submit run produce_credentials
		ProhibitMultiJobs,             // Prohibit nodes that queue more than 1 job
		NfsLogError,                   // Error if nodes log is on NFS
		CacheDebug,                    // Cache DAGMan debugging
		ReportGraphMetrics,            // Report DAG metrics (hight, width, etc)
		_SIZE // MUST BE FINAL ITEM
	};

	enum class i { // DAGMan integer config options
		SubmitDelay = 0,               // Seconds delay between consecutive job submissions
		SubmitsPerInterval,            // Max number of job submits per cycle
		MaxSubmitAttempts,             // Max number of job submit attempts before giving up
		LogScanInterval,               // Interval of time between checking for new log events
		PendingReportInverval,         // Time interval to report pending nodes
		VerifyScheddInterval,          // Time in pending state before querying the schedd queue for verification
		MaxRescueNum,                  // Maximum rescue DAG number
		MaxJobHolds,                   // Maximum number of holds a node job can have before being declared failed; 0 = infinite
		HoldClaimTime,                 // Time schedd should hold match claim see submit command keep_claim_idle
		AllowEvents,                   // What BAD job events to not treat as fatal
		DebugCacheSize,                // Size of debug cache prior to writing messages
		MetricsVersion,                // DAGMan metrics file version (1 or 2)
		JobStateTableInterval,         // Seconds delay between outputting Job State Table to debug log (0 disables printing)
		_SIZE // MUST BE FINAL ITEM
	};

	enum class dbl { // DAGMan double config options
		ScheddUpdateInterval = 0,      // Time interval between DAGMan job Ad updates to/from Schedd
		_SIZE // MUST BE FINAL ITEM
	};

	enum class str { // DAGMan string config options
		SubmitExe = 0,                  // Path to condor_submit executable
		RemoveExe,                      // Path to condor_rm executable
		InheritAttrsPrefix,             // String prefix to add to all inherited job attrs
		NodesLog,                       // Shared *.nodes.log file
		MachineAttrs,                   // Comma separated list of machine attrs to add to Job Ad
		UlogMachineAttrs,               // Comma separated list of machine attrs to add to the nodes.log
		DagConfig,                      // User specified DAGMan config file (pulled from config ironically)
		_SIZE // MUST BE FINAL ITEM
	};
}

class DagmanConfig {
public:
	DagmanConfig() {
		using namespace DagmanConfigOptions;

		boolOpts[static_cast<size_t>(b::RetrySubmitFirst)] = true;
		boolOpts[static_cast<size_t>(b::MungeNodeNames)] = true;
		boolOpts[static_cast<size_t>(b::AbortDuplicates)] = true;
		boolOpts[static_cast<size_t>(b::AbortOnScarySubmit)] = true;
		boolOpts[static_cast<size_t>(b::PartialRescue)] = true;
		boolOpts[static_cast<size_t>(b::RescueResetRetry)] = true;
		boolOpts[static_cast<size_t>(b::GenerateSubdagSubmit)] = true;
		boolOpts[static_cast<size_t>(b::RemoveJobs)] = true;
		boolOpts[static_cast<size_t>(b::ProduceJobCreds)] = true;
		boolOpts[static_cast<size_t>(b::UseJoinNodes)] = true;
		boolOpts[static_cast<size_t>(b::RemoveTempSubFiles)] = true;

		intOpts[static_cast<size_t>(i::MaxSubmitAttempts)] = 6;
		intOpts[static_cast<size_t>(i::SubmitsPerInterval)] = MAX_SUBMITS_PER_INT_DEFAULT;
		intOpts[static_cast<size_t>(i::LogScanInterval)] = LOG_SCAN_INT_DEFAULT;
		intOpts[static_cast<size_t>(i::AllowEvents)] = CheckEvents::ALLOW_NONE;
		intOpts[static_cast<size_t>(i::VerifyScheddInterval)] = 28'800; // 8 hours (in seconds)
		intOpts[static_cast<size_t>(i::PendingReportInverval)] = 600;
		intOpts[static_cast<size_t>(i::JobStateTableInterval)] = 900; // 15 min
		intOpts[static_cast<size_t>(i::MaxRescueNum)] = MAX_RESCUE_DAG_DEFAULT;
		intOpts[static_cast<size_t>(i::MaxJobHolds)] = 100;
		intOpts[static_cast<size_t>(i::HoldClaimTime)] = 20;
		intOpts[static_cast<size_t>(i::DebugCacheSize)] = (1024 * 1024) * 5; // 5MB
		intOpts[static_cast<size_t>(i::MetricsVersion)] = 2;

		doubleOpts[static_cast<size_t>(dbl::ScheddUpdateInterval)] = 120.0;
	}

	bool operator[](DagmanConfigOptions::b opt) const { return boolOpts[static_cast<size_t>(opt)]; }
	bool& operator[](DagmanConfigOptions::b opt) { return boolOpts[static_cast<size_t>(opt)]; }

	int operator[](DagmanConfigOptions::i opt) const { return intOpts[static_cast<size_t>(opt)]; }
	int& operator[](DagmanConfigOptions::i opt) { return intOpts[static_cast<size_t>(opt)]; }

	double operator[](DagmanConfigOptions::dbl opt) const { return doubleOpts[static_cast<size_t>(opt)]; }
	double& operator[](DagmanConfigOptions::dbl opt) { return doubleOpts[static_cast<size_t>(opt)]; }

	std::string& operator[](DagmanConfigOptions::str opt) { return strOpts[static_cast<size_t>(opt)]; }
	const std::string& operator[](DagmanConfigOptions::str opt) const { return strOpts[static_cast<size_t>(opt)]; }

private:
	std::array<bool, static_cast<size_t>(DagmanConfigOptions::b::_SIZE)> boolOpts;
	std::array<int, static_cast<size_t>(DagmanConfigOptions::i::_SIZE)> intOpts;
	std::array<double, static_cast<size_t>(DagmanConfigOptions::dbl::_SIZE)> doubleOpts;
	std::array<std::string, static_cast<size_t>(DagmanConfigOptions::str::_SIZE)> strOpts;
};


class Dagman {
public:
	Dagman() = default;
	~Dagman() { CleanUp(); }

	inline void CleanUp() {
		// CleanUp() gets invoked multiple times, so check for null objects
		if (dag) {
			delete dag; 
			dag = nullptr;
		}
		if (_dagmanClassad) {
			delete _dagmanClassad;
			_dagmanClassad = nullptr;
		}
		if (_schedd) {
			delete _schedd;
			_schedd = nullptr;
		}
		if (_protectedUrlMap) {
			delete _protectedUrlMap;
			_protectedUrlMap = nullptr;
		}
		if (metrics) {
			delete metrics;
			metrics = nullptr;
		}
	}

	void ResolveDefaultLog(); // Resolve macro substitutions in nodes.log and verify NFS logging
	void PublishStats(); // Publish statistics to debug file.
	void UpdateAd() { if (_dagmanClassad) _dagmanClassad->Update(*this); }; // Two way info update from DAGMan job Ad and DAGMan
	void CreateMetrics() {
		using namespace DagmanConfigOptions;
		switch(config[i::MetricsVersion]) {
			case 1:
				metrics = new DagmanMetricsV1(*this);
				break;
			case 2:
				metrics = new DagmanMetricsV2(*this);
				break;
			default:
				EXCEPT("ERROR: Unsupported metrics file version: %d",
				       config[i::MetricsVersion]);
		}

		if ( ! metrics) { EXCEPT("ERROR: out of memory!"); }
	}
	void ReportMetrics(const int exitCode);
	void LocateSchedd();
	bool Config();
	void RemoveRunningJobs(const std::string& reason = "Removed by DAGMan", const bool rm_all = false);

	Dag *dag{nullptr};
	DCSchedd *_schedd{nullptr};
	MapFile *_protectedUrlMap{nullptr}; // Protected URL Mapfile
	DagmanClassad *_dagmanClassad{nullptr};
	DagmanMetrics *metrics{nullptr};

	DagmanOptions options{}; // All DAGMan options also set by config for this DAGMan to utilize
	DagmanOptions inheritOpts{}; // Only Command Line options for passing down to subdags
	DagmanConfig config{}; // DAGMan configuration values
	DagmanStats stats{}; // DAGMan Statistics
	CondorID DAGManJobId{}; // The HTCondor job id of the DAGMan job

	std::map<std::string, std::string> inheritAttrs{}; // Map of Attr->Expr of DAG job ad attrs to pass to all jobs

	std::string workingDir{}; // Directory in which DAGMan was invoked. Recoreded incase daemoncore hijacks
	std::string rescueFileToRun{}; // Name of rescue DAG being run. Will remain "" if not in rescue mode
	std::string commandSecret{}; // Secret provided by parent (i.e. Schedd) to verify incoming command is authorized

	bool paused{false}; // DAG is paused
	bool update_ad{false}; // DAGMan needs to update some state advertised in ClassAd

	static strict_level_t _strict;
};

#endif	// ifndef DAGMAN_MAIN_H
