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

/* this function will dump out the state of the _sysapi_* variables, then
   call sysapi_reconfig(), then dump out the stateof the variables again, 
   then actually call all of the sysapi_*(_raw)? functions and dump what
   those functions return. All of these functions are entry points and
   can be called seperately if need be to test stuff */

/* this function will dump the state of the cached variables in reconfig.C */
extern "C" void
sysapi_test_dump_internal_vars(void)
{
	dprintf(D_ALWAYS, "SysAPI: Dumping %s internal variables\n", 
		_sysapi_config==TRUE?"initialized":"uninitialized");

	dprintf(D_ALWAYS, "SysAPI: _sysapi_config = %s\n", 
		_sysapi_config==TRUE?"TRUE":"FALSE");

	dprintf(D_ALWAYS, 
		"SysAPI: _sysapi_console_devices = %p\n",_sysapi_console_devices);
	dprintf(D_ALWAYS, 
		"SysAPI: _sysapi_last_x_event = %d\n", _sysapi_last_x_event);
	dprintf(D_ALWAYS, "SysAPI: _sysapi_reserve_afs_cache = %s\n", 
		_sysapi_reserve_afs_cache==TRUE?"TRUE":"FALSE");
	dprintf(D_ALWAYS, 
		"SysAPI: _sysapi_reserve_disk = %d\n", _sysapi_reserve_disk);
	dprintf(D_ALWAYS, "SysAPI: _sysapi_startd_has_bad_utmp = %s\n",
		_sysapi_startd_has_bad_utmp==TRUE?"TRUE":"FALSE");
}

/* this function calls every function in sysapi that makes sense to call and
   prints out its value */
extern "C" void
sysapi_test_dump_functions(void)
{
	int foo = 0;
	float bar = 0;
	char *qux = NULL;
	time_t t0, t1;

	dprintf(D_ALWAYS, "SysAPI: Calling SysAPI functions....\n");

	foo = sysapi_phys_memory_raw();
	dprintf(D_ALWAYS, "SysAPI: sysapi_phys_memory_raw() -> %d\n", foo);
	foo = sysapi_phys_memory();
	dprintf(D_ALWAYS, "SysAPI: sysapi_phys_memory() -> %d\n", foo);

	foo = sysapi_disk_space_raw("/");
	dprintf(D_ALWAYS, "SysAPI: sysapi_disk_space_raw() -> %d\n", foo);
	foo = sysapi_disk_space("/");
	dprintf(D_ALWAYS, "SysAPI: sysapi_disk_space() -> %d\n", foo);

	foo = sysapi_ncpus_raw();
	dprintf(D_ALWAYS, "SysAPI: sysapi_ncpus_raw() -> %d\n", foo);
	foo = sysapi_ncpus();
	dprintf(D_ALWAYS, "SysAPI: sysapi_ncpus() -> %d\n", foo);

	foo = sysapi_mips_raw();
	dprintf(D_ALWAYS, "SysAPI: sysapi_mips_raw() -> %d\n", foo);
	foo = sysapi_mips();
	dprintf(D_ALWAYS, "SysAPI: sysapi_mips() -> %d\n", foo);

	foo = sysapi_kflops_raw();
	dprintf(D_ALWAYS, "SysAPI: sysapi_kflops_raw() -> %d\n", foo);
	foo = sysapi_kflops();
	dprintf(D_ALWAYS, "SysAPI: sysapi_kflops() -> %d\n", foo);

	sysapi_idle_time_raw(&t0, &t1);
	dprintf(D_ALWAYS,"SysAPI: sysapi_idle_time_raw() -> (%f,%f)\n",(float)t0,(float)t1);
	sysapi_idle_time(&t0, &t1);
	dprintf(D_ALWAYS, "SysAPI: sysapi_idle_time() -> (%f,%f)\n", (float)t0, (float)t1);

	bar = sysapi_load_avg_raw();
	dprintf(D_ALWAYS, "SysAPI: sysapi_load_avg_raw() -> %f\n", bar);
	bar = sysapi_load_avg();
	dprintf(D_ALWAYS, "SysAPI: sysapi_load_avg() -> %f\n", bar);

	qux = sysapi_condor_arch();
	dprintf(D_ALWAYS, "SysAPI: sysapi_condor_arch -> %s\n", qux);

	qux = sysapi_uname_arch();
	dprintf(D_ALWAYS, "SysAPI: sysapi_uname_arch -> %s\n", qux);

	qux = sysapi_opsys();
	dprintf(D_ALWAYS, "SysAPI: sysapi_opsys -> %s\n", qux);

	foo = sysapi_swap_space_raw();
	dprintf(D_ALWAYS, "SysAPI: sysapi_swap_space_raw() -> %d\n", foo);
	foo = sysapi_swap_space();
	dprintf(D_ALWAYS, "SysAPI: sysapi_swap_space() -> %d\n", foo);
}

/* the main entry function, this will do all the magic */
extern "C" void
sysapi_test_dump_all(void)
{
	dprintf(D_ALWAYS, "SysAPI: BEGIN SysAPI DUMP!\n");
	sysapi_test_dump_internal_vars();
	sysapi_reconfig();
	sysapi_test_dump_internal_vars();

	sysapi_test_dump_functions();
	dprintf(D_ALWAYS, "SysAPI: END SysAPI DUMP!\n");
}
