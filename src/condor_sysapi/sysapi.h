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
long long sysapi_disk_space_raw(const char *filename);
long long sysapi_disk_space(const char *filename);

/* return the number of cpus there on a machine */
int sysapi_ncpus_raw(void);
int sysapi_ncpus(void);
/* get raw ncpus without making any calls to config system */
void sysapi_ncpus_raw_no_param(int *num_cpus,int *num_hyperthread_cpus);

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
const char * sysapi_get_darwin_info(void);  
int sysapi_find_darwin_major_version( const char *opsys_long_name );
const char * sysapi_find_darwin_opsys_name( int opsys_major_version );
const char * sysapi_get_bsd_info(const char *opsys_short_name, const char *release); 
const char * sysapi_get_linux_info(void);
const char * sysapi_find_linux_name( const char *opsys_long_name );
const char * sysapi_get_unix_info( const char *sysname, const char *release, const char *version, int append_version );
void sysapi_get_windows_info( void );

/* set appropriate resource limits on each platform */
void sysapi_set_resource_limits( int stack_size );

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

/* determine the instruction set extensions on x86 machines */
const char* sysapi_processor_flags_raw( void );
const char* sysapi_processor_flags( void );

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

#if defined(__cplusplus)

#include <string>
#include <vector>

class NetworkDeviceInfo {
public:
	NetworkDeviceInfo(char const *the_name,char const *the_ip, bool the_up):
		m_name(the_name),
		m_ip(the_ip),
		m_up(the_up)
	{
	}

	NetworkDeviceInfo(NetworkDeviceInfo const &other):
		m_name(other.m_name),
		m_ip(other.m_ip),
		m_up(other.m_up)
	{
	}

	char const *name() { return m_name.c_str(); }
	char const *IP() { return m_ip.c_str(); }
	bool is_up() const { return m_up; }

private:
	std::string m_name;
	std::string m_ip;
	bool m_up;
};

bool sysapi_get_network_device_info(std::vector<NetworkDeviceInfo> &devices);

void sysapi_clear_network_device_info_cache();

#endif // __cplusplus

#endif


