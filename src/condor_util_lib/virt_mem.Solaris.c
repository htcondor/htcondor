/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
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
** Author:  Derek Wright
**
*/ 

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include "condor_debug.h"

int calc_virt_memory();
int ctok(int clicks);

int close_kmem(){};

/*
 *  DEFAULT_SWAPSPACE
 *
 *  This constant is used when we can't get the available swap space through
 *  other means.  -- Ajitk
 */

#define DEFAULT_SWAPSPACE       100000 /* ..dhaval 7/20 */


/* EVIL!!  This shouldn't have to be here! -Derek 7/30/97 */
int HasSigchldHandler = 0;


/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/
int
calc_virt_memory()
{
	struct anoninfo 	ai;
	int avail;

	if( swapctl(SC_AINFO, &ai) >= 0 ) {
		avail = ctok( ai.ani_max - ai.ani_resv );
		return avail;
	} else {
		dprintf( D_FULLDEBUG, "swapctl call failed, errno = %d\n", errno );
		return DEFAULT_SWAPSPACE;
	}
}

int
ctok(int clicks)
{
	static int factor = -1;
	if( factor == -1 ) {
		factor = sysconf(_SC_PAGESIZE) >> 10;
	}
	return( clicks*factor );
}

