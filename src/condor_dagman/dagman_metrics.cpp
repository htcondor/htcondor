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

#include <sys/time.h>
#include "dagman_metrics.h"
#include "debug.h"
#include "safe_fopen.h"
#include "condor_version.h"

double DagmanMetrics::_startTime;

//---------------------------------------------------------------------------
void
DagmanMetrics::SetStartTime()
{
	_startTime = GetTime();
}

//---------------------------------------------------------------------------
DagmanMetrics::DagmanMetrics( /*const*/ Dag *dag,
			const char *primaryDagFile, int rescueDagNum ) :
	_sendMetrics( true ),
	_simpleNodes( 0 ),
	_subdagNodes( 0 ),
	_simpleNodesSuccessful( 0 ),
	_simpleNodesFailed( 0 ),
	_subdagNodesSuccessful( 0 ),
	_subdagNodesFailed( 0 )

{
	debug_printf( DEBUG_NORMAL, "DIAG DagmanMetrics::DagmanMetrics(%s)\n", primaryDagFile );//TEMPTEMP
	debug_printf( DEBUG_NORMAL, "  DIAG num nodes: %d\n", dag->NumNodes( true ) );//TEMPTEMP

	_rescueDagNum = rescueDagNum;

		//
		// Figure out whether to actually report the metrics.
		//
	const char *tmp = getenv( "PEGASUS_METRICS" );
	if ( tmp ) {
		if ( ( strcasecmp( tmp, "true" ) != 0 ) &&
					( strcasecmp( tmp, "1" ) != 0 ) ) {
			_sendMetrics = false;
		}
	}

	tmp = param( "CONDOR_DEVELOPERS" );
	if ( !tmp ) _sendMetrics = false;

		//
		// Get the metrics file.
		//
	_metricsFile = primaryDagFile;
	_metricsFile += ".metrics";

	//TEMPTEMP -- read braindump file

		//
		// Get DAG node counts.
		// Note:  We don't check for nodes already marked being done (e.g.,
		// in a rescue DAG) because they should have already been reported
		// as being run.  wenger 2013-06-27
		//
	Job *node;
	dag->_jobs.Rewind();
	while ( (node = dag->_jobs.Next()) ) {
	debug_printf( DEBUG_NORMAL, "  DIAG node %s\n", node->GetJobName() );//TEMPTEMP
		if ( node->GetDagFile() != NULL ) {
			_subdagNodes++;
		} else {
			_simpleNodes++;
		}
	}

}

//---------------------------------------------------------------------------
DagmanMetrics::~DagmanMetrics()
{
	debug_printf( DEBUG_NORMAL, "DIAG DagmanMetrics::~DagmanMetrics()\n" );//TEMPTEMP
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
DagmanMetrics::Report( int exitCode, Dag::dag_status status )
{
	debug_printf( DEBUG_NORMAL, "DIAG DagmanMetrics::Report()\n" );//TEMPTEMP

	if ( !WriteMetricsFile( exitCode, status ) ) {
		return false;
	}

	if ( _sendMetrics ) {
		//TEMPTEMP -- send metrics file
	} else {
		debug_printf( DEBUG_NORMAL, "Metrics not sent because of PEGASUS_METRICS, CONDOR_DEVELOPERS or setting.\n" );
	}

	return true;
}

//---------------------------------------------------------------------------
bool
DagmanMetrics::WriteMetricsFile( int exitCode, Dag::dag_status status )
{
	debug_printf( DEBUG_NORMAL, "DIAG DagmanMetrics::WriteMetricsFile(%d, %d)\n", exitCode, status );//TEMPTEMP

	double endTime = GetTime();
	double duration = endTime - _startTime;

	//TEMPTEMP -- hmm -- do we have to do the 'create a tmp file and move' thing?

	//TEMPTEMP -- is this the right open function?
	FILE *fp = safe_fopen_wrapper_follow( _metricsFile.Value(), "w" );
	if ( !fp ) {
		debug_printf( DEBUG_QUIET, "Could not open %s for writing.\n",
					_metricsFile.Value() );
		return false;
	}

	fprintf( fp, "{\n" );
	fprintf( fp, "    \"client\": \"TEMPTEMP\"\n" );
	fprintf( fp, "    \"version\": \"TEMPTEMP\"\n" );
	fprintf( fp, "    \"type\": \"metrics\"\n" );
	fprintf( fp, "    \"wf_uuid\": \"TEMPTEMP\"\n" );
	fprintf( fp, "    \"root_wf_uuid\": \"TEMPTEMP\n" );
	fprintf( fp, "    \"start_time\": %lf\n", _startTime );
	fprintf( fp, "    \"end_time\": %lf\n", endTime );
	fprintf( fp, "    \"duration\": %lf\n", duration );
	fprintf( fp, "    \"exitcode\": %d\n", exitCode );
		//TEMPTEMP -- I think we just want something like "8.1.0" here...
	fprintf( fp, "    \"dagman_version\": \"%s\"\n", CondorVersion() );
	fprintf( fp, "    \"dagman_id\": \"TEMPTEMP\"\n" );
	fprintf( fp, "    \"parent_dagman_id\": \"TEMPTEMP\"\n" );
		//TEMPTEMP -- crap -- I didn't think about how to pass parent ID to sub-DAGs...
	fprintf( fp, "    \"rescue_dag_number\": %d\n", _rescueDagNum );
	fprintf( fp, "    \"jobs\": %d\n", _simpleNodes );
	fprintf( fp, "    \"jobs_failed\": %d\n", _simpleNodesFailed );
	fprintf( fp, "    \"jobs_succeeded\": %d\n", _simpleNodesSuccessful );
	fprintf( fp, "    \"dag_jobs\": %d\n", _subdagNodes );
	fprintf( fp, "    \"dag_jobs_failed\": %d\n", _subdagNodesFailed );
	fprintf( fp, "    \"dag_jobs_succeeded\": %d\n", _subdagNodesSuccessful );
	fprintf( fp, "    \"total_jobs\": %d\n", _simpleNodes + _subdagNodes );
	int totalNodesRun = _simpleNodesSuccessful + _simpleNodesFailed +
				_subdagNodesSuccessful + _subdagNodesFailed;
	fprintf( fp, "    \"total_jobs_run\": %d\n", totalNodesRun );
	fprintf( fp, "    \"total_job_time\": TEMPTEMP\n" );
	fprintf( fp, "    \"dag_status\": %d\n", status );
	fprintf( fp, "}\n" );

	//TEMPTEMP -- check return value?
	fclose( fp );

	debug_printf( DEBUG_NORMAL, "Wrote metrics file %s.\n",
				_metricsFile.Value() );

	return true;
}

//---------------------------------------------------------------------------
double
DagmanMetrics::GetTime()
{
	struct timeval tv;
	//TEMPTEMP -- check return value
	gettimeofday( &tv, NULL );

	debug_printf( DEBUG_NORMAL, "DIAG tv_sec: %ld\n", tv.tv_sec );//TEMPTEMP
	debug_printf( DEBUG_NORMAL, "DIAG tv_usec: %ld\n", tv.tv_usec );//TEMPTEMP

	//TEMPTEMP -- do we want to round here?
	double millisec = tv.tv_usec / 1000;

	double time = ((double)tv.tv_sec) + millisec / 1000.0;
	debug_printf( DEBUG_NORMAL, "DIAG time: %lf\n", time );//TEMPTEMP
	return time;
}
