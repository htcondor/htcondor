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
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"
#include "stdio.h"

#define ITERATIONS_PER_SECOND 10000000

int load_avg_test(int trials, int interval, int num_children, double warn_ok_ratio);

int load_avg_test(int trials, int interval, int num_children, double warn_ok_ratio) {
	float		foo = 0;
	float		foo2 = 0;
	float		bar = 0;
	float		bar2 = 0;
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
			dprintf(D_ALWAYS, "SysAPI: ERROR! Warning tolerance exceeded (%2f%% warnings > %2f%% tolerance) .\n", ((double)num_warnings/(double)num_tests)*100, warn_ok_ratio*100);
			return_val = return_val || 1;
	}

	return return_val;
}
