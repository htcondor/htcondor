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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <nlist.h>
#include <kvm.h>

#include "debug.h"
#include "except.h"
#include "condor_uid.h"

struct nlist nl[] = {
    { "avenrun" }, {0}
};

static int Kmem;
static int KernelLookupFailed = 0;

#define DEFAULT_LOADAVG         0.10

float lookup_load_avg_via_uptime();
double kvm_load_avg();

float
calc_load_avg()
{
	float val;

	if (!KernelLookupFailed)
		val = (float)kvm_load_avg();
	if (KernelLookupFailed)
		val = lookup_load_avg_via_uptime();

	return val;
}

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

double
kvm_load_avg(void)
{
	long avenrun[3];
	int i;
	int offset;
	kvm_t *kd;
	priv_state priv;

	priv = set_root_priv();
	kd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL);
	set_priv(priv);

	/* test kvm_open return value */
	if (kd == NULL) {
		dprintf(D_ALWAYS, "kvm_load_avg: kvm_open failed, "
				"getting loadavg from uptime\n");
		KernelLookupFailed = 1;
		return 0.0;
	}

	if (kvm_nlist(kd, nl) < 0) {
		kvm_close(kd);
		dprintf(D_ALWAYS, "kvm_load_avg: kvm_nlist failed, "
				"getting loadavg from uptime\n");
		KernelLookupFailed = 1;
		return 0.0;
	}

	if (nl[0].n_type == 0) {
		kvm_close(kd);
		dprintf(D_ALWAYS, "kvm_load_avg: avenrun not found, "
				"getting loadavg from uptime\n");
		KernelLookupFailed = 1;
	}

	if (kvm_read(kd, nl[0].n_value, (char *) avenrun, sizeof (avenrun))
	    != sizeof (avenrun)) {
		kvm_close(kd);
		dprintf(D_ALWAYS, "kvm_load_avg: avenrun not found, "
				"getting loadavg from uptime\n");
		KernelLookupFailed = 1;
  	}

	kvm_close(kd);
	return ((double)(avenrun[2]) / FSCALE);
}

/* just adding get_k_vars to avoid runtime errors */

get_k_vars()
{
}
