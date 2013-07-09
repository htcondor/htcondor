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

//TEMPTEMP -- explain the class here

#include "condor_common.h"
#include "MyString.h"
#include "dag.h"

class DagmanMetrics {
public:
		//TEMPTEMP -- document
	static void SetStartTime();

		/** Constructor.
		*/
	DagmanMetrics( /*const*/ Dag * dag, const char *primaryDagFile,
				int rescueDagNum );

	static void SetDagmanIds( const char *scheddAddr,
				const CondorID &DAGManJobId, int parentDagmanCluster );

		/** Destructor.
		*/
	~DagmanMetrics();

	void ProcStarted( const struct tm &eventTime );

	void ProcFinished( const struct tm &eventTime );

		//TEMPTEMP -- add count updates here
	void NodeFinished( bool isSubdag, bool successful );

		//TEMPTEMP -- document
	bool Report( int exitCode, Dag::dag_status status );

		//TEMPTEMP -- document
	bool WriteMetricsFile( int exitCode, Dag::dag_status status );

private:
		//TEMPTEMP -- document
	static double GetTime();

		//TEMPTEMP -- document
	static double GetTime( const struct tm &eventTime );

		//TEMPTEMP -- document
	static MyString GetVersion();

		//TEMPTEMP -- document
	void ParseBraindumpFile();

	static double _startTime;

	static MyString _dagmanId;
	static MyString _parentDagmanId;

		// Actually send metrics iff this is true.
	bool _sendMetrics;

	char *_primaryDagFile;
	int _rescueDagNum;

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

		// Total execute time of all node jobs.
	double _totalNodeJobTime;
};

#endif	// _DAGMAN_METRICS_H
