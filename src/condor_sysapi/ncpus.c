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
#include "sysapi.h"

/* Calculate how many cpus a machine has using the various method each OS
	allows us */

#if defined(IRIX53)
#include <sys/sysmp.h>
#endif

#ifdef HPUX
#include <sys/pstat.h>
#endif

int
sysapi_ncpus_raw(void)
{
#ifdef sequent
	int     cpus = 0;
	int     eng;
	sysapi_internal_reconfig();

	if ((eng = tmp_ctl(TMP_NENG,0)) < 0) {
		perror( "tmp_ctl(TMP_NENG,0)" );
		exit(1);
	}

	while (eng--) {
		if( tmp_ctl(TMP_QUERY,eng) == TMP_ENG_ONLINE )
			cpus++;
	}
	return cpus;
#elif defined(HPUX)
        struct pst_dynamic d;
		sysapi_internal_reconfig();
        if ( pstat_getdynamic ( &d, sizeof(d), (size_t)1, 0) != -1 ) {
          return d.psd_proc_cnt;
        }
        else {
          return 0;
        }
#elif defined(Solaris) || defined(DUX)
	sysapi_internal_reconfig();
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(IRIX53)
	sysapi_internal_reconfig();
	return sysmp(MP_NPROCS);
#elif defined(WIN32)
	SYSTEM_INFO info;
	sysapi_internal_reconfig();
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
#elif defined(LINUX)

	FILE        *proc;
	char 		buf[256];
	int 		num_cpus = 0;

	sysapi_internal_reconfig();
	proc = fopen( "/proc/cpuinfo", "r" );
	if( !proc ) {
		return 1;
	}

/*
/proc/cpuinfo looks something like this on an I386 machine...:
The alpha linux port does not use "processor", so we use bogomips instead.
(For 1 cpu machines, there's only 1 entry).

processor       : 0
cpu             : 686
model           : 3
vendor_id       : GenuineIntel
stepping        : 4
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid           : yes
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic 11 mtrr pge mca cmov mmx
bogomips        : 298.19

processor       : 1
cpu             : 686
model           : 3
vendor_id       : GenuineIntel
stepping        : 4
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid           : yes
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic 11 mtrr pge mca cmov mmx
bogomips        : 299.01
*/
	// Count how many lines begin with the string "processor".
	while( fgets( buf, 256, proc) ) {
		if( !strincmp( buf, "bogomips", 8 ) ) {
			num_cpus++;
		}
	}
	fclose( proc );
	return num_cpus;
#else sequent
	sysapi_internal_reconfig();
	return 1;
#endif sequent
}

/* the cooked version */
int
sysapi_ncpus(void)
{	
	sysapi_internal_reconfig();
	return sysapi_ncpus_raw();
}


