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
#include "sysapi.h"

/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/

/* If for some reason, the call fails, then -1 is returned */

#if defined(WIN32)

int
sysapi_swap_space_raw()
{
	MEMORYSTATUS status;
	sysapi_internal_reconfig();
	GlobalMemoryStatus(&status);
	return (int) (status.dwAvailPageFile/1024L);
		
}

#elif defined(LINUX)

/* On Linux, we just open /proc/meminfo and read the answer */
int
sysapi_swap_space_raw()
{
	FILE	*proc;
	double	free_swap;
	char	tmp_c[20];
	int	tmp_i;

	sysapi_internal_reconfig();
	proc = fopen("/proc/meminfo", "r");
	if(!proc)	{
		dprintf(D_ALWAYS,
			"sysapi_swap_space_raw(): Could not open /proc/meminfo\n");
		return -1;
	}
	
	fscanf(proc, "%s %s %s %s %s %s", tmp_c, tmp_c, tmp_c, tmp_c, tmp_c, tmp_c);
	fscanf(proc, "%s %d %d %d %d %d %d", tmp_c, &tmp_i, &tmp_i, &tmp_i, &tmp_i, &tmp_i, &tmp_i);
	fscanf(proc, "%s %d %d %lf", tmp_c, &tmp_i, &tmp_i, &free_swap);
	fclose(proc);

	/* check for overflow of an int */

	free_swap /= 1024.0;

	if (free_swap > INT_MAX)
	{
		return INT_MAX;
	}

	return (int)free_swap;
}

#elif defined(HPUX)

/* here's the deal:  Using any number in pst_getdynamic seems to be
   *way* to small for our purposes.  Those silly writers of HPUX return
   'free memory paging space' (a small region of physical system memory 
   that can be used for paging if all other swap areas are full 
   (aka "psuedo-swap")) in the pst_vm field returned in pstat_getdynamic.
   This number is wrong.  What we *really* want is the sum of the free
   space in 'dev' (the swap device) and 'localfs' (any file system 
   swapping set up).  So....

   I can use pstat_getswap for this.  It returns a number of structures, 
   one for each "swap pool".  These structures will either be 
   "Block Device Fields" or "File System Fields" (dev or localfs).  I'll
   sum up their 'pss_nfpgs' fields, multiply by pagesize,  and return.

   On the cae HPs, two swap spaces are returned, one of each type.
   The localfs 'nfpgs' field is 0 on cae machines.  Grrr.  Hopefully, it
   might work somewhere else.  Who knows...

   For more information, see /usr/include/sys/pstat.h and the
   pstat and swapinfo manual pages.

   -Mike Yoder 9-28-98
*/
#include <sys/pstat.h>

int
sysapi_swap_space_raw ()
{
  int pagesize;  /* in kB */
  double virt_mem_size = 0;  /* the so-called virtual memory size we seek */
  struct pst_static s;
  struct pst_swapinfo pss[10];
  int count, i;
  int idx = 0; /* index within the context */

	sysapi_internal_reconfig();
  if (pstat_getstatic(&s, sizeof(s), (size_t)1, 0) != -1) {
    pagesize = s.page_size / 1024;   /* it's right here.... */
  }
  else {
    dprintf ( D_ALWAYS,
		"sysapi_swap_space_raw(): Error getting pagesize. Errno = %d\n",errno);
    return -1;
  }

  /* loop until count == 0, will occur when we've got 'em all */
  while ((count = pstat_getswap(pss, sizeof(pss[0]), 10, idx)) > 0) {
    /* got 'count' entries.  process them. */

    for ( i = 0 ; i < count ; i++ ) {

      /* first ensure that it's enabled */
      if ( (pss[i].pss_flags & 1) == 1 ) {
        /* now make sure it's a SW_BLOCK or FS_BLOCK*/
        /* FS_BLOCK has always returned a 0 for pss_nfpgs on cae hpuxs.
	   Grrr.  It's here in case it works somewhere else. */
	if (((pss[i].pss_flags & 2) == 2) || ((pss[i].pss_flags & 4) == 4)) {
          /* add free pages to total */
          virt_mem_size += (double)pss[i].pss_nfpgs * (double)pagesize;
        }
      }
    }

    idx = pss[count-1].pss_idx+1;
  }

  if (count == -1)
    dprintf ( D_ALWAYS,
		"sysapi_swap_space_raw(): Error in pstat_getswap().  Errno = %d\n",
		errno );

	/* under HP-UX, it is considered an error if virt_mem_size == 0 */
  if ( virt_mem_size != 0 ) {
  	if (virt_mem_size > INT_MAX) {
		return INT_MAX;
	}
    return (int)virt_mem_size;
  } else {
    /* Print an error */
    dprintf ( D_ALWAYS, 
		"sysapi_swapspace_raw(): Error virt_mem_size == 0.\n");
    return -1;
  }
}

#elif defined(IRIX)

#include <sys/stat.h>
#include <sys/swap.h>
#include <sys/types.h>

close_kmem() {}

int
sysapi_swap_space_raw()
{
	off_t freeswap;
	sysapi_internal_reconfig();
	if( swapctl(SC_GETFREESWAP, &freeswap) == -1 ) {
		dprintf(D_ALWAYS, 
			"sysapi_swap_space_raw(): swapctl failed. Errno = %d\n", errno);
		return -1;
	}
	/* freeswap is in 512 byte blocks, so convert it to 1K blocks */
	freeswap /= 2;

	/* check it for overflow of an int */
	if (freeswap > INT_MAX)
		return INT_MAX;

	return (int)freeswap;
}

#elif defined(OSF1)

#include <net/route.h>
#include <sys/mbuf.h>
#include <sys/table.h>

int
sysapi_swap_space_raw()
{
	struct tbl_swapinfo	swap;
	double freeswap;

	sysapi_internal_reconfig();
	if( table(TBL_SWAPINFO,-1,(char *)&swap,1,sizeof(swap)) < 0 ) {
		return -1;
	}

	/* hopefully, this shouldn't overflow a double */
	freeswap = ((double)swap.free * (double)NBPG) / (double)1024;

	if (freeswap > INT_MAX)
		return INT_MAX;

	return (int)freeswap;
}

#elif defined(Solaris)

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/swap.h>

/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/
int
sysapi_swap_space_raw()
{
	struct anoninfo 	ai;
	double avail;
	int factor;

	sysapi_internal_reconfig();

	factor = sysconf(_SC_PAGESIZE) >> 10;

	memset( &ai, 0, sizeof(ai) );
	if( swapctl(SC_AINFO, &ai) >= 0 ) {
		avail = (double)(ai.ani_max - ai.ani_resv) * (double)factor;
		if (avail > INT_MAX) {
			return INT_MAX;
		}
		return (int)avail;
	} else {
		dprintf( D_FULLDEBUG, "swapctl call failed, errno = %d\n", errno );
		return -1;
	}
}

#endif /* !defined(WIN32) */


/* the cooked version of the function */
int
sysapi_swap_space(void)
{
	sysapi_internal_reconfig();

	return sysapi_swap_space_raw();
}



