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

/* here's the deal:  Using any number in pst_getdynamic seems to be
   *way* to small for our purposes.  I don't know why.  I have found
   another way to get a number for virtual memory:  pst_swapinfo.  When
   this is called, one "pool" of swap space on the system is described.
   I don't know how many there will be; I don't know how this will vary
   from system to system.  There are two types of swap pools:
   Block Device Fields and File System Fields.  I'm going to ignore the
   File System Fields.  The number I will return is the sum of the
   pss_nfpgs (number of free pages in pool) * pagesize across all
   Block Device Fields.  Sound confusing enough?

   On the cae HPs, two swap spaces are returned.  One is the FS pool.

   For more information, see /usr/include/sys/pstat.h and the
   pstat manual pages.

   -Mike Yoder 9-28-98
*/
#include <sys/pstat.h>

int
calc_virt_memory ()
{
  int pagesize;  /* in kB */
  long virt_mem_size = 0;  /* the so-called virtual memory size we seek */
  struct pst_static s;
  struct pst_swapinfo pss[10];
  int count, i;
  int idx = 0; /* index within the context */

  if (pstat_getstatic(&s, sizeof(s), (size_t)1, 0) != -1) {
    pagesize = s.page_size / 1024;   /* it's right here.... */
  }
  else {
    dprintf ( D_ALWAYS, "Error getting pagesize.  Errno = %d\n", errno );
    return 100000;
  }


  /* loop until count == 0, will occur when we've got 'em all */
  while ((count = pstat_getswap(pss, sizeof(pss[0]), 10, idx)) > 0) {
    /* got 'count' entries.  process them. */

    for ( i = 0 ; i < count ; i++ ) {

      /* first ensure that it's enabled */
      if ( (pss[i].pss_flags & 1) == 1 ) {
        /* now make sure it's a SW_BLOCK */
        if ( (pss[i].pss_flags & 2) == 2 ) {
          /* add free pages to total */
          virt_mem_size += pss[i].pss_nfpgs * pagesize;
        }
      }
    }

    idx = pss[count-1].pss_idx+1;
  }

  if (count == -1)
    dprintf ( D_ALWAYS, "Error in pstat_getswap().  Errno = %d\n", errno );

  if ( virt_mem_size != 0 )
    return virt_mem_size;
  else {
    /* Print an error and guess.  :-) */
    dprintf ( D_ALWAYS, "Error determining virt_mem_size.  Guessing 100MB.\n");
    return 100000;
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




