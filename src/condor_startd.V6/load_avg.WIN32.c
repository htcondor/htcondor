/* 
** Copyright 1997 by the Condor Design Team
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
** Author:   Jim Basney
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/

/* 
** Load average on WIN32 is calculated by sampling the System
** Processor Queue Length.  The first time it is called, calc_load_avg(),
** starts a thread which samples the counter periodically.  The sample
** interval is set by constants below.
*/

#include <stdio.h>
#include "condor_common.h"
#include "condor_debug.h"
#include <winperf.h>
#include <pdh.h>
#include <pdhmsg.h>

#define SAMPLE_INTERVAL 1000		/* take a sample every 1000 ms */
#define NUM_SAMPLES 60000/SAMPLE_INTERVAL	/* num samples per 1 minute */

#undef TESTING
#if defined(TESTING)
int SetSyscalls(int x) { return x; }	/* dummy stub */
#endif

static CRITICAL_SECTION cs;		/* protects samples array */
static struct {
	long queuelen;
	time_t sampletime;
} samples[NUM_SAMPLES];

static int WINAPI
sample_load(void *thr_data)
{
	HQUERY hQuery = NULL;
	HCOUNTER hCounter;
	int nextsample = 0, i;
	PDH_STATUS pdhStatus;
	PDH_FMT_COUNTERVALUE loadavg;

	EnterCriticalSection(&cs);
	for (i=0; i < NUM_SAMPLES; i++) {
		samples[i].queuelen = 0;
		samples[i].sampletime = (time_t)0;
	}
	LeaveCriticalSection(&cs);

	pdhStatus = PdhOpenQuery(NULL, 0, &hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		/* dprintf(D_ALWAYS, "PdhOpenQuery returns 0x%x\n", 
			    (int)pdhStatus); */
		return 1;
	}
	pdhStatus = PdhAddCounter(hQuery, 
							  "\\System\\Processor Queue Length", 
							  0, &hCounter);
	if (pdhStatus != ERROR_SUCCESS) {
		/* dprintf(D_ALWAYS, "PdhAddCounter returns 0x%x\n", 
						   (int)pdhStatus); */
		return 2;
	}

	while (1) {

		pdhStatus = PdhCollectQueryData(hQuery);
		if (pdhStatus != ERROR_SUCCESS) {
			/* dprintf(D_ALWAYS, "PdhCollectQueryData returns 0x%x\n", 
							   (int)pdhStatus); */
			return 3;
		}

		pdhStatus = PdhGetFormattedCounterValue(hCounter, PDH_FMT_LONG,
												NULL, &loadavg);
		if (pdhStatus != ERROR_SUCCESS) {
			/* dprintf(D_ALWAYS, "PdhGetFormattedCounterValue returns 0x%x\n",
							   (int)pdhStatus); */
			return 4;
		}

		EnterCriticalSection(&cs);
		samples[nextsample].queuelen = loadavg.longValue;
		samples[nextsample].sampletime = time(NULL);
		LeaveCriticalSection(&cs);
		nextsample++;
		nextsample %= NUM_SAMPLES;

		Sleep(SAMPLE_INTERVAL);

	}

	return 0;	/* should never reach this point */
}

float
calc_load_avg()
{
	static HANDLE threadHandle = NULL;
	static int threadID = -1;
	time_t currentTime;
	long totalQueueLen=0;
	int numSamples=0, i;

	if (threadHandle == NULL) {
		InitializeCriticalSection(&cs);
		threadHandle = CreateThread(NULL, 0, sample_load, 
									NULL, 0, &threadID);
		if (threadHandle == NULL) {
			dprintf(D_ALWAYS, "failed to create loadavg thread, errno = %d\n",
					GetLastError());
			return 0.0;
		}
		Sleep(SAMPLE_INTERVAL*5);	/* wait for ~5 samples */
	}

	currentTime = time(NULL);

	EnterCriticalSection(&cs);
	for (i=0; i < NUM_SAMPLES; i++) {
		/* if this sample occurred within the minute, then add to total */
		if ((currentTime-60) <= samples[i].sampletime) {
			totalQueueLen += samples[i].queuelen;
			numSamples++;
		}
	}
	LeaveCriticalSection(&cs);

	if (numSamples == 0) {
		dprintf(D_ALWAYS, "no loadavg samples this minute, maybe thread died???\n");
		return 0.0;
	}

	dprintf(D_LOAD, "loadavg=%f with %d samples\n",
			((float)totalQueueLen)/((float)numSamples), numSamples);

	return ((float)totalQueueLen)/((float)numSamples);
}

#if defined(TESTING)
int main()
{
	while (1) {
		printf("%f\n", calc_load_avg());
		Sleep(5000);
	}

	return 0;
}
#endif