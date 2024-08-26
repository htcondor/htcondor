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
#include "utc_time.h"

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
	for (auto & node : dag->_nodes) {
		_graphNumVertices++;
		_graphNumEdges += node->CountChildren();
		if ( node->GetDagFile() ) {
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
	fprintf( fp, "    \"type\":\"metrics\",\n" );
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
	// if we haven't alrady run the DFS cycle detection do that now
	// it has the side effect of determining the width and height of the graph
	if ( ! dag->_graph_width) {
		dag->isCycle();
	}
	_graphWidth = dag->_graph_width;
	_graphHeight = dag->_graph_height;
}

//---------------------------------------------------------------------------
std::string
DagmanMetrics::GetVersion()
{
	std::string result;

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
