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


#ifndef _DAGMAN_METRICS_H
#define _DAGMAN_METRICS_H

// This class records various metrics related to a DAGMan run, and
// optionally reports them to the Pegasus metrics server (see gittrac
// #3532).

#include "condor_common.h"
#include "dag.h"

class DagmanMetrics {
public:
	DagmanMetrics(const Dagman& dm);
	~DagmanMetrics() = default;

	void SetParentDag(const int parentDAG) { if (parentDAG >= 0) _parentDagId = std::to_string(parentDAG); }
	void SetRescueNum(const int num) { _rescueDagNum = num; }
	void CountNodes(const Dag* dag);

	void NodeFinished(bool isSubdag, bool successful);
	bool Report(int exitCode, Dagman& dm);

private:
	static double GetTime() { return condor_gettimestamp_double(); }

	std::string _metricsFile{};
	std::string _version{};
	std::string _dagmanId{};
	std::string _parentDagId{};

	double _startTime{0.0};

	// Node counts.
	int _simpleNodes{0};
	int _subdagNodes{0};
	int _simpleNodesSuccessful{0};
	int _simpleNodesFailed{0};
	int _subdagNodesSuccessful{0};
	int _subdagNodesFailed{0};

	int _rescueDagNum{0}; // The number of the rescue DAG we're running (0 if not running a rescue DAG).

	// Graph metrics
	int _graphNumEdges{0};
	int _graphNumVertices{0};
};

#endif	// _DAGMAN_METRICS_H
