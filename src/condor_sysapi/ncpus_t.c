/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

int ncpus_test(int trials, double warn_ok_ratio)
{
	int foo,  bar;
	int foo2,  bar2;
	int		return_val = 0;
	int i;
	int	num_tests = 0;
	int	num_warnings = 0;

	foo = sysapi_ncpus_raw();
	dprintf(D_ALWAYS, "SysAPI: sysapi_ncpus_raw -> %d\n", foo);
	bar = sysapi_ncpus();
	dprintf(D_ALWAYS, "SysAPI: sysapi_ncpus -> %d\n", bar);

	foo2 = foo;
	bar2 = bar;

	dprintf(D_ALWAYS, "SysAPI: Doing %d trials\n", trials);
	for (i=0; i<trials; i++) {

		foo = sysapi_ncpus_raw();
		bar = sysapi_ncpus();

		num_tests++;
		if (foo <= 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Raw number of CPUs (%d) is negative!", foo);
			return_val = 1;
		}
		else if (foo != foo2 ) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Raw number of CPUs (%d) does not match previous number (%d)!", foo, foo2);
			return_val = 1;
		}

		num_tests++;
		if (bar <= 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Cooked number of CPUs (%d) is negative!", bar);
			return_val = 1;
		}
		else if (bar != bar2 ) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Cooked number of CPUs (%d) does not match previous number (%d)!", bar, bar2);
			return_val = 1;
		}

		foo2 = foo;
		bar2 = bar;
	}

	foo = sysapi_ncpus_raw();
	dprintf(D_ALWAYS, "SysAPI: sysapi_ncpus_raw -> %d\n", foo);
	bar = sysapi_ncpus();
	dprintf(D_ALWAYS, "SysAPI: sysapi_ncpus -> %d\n", bar);

	if (((double)num_warnings/(double)num_tests) > warn_ok_ratio) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Warning warn_ok_ratio exceeded (%2f\% warnings > %2f\% warn_ok_ratio) .\n", ((double)num_warnings/(double)num_tests)*100, warn_ok_ratio*100);
			return_val = return_val | 1;
	}
	return return_val;
}
