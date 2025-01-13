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
#include "condor_config.h"
#include "sysapi.h"
#include "sysapi_externs.h"
#include "string.h"
#include "test.h"


// Bit-map of tests to enable
#define	ARCH			0x0001
#define	FREE_FS_BLOCKS	0x0002
#define	IDLE_TIME		0x0004
#define	KFLOPS			0x0008
#define	LAST_X_EVENT	0x0010
#define	LOAD_AVG		0x0020
#define	MIPS			0x0040
#define	NCPUS			0x0080
#define	PHYS_MEM		0x0100
#define	VIRT_MEM		0x0200
#define	DUMP			0x0400
//#define	CKPTPLTFRM		0x0800
//#define	KERN_VERS		0x1000
//#define	KERN_MEMMOD		0x2000
#define	TEST_ALL		0xFFFF
#define	TEST_NONE		0x0000

// Default test limits

// FS Free blocks
const int		FREEBLOCKS_TRIALS			= 5;		// Total # of trials
const double	FREEBLOCKS_TOLERANCE		= 0.10;		// Ratio: Max SD variation from mean
const double	FREEBLOCKS_MAX_WARN_OK		= 0.10;		// Ratio: Max warnings to allow

// KFLOPS
const int		KFLOPS_TRIALS				= 5;		// Total # of trials
const double	KFLOPS_MAX_SD_VAR			= 0.05;		// Ratio: Max SD variation from mean
const double	KFLOPS_MAX_WARN_OK			= 0.2;		// Ratio: Max warnings to allow

// MIPS
const int		MIPS_TRIALS					= 5;		// Total # of trials
const double	MIPS_MAX_SD_VAR				= 0.05;		// Ratio: Max SD variation from mean
const double	MIPS_MAX_WARN_OK			= 0.2;		// Ratio: Max warnings to allow

// Idle time
const int		IDLETIME_TRIALS				= 5;		// Total # of trials
const int		IDLETIME_INTERVAL			= 5;		// Interval time
const int		IDLETIME_TOLERANCE			= 1;		//
const double	IDLETIME_MAX_WARN_OK		= 1.0;		// Ratio: Max warnings to allow

// Load average test
const int		LOADAVG_TRIALS				= 3;		// Total # of trials
const int		LOADAVG_INTERVAL			= 5;		// Interval time
const int		LOADAVG_CHILDREN			= 20;		// Number of children
const double	LOADAVG_MAX_WARN_OK			= 1.0;		// Ratio: Max warnings to allow

// # CPUs test
const int		NCPUS_TRIALS				= 500;		// Total # of trials
const double	NCPUS_MAX_WARN_OK			= 0.0;		// Ratio: Max warnings to allow

// Physical memory test
const int		PHYSMEM_TRIALS				= 500;		// Total # of trials
const double	PHYSMEM_MAX_WARN_OK			= 0.0;		// Ratio: Max warnings to allow

// Physical memory test
const int		VIRTMEM_TESTBLOCK_SIZE		= 10;		// Total # of trials
const double	VIRTMEM_MAX_SD_VAR			= 0.01;		// Ratio: Max SD variation from mean
const double	VIRTMEM_MAX_FAIL_OK			= 0.05;		// Ratio: Max failures to allow


/* this function will dump out the state of the _sysapi_* variables, then
   call sysapi_reconfig(), then dump out the stateof the variables again,
   then actually call all of the sysapi_*(_raw)? functions and dump what
   those functions return. All of these functions are entry points and
   can be called seperately if need be to test stuff */

/* this function will dump the state of the cached variables in reconfig.C */
void
sysapi_test_dump_internal_vars(void)
{
	dprintf(D_ALWAYS, "SysAPI: Dumping %s internal variables\n",
		_sysapi_config==TRUE?"initialized":"uninitialized");

	dprintf(D_ALWAYS, "SysAPI: _sysapi_config = %s\n",
		_sysapi_config==TRUE?"TRUE":"FALSE");

	dprintf(D_ALWAYS,
		"SysAPI: _sysapi_console_devices = %p\n",_sysapi_console_devices);
	dprintf(D_ALWAYS,
		"SysAPI: _sysapi_last_x_event = %d\n", static_cast<int>(_sysapi_last_x_event));
	dprintf(D_ALWAYS,
		"SysAPI: _sysapi_reserve_disk = %d\n", _sysapi_reserve_disk);
	dprintf(D_ALWAYS, "SysAPI: _sysapi_startd_has_bad_utmp = %s\n",
		_sysapi_startd_has_bad_utmp?"TRUE":"FALSE");
}

