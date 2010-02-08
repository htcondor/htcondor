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
#include "sysapi_externs.h"
 
#if defined(HPUX)

#include <sys/pstat.h>

int
sysapi_phys_memory_raw_no_param(void)
{
	struct pst_static s;
	unsigned long pages, pagesz;
	double size;
						   
	if (pstat_getstatic(&s, sizeof(s), (size_t)1, 0) != -1) {
		pages = s.physical_memory;
		pagesz = s.page_size >> 10;
		size = (double)pages * (double)pagesz;
		size /= 1024.0;

		if (size > INT_MAX){
			return INT_MAX;
		}
		return (int)size;
	} else {
		return -1;
	}
}

#elif defined(IRIX62) || defined(IRIX65)

#include <unistd.h>
#include <sys/types.h>
#include <sys/sysmp.h>

int
sysapi_phys_memory_raw_no_param(void)
{
	struct rminfo rmstruct;
	long pages, pagesz;
	double size;

	pagesz = (sysconf(_SC_PAGESIZE) >> 10);		// We want kbytes.
	
	if( (sysmp(MP_SAGET,MPSA_RMINFO,&rmstruct,sizeof(rmstruct)) < 0) ||
		(pagesz == -1) ) { 
		return -1;
	}
		/* Correct what appears to be some kind of rounding error */
	if( rmstruct.physmem % 2 ) {
		pages = rmstruct.physmem + 1;
	} else {
		pages = rmstruct.physmem;
	}

	/* Return the answer in megs */
	size = (double)pages * (double)pagesz;
	size /= 1024.0;
	if (size > INT_MAX){
		return INT_MAX;
	}
	return (int)size;
}

#elif defined(Solaris) 

/*
 * This works for Solaris >= 2.3
 */
#include <unistd.h>

int
sysapi_phys_memory_raw_no_param(void)
{
	long pages, pagesz;
	double hack, size;

	pages = sysconf(_SC_PHYS_PAGES);
	pagesz = (sysconf(_SC_PAGESIZE) >> 10);		// We want kbytes.

	if (pages == -1 || pagesz == -1 ) {
		return -1;
	}

#if defined(X86)
		/* 
		   This is super-ugly.  For some reason, Intel Solaris seems
		   to have some kind of rounding error for reporting memory.
		   These values just came from a little trail and error and
		   seem to work pretty well.  -Derek Wright (1/29/98)
 	    */
	hack = (double)pages * (double)pagesz;
	
	/* I don't know if this divisor is right, but it'll do for now.
		Keller 05/20/99 */
	if ((hack / 1024.0) > INT_MAX)
	{
		return INT_MAX;
	}

	if( hack > 260000 ) {
		return (int) (hack / 1023.0);		
	} else if( hack > 130000 ) {
		return (int) (hack / 1020.0);
	} else if( hack > 65000 ) {
		return (int) (hack / 1010.0);
	} else {
		return (int) (hack / 1000.0);
	}
#else
	size = (double)pages * (double)pagesz;
	size /= 1024.0;

	if (size > INT_MAX) {
		return INT_MAX;
	}
	return (int)size;
#endif
}

#elif defined(LINUX)
#include <stdio.h>

int 
sysapi_phys_memory_raw_no_param(void) 
{	

	double bytes;
	double megs;

	/* in bytes */
	bytes = 
		(double)sysconf(_SC_PHYS_PAGES) * (double)sysconf(_SC_PAGESIZE);

	/* convert it to Megabytes */
	megs = bytes / (1024.0*1024.0);

	if (megs > INT_MAX) {
		return INT_MAX;
	}

	return (int)megs;
}

#elif defined(WIN32)

int
sysapi_phys_memory_raw_no_param(void)
{
	MEMORYSTATUSEX statex;		
	
	statex.dwLength = sizeof(statex);
	
	GlobalMemoryStatusEx(&statex);
	
	return (int)(statex.ullTotalPhys/(1024*1024));
}

