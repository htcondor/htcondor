/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.
 * No use of the CONDOR Software Program Source Code is authorized
 * without the express consent of the CONDOR Team.  For more information
 * contact: CONDOR Team, Attention: Professor Miron Livny,
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685,
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure
 * by the U.S. Government is subject to restrictions as set forth in
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison,
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"
#include "stdio.h"

#define ITERATIONS_PER_SECOND 10000000

int load_avg_test(int trials, int interval, int num_children, double perc_warn_ok) {
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
	
	if (((double)num_warnings/(double)num_tests) > perc_warn_ok) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Warning tolerance exceeded (%2f\% warnings > %2f\% tolerance) .\n", ((double)num_warnings/(double)num_tests)*100, perc_warn_ok*100);
			return_val = return_val || 1;
	}

	return return_val;
}
