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

class Dagman {
  public:
	Dagman();
	~Dagman();
    inline void CleanUp () { delete dag; }
    Dag * dag;
    int maxJobs;  // Maximum number of Jobs to run at once
    int maxPreScripts;  // max. number of PRE scripts to run at once
    int maxPostScripts;  // max. number of POST scripts to run at once
    char *rescue_file;
	bool paused;
	// number of seconds to wait before consecutive calls to
	// condor_submit (or dap_submit, etc.)
    int submit_delay;
		// number of times in a row to attempt to execute
		// condor_submit (or dap_submit) before giving up
    int max_submit_attempts;
		// maximum number of jobs to submit in a single periodic timer
		// interval
    int max_submits_per_interval;

    char *datafile;
		// whether to peform expensive cycle-detection at startup
		// (note: we perform run-time cycle-detection regardless)
	bool startup_cycle_detect;
    char* stork_server;
	bool doEventChecks;

		// Allow the job to execute even if we have an error determining
		// the log files (e.g., the log file is missing from one of the
		// node submit files).
	bool allowLogError;

		// Setting this causes DAGMan to tolerate the Condor bug of
		// a job getting run more than once; otherwise this will be
		// a fatal error.  (Default is false.)
	bool allowExtraRuns;

		// If this is true, nodes for which a submit fails are retried
		// before any other ready nodes; otherwise a submit failure puts
		// a node at the back of the ready queue.  (Default is true.)
	bool retrySubmitFirst;

    bool Config();
};

#endif	// ifndef DAGMAN_MAIN_H
