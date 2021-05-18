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

#include "condor_common.h"
#include "dagman_metrics.h"
#include "debug.h"
#include "safe_fopen.h"
#include "condor_version.h"
#include "condor_string.h" // for getline()
#include "MyString.h"
#include "condor_arglist.h"
#include "utc_time.h"

#include <iostream>
#include <queue>
#include <unordered_map>

using namespace std;

double DagmanMetrics::_startTime = 0.0;
std::string DagmanMetrics::_dagmanId;
std::string DagmanMetrics::_parentDagmanId;

//---------------------------------------------------------------------------
void
DagmanMetrics::SetStartTime()
{
	_startTime = GetTime();
}

//---------------------------------------------------------------------------
void
DagmanMetrics::SetDagmanIds( const CondorID &DAGManJobId,
			int parentDagmanCluster )
{
	_dagmanId = std::to_string( DAGManJobId._cluster );

	if ( parentDagmanCluster >= 0 ) {
		_parentDagmanId = std::to_string( parentDagmanCluster );
	}
}

//---------------------------------------------------------------------------
DagmanMetrics::DagmanMetrics( /*const*/ Dag *dag,
			const char *primaryDagFile, int rescueDagNum ) :
	_simpleNodes( 0 ),
	_subdagNodes( 0 ),
	_simpleNodesSuccessful( 0 ),
	_simpleNodesFailed( 0 ),
	_subdagNodesSuccessful( 0 ),
	_subdagNodesFailed( 0 ), 
	_graphHeight( 0 ),
	_graphWidth( 0 ),
	_graphNumEdges( 0 ),
	_graphNumVertices( 0 )
{
	_primaryDagFile = strdup(primaryDagFile);

	_rescueDagNum = rescueDagNum;

		//
		// Set the metrics file name.
		//
	_metricsFile = primaryDagFile;
	_metricsFile += ".metrics";

		//
		// Get DAG node counts. Also gather some simple graph metrics here 
		// (ie. number of edges) to save other iterations through the jobs list 
		// later.
		// Note:  We don't check for nodes already marked being done (e.g.,
		// in a rescue DAG) because they should have already been reported
		// as being run.  wenger 2013-06-27
		//
	for (auto it = dag->_jobs.begin(); it != dag->_jobs.end(); it++) {
		_graphNumVertices++;
#ifdef DEAD_CODE
		_graphNumEdges += node->NumChildren();
#else
		_graphNumEdges += (*it)->CountChildren();
#endif
		if ((*it)->GetDagFile() ) {
			_subdagNodes++;
		} else {
			_simpleNodes++;
		}
	}
}

