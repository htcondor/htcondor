#include <stdlib.h>
#include <string.h>

#include <vector>
#include <string>

// For pi_dynlink.h
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define NOSERVICE
#define NOMCX
#define NOIME
#include <Windows.h>
#endif

#include "pi_dynlink.h"
#include "hip_header_doc.h"
#include "hip_device_enumeration.h"




#include "print_error.h"

static struct {
	hipDeviceCount_t hipGetDeviceCount;
	hipDeviceProperties_t hipGetDeviceProperties;
	hipDriverGetVersion_t 	hipDriverGetVersion;
} hip = { NULL, NULL,NULL };




static int hip_device_count = 0;
static int hip_was_initialized = 0;
static std::vector<hipDeviceProp_t> hip_platforms;

int hip_Init(void) {

	print_error(MODE_DIAGNOSTIC_MSG, "diag: hip_Init()\n");
	if (hip_was_initialized) {
		return 0;
	}
	hip_was_initialized = 1;

	if (! hip.hipGetDeviceCount) {
		return -1;
	}

	int hPlatforms = 0;
	hipError_t hip_return = hip.hipGetDeviceCount(&hPlatforms);
	print_error(MODE_DIAGNOSTIC_MSG, "diag: hipDevice count: %d \n",hPlatforms);	
	if (hip_return == hipSuccess && hPlatforms > 0) {
		hip_platforms.reserve(hPlatforms);
		for (int ii = 0; ii < hPlatforms; ++ii) {
			hipDeviceProp_t tmp;
			hip.hipGetDeviceProperties(&tmp,ii);
			
			hip_platforms.push_back(tmp);
		}
	}

	// enable logging in oclGetInfo if verbose

	hip_device_count = (int)hPlatforms;
	return 0;
}

int HIP_CALL hip_GetDeviceCount(int * pcount) {
	hip_Init();
	*pcount = hip_device_count;
	return HIP_SUCCESS;
}

dlopen_return_t
loadHipLibrary() {
#if       defined(WIN32)
	const char * hip_library = "amdhip64.dll";
#else  /* defined(WIN32) */
	const char * hip_library = "libamdhip64.so.6";
#endif /* defined(WIN32) */

	dlopen_return_t hip_handle = dlopen( hip_library, RTLD_LAZY );
	if(! hip_handle) {
		print_error(MODE_DIAGNOSTIC_MSG, "Can't open library '%s': '%s'\n", hip_library, dlerror());
	}

	dlerror(); // reset error
	return hip_handle;
}

dlopen_return_t
setHIPFunctionPointers() {
	dlopen_return_t hip_handle = loadHipLibrary();
	if(! hip_handle) { return NULL; }

	hip.hipGetDeviceCount = (hipDeviceCount_t) dlsym(hip_handle, "hipGetDeviceCount");
	hip.hipGetDeviceProperties = (hipDeviceProperties_t) dlsym(hip_handle, "hipGetDevicePropertiesR0600");
	hip.hipDriverGetVersion = (hipDriverGetVersion_t) dlsym(hip_handle,"hipDriverGetVersion");
	return hip_handle;
}
hipError_t hip_getBasicProps(int dev, BasicProps *bp);

bool enumerateHIPDevices( std::vector< BasicProps > & devices ){
	hip_Init();
	for( int dev = 0; dev < hip_device_count; ++dev){
		BasicProps bp;
		if( hipSuccess == hip_getBasicProps(dev, &bp) ){
			devices.push_back(bp);
		}
	}
	return true;

}

hipError_t hip_getBasicProps(int dev, BasicProps *bp){

	hipDeviceProp_t dev_props = hip_platforms[dev];
	bp->setUUIDFromBuffer((unsigned char *)dev_props.uuid);
	bp->ccMajor = dev_props.major;
	bp->ccMinor = dev_props.minor;
	bp->totalGlobalMem = dev_props.totalGlobalMem;
	bp->clockRate = dev_props.clockRate;
	bp->multiProcessorCount = dev_props.multiProcessorCount;
	bp->ECCEnabled = dev_props.ECCEnabled;
	bp->name = dev_props.gcnArchName;	
	bp->xNACK = dev_props.unifiedAddressing;	
	bp->warpSize = dev_props.warpSize;
	hip.hipDriverGetVersion(& bp->driverVersion);
	return hipSuccess;
}

