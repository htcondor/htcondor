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

#include "condor_common.h"
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"
#include "stdio.h"

#define ITERATIONS_PER_SECOND 10000000

int load_avg_test(int trials, int interval, int num_children, double warn_ok_ratio) {
	int		foo = 0;
	int		foo2 = 0;
	int		bar = 0;
	int		bar2 = 0;
	int		i, j, k;
	int		unslept;
	int		pid;
	int		return_val = 0;
	int		num_warnings = 0;
	int		num_tests = 0;

	for (j=0; j<trials; j++) {
		dprintf(D_ALWAYS, "SysAPI: I will Wait for %d seconds, then testing load with %d children. I will repeat this %d times.\n", interval, num_children, trials);

		foo = sysapi_load_avg_raw();
		dprintf(D_ALWAYS, "SysAPI: Without children, sysapi_load_avg_raw() -> %f\n", foo);
		foo2 = sysapi_load_avg();
		dprintf(D_ALWAYS, "SysAPI: Without children, sysapi_load_avg() -> %f\n", foo2);

		unslept = interval;
		while (unslept > 0) 
			unslept = sleep(unslept);

		for (i=0; i<num_children; i++) {
			if ((pid = fork()) < 0) {
				dprintf(D_ALWAYS, "SysAPI: Fork error!\n");
				return_val = return_val || 1;
				return return_val;
			} else if (pid == 0) {
        		// child process 
				for (i=0; i<20*ITERATIONS_PER_SECOND; i++) {}
				//dprintf(D_ALWAYS, "Child process terminating\n");
				kill(getpid(), 15);
			} else {
        		// parent process
			}
		}
	
	
		bar = sysapi_load_avg_raw();
		dprintf(D_ALWAYS, "SysAPI: With %d spinwaits running, sysapi_load_avg_raw() -> %f\n", num_children, bar);
		bar2 = sysapi_load_avg();
		dprintf(D_ALWAYS, "SysAPI: With %d spinwaits running, sysapi_load_avg() -> %f\n", num_children, bar2);
		num_tests += 2;

		for (k=0; k<num_children; k++)
				wait(0);
	
		if (bar < foo) {
				dprintf(D_ALWAYS, "SysAPI: Warning! Raw load went from %f to %f with %d additional processes, but probably should have increased. Perhaps other processes stopped.\n", foo, bar, num_children);
				//num_warnings++;
		}
		if (bar2 < foo2) {
				dprintf(D_ALWAYS, "SysAPI: Warning! Cooked load went from %f to %f with %d additional processes, but probably should have increased. Perhaps other processes stopped.\n", foo2, bar2, num_children);
				//num_warnings++;
		}
	}
	
	if (((double)num_warnings/(double)num_tests) > warn_ok_ratio) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Warning tolerance exceeded (%2f\% warnings > %2f\% tolerance) .\n", ((double)num_warnings/(double)num_tests)*100, warn_ok_ratio*100);
			return_val = return_val || 1;
	}

	return return_val;
}
