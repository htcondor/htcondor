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

namespace METRIC {
	enum COUNT {
		FAILURE = 0,
		SUCCESS,
		TOTAL,
	};
	enum TYPE {
		NORMAL = 0,
		SUBDAG,
		SERVICE,
	};
}

class DagmanMetrics {
public:
	/* Rule of five for abstract class*/
	DagmanMetrics() = default;
	DagmanMetrics(const DagmanMetrics&) = default;
	DagmanMetrics(DagmanMetrics&&) = default;
	DagmanMetrics& operator=(const DagmanMetrics&) = default;
	DagmanMetrics& operator=(DagmanMetrics&&) = default;
	virtual ~DagmanMetrics() = default;

	void SetParentDag(const int parentDAG) { if (parentDAG >= 0) _parentDagId = std::to_string(parentDAG); }
	void SetRescueNum(const int num) { _rescueDagNum = num; }
	void CountNodes(const Dag* dag);

	void NodeFinished(int type, bool success) { nodeCounts[type][success]++; }
	virtual bool Report(int exitCode, Dagman& dm) = 0;

protected:
	static double GetTime() { return condor_gettimestamp_double(); }
	void Init(const Dagman& dm);
	std::tuple<int, int> GetSums();

	std::string _metricsFile{};
	std::string _version{};
	std::string _dagmanId{};
	std::string _parentDagId{};

	double _startTime{0.0};

	// For each type (sub-array) count: {Failure, Success, Total}
	// Types: 0 - Normal Nodes, 1 - SubDAG Nodes, 2 - Service Nodes
	// NOTE: This does not follow the normal node types (JOB, PROVISIONER, FINAL, etc.)
	std::array<std::array<int, 3>, 3> nodeCounts{};

	int _rescueDagNum{0}; // The number of the rescue DAG we're running (0 if not running a rescue DAG).

	// Graph metrics
	int _graphNumEdges{0};
	int _graphNumVertices{0};
	bool sumServiceNodes{false};
};

// V1 metrics refers to nodes as jobs still
class DagmanMetricsV1 : public DagmanMetrics {
public:
	DagmanMetricsV1(const Dagman& dm) { Init(dm); }
	virtual bool Report(int exitCode, Dagman& dm);
};

// V2 appropraitely refers to nodes as nodes
class DagmanMetricsV2 : public DagmanMetrics {
public:
	DagmanMetricsV2(const Dagman& dm) { Init(dm); sumServiceNodes = true; }
	virtual bool Report(int exitCode, Dagman& dm);
};

#endif	// _DAGMAN_METRICS_H
