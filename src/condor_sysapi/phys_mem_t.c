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
#include "string.h"

int phys_memory_test(int trials, double perc_warn_ok)
{
	int foo,  bar;
	int foo2,  bar2;
	time_t	t0, t1;
	int		return_val = 0;
	int		bad_string = 0;
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

	if (((double)num_warnings/(double)num_tests) > perc_warn_ok) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Warning perc_warn_ok exceeded (%2f\% warnings > "
							"%2f\% perc_warn_ok) .\n", 
							((double)num_warnings/(double)num_tests)*100, perc_warn_ok*100);
			return_val = return_val | 1;
	}
	return return_val;
}