/* this function calls every function in sysapi that makes sense to call and
   prints out its value */
void
sysapi_test_dump_functions(void)
{
	int foo = 0;
	long long loo = 0;
	float bar = 0;
	const char *qux = NULL;
	time_t t0, t1;

	dprintf(D_ALWAYS, "SysAPI: Calling SysAPI functions....\n");

	foo = sysapi_phys_memory_raw();
	dprintf(D_ALWAYS, "SysAPI: sysapi_phys_memory_raw() -> %d\n", foo);
	foo = sysapi_phys_memory();
	dprintf(D_ALWAYS, "SysAPI: sysapi_phys_memory() -> %d\n", foo);

	loo = sysapi_disk_space_raw("/");
	dprintf(D_ALWAYS, "SysAPI: sysapi_disk_space_raw() -> %" PRIi64 "\n", loo);
	loo = sysapi_disk_space("/");
	dprintf(D_ALWAYS, "SysAPI: sysapi_disk_space() -> %" PRIi64 "\n", loo);

	sysapi_ncpus_raw(&foo,NULL);
	dprintf(D_ALWAYS, "SysAPI: sysapi_ncpus_raw() -> %d\n", foo);
	sysapi_ncpus_raw(&foo, NULL);
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
int
sysapi_test_dump_all(int argc, char** argv)
{
	int foo;
	int return_val = 0;
	int	tests = TEST_NONE;
	int failed_tests = 0;
	int passed_tests = 0;
	int i;
	int print_help = 0;
#if defined(LINUX)
	const char *linux_cpuinfo_file = NULL;
	int			linux_cpuinfo_debug = 0;
	const char *linux_uname = NULL;
	int 		linux_num = -1;
	int			linux_processors = -1;
	int			linux_hthreads = -1;
	int			linux_hthreads_core = -1;
	int			linux_cpus = -1;
#endif
	int ncpus_trials = NCPUS_TRIALS;
	int skip = 0;
	const char *free_fs_dir = "/tmp";

	if (argc <= 1)
		tests = TEST_ALL;
	for (i=1; i<argc; i++) {
		if ( skip ) {
			skip--;
			continue;
		}
		if (strcmp(argv[i], "--arch") == 0)
			tests |= ARCH;
		else if (strcmp(argv[i], "--dump") == 0)
			tests |= DUMP;
		else if (strcmp(argv[i], "--free_fs_blocks") == 0) {
			tests |= FREE_FS_BLOCKS;
			if (  ( i+1 < argc ) &&
				  ( (*argv[i+1] == '/') || (*argv[i+1] == '.') )  ) {
				skip = 1;
				free_fs_dir = argv[i+1];
			}
		}
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
		else if (strcmp(argv[i], "--ncpus") == 0) {
			tests |= NCPUS;
		}
		else if (strcmp(argv[i], "--phys_mem") == 0)
			tests |= PHYS_MEM;
		else if (strcmp(argv[i], "--virt_mem") == 0)
			tests |= VIRT_MEM;
		else if ( (strcmp(argv[i], "--debug") == 0) && (i+1 < argc)  ) {
			set_debug_flags( argv[i+1], 0 );
			skip = 1;
		}
#	  if defined(LINUX)
		else if (strcmp(argv[i], "--proc_cpuinfo") == 0) {
			tests |= NCPUS;
			linux_cpuinfo_file = "/proc/cpuinfo";
			linux_cpuinfo_debug = 1;	// Turn on debugging
			linux_num = 0;
		}
		else if ( (strcmp(argv[i], "--cpuinfo_file") == 0) && (i+2 < argc)  ) {
			tests |= NCPUS;
			linux_cpuinfo_file = argv[i+1];
			linux_cpuinfo_debug = 1;	// Turn on debugging
			if ( isdigit( *argv[i+2] ) ) {
				linux_num = atoi( argv[i+2] );
			} else {
				linux_uname = argv[i+2];
			}
			skip = 2;
		}
#	  endif
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			print_help = 1;
		else {
			printf("%s is not an understood option. ", argv[i]);
			print_help = 1;
		}

		if (print_help != 0) {
			printf("Please use zero or more or:\n");
			printf("--debug <D_xxx>\n");
			printf("--arch\n");
			printf("--kern_vers\n");
			printf("--dump\n");
			printf("--free_fs_blocks [dir]\n");
			printf("--idle_time\n");
			printf("--kflops\n");
			printf("--last_x_event\n");
			printf("--load_avg\n");
			printf("--mips\n");
			printf("--ncpus\n");
			printf("--phys_mem\n");
			printf("--virt_mem\n");
#		  if defined(LINUX)
			printf("--proc_cpuinfo\n");
			printf("--cpuinfo_file <file> <uname match string>|<cpuinfo #>\n");
#		  endif
			return 0;
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
		if (foo == 0) {
			dprintf(D_ALWAYS, "SysAPI: Passed ARCH_TEST.\n\n");
			passed_tests++;
		} else {
			dprintf(D_ALWAYS, "SysAPI: Failed ARCH_TEST.\n\n");
			failed_tests++;
		}
	}


	if ((tests & FREE_FS_BLOCKS) == FREE_FS_BLOCKS) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN FREE_FS_BLOCKS_TEST:\n");
		foo = 0;
		foo = free_fs_blocks_test(free_fs_dir,
								  FREEBLOCKS_TRIALS,
								  FREEBLOCKS_TOLERANCE,
								  FREEBLOCKS_MAX_WARN_OK );
		dprintf(D_ALWAYS, "SysAPI: END FREE_FS_BLOCKS_TEST:\n");
		return_val |= foo;
		if (foo == 0) {
			dprintf(D_ALWAYS, "SysAPI: Passed FREE_FS_BLOCKS_TEST.\n\n");
			passed_tests++;
		} else {
			dprintf(D_ALWAYS, "SysAPI: Failed FREE_FS_BLOCKS_TEST.\n\n");
			failed_tests++;
		}
	}
	

	if ((tests & IDLE_TIME) == IDLE_TIME) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN IDLE_TIME_TEST:\n");
		foo = 0;
		foo = idle_time_test(IDLETIME_TRIALS,
							 IDLETIME_INTERVAL,
							 IDLETIME_TOLERANCE,
							 IDLETIME_MAX_WARN_OK );
		dprintf(D_ALWAYS, "SysAPI: END IDLE_TIME_TEST:\n");
		return_val |= foo;
		if (foo == 0) {
			dprintf(D_ALWAYS, "SysAPI: Passed IDLE_TIME_TEST.\n\n");
			passed_tests++;
		} else {
			dprintf(D_ALWAYS, "SysAPI: Failed IDLE_TIME_TEST.\n\n");
			failed_tests++;
		}
	}

	
	if ((tests & KFLOPS) == KFLOPS) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN KFLOPS_TEST:\n");
		foo = 0;
		foo = kflops_test(KFLOPS_TRIALS,
						  KFLOPS_MAX_SD_VAR,
						  KFLOPS_MAX_WARN_OK
						  );
		dprintf(D_ALWAYS, "SysAPI: END KFLOPS_TEST:\n");
		return_val |= foo;
		if (foo == 0) {
			dprintf(D_ALWAYS, "SysAPI: Passed KFLOPS_TEST.\n\n");
			passed_tests++;
		} else {
			dprintf(D_ALWAYS, "SysAPI: Failed KFLOPS_TEST.\n\n");
			failed_tests++;
		}
	}


	if ((tests & LAST_X_EVENT) == LAST_X_EVENT) {
		dprintf(D_ALWAYS, "SysAPI: SKIPPING LASE_X_EVENT_TEST: internally used variable only.\n\n");
	}
	

	if ((tests & LOAD_AVG) == LOAD_AVG) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN LOAD_AVG_TEST:\n");
		foo = 0;
		foo = load_avg_test(LOADAVG_TRIALS,
							LOADAVG_INTERVAL,
							LOADAVG_CHILDREN,
							LOADAVG_MAX_WARN_OK);
		dprintf(D_ALWAYS, "SysAPI: END LOAD_AVG_TEST:\n");
		return_val |= foo;
		if (foo == 0) {
			dprintf(D_ALWAYS, "SysAPI: Passed LOAD_AVG_TEST.\n\n");
			passed_tests++;
		} else {
			dprintf(D_ALWAYS, "SysAPI: Failed LOAD_AVG_TEST.\n\n");
			failed_tests++;
		}
	}

	
	if ((tests & MIPS) == MIPS) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN MIPS_TEST:\n");
		foo = 0;
		foo = mips_test(MIPS_TRIALS,
						MIPS_MAX_SD_VAR,
						MIPS_MAX_WARN_OK );
		dprintf(D_ALWAYS, "SysAPI: END MIPS_TEST:\n");
		return_val |= foo;
		if (foo == 0) {
			dprintf(D_ALWAYS, "SysAPI: Passed MIPS_TEST.\n\n");
			passed_tests++;
		} else {
			dprintf(D_ALWAYS, "SysAPI: Failed MIPS_TEST.\n\n");
			failed_tests++;
		}
	}
	
	/* Special test: /proc/cpuinfo on a file */
