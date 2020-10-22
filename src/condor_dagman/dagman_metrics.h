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
#include "MyString.h"
#include "dag.h"

#include <unordered_map>

using namespace std;

class DagmanMetrics {
public:
		/** Sets the start time for the metrics.  This is not in the
			constructor because the Pegasus people want the start time
			to be as early as possible (e.g., before the DAG file is
			parsed).
		*/
	static void SetStartTime();

		/** Constructor.
		*/
	DagmanMetrics( /*const*/ Dag * dag, const char *primaryDagFile,
				int rescueDagNum );

		/** Set the DAGMan ID and parent DAGMan ID for this run.
			@param DAGManJobId The HTCondor ID of this DAGMan.
			@param parentDagmanCluster The cluster ID of the parent of
					this DAGMan (if this is not a sub-DAG,
					parentDagmanCluster should be 0).
		*/
	static void SetDagmanIds( const CondorID &DAGManJobId,
				int parentDagmanCluster );

		/** Destructor.
		*/
	~DagmanMetrics();

	void ProcStarted( const struct tm &eventTime );

	void ProcFinished( const struct tm &eventTime );

		/** Add information for a finished node to the metrics.
			@param isSubdag True iff the node is a sub-DAG.
			@param successful True iff the node was successful.
		*/
	void NodeFinished( bool isSubdag, bool successful );

		/** Report the metrics to the Pegasus metrics server(s), assuming
			that reporting is enabled.
			@param exitCode The exit code of this DAGMan.
			@param status The status of this DAGMan (see DagStatus in
					dag.h).
		 	@return false if an error occurred, true otherwise (note
				that a return value of true does not necessarily
				meant that metrics were reported; for example, if
				metrics reporting is disabled, metrics are not reported
				but the return value is true)
		*/
	bool Report( int exitCode, DagStatus status );

		/** Calls other functions which measure some graph metrics. Assumes the
			DAG is valid and does not contain any cycles.
		*/
	void GatherGraphMetrics(  Dag* dag  );

		/** Write the metrics file.
			@param exitCode The exit code of this DAGMan.
			@param status The status of this DAGMan (see DagStatus in
					dag.h).
			@return true if writing succeeded, false otherwise
		*/
	bool WriteMetricsFile( int exitCode, DagStatus status );

private:
		/** Get the current time, in seconds (and fractional seconds)
			since the epoch.
			@return Seconds since the epoch.
		*/
	static double GetTime();

		/** Get the time, in seconds (and fractional seconds) since the
			epoch, represented by eventTime.
			@param eventTime A struct tm.
			@return Seconds since the epoch.
		*/
	static double GetTime( const struct tm &eventTime );

		/** Get the "main" part of the HTCondor version string (e.g.,
			"8.1.0").
			@return The main part of the HTCondor version.
		*/
	static MyString GetVersion();

#ifdef DEAD_CODE
	/** Returns the height of the graph, ie. the longest possible route
			 from root node to leaf node.
		*/
	static int GetGraphHeight( Dag* dag );
	static int GetGraphHeightRecursive( Job* node, Dag* dag, unordered_map<string, bool>* visited );

		/** Returns the width of the graph, ie. the largest number of siblings
			 that occurs for any given node.
		*/
	static int GetGraphWidth( Dag* dag );
#endif

		// The time at which this DAGMan run started, in seconds since
		// the epoch.
	static double _startTime;

		// The IDs (in the form to be reported in the metrics) of this
		// DAGMan, and it's parent DAGMan (if there is one).
	static std::string _dagmanId;
	static std::string _parentDagmanId;

		// The name of the primary DAG file.
	char *_primaryDagFile;

		// Pointer to the DAG we're running 
	static Dag* _dag;

		// The number of the rescue DAG we're running (0 if not running
		// a rescue DAG).
	int _rescueDagNum;

		// The name of the metrics file we're going to write.
	MyString _metricsFile;

		// Pegasus information.
	MyString _workflowId;
	MyString _rootWorkflowId;
	MyString _plannerName;
	MyString _plannerVersion;

		// Node counts.
	int _simpleNodes;
	int _subdagNodes;
	int _simpleNodesSuccessful;
	int _simpleNodesFailed;
	int _subdagNodesSuccessful;
	int _subdagNodesFailed;

		// Graph metrics
	int _graphHeight;
	int _graphWidth;
	int _graphNumEdges;
	int _graphNumVertices;
};

#endif	// _DAGMAN_METRICS_H
