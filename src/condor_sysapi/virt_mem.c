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

int
sysapi_swap_space_raw()
{
	struct sysinfo si;
	double free_swap;

	sysapi_internal_reconfig();

	if (sysinfo(&si) == -1) {
		dprintf(D_ALWAYS, 
			"sysapi_swap_space_raw(): error: sysinfo(2) failed: %d(%s)",
			errno, strerror(errno));
		return -1;
	}

	/* On Linux before 2.3.23, mem_unit was not present
		and the pad region of space in this structure appears to
		have been occupying was set to 0; units are bytes */
	if (si.mem_unit == 0) {
		si.mem_unit = 1;
	}

	/* in B */
	free_swap = (double)si.freeswap * (double)si.mem_unit;
	/* in KB */
	free_swap /= 1024.0;

	if ((int)free_swap > INT_MAX)
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

#elif defined(Darwin) || defined(CONDOR_FREEBSD)
#include <sys/sysctl.h>
int
sysapi_swap_space_raw() {
        int mib[2];
        unsigned int usermem;
        size_t len;   
        mib[0] = CTL_HW;     
        mib[1] = HW_USERMEM;        
        len = sizeof(usermem);   
        sysctl(mib, 2, &usermem, &len, NULL, 0);   
	return usermem / 1024;
}

#elif defined(AIX)

int
sysapi_swap_space_raw() {
    struct pginfo p;
    int ret;

    CLASS_SYMBOL cuat;
    struct CuAt paging_ent;
    struct CuAt *pret = NULL;
    unsigned long free_swap_space = 0;
	char buf[1024];
	char *path = NULL;

    if (odm_initialize() < 0)
    {
		/* This is quite terrible if it happens */
        dprintf(D_ALWAYS, 
			"sysapi_swap_space_raw(): Could not initialize the ODM database: "
			"%d\n", odmerrno);
		return -1;
    }

	/* remember to free this memory just before I leave this function */
    path = odm_set_path("/etc/objrepos");
	if (path == (char*)-1) /* eewww */
	{
        dprintf(D_ALWAYS, "sysapi_swap_space_raw(): Could not set class path! "
			"%d\n", odmerrno);
		return -1;
	}

	/* open up a predefined class symbol found in libcfg.a */
    cuat = odm_open_class(CuAt_CLASS);
    if (cuat == NULL)
    {
        dprintf(D_ALWAYS, "sysapi_swap_space_raw(): Could not open CuAt! %d\n",
			odmerrno);
    	if (odm_terminate() < 0)
    	{
        	dprintf(D_ALWAYS, "Could not terminate using the ODM database: "
				"%d\n", odmerrno);
			free(path);
			return -1;
    	}
		free(path);
		return -1;
    }

    /* odm_get_list() is scary cause I can't tell if it is going to actually
        remove the entries from the ODM when it returns them to me or not.
        So I'm traversing the list in the safe way that I know how */

    /* get me the objects that are paging devices */
    pret = odm_get_obj(cuat, "value='paging'", &paging_ent, ODM_FIRST);
    while(pret != NULL)
    {
		memset(buf, 0, 1024);
		snprintf(buf, 1024, "%s/%s", "/dev", paging_ent.name);

		ret = swapqry(buf, &p);
        if (ret == -1)
        {
			/* XXX when non root, some swap partitions cannot be inspected,
				so skip them. */
        	pret = odm_get_obj(cuat, NULL, &paging_ent, ODM_NEXT);
			continue;
        }

        free_swap_space += p.free;

        pret = odm_get_obj(cuat, NULL, &paging_ent, ODM_NEXT);
    }

    if (odm_close_class(cuat) < 0)
    {
        dprintf(D_ALWAYS, "Could not close CuAt in the ODM database: %d\n",
			odmerrno);
		free(path);
		return -1;
    }

    if (odm_terminate() < 0)
    {
        dprintf(D_ALWAYS, "Could not terminate using the ODM database: %d\n",
			odmerrno);
		free(path);
		return -1;
    }

	free(path);

	/* the free_swap_space unit is in PAGESIZE blocks */
	/* so convert it into bytes, and then convert it into KB units */
    return (free_swap_space * PAGESIZE) / 1024;
}

#else

#error "Please define: sysapi_swap_space_raw()"

#endif /* !defined(WIN32) */


/* the cooked version of the function */
int
sysapi_swap_space(void)
{
	sysapi_internal_reconfig();

	return sysapi_swap_space_raw();
}



