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

/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/


#if defined(WIN32)

int
calc_virt_memory()
{
	MEMORYSTATUS status;
	GlobalMemoryStatus(&status);
	return (int)status.dwAvailPageFile/1024;
}

#elif defined(LINUX)

/* On Linux, we just open /proc/meminfo and read the answer */
calc_virt_memory()
{
	FILE	*proc;
	int	free_swap;
	char	tmp_c[20];
	int	tmp_i;

	proc = fopen("/proc/meminfo", "r");
	if(!proc)	{
		return -1;
	}
	
	fscanf(proc, "%s %s %s %s %s %s", tmp_c, tmp_c, tmp_c, tmp_c, tmp_c, &tmp_c);
	fscanf(proc, "%s %d %d %d %d %d %d", tmp_c, &tmp_i, &tmp_i, &tmp_i, &tmp_i, &tmp_i, &tmp_i);
	fscanf(proc, "%s %d %d %d", tmp_c, &tmp_i, &tmp_i, &free_swap);
	fclose(proc);

	return free_swap / 1024;
}

#elif defined(HPUX)

/* The HPUX code here grabs the amount of free swap in Kbyes.
 * This code doesn't require being root.  -Mike Yoder 7-16-98*/
#include <sys/pstat.h>

int
calc_virt_memory()
{
	int pagesize;
	struct pst_static s;
	struct pst_dynamic d;
  
	if (pstat_getstatic(&s, sizeof(s), (size_t)1, 0) != -1) {
		pagesize = s.page_size / 1024;   /* it's right here.... */
	} else {
		return -1;
	}
  
	if ( pstat_getdynamic ( &d, sizeof(d), (size_t)1, 0) != -1 ) {
		return d.psd_vm * pagesize;
	} else {
		return -1;
	}	
}

#elif defined(IRIX)

#include <sys/stat.h>
#include <sys/swap.h>

close_kmem() {}
calc_virt_memory()
{
	int freeswap;
	if( swapctl(SC_GETFREESWAP, &freeswap) == -1 ) {
		return -1;
	}
	return( freeswap / 2 );
}

#elif defined(OSF1)

#include <net/route.h>
#include <sys/mbuf.h>
#include <sys/table.h>
int
calc_virt_memory()
{
	struct tbl_swapinfo	swap;
	if( table(TBL_SWAPINFO,-1,(char *)&swap,1,sizeof(swap)) < 0 ) {
		return -1;
	}
	return (swap.free * NBPG) / 1024;
}

#endif /* !defined(WIN32) */
