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

#ifndef DAGMAN_MAIN_H
#define DAGMAN_MAIN_H

#include "dag.h"
#include "string_list.h"
#include "dagman_classad.h"

	// Don't change these values!  Doing so would break some DAGs.
enum exit_value {
	EXIT_OKAY = 0,
	EXIT_ERROR = 1,
	EXIT_ABORT = 2, // condor_rm'ed or hit special abort DAG exit code
	EXIT_RESTART = 3,	// exit but indicate that we should be restarted
};

void main_shutdown_rescue( int exitVal, Dag::dag_status dagStatus );
void main_shutdown_graceful( void );
void print_status();

class Dagman {
  public:
	Dagman();
	~Dagman();

    inline void CleanUp () { 
		if ( dag != NULL ) {
			delete dag; 
			dag = NULL;
		}
		delete _dagmanClassad;
		_dagmanClassad = NULL;
	}

		// Check (based on the version from the .condor.sub file, etc.),
		// whether we should fall back to non-default log mode.
	void CheckLogFileMode( const CondorVersionInfo &submitFileVersion );

		// Disable use of the default node log (use the log files from
		// the submit files instead).
	void DisableDefaultLog();

		// Resolve macro substitutions in _defaultNodeLog.  Also check
		// for some errors/warnings.
	void ResolveDefaultLog();

    Dag * dag;
    int maxIdle;  // Maximum number of idle DAG nodes
    int maxJobs;  // Maximum number of Jobs to run at once
    int maxPreScripts;  // max. number of PRE scripts to run at once
    int maxPostScripts;  // max. number of POST scripts to run at once
	bool paused;

	char* condorSubmitExe;
	char* condorRmExe;
	char* storkSubmitExe;
	char* storkRmExe;

	// number of seconds to wait before consecutive calls to
	// condor_submit (or dap_submit, etc.)
    int submit_delay;
		// number of times in a row to attempt to execute
		// condor_submit (or dap_submit) before giving up
    int max_submit_attempts;
		// maximum number of jobs to submit in a single periodic timer
		// interval
    int max_submits_per_interval;

		// How long dagman waits before checking the log files to see if
		// some events happened. With very short running jobs in a linear
		// dag, dagman spends a lot of its time waiting just to see that the
		// job finished so it can submit the next one. This allows us to
		// configure that to be much faster with a minimum of 1 second.
	int m_user_log_scan_interval;

		// "Primary" DAG file -- if we have multiple DAG files this is
		// the first one.  The lock file name, rescue DAG name, etc., 
		// are based on this name.
		// Note: if we autorun a rescue DAG, this name needs to stay
		// what it was originally, so a subsequent rescue DAG (if any)
		// is written to the right file.  It can't be a char * because
		// that will get goofed up when the dagFiles list is cleared.
		// wenger 2008-02-27
	MyString primaryDagFile;

		// The list of all DAG files to be run by this invocation of
		// condor_dagman.
	StringList dagFiles;

		// Whether we have more than one DAG file; we need to save this
		// separately because dagFiles will get reset if we're automatically
		// running a rescue DAG.
	bool multiDags;

		// whether to peform expensive cycle-detection at startup
		// (note: we perform run-time cycle-detection regardless)
	bool startup_cycle_detect;

		// Allow the job to execute even if we have an error determining
		// the log files (e.g., the log file is missing from one of the
		// node submit files).
	bool allowLogError;

		// Whether to treat the dirname portion of any DAG file paths
		// as a directory that the DAG should effectively be run from.
	bool useDagDir;

		// What "bad" events to treat as non-fatal (as opposed to fatal)
		// errors; see check_events.h for values.
	int allow_events;

		// If this is true, nodes for which the job submit fails are retried
		// before any other ready nodes; otherwise a submit failure puts
		// a node at the back of the ready queue.  (Default is true.)
	bool retrySubmitFirst;

		// If this is true, nodes for which the node fails (and the node
		// has retries) are retried before any other ready nodes; 
		// otherwise a node failure puts a node at the back of the ready 
		// queue.  (Default is false.)
	bool retryNodeFirst;

		// Whether to munge the node names for multi-DAG runs to make
		// sure they're unique.  The default is true, but the user can
		// turn this off if their node names are globally unique.
	bool mungeNodeNames;

		// whether or not to prohibit multiple job proc submits (e.g.,
		// node jobs that create more than one job proc)
	bool prohibitMultiJobs;

		// Whether to abort duplicates DAGMans (if multiple DAGMans are
		// run on the same DAG at the same time on the same machine,
		// all but the first will be aborted).
	bool abortDuplicates;

		// Whether to submit ready nodes in depth-first order (as opposed
		// to breadth-first).
	bool submitDepthFirst;

		// Whether to abort on a "scary" submit event (Condor ID doesn't
		// match expected value).
	bool abortOnScarySubmit;

		// The interval (in seconds) between reports on what nodes
		// are pending.
	int pendingReportInterval;

		// the Condor job id of the DAGMan job
	CondorID DAGManJobId;

		// The DAGMan configuration file (NULL if none is specified).
	char *_dagmanConfigFile;

		// Whether to automatically run a rescue DAG if one exists.
	bool autoRescue;

		// "New-style" rescue DAG number to run; 0 means no rescue DAG
		// specified
	int doRescueFrom;

		// The maximum allowed rescue DAG number.
	int maxRescueDagNum;

		// The name of the rescue DAG we're running, if any.  This
		// will remain set to "" unless we're running a rescue DAG.
		// This is *not* the name of the rescue DAG to write, if the
		// current run fails.
	MyString rescueFileToRun;

		// Whether to dump a rescue DAG and exit after parsing the input
		// DAG(s).
	bool dumpRescueDag;

		// Whether the rescue DAG we write will be only a partial DAG file
		// (new for 7.7.2).
	bool _writePartialRescueDag;

		// The default log file for node jobs that don't specify a
		// log file.
	MyString _defaultNodeLog;

		// Whether to generate the .condor.sub files for sub-DAGs
		// at run time (just before the node is submitted).
	bool _generateSubdagSubmits;

		// This object must remain in existance the whole time the DAG
		// is running, since we're just passing the pointer to the
		// DAG object, and we're not actually copying the SubmitDagOptions
		// object.
	SubmitDagDeepOptions _submitDagDeepOpts;

    bool Config();

		// The maximum number of times a node job can go on hold before
		// we declare it a failure and remove it; 0 means no limit.
	int _maxJobHolds;
	static strict_level_t _strict;

		// If _runPost is true, we run a POST script even if the PRE
		// script for the node fails.
	bool _runPost;

		// Default priority that DAGman uses for nodes.
	int _defaultPriority;

	int _claim_hold_time;

		// True iff -DoRecov is specified on the command line.
	bool _doRecovery;

	DagmanClassad *_dagmanClassad;
};

#endif	// ifndef DAGMAN_MAIN_H
