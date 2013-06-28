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

//TEMPTEMP -- ah, we have to pass in the dag file name here somewhere so we know what to call the metrics file
class DagmanMetrics {
public:
	static void SetStartTime();

	/** Constructor.
	*/
	//TEMPTEMP -- need to break out DAG stats from constructor (for timing)
	//TEMPTEMP -- or have a static method to do start time
	DagmanMetrics( /*const*/ Dag * dag, const char *primaryDagFile,
				int rescueDagNum );

	/** Destructor.
	*/
	~DagmanMetrics();

	//TEMPTEMP -- add count updates here
	void NodeFinished( bool isSubdag, bool successful );

	//TEMPTEMP -- need to pass exitcode, dag status
	bool Report( int exitCode, Dag::dag_status status );

	bool WriteMetricsFile( int exitCode, Dag::dag_status status );

private:
	static double GetTime();

	void ParseBraindumpFile();

	static double _startTime;

	bool _sendMetrics;

	int _rescueDagNum;
	MyString _metricsFile;

	MyString _workflowID;
	MyString _rootWorkflowID;
	MyString _plannerName;
	MyString _plannerVersion;

		// Node counts.
	int _simpleNodes;
	int _subdagNodes;
	int _simpleNodesSuccessful;
	int _simpleNodesFailed;
	int _subdagNodesSuccessful;
	int _subdagNodesFailed;

	double _totalNodeJobTime;
};

#endif	// _DAGMAN_METRICS_H
