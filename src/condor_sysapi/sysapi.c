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



