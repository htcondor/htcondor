/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#ifndef SYSAPI_H
#define SYSAPI_H


BEGIN_C_DECLS

/* How to get the physical memory on a machine, answer in Megs */
int sysapi_phys_memory_raw(void);
int sysapi_phys_memory(void);

/* How to get the free disk blocks from a full pathname, answer in KB */
int sysapi_disk_space_raw(const char *filename);
int sysapi_disk_space(const char *filename);

/* return the number of cpus there on a machine */
int sysapi_ncpus_raw(void);
int sysapi_ncpus(void);

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
void sysapi_test_dump_all(int, char *[]);
void sysapi_test_dump_internal_vars(void);
void sysapi_test_dump_functions(void);

/* tranlate between uname-type or LDAP entry values and Condor values */
char * sysapi_translate_arch( char *machine, char *sysname );
char *sysapi_translate_opsys( char *sysname, char *release, char *version );

/* set appropriate resource limits on each platform */
void sysapi_set_resource_limits( void );

/* check the magic number of the given executable */
int sysapi_magic_check( char* executable );

/* make sure the given executable is linked with Condor */
int sysapi_symbol_main_check( char* executable );

END_C_DECLS

#endif
