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
#include "string.h"
#include "test.h"

#define		ARCH			1
#define		FREE_FS_BLOCKS	2
#define		IDLE_TIME		4
#define		KFLOPS			8
#define		LAST_X_EVENT	16
#define		LOAD_AVG		32
#define		MIPS			64
#define		NCPUS			128
#define		PHYS_MEM		256
#define		VIRT_MEM		512
#define		DUMP			1024

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
	const char *qux = NULL;
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
sysapi_test_dump_all(int argc, char** argv)
{
	int foo;
	int return_val = 0;
	int	tests = 0;
	int i;
	int print_help = 0;

	if (argc <= 1)
		tests = 0xffff;
	for (i=1; i<argc; i++) {
		if (strcmp(argv[i], "--arch") == 0)
			tests |= ARCH;
		else if (strcmp(argv[i], "--dump") == 0)
			tests |= DUMP;
		else if (strcmp(argv[i], "--free_fs_blocks") == 0)
			tests |= FREE_FS_BLOCKS;
		else if (strcmp(argv[i], "--idle_time") == 0)
			tests |= IDLE_TIME;
		else if (strcmp(argv[i], "--kflops") == 0)
			tests |= KFLOPS;
		else if (strcmp(argv[i], "--last_x_event") == 0)
			tests |= LAST_X_EVENT;
		else if (strcmp(argv[i], "--load_avg") == 0)
			tests |= LOAD_AVG;
		else if (strcmp(argv[i], "--mips") == 0)
			tests |= MIPS;
		else if (strcmp(argv[i], "--ncpus") == 0)
			tests |= NCPUS;
		else if (strcmp(argv[i], "--phys_mem") == 0)
			tests |= PHYS_MEM;
		else if (strcmp(argv[i], "--virt_mem") == 0)
			tests |= VIRT_MEM;
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			print_help = 1;
		else {
			printf("%s is not an understood option. ", argv[i]);
			print_help = 1;
		}

		if (print_help != 0) {
			printf("Please use zero or more or:\n");
			printf("--arch\n");
			printf("--dump\n");
			printf("--free_fs_blocks\n");
			printf("--idle_time\n");
			printf("--kflops\n");
			printf("--last_x_event\n");
			printf("--load_avg\n");
			printf("--mips\n");
			printf("--ncpus\n");
			printf("--phys_mem\n");
			printf("--virt_mem\n");
			return;
		}
	}

	if ((tests & DUMP) == DUMP) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN SysAPI DUMP!\n");
		sysapi_test_dump_internal_vars();
		sysapi_reconfig();
		sysapi_test_dump_internal_vars();
		//sysapi_test_dump_functions();
		dprintf(D_ALWAYS, "SysAPI: END SysAPI DUMP!\n\n");
	}

	if ((tests & ARCH) == ARCH) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN ARCH_TEST:\n");
		foo = 0;
		foo = arch_test(500);
		dprintf(D_ALWAYS, "SysAPI: END ARCH_TEST:\n");
		return_val |= foo;
		if (foo == 0)
			dprintf(D_ALWAYS, "SysAPI: Passed ARCH_TEST.\n\n");
		else
			dprintf(D_ALWAYS, "SysAPI: Failed ARCH_TEST.\n\n");
	}


	if ((tests & FREE_FS_BLOCKS) == FREE_FS_BLOCKS) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN FREE_FS_BLOCKS_TEST:\n");
		foo = 0;
		foo = free_fs_blocks_test(5, .10, .1);
		dprintf(D_ALWAYS, "SysAPI: END FREE_FS_BLOCKS_TEST:\n");
		return_val |= foo;
		if (foo == 0)
			dprintf(D_ALWAYS, "SysAPI: Passed FREE_FS_BLOCKS_TEST.\n\n");
		else
			dprintf(D_ALWAYS, "SysAPI: Failed FREE_FS_BLOCKS_TEST.\n\n");
	}
	

	if ((tests & IDLE_TIME) == IDLE_TIME) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN IDLE_TIME_TEST:\n");
		foo = 0;
		foo = idle_time_test(5, 5, 1, 1);
		dprintf(D_ALWAYS, "SysAPI: END IDLE_TIME_TEST:\n");
		return_val |= foo;
		if (foo == 0)
			dprintf(D_ALWAYS, "SysAPI: Passed IDLE_TIME_TEST.\n\n");
		else
			dprintf(D_ALWAYS, "SysAPI: Failed IDLE_TIME_TEST.\n\n");
	}

	
	if ((tests & KFLOPS) == KFLOPS) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN KFLOPS_TEST:\n");
		foo = 0;
		foo = kflops_test(5, .05, .2);
		dprintf(D_ALWAYS, "SysAPI: END KFLOPS_TEST:\n");
		return_val |= foo;
		if (foo == 0)
			dprintf(D_ALWAYS, "SysAPI: Passed KFLOPS_TEST.\n\n");
		else
			dprintf(D_ALWAYS, "SysAPI: Failed KFLOPS_TEST.\n\n");
	}


	if ((tests & LAST_X_EVENT) == LAST_X_EVENT) {
		dprintf(D_ALWAYS, "SysAPI: SKIPPING LASE_X_EVENT_TEST: internally used variable only.\n\n");
	}
	

	if ((tests & LOAD_AVG) == LOAD_AVG) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN LOAD_AVG_TEST:\n");
		foo = 0;
		foo = load_avg_test(3, 5, 20, 1);
		dprintf(D_ALWAYS, "SysAPI: END LOAD_AVG_TEST:\n");
		return_val |= foo;
		if (foo == 0)
			dprintf(D_ALWAYS, "SysAPI: Passed LOAD_AVG_TEST.\n\n");
		else
			dprintf(D_ALWAYS, "SysAPI: Failed LOAD_AVG_TEST.\n\n");
	}

	
	if ((tests & MIPS) == MIPS) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN MIPS_TEST:\n");
		foo = 0;
		foo = mips_test(5, .05, .2);
		dprintf(D_ALWAYS, "SysAPI: END MIPS_TEST:\n");
		return_val |= foo;
		if (foo == 0)
			dprintf(D_ALWAYS, "SysAPI: Passed MIPS_TEST.\n\n");
		else
			dprintf(D_ALWAYS, "SysAPI: Failed MIPS_TEST.\n\n");
	}
	

	if ((tests & NCPUS) == NCPUS) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN NUMBER_CPUS_TEST:\n");
		foo = 0;
		foo = ncpus_test(500, 0);
		dprintf(D_ALWAYS, "SysAPI: END NUMBER_CPUS_TEST:\n");
		return_val |= foo;
		if (foo == 0)
			dprintf(D_ALWAYS, "SysAPI: Passed NUMBER_CPUS_TEST.\n\n");
		else
			dprintf(D_ALWAYS, "SysAPI: Failed NUMBER_CPUS_TEST.\n\n");
	}
	

	if ((tests & PHYS_MEM) == PHYS_MEM) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN PHYSICAL_MEMORY_TEST:\n");
		foo = 0;
		foo = phys_memory_test(500, 0);
		dprintf(D_ALWAYS, "SysAPI: END PHYSICAL_MEMORY_TEST:\n");
		return_val |= foo;
		if (foo == 0)
			dprintf(D_ALWAYS, "SysAPI: Passed PHYSICAL_MEMORY_TEST.\n\n");
		else
			dprintf(D_ALWAYS, "SysAPI: Failed PHYSICAL_MEMORY_TEST.\n\n");
	}

	
	if ((tests & VIRT_MEM) == VIRT_MEM) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN VIRTUAL_MEMORY_TEST:\n");
		foo = 0;
		foo = virt_memory_test(10, .01, .05);
		dprintf(D_ALWAYS, "SysAPI: END VIRTUAL_MEMORY_TEST:\n");
		return_val |= foo;
		if (foo == 0)
			dprintf(D_ALWAYS, "SysAPI: Passed VIRTUAL_MEMORY_TEST.\n\n");
		else
			dprintf(D_ALWAYS, "SysAPI: Failed VIRTUAL_MEMORY_TEST.\n\n");
	}
}
