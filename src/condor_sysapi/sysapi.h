#ifndef SYSAPI_H
#define SYSAPI_H

#include "condor_common.h"

BEGIN_C_DECLS

/* How to get the physical memory on a machine, answer in Megs */
int sysapi_phys_memory_raw();
int sysapi_phys_memory();

/* How to get the free disk blocks from a full pathname, answer in KB */
int sysapi_diskspace_raw(const char *filename);
int sysapi_diskspace(const char *filename);

/* return the number of cpus there on a machine */
int sysapi_ncpus_raw();
int sysapi_ncpus();

/* calculate the number of mips the machine is. Even though this is a user
	thing and on platforms like NT it is done by hand, it goes in the sysapi.
	The reasoning is that you are still asking for a fundamental parameter
	from the machine, and even if there isn't OS support for it, it is
	still a lowlevel number. */
int sysapi_mips_raw();
int sysapi_mips();

/* as above, but for kflops */
int sysapi_kflops_raw();
int sysapi_kflops();

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

END_C_DECLS

#endif
