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
	StringList condorLogFiles;
		// whether to peform expensive cycle-detection at startup
		// (note: we perform run-time cycle-detection regardless)
	bool startup_cycle_detect;
    char* stork_server;

    bool Config();
};

extern Dagman dagman;

#endif	// ifndef DAGMAN_MAIN_H
