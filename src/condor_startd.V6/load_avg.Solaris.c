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
#include "condor_uid.h"
#include <kstat.h>

static int KernelLookupFailed = 0;

double kstat_load_avg();

extern float lookup_load_avg_via_uptime();

float
calc_load_avg()
{

	float val;

	if (!KernelLookupFailed)
		val = (float)kstat_load_avg();
	if (KernelLookupFailed)
		val = lookup_load_avg_via_uptime();

	dprintf( D_FULLDEBUG, "Load avg: %f\n", val );
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