#elif defined(OSF1)


/* Need these two to avoid compiler warning since <sys/table.h>
   includes a stupid version of <net/if.h> that does forward decls of
   struct mbuf and struct rtentry for C++, but not C. -Derek 6/3/98 */
#include <sys/mbuf.h>
#include <net/route.h>

#include <sys/table.h>

int
sysapi_phys_memory_raw_no_param(void)
{
	struct tbl_pmemstats s;

	if (table(TBL_PMEMSTATS,0,(void *)&s,1,sizeof(s)) < 0) {
		return -1;
	}
	return (int)(s.physmem/(1024*1024));
}

// See GNATS 529. This code should now detect >= 2Gigs properly.
#elif defined(Darwin) || defined(CONDOR_FREEBSD)
#include <sys/sysctl.h>
int
sysapi_phys_memory_raw_no_param(void)
{
	int megs;
	uint64_t mem = 0;
	size_t len = sizeof(mem);

#ifdef Darwin
	if (sysctlbyname("hw.memsize", &mem, &len, NULL, 0) < 0) 
#elif defined(CONDOR_FREEBSD)
	if (sysctlbyname("hw.physmem", &mem, &len, NULL, 0) < 0) 
#endif
	{
        dprintf(D_ALWAYS, 
			"sysapi_phys_memory_raw(): sysctlbyname(\"hw.memsize\") "
			"failed: %d(%s)\n",
			errno, strerror(errno));
		return -1;
	}

	megs = mem / (1024 * 1024);

	return megs;
}
#elif defined(AIX)
int
sysapi_phys_memory_raw_no_param(void)
{
	CLASS_SYMBOL cuat;
	struct CuAt mem_ent;
	struct CuAt *mret = NULL;
	unsigned long memory_size = 0;
	char *path = NULL;

	/* will eventually use the ODMI to figure this out */
    if (odm_initialize() < 0)
    {
		/* This is quite terrible if it happens */
        dprintf(D_ALWAYS, 
			"sysapi_phys_memory_raw(): Could not initialize the ODM database: "
			"%d\n", odmerrno);
		return -1;
    }

	/* remember to free this memory just before I leave this function */
    path = odm_set_path("/etc/objrepos");
	if (path == (char*)-1) /* eewww */
	{
        dprintf(D_ALWAYS, "sysapi_phys_memory_raw(): Could not set class path! "
			"%d\n", odmerrno);
		return -1;
	}

	/* open up a predefined class symbol found in libcfg.a */
    cuat = odm_open_class(CuAt_CLASS);
    if (cuat == NULL)
    {
        dprintf(D_ALWAYS, "sysapi_phys_memory_raw(): Could not open CuAt! %d\n",
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

	/* get me the various memory entries that represent the full amount of 
		memory on the machine */
    mret = odm_get_obj(cuat, "name like mem? AND attribute='size'", 
		&mem_ent, ODM_FIRST);
    while(mret != NULL)
    {
		/* This value appears to be in Megabytes. */
		memory_size += atoi(mem_ent.value);

        mret = odm_get_obj(cuat, NULL, &mem_ent, ODM_NEXT);
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

	return (int)memory_size;
}
#else
#error "sysapi.h: Please define a sysapi_phys_memory_raw() for this platform!"
#endif


int sysapi_phys_memory_raw(void)
{
	sysapi_internal_reconfig();
	return sysapi_phys_memory_raw_no_param();
}

/* This is the cooked version of sysapi_phys_memory_raw(). Where as the raw
	function gives you the raw number, this function can process the number
	for you for some reason and return that instead.
*/
int
sysapi_phys_memory(void)
{
	int mem;
	sysapi_internal_reconfig();
	mem = _sysapi_memory;
	if (!_sysapi_memory) {
		mem = sysapi_phys_memory_raw();
	}
	if (mem < 0) return mem;
	mem -= _sysapi_reserve_memory;
	if (mem < 0) return 0;
	return mem;
}
