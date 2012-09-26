/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


/* This file contains each implementation of load average that we need. */

/* The essence of the format looks like this for each type of machine:
	#if defined(HPUX) 
		code
	#endif

	The ------- line separates the load average calculating code from a
	function that gets the load average through the unix command 'uptime'.
	It is conditionally compiled as some architectures need it and others don't
*/

/* header files everyone needs */
#include "condor_common.h"
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"
#include "my_popen.h"

/* the cooked version */
float
sysapi_load_avg(void)
{
	sysapi_internal_reconfig();

	if ( _sysapi_getload ) {
		return sysapi_load_avg_raw();
	} else {
		return 0.0;
	}
}

#if defined(HPUX)
/** Nicely ask the system what the one minute load average is.
    For more info, see the pstat manpage and sys/pstat.h
*/

#include "condor_uid.h"

#include <sys/pstat.h>

float
sysapi_load_avg_raw(void)
{
  struct pst_dynamic d;
  	/* make numcpus static so we do not have to recompute
	 * numcpus every time the load average is requested.
	 * after all, the number of cpus is not going to change!
	 * we need to multiply the value the HPUX kerenel gives
	 * us by the number of CPUs, because on SMP HPUX the kernel
	 * "distributes" the load average across all CPUs.  But
	 * no other Unix does that, so our startd assumes otherwise.
	 * So we multiply by the number of CPUs so HPUX SMP load avg
	 * is reported the same way as other Unixes. -Todd
	 */
  static int numcpus = 0;  

  sysapi_internal_reconfig();
  if ( numcpus == 0 ) {
    numcpus = sysapi_ncpus();
	if ( numcpus < 1 ) {
		numcpus = 1;
	}
  }

  if ( pstat_getdynamic ( &d, sizeof(d), (size_t)1, 0) != -1 ) {
    return (d.psd_avg_1_min * numcpus);
  }
  else {
    return -1.0;
  }
}

#elif defined(LINUX)

//prototype
void get_k_vars(void);

float
sysapi_load_avg_raw(void)
{
    FILE	*proc;
	struct utsname buf;
	int		major, minor, patch;
	float	short_avg, medium_avg, long_avg;

	sysapi_internal_reconfig();

	// Obtain the kernel version so that we know what /proc looks like..
	if( uname(&buf) < 0 )  return -1;
	sscanf(buf.release, "%d.%d.%d", &major, &minor, &patch);

	// /proc/loadavg looks like:

	// Kernel Version 2.0.0:
	// 0.03 0.03 0.09 2/42 15582

    proc=safe_fopen_wrapper_follow("/proc/loadavg","r",0644);
    if(!proc)
	return -1;

	switch(major) {
		case 1:
		case 2:
		case 3:
    		if (fscanf(proc, "%f %f %f", &short_avg, &medium_avg, &long_avg) != 3) {
				dprintf(D_ALWAYS, "Failed to fscanf 3 floats from /proc/loadavg\n");
				fclose(proc);
				return -1;
			}
			break;

		default:
			dprintf(D_ALWAYS, "/proc format unknown for kernel version %d.%d.%d\n",
				major, minor, patch);
    		fclose(proc);
			return -1;
			break;
	}

    fclose(proc);

	if( IsDebugVerbose( D_LOAD ) ) {
		dprintf( D_LOAD, "Load avg: %.2f %.2f %.2f\n", short_avg, 
				 medium_avg, long_avg );
	}
	return short_avg;
}


void
get_k_vars() {}


#elif defined(Solaris)

#include "condor_uid.h"
#include <kstat.h>

static int KernelLookupFailed = 0;

double kstat_load_avg();
float lookup_load_avg_via_uptime();

void get_k_vars(void);

