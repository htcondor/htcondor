/* 
** Copyright 1993 by the Miron Livny, Mike Litzkow, and Todd Tannenbaum
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Todd Tannenbaum
**
*/ 

#if defined(SUNOS41)

#include <stdio.h>
#include <nlist.h>
#include <sys/types.h>
#include <sys/file.h>
#include <kvm.h>

#include "debug.h"
#include "except.h"
static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */


/*
 * Get the amount of installed RAM memory in megabytes
 *
*/
int
calc_phys_memory()
{
        unsigned int physmem;
        char *namelist = 0;
        char *corefile = 0;
        kvm_t   *kvm_des;
        struct nlist mynl[2];
 
        mynl[0].n_name = "_physmem";
        mynl[1].n_name = "";
        kvm_des = kvm_open(namelist,corefile, (char *)NULL, O_RDONLY, NULL);
        if (kvm_des == NULL) {
		/* The most likely reason for this error to occur is 
		 * we cannot read /dev/kmem.  We should either be root or
		 * in the group kmem (and permissions on /dev/kmem should be
		 * correct!) */
                return(-1);
        }
 
        if (kvm_nlist(kvm_des, mynl) < 0) {
                return(-1);
        }
 
        if ( mynl[0].n_value == 0 ) {
                return(-1);
        }

        if ( kvm_read(kvm_des, (u_long) mynl[0].n_value, &physmem, sizeof(physmem))
< 0 ) {
                return(-1);
        }
 
        kvm_close(kvm_des);
 
        physmem *= getpagesize();
 
        physmem += 300000;      /* add about 300K to offset for RAM that the
				   Sun PROM program gobbles at boot time (at 
				   least I think that's where its going).  If
				   we fail to do this the integer divide we do
				   next will incorrectly round down one meg. */
 
        physmem /= 1048576;     /* convert from bytes to megabytes */
 
 	return(physmem);
}
#elif defined(HPUX9) || defined(IRIX53) 

#include <stdio.h>
#include <nlist.h>
#include <sys/types.h>
#include <sys/file.h>

struct nlist memnl[] = {
	{ "physmem",0,0 },
    { NULL }
};

#if defined(IRIX62)
struct nlist64 memnl64[] = {
	{ "physmem",0,0 },
    { NULL }
};
#endif  /* IRIX62 */

calc_phys_memory()
{
  int kmem;
  int maxmem, physmem;


  /*
   *   Lookup addresses of variables.
  */
#if defined(HPUX9) && !defined(HPUX10)
  if ((nlist("/hp-ux",memnl) <0) || (memnl[0].n_type ==0)) return(-1); 
#endif
#if defined(HPUX10)
  if ((nlist("/stand/vmunix",memnl) <0) || (memnl[0].n_type ==0)) return(-1);
#endif

/* for IRIX62, we assume that the kernel is a 32 or 64-bit ELF binary, and
 * for IRIX53, we assume the kernel is a 32-bit COFF.  Todd 2/97 */
#if defined(IRIX62)
  if (_libelf_nlist("/unix",memnl) == -1)
     if (_libelf_nlist64("/unix",memnl64) == -1)
         return(-1);
#elif defined(IRIX53)
  if ((nlist("/unix",memnl) <0) || (memnl[0].n_type ==0)) return(-1); 
#endif

  /*
   *   Open kernel memory and read variables.
  */
  if ((kmem=open("/dev/kmem",0)) <0) return (-1);

#if defined(IRIX62)
  /* if IRIX62, figure out if we grabbed a 32- or 64-bit address */
  if ( memnl[0].n_type ) {
  	if (-1==lseek(kmem,(long) memnl[0].n_value,0)) return (-1);
  } else {
  	if (-1==lseek(kmem,(long) memnl64[0].n_value,0)) return (-1);
  }
#else
  if (-1==lseek(kmem,(long) memnl[0].n_value,0)) return (-1);
#endif

  if (read(kmem,(char *) &physmem, sizeof(int)) <0) return (-1);
  
  close(kmem);

  /*
   *  convert to megabytes
  */

#if defined(HPUX9)
  physmem/=256; /* *4 /1024 */
#elif defined(IRIX53)
  physmem = physmem<<2;			/* assumes that a page is 4K */
  physmem /= 1024;				/* convert from kbytes to mbytes */
#endif

  return(physmem);
}

#elif defined(Solaris)

/*
 * This works for Solaris >= 2.3
 */
#include <unistd.h>
 
int
calc_phys_memory()
{
	long pages, pagesz;

	pages = sysconf(_SC_PHYS_PAGES);
	pagesz = sysconf(_SC_PAGESIZE);

	if (pages == -1 || pagesz == -1)
		return -1;
 
        return (int)((pages * pagesz) / 1048576);
}
#elif defined(LINUX)
#include <stdio.h>

int calc_phys_memory() 
{	

	FILE        *proc;
	long        phys_mem;
	char        tmp_c[20];
	long        tmp_l;
	char   		c;

	proc=fopen("/proc/meminfo","r");
	if(!proc) {
		return -1;
	}

	  /*
	  // The /proc/meminfo looks something like this:

	  //       total:    used:    free:  shared: buffers:  cached:
	  //Mem:  19578880 19374080   204800  7671808  1191936  8253440
	  //Swap: 42831872  8368128 34463744
	  //MemTotal:     19120 kB
	  //MemFree:        200 kB
	  //MemShared:     7492 kB
	  //Buffers:       1164 kB
	  //Cached:        8060 kB
	  //SwapTotal:    41828 kB
	  //SwapFree:     33656 kB
	  */	  
	while((c=fgetc(proc))!='\n');
	fscanf(proc, "%s %d", tmp_c, &phys_mem);
	fclose(proc);
	return (int)(phys_mem/(1024000));
}


#else	/* Don't know how to do this on other than SunOS and HPUX yet */
int
calc_phys_memory()
{
	return -1;
}
#endif
