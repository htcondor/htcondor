#include <stdio.h>
#include <sys/select.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <dlfcn.h>
#include "nvml_stub.h"

typedef nvmlReturn_t (*nvml_void)(void);
typedef nvmlReturn_t (*nvml_dghbi)(unsigned int, nvmlDevice_t *);
typedef nvmlReturn_t (*nvml_unsigned_int)(unsigned int *);
typedef nvmlReturn_t (*nvml_dgs)( nvmlDevice_t, nvmlSamplingType_t, unsigned long long, nvmlValueType_t *, unsigned int *, nvmlSample_t * );
typedef nvmlReturn_t (*nvml_dm)( nvmlDevice_t, nvmlMemory_t * );
typedef const char * (*cc_nvml)( nvmlReturn_t );

nvml_dgs nvmlDeviceGetSamples = NULL;
nvml_unsigned_int nvmlDeviceGetCount = NULL;
nvml_dghbi nvmlDeviceGetHandleByIndex = NULL;
nvml_dm nvmlDeviceGetMemoryInfo = NULL;
cc_nvml nvmlErrorString = NULL;

unsigned debug = 0;
unsigned reportInterval = 10;

int compareSamples( const void * vpA, const void * vpB ) {
	const nvmlSample_t * a = (const nvmlSample_t *)vpA;
	const nvmlSample_t * b = (const nvmlSample_t *)vpB;
	if( a->timeStamp < b->timeStamp ) {
		return -1;
	} else if( a->timeStamp == b->timeStamp ) {
		return 0;
	} else {
		return 1;
	}
}

void fail() __attribute__((__noreturn__));

void fail() {
	fprintf( stderr, "Hanging to prevent process churn.\n" );
	while( 1 ) { sleep( 1024 ); }
}

nvmlReturn_t getElapsedTimeForDevice( nvmlDevice_t d, unsigned long long * lastSample, unsigned long long * elapsedTime, unsigned maxSampleCount ) {
	if( elapsedTime == NULL || lastSample == NULL ) {
		return NVML_ERROR_INVALID_ARGUMENT;
	}

	unsigned sampleCount = maxSampleCount;
	nvmlSample_t samples[maxSampleCount];
	nvmlValueType_t sampleValueType;

	nvmlReturn_t r = nvmlDeviceGetSamples( d, NVML_GPU_UTILIZATION_SAMPLES, * lastSample, & sampleValueType, & sampleCount, samples );
	switch( r ) {
		case NVML_ERROR_NOT_FOUND:
			return NVML_SUCCESS;
		case NVML_SUCCESS:
			break;
		default:
			return r;
	}

	// Samples are usually but not always in order.
	qsort( samples, sampleCount, sizeof( nvmlSample_t ), compareSamples );

	// Convert the percentage to elapsed time, since that's what we
	// actually care about (and the data representation is simpler).
	if( (*lastSample) == 0 && sampleCount > 0 ) {
		(*lastSample) = samples[0].timeStamp;
	}
	for( unsigned i = 0; i < sampleCount; ++i ) {
		/* debug fprintf( stderr, "%llu + (%llu - %llu) * %f = ",
			* elapsedTime, samples[i].timeStamp, * lastSample,
			(double)(samples[i].sampleValue.uiVal)/100.0 ); */

		(*elapsedTime) += (unsigned long long)(
			(samples[i].timeStamp - (*lastSample)) *
			((double)(samples[i].sampleValue.uiVal)/100.0)
		);

		/* debug fprintf( stderr, "%llu\n", * elapsedTime ); */

		(*lastSample) = samples[i].timeStamp;
	}

	return NVML_SUCCESS;
}

