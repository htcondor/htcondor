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

#include <sys/sysmp.h>
#include <sys/sysinfo.h>

static int KernelLookupFailed = 0;

static float kernel_load_avg();

extern float lookup_load_avg_via_uptime();

/*
** Where is FSCALE defined on IRIX ?  Apparently nowhere...
** The SGI folks apparently use a scaled integer, but where is the scale
** factor defined?  This is a wild guess that seems to work.
*/
#ifndef FSCALE
#define FSCALE 1000
#endif

float
calc_load_avg()
{

	float val;

	if (!KernelLookupFailed)
		val = kernel_load_avg();
	if (KernelLookupFailed)
		val = lookup_load_avg_via_uptime();

	dprintf( D_FULLDEBUG, "Load avg: %f\n", val );
	return val;
}


#undef RETURN
#define RETURN \
    dprintf( D_ALWAYS, "Getting load avg from uptime.\n" );\
	KernelLookupFailed = 1;\
	set_priv(priv);\
	return 0.0

float
kernel_load_avg(void)
{
	static int addr_sysmp = -1;
	static int Kmem = -1;
	priv_state priv;
	long avenrun[3];


	/* get root access if we can */
	priv = set_root_priv();

	errno = 0;
	if ( addr_sysmp < 0 ) {
		addr_sysmp = sysmp(MP_KERNADDR,MPKA_AVENRUN);
		if ( !addr_sysmp || errno ) {
			dprintf(D_ALWAYS,"sysmp(MP_KERNADDR) failed, errno=%d\n",errno);
			RETURN;
		}
	}

	if ( Kmem < 0 ) {
		if ( (Kmem=open("/dev/kmem",O_RDONLY,0)) < 0 ) {
			dprintf(D_ALWAYS,"open /dev/kmem failed, errno=%d\n",errno);
			RETURN;
		}
	}

	if ( lseek(Kmem,addr_sysmp,0) != addr_sysmp ) {
		dprintf(D_ALWAYS,"lseek(/dev/kmem,%d,0) failed, errno=%d\n",addr_sysmp,errno);
		RETURN;
	}

	if ( read(Kmem,(char *)avenrun,sizeof(avenrun)) != sizeof(avenrun) ) {
		dprintf(D_ALWAYS,"read on /dev/kmem failed, errno=%d\n",errno);
		RETURN;
	}

	/* Disable root access */
	set_priv(priv);

	return ( (float)avenrun[0] / FSCALE );
}

/* just adding get_k_vars to avoid runtime errors */
get_k_vars()
{
}
