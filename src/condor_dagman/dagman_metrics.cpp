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

#include <stdlib.h>
#include "dagman_metrics.h"
#include "debug.h"
#include "safe_fopen.h"
#include "condor_version.h"
#include "condor_string.h" // for getline()
#include "MyString.h"
#include "condor_arglist.h"
#include "utc_time.h"

double DagmanMetrics::_startTime = 0.0;
MyString DagmanMetrics::_dagmanId = "";
MyString DagmanMetrics::_parentDagmanId = "";

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
	_dagmanId = "";
	_dagmanId += DAGManJobId._cluster;

	if ( parentDagmanCluster >= 0 ) {
		_parentDagmanId = "";
		_parentDagmanId += parentDagmanCluster;
	}
}

//---------------------------------------------------------------------------
DagmanMetrics::DagmanMetrics( /*const*/ Dag *dag,
			const char *primaryDagFile, int rescueDagNum ) :
	_sendMetrics( false ),
	_simpleNodes( 0 ),
	_subdagNodes( 0 ),
	_simpleNodesSuccessful( 0 ),
	_simpleNodesFailed( 0 ),
	_subdagNodesSuccessful( 0 ),
	_subdagNodesFailed( 0 ), 
	_totalNodeJobTime( 0.0 )
{
	_primaryDagFile = strnewp(primaryDagFile);

	_rescueDagNum = rescueDagNum;

		//
		// Figure out whether to actually report the metrics.  We only
		// send metrics if it's enabled with the PEGASUS_METRICS
		// environment variable.  But that can be overridden by the
		// CONDOR_DEVELOPERS config macro.
		//
	const char *tmp = getenv( "PEGASUS_METRICS" );
	if ( tmp && ( ( strcasecmp( tmp, "true" ) == 0 ) ||
				( strcasecmp( tmp, "1" ) == 0 ) ) ) {
		_sendMetrics = true;
	}

	tmp = param( "CONDOR_DEVELOPERS" );
	if ( tmp && strcmp( tmp, "NONE" ) == 0 ) {
		_sendMetrics = false;
	}

		//
		// Set the metrics file name.
		//
	_metricsFile = primaryDagFile;
	_metricsFile += ".metrics";

		//
		// If we're actually sending metrics, Pegasus should have
		// created a braindump.txt file that includes a bunch of
		// Pegasus information.
		//
	if ( _sendMetrics ) {
		ParseBraindumpFile();
	}

		//
		// Get DAG node counts.
		// Note:  We don't check for nodes already marked being done (e.g.,
		// in a rescue DAG) because they should have already been reported
		// as being run.  wenger 2013-06-27
		//
	Job *node;
	dag->_jobs.Rewind();
	while ( (node = dag->_jobs.Next()) ) {
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
	delete[] _primaryDagFile;
}

//---------------------------------------------------------------------------
void
DagmanMetrics::ProcStarted( const struct tm &eventTime )
{
		// Avoid possible mktime() craziness unless we really need the
		// metrics -- see gittrac #2898.
	if ( _sendMetrics ) {
		double et = GetTime( eventTime );
			// We decrement by et - _startTime instead of just et here to
			// reduce numerical error.
		_totalNodeJobTime -= ( et - _startTime );
	}
}

//---------------------------------------------------------------------------
void
DagmanMetrics::ProcFinished( const struct tm &eventTime )
{
		// Avoid possible mktime() craziness unless we really need the
		// metrics -- see gittrac #2898.
	if ( _sendMetrics ) {
		double et = GetTime( eventTime );
			// We increment by et - _startTime instead of just et here to
			// reduce numerical error.
		_totalNodeJobTime += ( et - _startTime );
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
DagmanMetrics::Report( int exitCode, Dag::dag_status status )
{
	if ( !WriteMetricsFile( exitCode, status ) ) {
		return false;
	}

	bool disabled = false;

#if defined(WIN32)
	disabled = true;
#endif

	if ( disabled ) {
		debug_printf( DEBUG_NORMAL,
					"Metrics reporting is not available on this platform.\n" );
	} else if ( _sendMetrics ) {
		MyString reporterPath;
		const char* exe = param( "DAGMAN_PEGASUS_REPORT_METRICS" );	
		if(exe) {
			reporterPath = exe;
		} else {
			const char *libexec = param( "LIBEXEC" );
			if ( !libexec ) {
				debug_printf( DEBUG_QUIET,
							"LIBEXEC not defined; can't find condor_dagman_metrics_reporter\n" );
				return false;
			}
			reporterPath = libexec;
			reporterPath += "/";
			reporterPath += "condor_dagman_metrics_reporter";
		}

		MyString duration = param_integer( "DAGMAN_PEGASUS_REPORT_TIMEOUT", 100, 0 );

		MyString metricsOutputFile( _primaryDagFile );
		metricsOutputFile += ".metrics.out";

		debug_printf( DEBUG_NORMAL,
					"Reporting metrics to Pegasus metrics server(s); output is in %s.\n",
					metricsOutputFile.Value() );

		ArgList args;
		args.AppendArg(reporterPath.Value());	
		args.AppendArg("-f");
		args.AppendArg(_metricsFile.Value());
			// If the DAG was condor_rm'ed, we want the reporter to sleep.
		if ( status == Dag::DAG_STATUS_RM ) {
			args.AppendArg("-s");
		}
		args.AppendArg( "-t" );
		args.AppendArg( duration.Value() );
			// Dump the args to the dagman.out file
		MyString cmd; // for debug output
		args.GetArgsStringForDisplay( &cmd );
		debug_printf( DEBUG_NORMAL, "Running command <%s>\n", cmd.Value() );

		int stdFds[3];
		stdFds[0] = -1; // stdin
		stdFds[1] = safe_open_wrapper_follow( metricsOutputFile.Value(),
					O_WRONLY | O_CREAT | O_TRUNC | O_APPEND ); // stdout
		stdFds[2] = stdFds[1]; // stderr goes to the same file as stdout

		int pid = daemonCore->Create_Process(
					reporterPath.Value(),
					args,
					PRIV_UNKNOWN,
					1, // reaper
					false, // no command port
					NULL, // just inherit env of parent
					NULL, // no cwd
					NULL, // no FamilyInfo
					NULL, // no sock_inherit_list
					stdFds );

		if ( pid == 0 ) {
			debug_printf( DEBUG_QUIET,
						"Error: failed to start condor_dagman_metrics_reporter (%d, %s)\n",
						errno, strerror( errno ) );
		}

		if ( close( stdFds[1] ) != 0 ) {
			debug_printf( DEBUG_QUIET, "ERROR: closing stdout for metrics "
						"reporter; errno %d (%s)\n", errno,
						strerror( errno ) );
		}

	} else {
		debug_printf( DEBUG_NORMAL, "Metrics not sent because of PEGASUS_METRICS or CONDOR_DEVELOPERS setting.\n" );
	}

	return true;
}

//---------------------------------------------------------------------------
bool
DagmanMetrics::WriteMetricsFile( int exitCode, Dag::dag_status status )
{
	double endTime = GetTime();
	double duration = endTime - _startTime;

	FILE *fp = safe_fopen_wrapper_follow( _metricsFile.Value(), "w" );
	if ( !fp ) {
		debug_printf( DEBUG_QUIET, "Could not open %s for writing.\n",
					_metricsFile.Value() );
		return false;
	}

	fprintf( fp, "{\n" );
	fprintf( fp, "    \"client\":\"%s\",\n", "condor_dagman" );
	fprintf( fp, "    \"version\":\"%s\",\n", GetVersion().Value() );
	fprintf( fp, "    \"planner\":\"%s\",\n", _plannerName.Value() );
	fprintf( fp, "    \"planner_version\":\"%s\",\n", _plannerVersion.Value() );
	fprintf( fp, "    \"type\":\"metrics\",\n" );
	fprintf( fp, "    \"wf_uuid\":\"%s\",\n", _workflowId.Value() );
	fprintf( fp, "    \"root_wf_uuid\":\"%s\",\n", _rootWorkflowId.Value() );
	fprintf( fp, "    \"start_time\":%.3lf,\n", _startTime );
	fprintf( fp, "    \"end_time\":%.3lf,\n", endTime );
	fprintf( fp, "    \"duration\":%.3lf,\n", duration );
	fprintf( fp, "    \"exitcode\":%d,\n", exitCode );
	fprintf( fp, "    \"dagman_id\":\"%s\",\n", _dagmanId.Value() );
	fprintf( fp, "    \"parent_dagman_id\":\"%s\",\n",
				_parentDagmanId.Value() );
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
	fprintf( fp, "    \"total_job_time\":%.3lf,\n", _totalNodeJobTime );

		// Last item must NOT have trailing comma!
	fprintf( fp, "    \"dag_status\":%d\n", status );
	fprintf( fp, "}\n" );

	if ( fclose( fp ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: closing metrics file %s; errno %d (%s)\n",
					_metricsFile.Value(), errno, strerror( errno ) );
	}

	debug_printf( DEBUG_NORMAL, "Wrote metrics file %s.\n",
				_metricsFile.Value() );

	return true;
}

//---------------------------------------------------------------------------
double
DagmanMetrics::GetTime()
{
	UtcTime curTime( true );
	return curTime.combined();
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

//---------------------------------------------------------------------------
void
DagmanMetrics::ParseBraindumpFile()
{
	const char *filename = getenv( "PEGASUS_BRAINDUMP_FILE" );
	if ( !filename ) {
		filename = "braindump.txt";
	}

	FILE *fp = safe_fopen_wrapper_follow( filename, "r" );
	if ( !fp ) {
		debug_printf( DEBUG_QUIET,
					"Warning:  could not open Pegasus braindump file %s\n",
					filename );
		check_warning_strictness( DAG_STRICT_2 );
		return;
	}

	const char *line;
		// Note:  getline() frees memory from the previous call each time.
	while ( (line = getline( fp ) ) ) {
		MyString lineStr( line );
		lineStr.Tokenize();
		const char *token1;
		token1 = lineStr.GetNextToken( " \t", true );
		if ( token1 ) {
			const char *token2 = lineStr.GetNextToken( " \t", true );
			if ( token2 ) {
				if ( strcmp( token1, "wf_uuid" ) == 0 ) {
					_workflowId = token2;
				} else if ( strcmp( token1, "root_wf_uuid" ) == 0 ) {
					_rootWorkflowId = token2;
				} else if ( strcmp( token1, "planner" ) == 0 ) {
					_plannerName = token2;
				} else if ( strcmp( token1, "planner_version" ) == 0 ) {
					_plannerVersion = token2;
				}
			} else {
				debug_printf( DEBUG_QUIET,
							"Warning:  no value for %s in braindump file\n",
							token1 );
				check_warning_strictness( DAG_STRICT_2 );
			}
		}
	}

	if ( fclose( fp ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: closing Pegasus braindump file %s; errno %d (%s)\n",
					filename, errno, strerror( errno ) );
	}
}