//---------------------------------------------------------------------------
DagmanMetrics::~DagmanMetrics()
{
	free(_primaryDagFile);
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
DagmanMetrics::Report( int exitCode, DagStatus status )
{
	if ( !WriteMetricsFile( exitCode, status ) ) {
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
bool
DagmanMetrics::WriteMetricsFile( int exitCode, DagStatus status )
{
	double endTime = GetTime();
	double duration = endTime - _startTime;

	FILE *fp = safe_fopen_wrapper_follow( _metricsFile.c_str(), "w" );
	if ( !fp ) {
		debug_printf( DEBUG_QUIET, "Could not open %s for writing.\n",
					_metricsFile.c_str() );
		return false;
	}

	fprintf( fp, "{\n" );
	fprintf( fp, "    \"client\":\"%s\",\n", "condor_dagman" );
	fprintf( fp, "    \"version\":\"%s\",\n", GetVersion().c_str() );
	fprintf( fp, "    \"planner\":\"%s\",\n", _plannerName.c_str() );
	fprintf( fp, "    \"planner_version\":\"%s\",\n", _plannerVersion.c_str() );
	fprintf( fp, "    \"type\":\"metrics\",\n" );
	fprintf( fp, "    \"wf_uuid\":\"%s\",\n", _workflowId.c_str() );
	fprintf( fp, "    \"root_wf_uuid\":\"%s\",\n", _rootWorkflowId.c_str() );
	fprintf( fp, "    \"start_time\":%.3lf,\n", _startTime );
	fprintf( fp, "    \"end_time\":%.3lf,\n", endTime );
	fprintf( fp, "    \"duration\":%.3lf,\n", duration );
	fprintf( fp, "    \"exitcode\":%d,\n", exitCode );
	fprintf( fp, "    \"dagman_id\":\"%s\",\n", _dagmanId.c_str() );
	fprintf( fp, "    \"parent_dagman_id\":\"%s\",\n",
				_parentDagmanId.c_str() );
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

	bool report_graph_metrics = param_boolean( "DAGMAN_REPORT_GRAPH_METRICS", false );
	if ( report_graph_metrics == true ) {
		fprintf( fp, "    \"graph_height\":%d,\n", _graphHeight );
		fprintf( fp, "    \"graph_width\":%d,\n", _graphWidth );
		fprintf( fp, "    \"graph_num_edges\":%d,\n", _graphNumEdges );
		fprintf( fp, "    \"graph_num_vertices\":%d,\n", _graphNumVertices );
	}

		// Last item must NOT have trailing comma!
	fprintf( fp, "    \"DagStatus\":%d\n", status );
	fprintf( fp, "}\n" );

	if ( fclose( fp ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: closing metrics file %s; errno %d (%s)\n",
					_metricsFile.c_str(), errno, strerror( errno ) );
	}

	debug_printf( DEBUG_NORMAL, "Wrote metrics file %s.\n",
				_metricsFile.c_str() );

	return true;
}

//---------------------------------------------------------------------------
double
DagmanMetrics::GetTime()
{
	return condor_gettimestamp_double();
}

//---------------------------------------------------------------------------
double
DagmanMetrics::GetTime( const struct tm &eventTime )
{
	struct tm tmpTime = eventTime;
	time_t result = mktime( &tmpTime );

	return (double)result;
}

//---------------------------------------------------------------------------
/* This function gathers metrics of a graph using various DFS and BFS
   algorithms.
*/
void
DagmanMetrics::GatherGraphMetrics( Dag* dag )
{
#ifdef DEAD_CODE
	// Gather metrics about the size, shape of the graph.
	_graphWidth = GetGraphWidth( dag );
	_graphHeight = GetGraphHeight( dag );
#else
	// if we haven't alrady run the DFS cycle detection do that now
	// it has the side effect of determining the width and height of the graph
	if ( ! dag->_graph_width) {
		dag->isCycle();
	}
	_graphWidth = dag->_graph_width;
	_graphHeight = dag->_graph_height;
#endif
}

#ifdef DEAD_CODE
//---------------------------------------------------------------------------
int
DagmanMetrics::GetGraphHeight( Dag* dag )
{
	int maxHeight = 0;
	Job* node;
	unordered_map<string, bool> visited;

	dag->_jobs.Rewind();
	while ( (node = dag->_jobs.Next()) ) {
		// Check if we've already visited this node before
		string jobName = node->GetJobName();
		unordered_map<string, bool>::const_iterator results = visited.find( jobName );

		// If this node does not appear in the visited list, get its height
		// (maximum length from this node to any connected leaf node)
		if ( results == visited.end() ) {
			int thisNodeHeight = GetGraphHeightRecursive( node, dag, &visited );
			maxHeight = ( thisNodeHeight > maxHeight ) ? thisNodeHeight : maxHeight;
		}
	}
	return maxHeight;
}

//---------------------------------------------------------------------------
int
DagmanMetrics::GetGraphHeightRecursive( Job* node, Dag* dag, unordered_map<string, bool>* visited )
{
	// This should never happen, but let's check just in case
	if( node == NULL ) {
		return 0;
	}

	// Check if this node has been visited. If not, add it to the visited list.	
	// If this node is non-null, add it to the visited list
	if ( visited->find( node->GetJobName() ) == visited->end() ) {
		pair<string, bool> thisNode( node->GetJobName(), true );
		visited->insert( thisNode );
	}

	// Base case: if this is a leaf node, return 1
	if (node->NoChildren()) {
		return 1;
	}

	// Recursive case: call this function recursively on all child nodes, then
	// return the greatest height found among all children.
	int maxHeight = 0;
	set<JobID_t>& childNodes = node->GetQueueRef( Job::Q_CHILDREN );
	set<JobID_t>::const_iterator it;
	for ( it = childNodes.begin(); it != childNodes.end(); it++ ) {
		Job* child = dag->FindNodeByNodeID( *it );
		int thisChildHeight = 1 + GetGraphHeightRecursive( child, dag, visited );
		maxHeight = ( thisChildHeight > maxHeight ) ? thisChildHeight : maxHeight;
	}

	return maxHeight;
}

//---------------------------------------------------------------------------
int
DagmanMetrics::GetGraphWidth( Dag* dag )
{
	int currentNodeLevel = 0;
	int currentLevelWidth = 0;
	int maxWidth = 0;
	Job *job;
	set<Job*> bfsJobTracker;
	unordered_map<string, bool> visited;

	// The queue we use for BFS traversal keeps track of the jobs as well as
	// their level in the graph.
	queue<pair<Job*, int>> bfsQueue;

	// Iterate through the list of jobs. Now we'll use iterative BFS to 
	// determine the maximum width of the graph (largest number of sibling nodes 
	// at the same level). We also have to account for the fact the graph might 
	// be disconnected, so we'll need to kick off a BFS algorithm for every 
	// unvisited node.
	dag->_jobs.Rewind();
	while ( ( job = dag->_jobs.Next() ) ) {
		
		// Check if this job has already been visited. If so, move along. If
		// not, leave it for now, it will get added during the BFS sequence
		if ( visited.find( job->GetJobName() ) != visited.end() ) {
			continue;
		}
			
		// Now do the BFS dance for this job
		bfsQueue.emplace( make_pair( job, 1 ) );
		bfsJobTracker.insert( job );
		while ( !bfsQueue.empty() ) {
			Job* thisNode = bfsQueue.front().first;
			int thisNodeLevel = bfsQueue.front().second;
			
			// Add to visited list if not already there
			if ( visited.find( thisNode->GetJobName() ) == visited.end() ) {
				visited.insert( make_pair( thisNode->GetJobName(), true ) ) ;
			}

			// Check the level of the front node of the queue. If this is a 
			// different level than what we were previously tracking, adjust
			// counters accordingly.
			if ( ( thisNodeLevel != currentNodeLevel )) {
				maxWidth = ( maxWidth > currentLevelWidth ) ? maxWidth : currentLevelWidth;
				currentNodeLevel = thisNodeLevel;
				currentLevelWidth = 1;
			}
			// If the same level, just increment current level width counter.
			else {
				currentLevelWidth ++;
			}

			// For each child of this node, check if they've already in the BFS
			// queue by looking in bfsJobTracker. If not, then add them to the 
			// BFS queue. Otherwise just ignore and move on.
			set<JobID_t>& childNodes = thisNode->GetQueueRef( Job::Q_CHILDREN );
			set<JobID_t>::const_iterator it;
			for ( it = childNodes.begin(); it != childNodes.end(); it++ ) {
				Job* child = dag->FindNodeByNodeID( *it );
				if ( bfsJobTracker.find( child ) == bfsJobTracker.end() ) {
					bfsQueue.emplace( make_pair( child, thisNodeLevel+1 ) );
					bfsJobTracker.insert( child );
				}
			}

			// Finally, remove the front node.
			bfsQueue.pop();
		}

		// Check the width of the last level of the graph
		maxWidth = ( maxWidth > currentLevelWidth ) ? maxWidth : currentLevelWidth;
	}

	return maxWidth;
}

#endif // DEAD_CODE

//---------------------------------------------------------------------------
MyString
DagmanMetrics::GetVersion()
{
	MyString result;

	const char *cv = CondorVersion();

	const char *ptr = cv;
	while ( *ptr && !isdigit( *ptr ) ) {
		++ptr;
	}
	while ( *ptr && !isspace( *ptr ) ) {
		result += *ptr;
		++ptr;
	}

	return result;
}