#if defined(LINUX)
	if ((tests & NCPUS) == NCPUS && linux_cpuinfo_file ) {
		bool is_proc_cpuinfo = false;

		/* set_debug_flags(NULL, D_FULLDEBUG); */
		if ( strcmp( linux_cpuinfo_file, "/proc/cpuinfo" ) == 0 ) {
			is_proc_cpuinfo = true;
			dprintf( D_ALWAYS, "Using the real /proc/cpuinfo\n" );
		}

		FILE	*fp = safe_fopen_wrapper_follow( linux_cpuinfo_file, "r", 0644 );
		if ( !fp ) {
			dprintf(D_ALWAYS, "SysAPI: Can't open cpuinfo file '%s'.\n\n",
					linux_cpuinfo_file);
			return(++failed_tests);
		}
		else {
			/* Skip 'til we find the "right" uname */
			char	buf[256];
			char	uname[256];
			int		found = 0;
			int		linenum = 1;
			int		cpuinfo_num = 0;		/* # of this cpuinfo block */

			if ( is_proc_cpuinfo ) {
				found = true;
				strcpy( uname, "" );
			}
			else {
				while( fgets( buf, sizeof(buf), fp) ) {
					linenum++;
					buf[sizeof(buf)-1] = '\0';
					if ( !strncmp( buf, "UNAME:", 6 ) && ( strlen(buf) > 6 ) ){
						cpuinfo_num++;
						if ( ( ( linux_num >= 0 ) &&
							   ( linux_num == cpuinfo_num ) ) ||
							 ( linux_uname &&
							   strstr( buf, linux_uname ) )   ) {
							strncpy( uname, buf+6, sizeof(uname) );
							found = linenum;
						}
					}
					else if ( found ) {
						if ( !strncmp( buf, "START", 5 ) && found ) {
							break;
						}
						sscanf( buf, "PROCESSORS: %d", &linux_processors );
						sscanf( buf, "HTHREADS: %d", &linux_hthreads );
						sscanf( buf, "HTHREADS_CORE: %d",
								&linux_hthreads_core );
					}
				}
			}

			// Store the current file position & file name
			_SysapiProcCpuinfo.file = linux_cpuinfo_file;
			_SysapiProcCpuinfo.debug = linux_cpuinfo_debug;
			_SysapiProcCpuinfo.offset = ftell( fp );
			fclose( fp );
			fp = NULL;

			// Calculate total "non-primary" hyper threads
			if ( ! is_proc_cpuinfo ) {
				if ( ( linux_hthreads < 0 )		  &&
					 ( linux_hthreads_core > 0 )  &&
					 ( linux_processors > 0 )  )  {
					linux_hthreads = (  linux_processors -
										( linux_processors /
										  linux_hthreads_core )  );
				}
			}

			// Calculate the total # of CPUs
			if ( linux_processors ) {
				if ( param_boolean("COUNT_HYPERTHREAD_CPUS", true) ) {
					linux_cpus = linux_processors;
				}
				else {
					linux_cpus = linux_processors - linux_hthreads;
				}
			}

			if ( !found ) {
				if ( linux_num >= 0 ) {
					dprintf(D_ALWAYS,
							"SysAPI: Can't find uname # %d in %s.\n\n",
							linux_num, linux_cpuinfo_file );
				}
				else {
					dprintf(D_ALWAYS,
							"SysAPI: Can't find uname '%s' in %s.\n\n",
							linux_uname, linux_cpuinfo_file );
				}
				return(++failed_tests);
			}
			if ( strlen( uname ) ) {
				dprintf(D_ALWAYS,
						"SysAPI: Using uname string on line %d:\n%s\n",
						found, uname );
			}
			ncpus_trials = 1;
		}
	}
