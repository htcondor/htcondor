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
#include <errno.h>
#include <kstat.h>
#include <sys/param.h>

#include "debug.h"
#include "except.h"
#include "condor_uid.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */
static int KernelLookupFailed = 0;

double kstat_load_avg();

float
calc_load_avg()
{

	float val;

	if (!KernelLookupFailed)
		val = (float)kstat_load_avg();
	if (KernelLookupFailed)
		val = lookup_load_avg_via_uptime();

	return val;
}


#undef RETURN
#define RETURN \
    dprintf( D_ALWAYS, "Getting load avg from uptime.\n" );\
	KernelLookupFailed = 1;\
	return 0.0

double
kstat_load_avg(void)
{
	static kstat_ctl_t	*kc = NULL;		/* libkstat cookie */
	static kstat_t		*ksp = NULL;	/* kstat pointer */
	kstat_named_t 		*ksdp = NULL;  	/* kstat data pointer */

	if( ! kc ) {
		if( (kc = kstat_open()) == NULL ) {
			dprintf( D_ALWAYS, "kstat_open() failed, errno = %d\n", errno );
			RETURN;
		}
	}

	if( ! ksp ) {
		if( (ksp = kstat_lookup(kc, "unix", 0, "system_misc")) == NULL ) {
			dprintf( D_ALWAYS, "kstat_lookup() failed, errno = %d\n", errno );
			RETURN;
		}
	}

	if( kstat_read(kc, ksp, NULL) == -1 ) {
		dprintf( D_ALWAYS, "kstat_read() failed, errno = %d\n", errno );
		RETURN;
	}

	ksdp = (kstat_named_t *) kstat_data_lookup(ksp, "avenrun_1min");
	if( ksdp ) {
		return (double) ksdp->value.l / FSCALE;
	} else {
		dprintf( D_ALWAYS, "kstat_data_lookup() failed, errno = %d\n",
				 errno);
		RETURN;
	}		
}

/* just adding get_k_vars to avoid runtime errors */
get_k_vars()
{
}
