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

#include "condor_common.h"
#include "dagman_metrics.h"
#include "debug.h"
#include "safe_fopen.h"
#include "condor_version.h"
#include "utc_time.h"
#include "dagman_main.h"

namespace conf = DagmanConfigOptions;

//---------------------------------------------------------------------------
DagmanMetrics::DagmanMetrics(const Dagman& dm) {
	_startTime = GetTime();

	_metricsFile = dm.options.primaryDag() + ".metrics";
	_dagmanId = std::to_string(dm.DAGManJobId._cluster);

	// Get the "main" part of the HTCondor version string (e.g. "8.1.0").
	const char* cv = CondorVersion();
	const char* ptr = cv;
	while (*ptr && !isdigit(*ptr)) { ++ptr; }
	while (*ptr && !isspace(*ptr)) { _version += *ptr; ++ptr; }

}

//---------------------------------------------------------------------------
void DagmanMetrics::CountNodes(const Dag* dag) {
	for (const auto& node : dag->_nodes) {
		_graphNumVertices++;
		_graphNumEdges += node->CountChildren();
		if (node->GetDagFile()) {
			_subdagNodes++;
		} else {
			_simpleNodes++;
		}
	}
}

//---------------------------------------------------------------------------
void
DagmanMetrics::NodeFinished( bool isSubdag, bool successful )
{
	if ( isSubdag ) {
		if ( successful ) {
			_subdagNodesSuccessful++;
		} else {
			_subdagNodesFailed++;
		}
	} else {
		if ( successful ) {
			_simpleNodesSuccessful++;
		} else {
			_simpleNodesFailed++;
		}
	}
}

//---------------------------------------------------------------------------
bool
DagmanMetrics::Report(int exitCode, Dagman& dm) {
	double endTime = GetTime();
	double duration = endTime - _startTime;
	DagStatus status = dm.dag->_dagStatus;

	FILE *fp = safe_fopen_wrapper_follow(_metricsFile.c_str(), "w");
	if ( ! fp) {
		debug_printf(DEBUG_QUIET, "Could not open %s for writing.\n", _metricsFile.c_str());
		return false;
	}

	fprintf( fp, "{\n" );
	fprintf( fp, "    \"client\":\"%s\",\n", "condor_dagman" );
	fprintf( fp, "    \"version\":\"%s\",\n", _version.c_str() );
	fprintf( fp, "    \"type\":\"metrics\",\n" );
	fprintf( fp, "    \"start_time\":%.3lf,\n", _startTime );
	fprintf( fp, "    \"end_time\":%.3lf,\n", endTime );
	fprintf( fp, "    \"duration\":%.3lf,\n", duration );
	fprintf( fp, "    \"exitcode\":%d,\n", exitCode );
	fprintf( fp, "    \"dagman_id\":\"%s\",\n", _dagmanId.c_str() );
	fprintf( fp, "    \"parent_dagman_id\":\"%s\",\n", _parentDagId.c_str() );
	fprintf( fp, "    \"rescue_dag_number\":%d,\n", _rescueDagNum );
	fprintf( fp, "    \"jobs\":%d,\n", _simpleNodes );
	fprintf( fp, "    \"jobs_failed\":%d,\n", _simpleNodesFailed );
	fprintf( fp, "    \"jobs_succeeded\":%d,\n", _simpleNodesSuccessful );
	fprintf( fp, "    \"dag_jobs\":%d,\n", _subdagNodes );
	fprintf( fp, "    \"dag_jobs_failed\":%d,\n", _subdagNodesFailed );
	fprintf( fp, "    \"dag_jobs_succeeded\":%d,\n", _subdagNodesSuccessful );
	fprintf( fp, "    \"total_jobs\":%d,\n", _simpleNodes + _subdagNodes );
	int totalNodesRun = _simpleNodesSuccessful + _simpleNodesFailed +
				_subdagNodesSuccessful + _subdagNodesFailed;
	fprintf( fp, "    \"total_jobs_run\":%d,\n", totalNodesRun );

	if (dm.config[conf::b::ReportGraphMetrics]) {
		// if we haven't alrady run the DFS cycle detection do that now
		// it has the side effect of determining the width and height of the graph
		int height, width;
		height = width = 0;
		if (status != DagStatus::DAG_STATUS_CYCLE) {
			if ( ! dm.dag->_graph_width) { dm.dag->isCycle(); }
			height = dm.dag->_graph_height;
			width = dm.dag->_graph_width;
		}

		fprintf( fp, "    \"graph_height\":%d,\n", height );
		fprintf( fp, "    \"graph_width\":%d,\n", width );
		fprintf( fp, "    \"graph_num_edges\":%d,\n", _graphNumEdges );
		fprintf( fp, "    \"graph_num_vertices\":%d,\n", _graphNumVertices );
	}

		// Last item must NOT have trailing comma!
	fprintf( fp, "    \"DagStatus\":%d\n", status );
	fprintf( fp, "}\n" );

	if (fclose(fp) != 0) {
		debug_printf(DEBUG_QUIET, "ERROR: closing metrics file %s; errno %d (%s)\n",
		             _metricsFile.c_str(), errno, strerror(errno));
	}

	debug_printf(DEBUG_NORMAL, "Wrote metrics file %s.\n", _metricsFile.c_str());

	return true;
}
