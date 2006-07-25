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
#include "string.h"

int phys_memory_test(int trials, double warn_ok_ratio)
{
	int foo,  bar;
	int foo2,  bar2;
	int		return_val = 0;
	int i;
	int	num_tests = 0;
	int	num_warnings = 0;

	foo = sysapi_phys_memory_raw();
	dprintf(D_ALWAYS, "SysAPI: sysapi_phys_memory_raw -> %d\n", foo);
	bar = sysapi_phys_memory();
	dprintf(D_ALWAYS, "SysAPI: sysapi_phys_memory -> %d\n", bar);

	foo2 = foo;
	bar2 = bar;

	dprintf(D_ALWAYS, "SysAPI: Doing %d trials\n", trials);
	for (i=0; i<trials; i++) {

		foo = sysapi_phys_memory_raw();
		bar = sysapi_phys_memory();

		num_tests++;
		if (foo <= 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Raw physical memory (%d MB) is negative!\n", foo);
			return_val = 1;
		}
		else if (foo != foo2 ) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Raw physical memory (%d MB) does not match "
							"previous size (%d MB)!\n", foo, foo2);
			return_val = 1;
		}

		num_tests++;
		if (bar <= 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Cooked physical memory (%d MB) is negative!\n", bar);
			return_val = 1;
		}
		else if (bar != bar2 ) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Cooked physical memory (%d MB) does not match "
							"previous size (%d MB)!\n", bar, bar2);
			return_val = 1;
		}

		foo2 = foo;
		bar2 = bar;
	}

	foo = sysapi_phys_memory_raw();
	dprintf(D_ALWAYS, "SysAPI: sysapi_phys_memory_raw -> %d\n", foo);
	bar = sysapi_phys_memory();
	dprintf(D_ALWAYS, "SysAPI: sysapi_phys_memory -> %d\n", bar);

	if (((double)num_warnings/(double)num_tests) > warn_ok_ratio) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Warning warn_ok_ratio exceeded (%2f\% warnings > "
							"%2f\% warn_ok_ratio) .\n", 
							((double)num_warnings/(double)num_tests)*100, warn_ok_ratio*100);
			return_val = return_val | 1;
	}
	return return_val;
}