#endif

	if ((tests & NCPUS) == NCPUS) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN NUMBER_CPUS_TEST:\n");
		foo = 0;
		foo = ncpus_test(ncpus_trials,
						 NCPUS_MAX_WARN_OK);
		dprintf(D_ALWAYS, "SysAPI: END NUMBER_CPUS_TEST:\n");
		return_val |= foo;
		if (foo == 0) {
			dprintf(D_ALWAYS, "SysAPI: Passed NUMBER_CPUS_TEST.\n\n");
			passed_tests++;
		} else {
			dprintf(D_ALWAYS, "SysAPI: Failed NUMBER_CPUS_TEST.\n\n");
			failed_tests++;
		}
#    if defined(LINUX)
		if ( ( linux_processors >= 0 ) &&
			 ( _SysapiProcCpuinfo.found_processors != linux_processors )  )  {
			dprintf(D_ALWAYS,
					"SysAPI/Linux: # Processors (%d) != expected (%d)\n",
					_SysapiProcCpuinfo.found_processors,
					linux_processors );
		}
		if ( ( linux_hthreads >= 0 ) &&
			 ( _SysapiProcCpuinfo.found_hthreads != linux_hthreads ) ) {
			dprintf(D_ALWAYS,
					"SysAPI/Linux: # HyperThreads (%d) != expected (%d)\n",
					_SysapiProcCpuinfo.found_hthreads,
					linux_hthreads );
		}
		if ( ( linux_cpus > 0 ) &&
			 ( _SysapiProcCpuinfo.found_ncpus != linux_cpus )  )    {
			dprintf(D_ALWAYS,
					"SysAPI/Linux: # CPUs (%d) != expected (%d)\n",
					_SysapiProcCpuinfo.found_ncpus,
					linux_cpus );
		}
		int	level = D_FULLDEBUG;
		if (linux_cpuinfo_debug) {
			level = D_ALWAYS;
		}
		dprintf( level,
				 "SysAPI: Detected %d Processors, %d HyperThreads"
				 " => %d CPUS\n",
				 _SysapiProcCpuinfo.found_processors,
				 _SysapiProcCpuinfo.found_hthreads,
				 _SysapiProcCpuinfo.found_ncpus );
				
