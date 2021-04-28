#include <time.h>
#include <string.h>

#include <string>
#include <vector>

// For pi_dynlink.h
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define NOSERVICE
#define NOMCX
#define NOIME
#include <Windows.h>
#endif

#include "pi_dynlink.h"

#include "nvml_stub.h"
#include "cuda_header_doc.h"
#include "cuda_device_enumeration.h"

int sim_index = 0;
int sim_device_count = 0;

//
// tables of simulated cuda devices, so we can test HTCondor startd and starter without 
// actually having a GPU.
//

struct _simulated_cuda_device {
	const char * name;
	int SM;    // capability 0xMm (hexidecimal notation), M = SM Major version, and m = SM minor version
	int clockRate;
	int multiProcessorCount;
	int ECCEnabled;
	size_t totalGlobalMem;
};

struct _simulated_cuda_config {
	int driverVer;
	int deviceCount;
	const struct _simulated_cuda_device * device;
};

static const struct _simulated_cuda_device GeForceGTX480 = { "GeForce GTX 480", 0x20, 1400*1000, 15, 0, 1536*1024*1024 };
static const struct _simulated_cuda_device GeForceGT330 = { "GeForce GT 330",  0x12, 1340*1000, 12,  0, 1024*1024*1024 };
static const struct _simulated_cuda_config aSimConfig[] = {
	{6000, 1, &GeForceGT330 },
	{4020, 2, &GeForceGTX480 },
};

const int sim_index_max = (int)(sizeof(aSimConfig)/sizeof(aSimConfig[0]));

cudaError_t CUDACALL sim_cudaGetDeviceCount(int* pdevs) {
	*pdevs = 0;
	if (sim_index < 0 || sim_index > sim_index_max)
		return cudaErrorInvalidValue;
	if (sim_index == sim_index_max) {
		*pdevs = sim_index_max * (sim_device_count?sim_device_count:1);
	} else if (sim_device_count) {
		*pdevs = sim_device_count;
	} else {
		*pdevs = aSimConfig[sim_index].deviceCount;
	}
	return cudaSuccess;
}

cudaError_t CUDACALL sim_cudaDriverGetVersion(int* pver) {
	if (sim_index < 0 || sim_index > sim_index_max)
		return cudaErrorInvalidValue;
	int ix = sim_index < sim_index_max ? sim_index : 0;
	*pver = aSimConfig[ix].driverVer;
	return cudaSuccess; 
}

cudaError_t CUDACALL sim_getBasicProps(int devID, BasicProps * p) {
	if (sim_index < 0 || sim_index > sim_index_max)
		return cudaErrorNoDevice;

	const struct _simulated_cuda_device * dev;
	if (sim_index == sim_index_max) {
		int iter = (sim_device_count ? sim_device_count : 1);
		dev = aSimConfig[devID/iter].device;
	} else {
		int cDevs = sim_device_count ? sim_device_count : aSimConfig[sim_index].deviceCount;
		if (devID < 0 || devID > cDevs)
			return cudaErrorInvalidDevice;
		dev = aSimConfig[sim_index].device;
	}

	p->name = dev->name;
	unsigned char uuid[16] = {0xa1,0x22,0x33,0x34,  0x44,0x45, 0x56,0x67, 0x89,0x9a, 0xab,0xbc,0xcd,0xde,0xef,0xf0 };
	uuid[0] = (unsigned char)((devID*0x11) + 0xa0);
	p->setUUIDFromBuffer( uuid );
	sprintf(p->pciId, "0000:%02x:00.0", devID + 0x40);
	p->ccMajor = (dev->SM & 0xF0) >> 4;
	p->ccMinor = (dev->SM & 0x0F);
	p->multiProcessorCount = dev->multiProcessorCount;
	p->clockRate = dev->clockRate;
	p->totalGlobalMem = dev->totalGlobalMem;
	p->ECCEnabled = dev->ECCEnabled;
	return cudaSuccess;
}

int sim_jitter = 0;
nvmlReturn_t sim_nvmlInit(void) { sim_jitter = (int)(time(NULL) % 10); return NVML_SUCCESS; }
nvmlReturn_t sim_nvmlDeviceGetFanSpeed(nvmlDevice_t dev, unsigned int * pval) { *pval = 9+sim_jitter+(int)(size_t)dev; return NVML_SUCCESS; }
nvmlReturn_t sim_nvmlDeviceGetPowerUsage(nvmlDevice_t dev, unsigned int * pval) { *pval = 29+sim_jitter+(int)(size_t)dev; return NVML_SUCCESS; }
nvmlReturn_t sim_nvmlDeviceGetTemperature(nvmlDevice_t dev, nvmlTemperatureSensors_t /*sensor*/, unsigned int * pval) { *pval = 89+sim_jitter+(int)(size_t)dev; return NVML_SUCCESS;}
nvmlReturn_t sim_nvmlDeviceGetTotalEccErrors(nvmlDevice_t /*dev*/, nvmlMemoryErrorType_t /*met*/, nvmlEccCounterType_t /*mec*/, unsigned long long * pval) { *pval = 0; /*sim_jitter-1+(int)dev;*/ return NVML_SUCCESS; }

nvmlReturn_t
sim_nvmlDeviceGetHandleByUUID(const char * uuid, nvmlDevice_t * pdev) {
	unsigned int devID;

	if( sscanf(uuid, "%02x223334-4445-5667-899aabbccddeeff0", & devID) != 1 ) {
		return NVML_ERROR_NOT_FOUND;
	}
	devID = (devID - 0xa0)/0x11;

	* pdev = (nvmlDevice_t)(size_t)(devID+1);
	return NVML_SUCCESS;
};

nvmlReturn_t
sim_findNVMLDeviceHandle(const std::string & uuid, nvmlDevice_t * device) {
	return nvmlDeviceGetHandleByUUID( uuid.c_str(), device );
}

void
setSimulatedCUDAFunctionPointers() {
    getBasicProps = sim_getBasicProps;
    cuDeviceGetCount = sim_cudaGetDeviceCount;
    cudaDriverGetVersion = sim_cudaDriverGetVersion;
}

void
setSimulatedNVMLFunctionPointers() {
    nvmlInit = sim_nvmlInit;
    nvmlShutdown = sim_nvmlInit;

    nvmlDeviceGetFanSpeed = sim_nvmlDeviceGetFanSpeed;
    nvmlDeviceGetPowerUsage = sim_nvmlDeviceGetPowerUsage;
    nvmlDeviceGetTemperature = sim_nvmlDeviceGetTemperature;
    nvmlDeviceGetTotalEccErrors = sim_nvmlDeviceGetTotalEccErrors;
    nvmlDeviceGetHandleByUUID = sim_nvmlDeviceGetHandleByUUID;

	findNVMLDeviceHandle = sim_findNVMLDeviceHandle;
}