float
sysapi_load_avg_raw(void)
{

	float val;

	sysapi_internal_reconfig();

	if (!KernelLookupFailed)
		val = (float)kstat_load_avg();
	if (KernelLookupFailed)
		val = lookup_load_avg_via_uptime();

	if( IsDebugVerbose( D_LOAD ) ) {
		dprintf( D_LOAD, "Load avg: %.2f\n", val );
	}
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
void get_k_vars()
{
}

#elif defined(Darwin) || defined(CONDOR_FREEBSD)

#include <sys/resource.h>
#include <sys/sysctl.h>
#if defined(CONDOR_FREEBSD)
#include <vm/vm_param.h>
#endif
float
sysapi_load_avg_raw(void)
{

	float first, second, third;
	int mib[4];
	struct loadavg load;
	size_t len;

	sysapi_internal_reconfig();

	mib[0] = CTL_VM;
	mib[1] = VM_LOADAVG;
	len = sizeof(struct loadavg);		
	sysctl(mib, 2, &load, &len, NULL, 0);
	first = (float)load.ldavg[0] / (float)load.fscale;
	second = (float)load.ldavg[1] / (float)load.fscale;
	third = (float)load.ldavg[2] / (float)load.fscale;

	if( IsDebugVerbose( D_LOAD ) ) {
		dprintf( D_LOAD, "Load avg: %.2f %.2f %.2f\n", 
			first, second, third );
	}
	return first;
}

#elif defined(WIN32)

/* 
** Load average on WIN32 is calculated by sampling the System
** Processor Queue Length and the % Processor Time of each processor
** in the system.  The first time it is called, calc_load_avg(),
** starts a thread which samples the counter periodically.  The sample
** interval is set by constants below.
*/

#include <stdio.h>
#include <winperf.h>
#include <pdh.h>
#include <pdhmsg.h>

// These next 2 lines allow us to use pdh.dll with MS VC++ 6.0
//#undef PdhOpenQuery
//long __stdcall PdhOpenQuery(LPCSTR  szD, DWORD  dw, HQUERY  *phQ);

#define SAMPLE_INTERVAL 1000		/* take a sample every 1000 ms */
#define NUM_SAMPLES 60000/SAMPLE_INTERVAL	/* num samples per 1 minute */

#undef TESTING

static CRITICAL_SECTION cs;		/* protects samples array */
static struct {
	double load;
	time_t sampletime;
} samples[NUM_SAMPLES];
static int ncpus;

typedef PDH_STATUS (WINAPI *PdhAddCounterPtr)(PDH_HQUERY,LPCSTR,DWORD_PTR,PDH_HCOUNTER*);
static PdhAddCounterPtr pdhAddCounterPtr = NULL;
static HMODULE pdhModule = NULL;

static int WINAPI
sample_load(void *thr_data)
{
	HQUERY hQuery = NULL;
	HCOUNTER hCounterQueueLength, *hCounterCpuLoad;
	int nextsample = 0, i, exit_status = 0;
	PDH_STATUS pdhStatus;
	PDH_FMT_COUNTERVALUE counterval;
	long queuelen;
	double cpuload;
	char counterpath[35];

	EnterCriticalSection(&cs);
	for (i=0; i < NUM_SAMPLES; i++) {
		samples[i].load = 0;
		samples[i].sampletime = (time_t)0;
	}
	LeaveCriticalSection(&cs);

	if(!pdhAddCounterPtr)
	{
		if(!pdhModule)
		{
			pdhModule = GetModuleHandle(TEXT("pdh"));
		}

		if(pdhModule)
			pdhAddCounterPtr = (PdhAddCounterPtr)GetProcAddress(pdhModule, "PdhAddEnglishCounterA");
		
		if(!pdhAddCounterPtr)
			pdhAddCounterPtr = PdhAddCounter;
	}

	pdhStatus = PdhOpenQuery(NULL, 0, &hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		/* dprintf(D_ALWAYS, "PdhOpenQuery returns 0x%x\n", 
			    (int)pdhStatus); */
		return pdhStatus;
	}

	pdhStatus = pdhAddCounterPtr(hQuery, 
							  "\\System\\Processor Queue Length", 
							  0, &hCounterQueueLength);
	
	if (pdhStatus != ERROR_SUCCESS) {
		/* dprintf(D_ALWAYS, "PdhAddCounter returns 0x%x\n", 
						   (int)pdhStatus); */
		PdhCloseQuery(hQuery);
		return pdhStatus;
	}
	hCounterCpuLoad = (HCOUNTER *) malloc(sizeof(HCOUNTER)*ncpus);
	ASSERT( hCounterCpuLoad );
	for (i=0; i < ncpus; i++) {
		sprintf(counterpath, "\\Processor(%d)\\%% Processor Time", i);
		pdhStatus = pdhAddCounterPtr(hQuery, counterpath, 0, 
								  hCounterCpuLoad+i);
		
		if (pdhStatus != ERROR_SUCCESS) {
			/* dprintf(D_ALWAYS, "PdhAddCounter returns 0x%x\n", 
							   (int)pdhStatus); */
			PdhCloseQuery(hQuery);
			return pdhStatus;
		}
	}

	while (1) {

		pdhStatus = PdhCollectQueryData(hQuery);
		if (pdhStatus != ERROR_SUCCESS) {
			/* dprintf(D_ALWAYS, "PdhCollectQueryData returns 0x%x\n", 
							   (int)pdhStatus); */
			exit_status = 4;
			break;
		}

		pdhStatus = PdhGetFormattedCounterValue(hCounterQueueLength, 
												PDH_FMT_LONG,
												NULL, &counterval);
		if (pdhStatus != ERROR_SUCCESS) {
			/* dprintf(D_ALWAYS, "PdhGetFormattedCounterValue returns 0x%x\n",
							   (int)pdhStatus); */
			exit_status = 5;
			break;
		}
		queuelen = counterval.longValue;
		cpuload = 0.0;
		for (i=0; i < ncpus; i++) {
			pdhStatus = PdhGetFormattedCounterValue(hCounterCpuLoad[i], 
													PDH_FMT_DOUBLE,
													NULL, &counterval);
			if (pdhStatus != ERROR_SUCCESS) {
				/* dprintf(D_ALWAYS, "PdhGetFormattedCounterValue returns 0x%x\n",
								   (int)pdhStatus); */
				exit_status = 6;
				break;
			}
			cpuload += counterval.doubleValue/100.0;
		}

		EnterCriticalSection(&cs);
		/* 
		** Here is the code to simulate Unix style load average on Win32.
		** If the system is not fully utilized, the length of the processor
		** queue should be near 0.  When the system is fully utilized,
		** we must discount two items on the processor queue: the system
		** thread and the thread which we displaced to take our measurement.
		** If there are more than 2 items on the queue, we want to add this
		** to our load average to show the additional load on the system.
		*/
		if (queuelen > 2) {
			samples[nextsample].load = cpuload + queuelen - 2;
		} else {
			samples[nextsample].load = cpuload;
		}
		samples[nextsample].sampletime = time(NULL);
		LeaveCriticalSection(&cs);
		nextsample++;
		nextsample %= NUM_SAMPLES;

		Sleep(SAMPLE_INTERVAL);
	}

	// we encountered a problem, so clean up everything and exit.
	for (i=0; i<ncpus;i++) { PdhRemoveCounter(hCounterCpuLoad[i]); }
	free(hCounterCpuLoad);
	PdhCloseQuery(hQuery);

	return exit_status;	
}

float
sysapi_load_avg_raw(void)
{
	static HANDLE threadHandle = NULL;
	static int threadID = -1;
	time_t currentTime;
	double totalLoad=0.0;
	DWORD exitCode = 0;
	BOOL createNewThread = FALSE;
	int numSamples=0, i;

	sysapi_internal_reconfig();
	
	if (threadHandle == NULL) {
		ncpus = sysapi_ncpus();
		InitializeCriticalSection(&cs);
		createNewThread = TRUE;	
	} else {
		if ( ! GetExitCodeThread( threadHandle, &exitCode ) ) {
			dprintf(D_ALWAYS, "GetExitCodeThread() failed. (err=%li)\n",
				   	GetLastError());
		} else {
			if ( exitCode != STILL_ACTIVE ) {
				// somehow the thread died, so we should get its
				// return code, dprintf it and then start a new thread.
				
				dprintf(D_ALWAYS, "loadavg thread died, restarting. "
						"(exit code=%li)\n", exitCode);
				CloseHandle(threadHandle);
				createNewThread = TRUE;
			} else {
				// thread is still alive and well. Go in peace.
				createNewThread = FALSE;
			}
		}
	}
	
	if ( createNewThread == TRUE ) {
		threadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)sample_load, 
			NULL, 0, (LPDWORD)&threadID);
		if (threadHandle == NULL) {
#ifndef TESTING
			dprintf(D_ALWAYS, "failed to create loadavg thread, errno = %d\n",
				GetLastError());
#endif
			return 0.0;
		}
		Sleep(SAMPLE_INTERVAL*5);	/* wait for ~5 samples */
	}
	
	currentTime = time(NULL);
	
	EnterCriticalSection(&cs);
	for (i=0; i < NUM_SAMPLES; i++) {
		/* if this sample occurred within the minute, then add to total */
		if ((currentTime-60) <= samples[i].sampletime) {
			totalLoad += samples[i].load;
			numSamples++;
		}
	}
	LeaveCriticalSection(&cs);

	if (numSamples == 0) {
#ifndef TESTING
		dprintf(D_ALWAYS, "no loadavg samples this minute, maybe thread died???\n");
#endif
		return 0.0;
	}

