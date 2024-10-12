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


#ifndef __CONDOR_SYSAPI_H__
#define __CONDOR_SYSAPI_H__

#include <string>

#include "condor_sockaddr.h"

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
long long sysapi_reserve_for_fs();
long long sysapi_disk_space_raw(const char *filename);
long long sysapi_disk_space(const char *filename);

/* get ncpus without making any calls to config system, this uses cached values */
void sysapi_ncpus_raw(int * num_cpus, int * num_hyperthreads);
/* For backward compatibility, this is an alias for the above call */
void sysapi_ncpus(int * num_cpus, int * num_hyperthreads);
/* call call cpu/hyperthread detection code.  this should only be called once per process. */
void sysapi_detect_cpu_cores(int * num_cpus, int * num_hyperthreads);


#if 0 // removed from condor_utils to reduce shadow memory use
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
#endif

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
const char* sysapi_opsys_versioned(void);
        int sysapi_opsys_version(void);
const char* sysapi_opsys_name(void);
const char* sysapi_opsys_long_name(void);
const char* sysapi_opsys_short_name(void);
        int sysapi_opsys_major_version(void);
const char* sysapi_opsys_legacy(void);
void sysapi_opsys_dump(int category);

// temporary attributes for raw utsname info
const char* sysapi_utsname_sysname(void);
const char* sysapi_utsname_nodename(void);
const char* sysapi_utsname_release(void);
const char* sysapi_utsname_version(void);
const char* sysapi_utsname_machine(void);

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
const char *sysapi_translate_arch( const char *machine, const char *sysname );

/* Generic methods for opsys params */ 
const char * sysapi_find_opsys_versioned( const char *opsys_short_name, int opsys_major_version );
int sysapi_translate_opsys_version( const char *opsys_long_name );
int sysapi_find_major_version( const char *opsys_long_name );

/* OS-specific methods for opsys params */
void sysapi_get_darwin_info(void);
const char * sysapi_get_bsd_info(const char *opsys_short_name, const char *release); 
const char * sysapi_get_linux_info(void);
const char * sysapi_find_linux_name( const char *opsys_long_name );
const char * sysapi_get_unix_info( const char *sysname, const char *release, const char *version );
void sysapi_get_windows_info( void );

/* set appropriate resource limits on each platform */
void sysapi_set_resource_limits( int stack_size );

/* determine a canonical kernel version */
const char* sysapi_kernel_version_raw( void );
const char* sysapi_kernel_version( void );

/* determine the instruction set extensions on x86 machines */
/* Would like to just use a classad here, but were in a 
 * classad-free layer */
struct sysapi_cpuinfo {
	sysapi_cpuinfo() :
		model_no(-1), family(-1), cache(-1), initialized(false) {}
	std::string processor_flags;
	std::string processor_flags_full;
	std::string processor_microarch;
	int model_no;
	int family;
	int cache;
	bool initialized;
};

const struct sysapi_cpuinfo *sysapi_processor_flags( void );

/* Produce a unique identifier for the disk partition containing the
   specified path.  Caller should free result.  The caller could use
   the result, for example, to see if two paths are on the same disk
   partition or not.
   Returns true on success.
*/
int sysapi_partition_id_raw(char const *path,char **result);
int sysapi_partition_id(char const *path,char **result);


#include <string>
#include <vector>

struct NetworkDeviceInfo {
	std::string name;
	condor_sockaddr addr;
	bool is_up;
};

bool sysapi_get_network_device_info(std::vector<NetworkDeviceInfo> &devices, bool want_ipv4, bool want_ipv6);

void sysapi_clear_network_device_info_cache();

/* determine if a linux version is version X or newer */
bool sysapi_is_linux_version_atleast(const char *version_to_check);

#ifdef LINUX
/* enum to represent the type of capability set mask we want to return*/
enum LinuxCapsMaskType {
	Linux_permittedMask,
	Linux_inheritableMask,
	Linux_effectiveMask,
};
/*	Return a 64 bit mask of linux process capabilites for a process passed
*	via pid. Also, takes optional argument for mask type based on above enum.
*	If no mask type is specified it will return the Effective capability mask.
*/
uint64_t sysapi_get_process_caps_mask(int pid, LinuxCapsMaskType type = Linux_effectiveMask);
#endif /* ifdef LINUX */

#endif


