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

#ifdef DYNIX
#include <sys/vmparam.h>
#endif

#ifdef MIPS
#include <sys/fixpoint.h>
#endif

#if defined(SUNOS41) || defined(MC68020) || defined(CMOS) || defined(HPUX9)
#include <sys/param.h>
#endif 

#include "debug.h"
#include "except.h"
static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

int		Kmem;



struct nlist nl[] = {
#if defined(IRIX331) || defined(HPUX9) 
    { "avenrun" },
#else
    { "_avenrun" },
#endif
#define X_AVENRUN 0
    { "_avenrun_scale" },		/* Really only ibm032, but it doesn't matter */
#define X_AVENRUN_SCALE 1
	{ "sysinfo" },				/* Really only ibm R6000, but doesn't matter */
#define X_SYSINFO 2
    { "" },
};

float
calc_load_avg()
{
#if defined(AIX31) || defined(AIX32)
	float build_load_avg();
	return build_load_avg();
#else
	float lookup_load_avg();
	return lookup_load_avg();
#endif
}

/*
* Find addresses of various kernel variables in /dev/kmem.
*/
get_k_vars()
{
	if (getuid() == 0)
	set_root_euid(); /* not doing conditional for Solaris because it is not
                            executed by Solaris anyway */

        /* Open kmem for reading */
    if( (Kmem=open("/dev/kmem",O_RDONLY,0)) < 0 ) {
		set_condor_euid();
        EXCEPT( "open(/dev/kmem,O_RDONLY,0)" );
    }

	/* Get the kernel's name list */

#if defined(DYNIX)						/* sequent */
	(void)nlist( "/dynix", nl );
#endif DYNIX

#if defined(AIX31) || defined(AIX32) 				/* ibm R6000 */
	(void)knlist( nl, 3, sizeof(struct nlist) );
	if( nl[X_SYSINFO].n_value == 0 ) {
		set_condor_euid();
		EXCEPT( "Can't find \"sysinfo\" in kernel's name list" );
	}
#endif

#if defined(IRIX331)						/* silicon graphics */
	(void)nlist( "/unix", nl );
	nl->n_value &= ~0x80000000;
#endif DYNIX

#if defined(HPUX9)
	(void)nlist( "/hp-ux", nl );
#endif

#if !defined(DYNIX) && !defined(AIX31) && !defined(AIX32) && !defined(IRIX331) && !defined(HPUX9) /* everybody else */
	(void)nlist( "/vmunix", nl );
#endif

	set_condor_euid();

}


#if defined(AIX31) || defined(AIX32)
/*
** AIX doesn't provide the berkeley style one, five, and fifteen minute
** load averages. Instead it provides information from which various
** flavors of load averages can be extracted.  Here we take a fairly
** simple approach which approximates the one minute load average provided
** by berkeley systems.
*/

#include <sys/sysinfo.h>

#define INFO_SIZE sizeof(struct sysinfo)
#define AVG_INTERVAL 60			/* Calculate a one minute load average */

int		PrevInRunQ, PrevTicks, CurInRunQ, CurTicks;
time_t	LastPoll;
off_t	KernelAddr;

float
build_load_avg()
{
	float	short_avg;
	static float	long_avg;
	static time_t	now, elapsed_time;
	static int		n_polls = 0;
	float	weighted_long, weighted_short;

	poll( &CurInRunQ, &CurTicks, &now );

	if( n_polls == 0 ) {	/* first time through, just set things up */
		PrevInRunQ = CurInRunQ;
		PrevTicks = CurTicks;
		LastPoll = now;
		n_polls = 1;
		short_avg = 1.0;
		long_avg = 1.0;
		dprintf( D_LOAD, "First Time, returning 1.0\n" );
		return 1.0;
	}
		
	if( CurTicks == PrevTicks ) {
		dprintf( D_LOAD, "Too short of time to compute avg, returning %5.2f\n",
																	long_avg );
		return long_avg;
	}
	short_avg = (float)(CurInRunQ - PrevInRunQ) / (float)(CurTicks - PrevTicks)
																	- 1.0;
	elapsed_time = now - LastPoll;

	if( n_polls == 1 ) {	/* second time through, init long average */
		long_avg = short_avg;
		PrevInRunQ = CurInRunQ;
		PrevTicks = CurTicks;
		LastPoll = now;
		n_polls = 2;
		dprintf( D_LOAD, "Second time, returning %5.2f\n", long_avg );
		return long_avg;
	}

		/* after second time through, update long average with new info */

	weighted_short = short_avg * (float)elapsed_time / (float)AVG_INTERVAL;
	weighted_long = long_avg *
				(float)(AVG_INTERVAL - elapsed_time) / (float)AVG_INTERVAL;
	long_avg = weighted_long + weighted_short;
	dprintf( D_LOAD, "ShortAvg = %5.2f LongAvg = %5.2f\n",
														short_avg, long_avg );
	dprintf( D_LOAD, "\n" );
	PrevInRunQ = CurInRunQ;
	PrevTicks = CurTicks;
	LastPoll = now;

	return long_avg;
}