#ifndef TESTING
	dprintf(D_LOAD, "loadavg=%.2f with %d samples\n",
			((float)totalLoad)/((float)numSamples), numSamples);
#endif

	return ((float)totalLoad)/((float)numSamples);
}

#if defined(TESTING)
int main()
{
	while (1) {
		printf("%.2f\n", sysapi_load_avg());
		Sleep(5000);
	}

	return 0;
}
#endif

/* END WIN32 */

#elif defined(AIX)

/* For now, just get this value out of uptime.... */

float lookup_load_avg_via_uptime();

float
sysapi_load_avg_raw(void)
{

	float val;

	sysapi_internal_reconfig();
	val = lookup_load_avg_via_uptime();

	if( IsDebugVerbose( D_LOAD ) ) {
		dprintf( D_LOAD, "Load avg: %.2f\n", val );
	}
	return val;
}

#else

#error You must define sysapi_load_avg_raw() for this platform!

#endif 


/*----------------------------------------------------------------------*/
/* only include this helper function on these architectures */

#if defined(Solaris) || defined(AIX)

/*
 *  We will use uptime(1) to get the load average.  We will return the one
 *  minute load average by parsing its output.  This is fairly portable and
 *  doesn't require root permission.  Sample output from a mips-dec-ultrix4.3
 *
 *  example% /usr/ucb/uptime
 *    8:52pm  up 1 day, 22:28,  4 users,  load average: 0.23, 0.08, 0.01
 *
 *  The third last number is the required load average.
 */

