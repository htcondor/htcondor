/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"

/* Calculate how many cpus a machine has using the various method each OS
	allows us */

#if defined(IRIX53) || defined(IRIX62) || defined(IRIX65)
#include <sys/sysmp.h>
#endif

#ifdef HPUX
#include <sys/pstat.h>
#endif

#ifdef Darwin
#include <sys/sysctl.h>
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
#elif defined(IRIX53) || defined(IRIX62) || defined(IRIX65)
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
	char		*tmp;
	int             siblings = 0;
#if defined(ALPHA)
	char		*tmp;
#endif
	int 		num_cpus = 0;

	sysapi_internal_reconfig();
	proc = fopen( "/proc/cpuinfo", "r" );
	if( !proc ) {
		return 1;
	}

/*
/proc/cpuinfo looks something like this on an I386 machine...:
The alpha linux port does not use "processor", nor does it provide
seperate entries on SMP machines.  In fact, for Alpha Linux, there's
another whole entry for "cpus detected" that just contains the number
we want.  So, on Alpha Linux we have to look for that, and on Intel
Linux, we just have to count the number of lines containing
"processor". 
   -Alpha/Linux wisdom added by Derek Wright on 12/7/99

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
#if defined(I386) || defined(IA64)
	    // For hyperthreads we assume processor will appear before 
	    // sibling.  If that fails we're screwed.
	    if( !strincmp(buf, "processor", 9) ) {
	        if( siblings <= 0 || param_boolean_int("COUNT_HYPERTHREAD_CPUS", 1) ) {
	             num_cpus++;
	       	}
	      	siblings--;
	    }
	    if( !strincmp(buf, "siblings", 8) && siblings < 0 ) {
	        tmp = strchr( buf, ':' );
		if( tmp ) {	  
		    tmp++;
		    siblings = atoi(tmp)-1;
		}
	    }
#elif defined(ALPHA)
		if( !strincmp(buf, "cpus detected", 13) ) {
			tmp = strchr( buf, ':' );
			if( tmp && tmp[1] ) {
				tmp++;
				num_cpus = atoi( tmp );
			} else {
				dprintf( D_ALWAYS, 
						 "ERROR: Unrecognized format for /proc/cpuinfo:\n(%s)\n",
						 buf );
				num_cpus = 1;
			}
		}
#else
#error YOU MUST CHECK THE FORMAT OF /proc/cpuinfo TO FIND N-CPUS CORRECTLY
#endif
	}
		/* If we didn't find something, default to 1 cpu. */
	if( !num_cpus ) {
		num_cpus = 1;
	}
	fclose( proc );
	return num_cpus;
#elif defined(AIX)
	sysapi_internal_reconfig();
	return sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(Darwin)
	sysapi_internal_reconfig();
	int mib[2], maxproc;
	size_t len;
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	len = sizeof(maxproc);
	sysctl(mib, 2, &maxproc, &len, NULL, 0);
	return(maxproc);
#else
#error DO NOT KNOW HOW TO COMPUTE NUMBER OF CPUS ON THIS PLATFORM!
#endif
}

/* the cooked version */
int
sysapi_ncpus(void)
{	
	sysapi_internal_reconfig();
	if( _sysapi_ncpus ) {
		return _sysapi_ncpus;
	} else {
		return sysapi_ncpus_raw();
	}
}


