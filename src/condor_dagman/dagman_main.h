/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#include "dagman_stats.h"
#include "utc_time.h"
#include "../condor_utils/dagman_utils.h"

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
void jobad_update();

class Dagman {
public:
	Dagman();
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
	}

	// Resolve macro substitutions in _defaultNodeLog.  Also check
	// for some errors/warnings.
	void ResolveDefaultLog();
	// Publish statistics to a log file.
	void PublishStats();
	void LocateSchedd();
	bool Config();

	Dag *dag{nullptr};
	DCSchedd *_schedd{nullptr};
	MapFile *_protectedUrlMap{nullptr}; // Protected URL Mapfile
	DagmanClassad *_dagmanClassad{nullptr};

	DagmanOptions options{}; // All DAGMan options also set by config for this DAGMan to utilize
	DagmanOptions inheritOpts{}; // Only Command Line options for passing down to subdags
	DagmanStats _dagmanStats{}; // DAGMan Statistics
	CondorID DAGManJobId{}; // The HTCondor job id of the DAGMan job

	std::string workingDir{}; // Directory in which DAGMan was invoked. Recoreded incase daemoncore hijacks
	std::string rescueFileToRun{}; // Name of rescue DAG being run. Will remain "" if not in rescue mode
	std::string _defaultNodeLog{}; // Shared node log file that DAGMan reads
	std::string _requestedMachineAttrs{}; // Comma separated list of machine attrs to add to Job Ad
	std::string _ulogMachineAttrs{}; // Comma separated list of machine attrs to add to user log
	std::string condorSubmitExe{}; // path to condor_submit executable
	std::string condorRmExe{}; // path to condor_rm executable
	std::string _dagmanConfigFile{}; // The DAGMan configuration file (NULL if none is specified).

	int submit_delay{0}; // Seconds delay between consecutive job submissions
	int max_submit_attempts{6}; // Max number of job submit attempts before giving up
	int max_submits_per_interval{MAX_SUBMITS_PER_INT_DEFAULT}; // Max number of job submits per cycle

	int allow_events{CheckEvents::ALLOW_NONE}; // What BAD job events to not treat as fatal

	int m_user_log_scan_interval{LOG_SCAN_INT_DEFAULT}; // Interval of time between checking for new log events
	int schedd_update_interval{120}; // Time interval between DAGMan job Ad updates to/from Schedd
	int pendingReportInterval{600}; // Time interval to report pending nodes
	int check_queue_interval{28'800}; // Time in pending state before querying the schedd queue for verification

	int maxRescueDagNum{MAX_RESCUE_DAG_DEFAULT}; // Maximum rescue DAG number
	int _maxJobHolds{100}; // Maximum number of holds a node job can have before being declared failed; 0 = infinite
	int _claim_hold_time{20};

	static strict_level_t _strict;

	bool paused{false}; // DAG is paused
	bool aggressive_submit{false}; // Override timer interval allowing submit cycle to keep submitting until no more available jobs or max_submit_attempts exceeded
	bool startup_cycle_detect{false}; // peform expensive cycle-detection at startup
	bool retrySubmitFirst{true}; // Retry a node that failed job submission before other nodes in ready queue
	bool retryNodeFirst{false}; // Retry a failed node with retries before other nodes in the ready queue
	bool mungeNodeNames{true}; // Munge node names for multi-DAG runs to make unique node names
	bool prohibitMultiJobs{false}; // Prohibit nodes that queue more than 1 job
	bool abortDuplicates{true}; // Abort duplicates of DAGMan running the same DAG at the same time
	bool submitDepthFirst{false}; // Submit DAG depth first as opposed to breadth first
	bool abortOnScarySubmit{true}; // Abort on submit event with HTCondor ID that doesn't match expected value
	bool doAppendVars{false}; // Determine if VARS are naturally appended or prepended to job submit descriptions
	bool jobInsertRetry{false}; // Insert Node retry value to job ad at submission time
	bool _writePartialRescueDag{true}; // Write partial rescue DAG
	bool _generateSubdagSubmits{true}; // Generate the *.condor.sub file for sub-DAGs at run time
	bool _suppressJobLogs{false}; // Suppress specified job log files (see gittrac #4353)
	bool _removeNodeJobs{true}; // DAGMan itself will remove managed node jobs when condor_rm'ed
};

#endif	// ifndef DAGMAN_MAIN_H
