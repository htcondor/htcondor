/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef DAGMAN_MAIN_H
#define DAGMAN_MAIN_H

#include "dag.h"
#include "string_list.h"

	// Don't change these values!  Doing so would break some DAGs.
enum exit_value {
	EXIT_OKAY = 0,
	EXIT_ERROR = 1,
	EXIT_ABORT = 2, // condor_rm'ed or hit special abort DAG exit code
};

int main_shutdown_rescue( int exitVal );

class Dagman {
  public:
	Dagman();
	~Dagman();
    inline void CleanUp () { 
		if ( dag != NULL ) {
			delete dag; 
			dag = NULL;
		}
	}
    Dag * dag;
    int maxIdle;  // Maximum number of idle DAG nodes
    int maxJobs;  // Maximum number of Jobs to run at once
    int maxPreScripts;  // max. number of PRE scripts to run at once
    int maxPostScripts;  // max. number of POST scripts to run at once
    char *rescue_file;
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

		// "Primary" DAG file -- if we have multiple DAG files this is
		// the first one.  The lock file name, rescue DAG name, etc., 
		// are based on this name.
	char *primaryDagFile;

		// The list of all DAG files to be run by this invocation of
		// condor_dagman.
	StringList dagFiles;

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

		// If this is true, nodes for which a submit fails are retried
		// before any other ready nodes; otherwise a submit failure puts
		// a node at the back of the ready queue.  (Default is true.)
	bool retrySubmitFirst;

		// Whether to munge the node names for multi-DAG runs to make
		// sure they're unique.  The default is true, but the user can
		// turn this off if their node names are globally unique.
	bool mungeNodeNames;

		// the Condor job id of the DAGMan job
	CondorID DAGManJobId;

    bool Config();
};

#endif	// ifndef DAGMAN_MAIN_H
