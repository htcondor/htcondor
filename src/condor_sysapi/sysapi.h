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


#ifndef SYSAPI_H
#define SYSAPI_H


BEGIN_C_DECLS

/* For debugging */
#if defined(LINUX)
typedef struct {
	const char	*file;
	long		offset;
	int			found_processors;
	int			found_hthreads;
	int			found_ncpus;
	int			debug;
} SysapiProcCpuinfo;
extern SysapiProcCpuinfo	_SysapiProcCpuinfo;
#endif

/* How to get the physical memory on a machine, answer in Megs */
int sysapi_phys_memory_raw(void);
int sysapi_phys_memory(void);
/* get raw phys memory without making any calls to config system */
int sysapi_phys_memory_raw_no_param(void);

/* How to get the free disk blocks from a full pathname, answer in KB */
int sysapi_disk_space_raw(const char *filename);
int sysapi_disk_space(const char *filename);

/* return the number of cpus there on a machine */
int sysapi_ncpus_raw(void);
int sysapi_ncpus(void);
/* get raw ncpus without making any calls to config system */
void sysapi_ncpus_raw_no_param(int *num_cpus,int *num_hyperthread_cpus);

/* calculate the number of mips the machine is. Even though this is a user
	thing and on platforms like NT it is done by hand, it goes in the sysapi.
	The reasoning is that you are still asking for a fundamental parameter
	from the machine, and even if there isn't OS support for it, it is
	still a lowlevel number. */
int sysapi_mips_raw(void);
int sysapi_mips(void);

/* as above, but for kflops */
int sysapi_kflops_raw(void);
int sysapi_kflops(void);

/* return the idle time on the machine in the arguments */
void sysapi_idle_time_raw(time_t *m_idle, time_t *m_console_idle);
void sysapi_idle_time(time_t *m_idle, time_t *m_console_idle);

/* some reconfigure functions that people need to know */
void sysapi_reconfig(void);
void sysapi_internal_reconfig(void);

/* if this is called, then the sysapi knows that a last_x_event has happend
	and records the time it happened, this is quite useful in idle_time.C */
void sysapi_last_xevent(void);

/* return the one minute load average on a machine */
float sysapi_load_avg_raw(void);
float sysapi_load_avg(void);

/* return information about the arch and operating system.
	The pointer returned points to a static buffer defined in the library,
	do not free it */
const char* sysapi_condor_arch(void);
const char* sysapi_uname_arch(void);
const char* sysapi_opsys(void);
const char* sysapi_uname_opsys(void);

/* return information about how many 1K blocks of swap space there are.
	If there are more 1K blocks than INT_MAX, then INT_MAX is returned */
int sysapi_swap_space_raw(void);
int sysapi_swap_space(void);

/* these are functions that spit out data about the library, useful for
	testing or debugging purposes */
int sysapi_test_dump_all(int, char *[]);
void sysapi_test_dump_internal_vars(void);
void sysapi_test_dump_functions(void);

/* tranlate between uname-type or LDAP entry values and Condor values */
const char *sysapi_translate_arch( const char *machine,
								   const char *sysname );
const char *sysapi_translate_opsys( const char *sysname,
									const char *release,
									const char *version );

/* set appropriate resource limits on each platform */
void sysapi_set_resource_limits( void );

/* check the magic number of the given executable */
int sysapi_magic_check( char* executable );

/* make sure the given executable is linked with Condor */
int sysapi_symbol_main_check( char* executable );

/* determine a canonical kernel version */
const char* sysapi_kernel_version_raw( void );
const char* sysapi_kernel_version( void );

/* determine a canonical memory model for the kernel */
const char* sysapi_kernel_memory_model_raw( void );
const char* sysapi_kernel_memory_model( void );

/* determine a checkpointing signature for a particular platform */
const char* sysapi_ckptpltfrm_raw( void );
const char* sysapi_ckptpltfrm( void );

/* determine the syscall gate address on machines where that makes sense */
const char * sysapi_vsyscall_gate_addr_raw( void );
const char * sysapi_vsyscall_gate_addr( void );

/* Produce a unique identifier for the disk partition containing the
   specified path.  Caller should free result.  The caller could use
   the result, for example, to see if two paths are on the same disk
   partition or not.
   Returns true on success.
*/
int sysapi_partition_id_raw(char const *path,char **result);
int sysapi_partition_id(char const *path,char **result);

END_C_DECLS

#endif