poll( queue, ticks, poll_time )
int		*queue;
int		*ticks;
int		*poll_time;
{
	struct sysinfo	 info;

	if( lseek(Kmem,nl[X_SYSINFO].n_value,L_SET) < 0 ) {
		perror( "lseek" );
		exit( 1 );
	}

	if( read(Kmem,(char *)&info,INFO_SIZE) < INFO_SIZE ) {
		perror( "read from kmem" );
		exit( 1 );
	}

	*queue = info.runque;
	*ticks = info.runocc;

	(void)time( poll_time );
}

#else /* AIX */
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
#if defined(IRIX331)
#define FSCALE 1000
#endif

float
lookup_load_avg()
{
    off_t addr = nl[X_AVENRUN].n_value;

#if defined(VAX) && defined(ULTRIX)			/* ULTRIX VAX */
    double  avenrun[3];

#elif defined(VAX) && defined(BSD43)			/* Berkeley VAX */
    double  avenrun[3];

#elif defined(MC68020) && defined(BSD43)		/* Bobcat */
    double  avenrun[3];

#elif defined(HPPAR) && defined(HPUX9)			/* HP-UX on HP 700*/
    double  avenrun[3];

#elif defined(ULTRIX42) || defined(ULTRIX43)
	fix		avenrun[3];

#elif defined(I386) && defined(DYNIX)			/* Sequent Symmetry */
    int     avenrun[3];

#elif defined(SUNOS32) || defined(SUNOS40) || defined(SUNOS41) || defined(IRIX331) || defined(CMOS)
    long avenrun[3];

#elif defined(IBM032) && defined(BSD43)		/* RT */
    double  avenrun[3];
#endif

	avenrun[0] = 0;
	avenrun[1] = 0;
	avenrun[2] = 0;

    if( lseek(Kmem,addr,0) != addr ) {
        dprintf( D_ALWAYS, "Can't seek to addr 0x%x in /dev/kmem\n", addr );
        EXCEPT( "lseek" );
    }

    if( read(Kmem,(char *)avenrun,sizeof avenrun) != sizeof avenrun ) {
        EXCEPT( "read" );
    }

        /* just give back the 1 minute average for now */

#if defined(VAX) && defined(ULTRIX)			/* ULTRIX VAX */
    return (float)avenrun[0];

#elif defined(VAX) && defined(BSD43)			/* Berkeley VAX */
    return (float)avenrun[0];

#elif defined(MC68020) && defined(BSD43)		/* Bobcat */
    return (float)avenrun[0];

#elif defined(HPPAR) && defined(HPUX9)		/* HP-UX */
    return (float)avenrun[0];

#elif defined(ULTRIX42) || defined(ULTRIX43)
	return (float)avenrun[0]/(1<<FBITS);

#elif defined(I386) && defined(DYNIX)			/* Sequent Symmetry */
    return (float)avenrun[0]/FSCALE;

#elif defined(SUNOS32) || defined(SUNOS40) || defined(SUNOS41) || defined(IRIX331) || defined(CMOS)
    return (float)avenrun[0]/FSCALE;

#elif defined(IBM032) && defined(BSD43)		/* RT */
    return (float)avenrun[0];
#endif
}

#endif /* AIX */
