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
#include "config.hpp"

extern DagmanUtils dagmanUtils;

// Don't change these values!  Doing so would break some DAGs.
enum exit_value {
	EXIT_OKAY = 0,
	EXIT_ERROR = 1,
	EXIT_ABORT = 2,     // condor_rm'ed or hit special abort DAG exit code
	EXIT_RESTART = 3,   // exit but indicate that we should be restarted
};

void main_shutdown_rescue(int exitVal, DagStatus dagStatus, bool removeCondorJobs = true);
void main_shutdown_graceful(void);
void main_shutdown_logerror(void);
void print_status(bool forceScheddUpdate = false);


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

		ASSERT(metrics);
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
