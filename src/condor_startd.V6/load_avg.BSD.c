/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include <stdio.h>
#include <nlist.h>
#include <sys/types.h>
#include <sys/file.h>
#include "condor_uid.h"

#ifdef MIPS
#include <sys/fixpoint.h>
#endif

#if defined(SUNOS41)
#include <sys/param.h>
#endif 

#include "debug.h"
#include "except.h"
static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

static int	Kmem;
static int	KernelLookupFailed = 0;
static void get_k_vars();

float lookup_load_avg();

struct nlist nl[] = {
#if defined(IRIX331) || defined(HPUX9) || defined(IRIX53)
    { "avenrun",0,0 },
#else
    { "_avenrun" },
#endif
    { "" },
};

#if defined(IRIX62)
struct nlist64  nl64[] = {
    { "avenrun",0,0 },
    { "" },
};
#endif /* IRIX62 */

float
calc_load_avg()
{
	float val;
	static int got_k_vars = 0;

	// open /dev/kmem if haven't already done so
	if ( !got_k_vars ) {
		got_k_vars = 1;
		get_k_vars();	// this will set KernelLookupFailed to 0 or 1
	}
	
	if (!KernelLookupFailed)
		val = lookup_load_avg();
	if (KernelLookupFailed)
		val = lookup_load_avg_via_uptime();

	return val;
}

/*
* Find addresses of various kernel variables in /dev/kmem.
*/
static void
get_k_vars()
{
	priv_state priv;

	priv = set_root_priv();

        /* Open kmem for reading */
    if( (Kmem=open("/dev/kmem",O_RDONLY,0)) < 0 ) {
		set_priv(priv);
		KernelLookupFailed = 1;
        dprintf(D_ALWAYS, "open(/dev/kmem,O_RDONLY) failed, "
				"getting loadavg from uptime");
		return;
    }

	/* Get the kernel's name list */

#if defined(IRIX62)
	/* here we assume that all IRIX 6.x kernels are compiled as a ELF binary.  If the
	 * kernel, i.e. /unix, is indeed a COFF binary, the below will fail with an exception.
	 * first try a 32-bit ELF binary, then a 64-bit ELF, then fail.-Todd 2/97 */
  if ( _libelf_nlist("/unix",nl) == -1 )
	  if (_libelf_nlist64("/unix",nl64) == -1 ) {
		  set_priv(priv);
		  KernelLookupFailed = 1;
		  dprintf(D_ALWAYS, "cannot read /unix as an ELF binary, "
				  "getting loadavg from uptime");
		  return;
	  }
  nl->n_value &= ~0x80000000;
#elif defined(IRIX331) || defined(IRIX53)
	(void)nlist( "/unix", nl );
	nl->n_value &= ~0x80000000;
#elif defined(HPUX10)
	(void)nlist( "/stand/vmunix", nl );
#elif defined(HPUX9)
	(void)nlist( "/hp-ux", nl );
#else 	/* everybody else */
	(void)nlist( "/vmunix", nl );
#endif

	set_priv(priv);
}


/*
** Berkeley style kernels maintain three variables which represent load
** averages over the previous one, five, and fifteen minute intervals.
** Systems vary in whether they use traditional floating point numbers
** for this calculation, or opt for the efficiency of some type of
** scaled integer.  Here we just look up the one minute load average,
** and if necessary convert to a float.
*/

/*
** The SGI folks apparently use a scaled integer, but where is the scale
** factor defined?  This is a wild guess -- most likely wrong! -- mike
*/
#if defined(IRIX331) || defined(IRIX53)
#define FSCALE 1000
#endif

float
lookup_load_avg()
{
    off_t 		addr;
	priv_state	priv;

#if defined(HPPAR) && defined(HPUX9)			/* HP-UX */
    double  	avenrun[3];
#elif defined(ULTRIX42) || defined(ULTRIX43)
	fix			avenrun[3];
#elif defined(SUNOS41) || defined(IRIX331) || defined(IRIX53)
    long 		avenrun[3];
#endif

#if defined(IRIX62)
	/* if IRIX62, figure out if we grabbed a 32- or 64-bit address */
	if ( nl[0].n_type ) 
		addr = nl[0].n_value;
	else
		addr = nl64[0].n_value;
#else
	addr = nl[0].n_value;
#endif

	avenrun[0] = 0;
	avenrun[1] = 0;
	avenrun[2] = 0;

	priv = set_root_priv();

    if( lseek(Kmem,addr,0) != addr ) {
		set_priv(priv);
		KernelLookupFailed = 1;
        dprintf( D_ALWAYS, "Can't seek to addr 0x%x in /dev/kmem\n", addr );
		return 0.0;
    }

    if( read(Kmem,(char *)avenrun,sizeof avenrun) != sizeof avenrun ) {
		set_priv(priv);
		KernelLookupFailed = 1;
        dprintf( D_ALWAYS, "read loadavg from /dev/kmem failed" );
		return 0.0;
    }

	set_priv(priv);

        /* just give back the 1 minute average for now */

#if defined(HPPAR) && defined(HPUX9)		/* HP-UX */
	if ((float)avenrun[0] >= 0.01) {
        dprintf( D_ALWAYS, "load average is %g\n", avenrun[0] );
		return (float)avenrun[0];
	}
	else {
        dprintf( D_ALWAYS, "load average is %g, returning 0.0\n",
				avenrun[0] );
		return 0.0;
	}

#elif defined(ULTRIX42) || defined(ULTRIX43)
	return (float)avenrun[0]/(1<<FBITS);

#elif defined(SUNOS41) || defined(IRIX331) || defined(IRIX53)
    return (float)avenrun[0]/FSCALE;

#endif
}
