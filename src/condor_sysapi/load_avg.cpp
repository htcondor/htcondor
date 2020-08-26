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
	#if defined(LINUX) 
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

#if defined(LINUX)

//prototype
void get_k_vars(void);

float
sysapi_load_avg_raw(void)
{
    FILE	*proc;
	float	short_avg, medium_avg, long_avg;

	sysapi_internal_reconfig();

	// /proc/loadavg looks like:

	
	// Kernel Version 2.0.0:
	// 0.03 0.03 0.09 2/42 15582
	// 
	// Update 6/1/2015:  Looks exactly the same in kernel 4.x

    proc=safe_fopen_wrapper_follow("/proc/loadavg","r",0644);
    if(!proc)
		return -1;

    if (fscanf(proc, "%f %f %f", &short_avg, &medium_avg, &long_avg) != 3) {
		dprintf(D_ALWAYS, "Failed to fscanf 3 floats from /proc/loadavg\n");
		fclose(proc);
		return -1;
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
		//PRAGMA_REMIND("FIXME: is it correct to use the hyperthread count for load average?")
		sysapi_ncpus(NULL, &ncpus);
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

#else

#error You must define sysapi_load_avg_raw() for this platform!

#endif 