// int main( int argc, char ** argv ) {
int main() {
	nvml_void nvmlInit = NULL;
	nvml_void nvmlShutdown = NULL;

	void * nvml_handle = NULL;

	const char * nvml_library = "libnvidia-ml.so";
	nvml_handle = dlopen( nvml_library, RTLD_LAZY );
	if(! nvml_handle) {
		fprintf( stderr, "Unable to load %s, aborting.\n", nvml_library );
		fail();
	}
	dlerror();

	nvmlInit                   = (nvml_void)dlsym( nvml_handle, "nvmlInit" );
	nvmlShutdown               = (nvml_void)dlsym( nvml_handle, "nvmlShutdown" );
	nvmlDeviceGetCount         = (nvml_unsigned_int)dlsym( nvml_handle, "nvmlDeviceGetCount" );
	nvmlDeviceGetSamples       = (nvml_dgs)dlsym( nvml_handle, "nvmlDeviceGetSamples" );
	nvmlDeviceGetMemoryInfo    = (nvml_dm)dlsym( nvml_handle, "nvmlDeviceGetMemoryInfo" );
	nvmlDeviceGetHandleByIndex = (nvml_dghbi)dlsym( nvml_handle, "nvmlDeviceGetHandleByIndex" );
	nvmlErrorString            = (cc_nvml)dlsym( nvml_handle, "nvmlErrorString" );

	nvmlReturn_t r = nvmlInit();
	if( r != NVML_SUCCESS ) {
		fprintf( stderr, "nvmlInit() failed, aborting.\n" );
		fail();
	}

	unsigned int deviceCount;
	r = nvmlDeviceGetCount( &deviceCount );
	if( r != NVML_SUCCESS ) {
		fprintf( stderr, "nvmlDeviceGetCount() failed, aborting.\n" );
		fail();
	}
	if( deviceCount <= 0 ) {
		fprintf( stderr, "Found 0 or fewer devices, aborting.\n" );
		fail();
	}

	nvmlDevice_t devices[deviceCount];
	unsigned maxSampleCounts[deviceCount];
	unsigned long long memoryUsage[deviceCount];
	unsigned long long lastSamples[deviceCount];
	unsigned long long firstSamples[deviceCount];
	unsigned long long elapsedTimes[deviceCount];
	for( unsigned i = 0; i < deviceCount; ++i ) {
		r = nvmlDeviceGetHandleByIndex( i, &(devices[i]) );
		if( r != NVML_SUCCESS ) {
			fprintf( stderr, "nvmlGetDeviceHandleByIndex(%u) failed (%d: %s), aborting.\n", i, r, nvmlErrorString( r ) );
			fail();
		}

		lastSamples[i] = 0;
		firstSamples[i] = 0;
		elapsedTimes[i] = 0;

		nvmlValueType_t sampleValueType;
		r = nvmlDeviceGetSamples( devices[i], NVML_GPU_UTILIZATION_SAMPLES, 0, & sampleValueType, & maxSampleCounts[i], NULL );
		if( r != NVML_SUCCESS ) {
			fprintf( stderr, "nvmlDeviceGetSamples(%u) failed while querying for the max sample count (%d: %s), aborting.\n", i, r, nvmlErrorString( r ) );
			fail();
		}
		if( sampleValueType != NVML_VALUE_TYPE_UNSIGNED_INT ) {
			fprintf( stderr, "nvmlDeviceGetSamples(%u) returned an unexpected type (%d) of sample when querying for the max sample count, aborting.\n", i, sampleValueType );
			fail();
		}

		memoryUsage[i] = 0;
	}

	// We deliberately ignore the first set of samples.  Partly, I think we
	// can claim that they're from before we started monitoring.  More
	// importantly, at least on a Tesla K40c (driver 384.81), initializing
	// the NVML library causes a one-second 99% usage spike on an other-
	// wise idle GPU.  So we'll ignore as much of that as we easily can.
	for( unsigned i = 0; i < deviceCount; ++i ) {
		getElapsedTimeForDevice( devices[i], &lastSamples[i], &elapsedTimes[i], maxSampleCounts[i] );
		firstSamples[i] = lastSamples[i];
		elapsedTimes[i] = 0;
	}

	time_t lastReport = time( NULL );
	while( 1 ) {
		usleep( 100000 );

		for( unsigned i = 0; i < deviceCount; ++i ) {
			r = getElapsedTimeForDevice( devices[i], &lastSamples[i], &elapsedTimes[i], maxSampleCounts[i] );
			if( r != NVML_SUCCESS ) {
				fprintf( stderr, "getElapsedTimeForDevice(%u) failed (%d: %s), aborting.\n", i, r, nvmlErrorString( r ) );
				fail();
			}

			if( debug ) {
				fprintf( stdout, "device %u: %llu / %llu = %u%%.\n",
					i, elapsedTimes[i], lastSamples[i] - firstSamples[i],
					(unsigned)(((double)elapsedTimes[i]) / (lastSamples[i] - firstSamples[i]) * 100)
				);
			}

			nvmlMemory_t mi = { 0, 0, 0 };
			nvmlDeviceGetMemoryInfo( devices[i], &mi );
			if( mi.used > memoryUsage[i] ) {
				memoryUsage[i] = mi.used;
			}
		}

		if( time( NULL ) - lastReport >= reportInterval ) {
			for( unsigned i = 0; i < deviceCount; ++i ) {
				fprintf( stdout, "SlotMergeConstraint = StringListMember( \"CUDA%u\", AssignedGPUs )\n", i );
				fprintf( stdout, "UptimeGPUsSeconds = %.6f\n", elapsedTimes[i] / 1000000.0 );
				fprintf( stdout, "UptimeGPUsMemoryPeakUsage = %llu\n", (memoryUsage[i] + (1024 * 1024) -1) / (1024 * 1024) );
				fprintf( stdout, "- GPUsSlot%u\n", i );
				fflush( stdout );

				// Report only the usage for each reporting period.
				elapsedTimes[i] = 0;
				firstSamples[i] = lastSamples[i];
				memoryUsage[i] = 0;
			}
			lastReport = time( NULL );
		}
	}

	r = nvmlShutdown();
	if( r != NVML_SUCCESS ) {
		fprintf( stderr, "nvmlShutdown() failed (%d: %s), aborting.\n", r, nvmlErrorString( r ) );
		return 1;
	}

	dlclose( nvml_handle );
	return 0;
}