#    endif
	}
	

	if ((tests & PHYS_MEM) == PHYS_MEM) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN PHYSICAL_MEMORY_TEST:\n");
		foo = 0;
		foo = phys_memory_test(PHYSMEM_TRIALS, PHYSMEM_MAX_WARN_OK);
		dprintf(D_ALWAYS, "SysAPI: END PHYSICAL_MEMORY_TEST:\n");
		return_val |= foo;
		if (foo == 0) {
			dprintf(D_ALWAYS, "SysAPI: Passed PHYSICAL_MEMORY_TEST.\n\n");
			passed_tests++;
		} else {
			dprintf(D_ALWAYS, "SysAPI: Failed PHYSICAL_MEMORY_TEST.\n\n");
			failed_tests++;
		}
	}

	
	if ((tests & VIRT_MEM) == VIRT_MEM) {
		dprintf(D_ALWAYS, "SysAPI: BEGIN VIRTUAL_MEMORY_TEST:\n");
		foo = 0;
		foo = virt_memory_test(VIRTMEM_TESTBLOCK_SIZE,
							   VIRTMEM_MAX_SD_VAR,
							   VIRTMEM_MAX_FAIL_OK);
		dprintf(D_ALWAYS, "SysAPI: END VIRTUAL_MEMORY_TEST:\n");
		return_val |= foo;
		if (foo == 0) {
			dprintf(D_ALWAYS, "SysAPI: Passed VIRTUAL_MEMORY_TEST.\n\n");
			passed_tests++;
		} else {
			dprintf(D_ALWAYS, "SysAPI: Failed VIRTUAL_MEMORY_TEST.\n\n");
			failed_tests++;
		}
	}
	printf("Passed tests = %d\n",passed_tests);
	return(failed_tests);
}