#define DEFAULT_LOADAVG         0.10

static char            *uptime_path;

/*
 *  path_to_uptime
 *
 *  Check for executable uptime is /usr/ucb and /usr/bin in that order.
 *  If uptime is found in either of these, return the full path of uptime,
 *  e.g., /usr/ucb/uptime.  Otherwise return NULL.
 */

char *path_to_uptime(void)
{
        static char upt_path[16];

        if (access("/usr/ucb/uptime", X_OK) == 0)
        {
                strcpy(upt_path, "/usr/ucb/uptime");
                return upt_path;
    }
        else if (access("/usr/bin/uptime", X_OK) == 0)
        {
                strcpy(upt_path, "/usr/bin/uptime");
                return upt_path;
    }
        else if (access("/usr/bsd/uptime", X_OK) == 0)
        {
                strcpy(upt_path, "/usr/bsd/uptime");
                return upt_path;
    }
        else
                return NULL;
}

float
lookup_load_avg_via_uptime()
{
	
	float    loadavg;
	FILE *output_fp;
	int counter;
	char word[20];

	if (uptime_path == NULL) {
		uptime_path = path_to_uptime();
	}

	/*  We start uptime and pipe its output to ourselves.
	 *  Then we read word by word till we get "load average".  We read the
	 *  next number.  This is the number we want.
	 */
	if (uptime_path != NULL) {
		char *args[2] = {uptime_path, NULL};
		if ((output_fp = my_popenv(args, "r", FALSE)) == NULL) {
			return DEFAULT_LOADAVG;
		}
		
		do { 
			if (fscanf(output_fp, "%s", word) == EOF) {
				dprintf(D_ALWAYS,"can't get \"average:\" from uptime\n");
				my_pclose(output_fp);
				return DEFAULT_LOADAVG;
			}
			
			if (strcmp(word, "average:") == 0) {
				/*
				 *  We are at the required position.  Read in the next
				 *  floating
				 *  point number.  That is the required average.
				 */
				if (fscanf(output_fp, "%f", &loadavg) != 1) {
					dprintf(D_ALWAYS, "can't read loadavg from uptime\n");
					my_pclose(output_fp);
					return DEFAULT_LOADAVG;
				}
				
				/*
				 *  Some callers of this routine may have a SIGCHLD handler.
				 *  If this is so, calling pclose will interfere withthat.
				 *  We check if this is the case and use fclose instead.
				 *  -- Ajitk
				 */
				my_pclose(output_fp);
				return loadavg;
			}
		} while (!feof(output_fp)); 
		
		/*
		 *  Reached EOF before getting at load average!  -- Ajitk
		 */
        
		my_pclose(output_fp);
	}

	
	/* not reached */
	return DEFAULT_LOADAVG;
}

#endif /* #if defined(Solaris) */

