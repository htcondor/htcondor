#include "sysapi.h"

/* 
	this is a temporary file to map all of the old function names to the new
	API system before I go through all of the old code and replace them.
	The sysapi is a comletely c linkaged entity.
*/

/* also note that these return the cooked versions of the functions */

int
calc_phys_memory()
{
	return sysapi_phys_memory();
}

int
free_fs_blocks(const char *filename)
{
	return sysapi_diskspace(filename);
}

int
calc_ncpus()
{
	return sysapi_ncpus();
}

int
calc_mips()
{
	return sysapi_mips();
}

int
calc_kflops()
{
	return sysapi_kflops();
}

void
calc_idle_time(time_t *m_idle, time_t *m_console_idle)
{
	sysapi_idle_time(m_idle, m_console_idle);
}

float
calc_load_avg()
{
	return sysapi_load_avg();
}



