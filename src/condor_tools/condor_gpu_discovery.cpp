/***************************************************************
 *
 * Copyright (C) 1990-2013, HTCondor Team, Computer Sciences Department,
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


#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <time.h>


// we don't want to use the actual cuda headers because we want to build on machines where they are not installed.
//#include <cuda_runtime_api.h>
//#include "nvml.h"
#include "cuda_header_doc.h"
#include "nvml_stub.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define NOSERVICE
#define NOMCX
#define NOIME
#include <Windows.h>
#define CUDACALL __stdcall
#else
#include <dlfcn.h>
#define CUDACALL 
#endif

#ifdef WIN32 
// to simplfy the dynamic loading of .so/.dll, make a Windows function
// for looking up a symbol that looks like the equivalent *nix function.
static void* dlsym(void* hlib, const char * symbol) {
	return (void*)GetProcAddress((HINSTANCE)(LONG_PTR)hlib, symbol);
}
#endif

// this coded was stolen from helper_cuda.h in the cuda 5.5 sdk.
int ConvertSMVer2Cores(int major, int minor)
{
	// Defines for GPU Architecture types (using the SM version to determine the # of cores per SM
    typedef struct {
        int SM; // 0xMm (hexidecimal notation), M = SM Major version, and m = SM minor version
        int Cores;
	} sSMtoCores;

    sSMtoCores nGpuArchCoresPerSM[] = 
    { 
        { 0x10,  8 }, // Tesla Generation (SM 1.0) G80 class
        { 0x11,  8 }, // Tesla Generation (SM 1.1) G8x class
        { 0x12,  8 }, // Tesla Generation (SM 1.2) G9x class
        { 0x13,  8 }, // Tesla Generation (SM 1.3) GT200 class
        { 0x20, 32 }, // Fermi Generation (SM 2.0) GF100 class
        { 0x21, 48 }, // Fermi Generation (SM 2.1) GF10x class
        { 0x30, 192}, // Kepler Generation (SM 3.0) GK10x class
        { 0x35, 192}, // Kepler Generation (SM 3.5) GK11x class
        {   -1, -1 }
    };

    int index = 0;
    while (nGpuArchCoresPerSM[index].SM != -1) {
        if (nGpuArchCoresPerSM[index].SM == ((major << 4) + minor) ) {
            return nGpuArchCoresPerSM[index].Cores;
        }
        index++;
    }
    // If we don't find the values, default to the last known generation
    return nGpuArchCoresPerSM[index-1].Cores;
}

// tables of simulated cuda devices, so we can test HTCondor startd and starter without 
// actually having a GPU.
//
struct _simulated_cuda_device {
	const char * name;
	int SM;    // capability 0xMm (hexidecimal notation), M = SM Major version, and m = SM minor version
	int clockRate;
	int multiProcessorCount;
	size_t totalGlobalMem;
};
struct _simulated_cuda_config {
	int driverVer;
	int runtimeVer;
	int deviceCount;
	const struct _simulated_cuda_device * device;
};

static const struct _simulated_cuda_device GeForceGTX480 = { "GeForce GTX 480", 0x20, 1400*1000, 15, 1536*1024*1024 };
static const struct _simulated_cuda_device GeForceGT330 = { "GeForce GT 330",  0x12, 1340*1000, 12,  1024*1024*1024 };
static const struct _simulated_cuda_config aSimConfig[] = {
	{6000, 5050, 1, &GeForceGT330 },
	{4020, 4010, 2, &GeForceGTX480 },
};
const int sim_index_max = (int)(sizeof(aSimConfig)/sizeof(aSimConfig[0]));

static int sim_index = 0;
static int sim_device_count = 0;

cudaError_t CUDACALL sim_cudaGetDeviceCount(int* pdevs) {
	*pdevs = 0;
	if (sim_index < 0 || sim_index >= sim_index_max)
		return cudaErrorInvalidValue;
	if (sim_device_count) {
		*pdevs = sim_device_count;
	} else {
		*pdevs = aSimConfig[sim_index].deviceCount;
	}
	return cudaSuccess; 
}
cudaError_t CUDACALL sim_cudaDriverGetVersion(int* pver) {
	*pver = aSimConfig[sim_index].driverVer;
	return cudaSuccess; 
}
cudaError_t CUDACALL sim_cudaRuntimeGetVersion(int* pver) {
	*pver = aSimConfig[sim_index].runtimeVer;
	return cudaSuccess; 
}

cudaError_t CUDACALL sim_cudaGetDeviceProperties(struct cudaDeviceProp * p, int devID) {
	if (sim_index < 0 || sim_index >= sim_index_max)
		return cudaErrorNoDevice;

	const struct _simulated_cuda_device * dev = aSimConfig[sim_index].device;
	int cDevs = sim_device_count ? sim_device_count : aSimConfig[sim_index].deviceCount;

	if (devID < 0 || devID > cDevs)
		return cudaErrorInvalidDevice;

	strncpy(p->name, dev->name, sizeof(p->name));
	p->major = (dev->SM & 0xF0) >> 4;
	p->minor = (dev->SM & 0x0F);
	p->multiProcessorCount = dev->multiProcessorCount;
	p->clockRate = dev->clockRate;
	p->totalGlobalMem = dev->totalGlobalMem;
	return cudaSuccess;
}

int sim_jitter = 0;
nvmlReturn_t sim_nvmlInit(void) { sim_jitter = (int)(time(NULL) % 10); return NVML_SUCCESS; }
nvmlReturn_t sim_nvmlDeviceGetHandleByIndex(unsigned int ix, nvmlDevice_t * pdev) { *pdev=(nvmlDevice_t)(size_t)(ix+1); return NVML_SUCCESS; };
nvmlReturn_t sim_nvmlDeviceGetFanSpeed(nvmlDevice_t dev, unsigned int * pval) { *pval = 9+sim_jitter+(int)(size_t)dev; return NVML_SUCCESS; }
nvmlReturn_t sim_nvmlDeviceGetPowerUsage(nvmlDevice_t dev, unsigned int * pval) { *pval = 29+sim_jitter+(int)(size_t)dev; return NVML_SUCCESS; }
nvmlReturn_t sim_nvmlDeviceGetTemperature(nvmlDevice_t dev, nvmlTemperatureSensors_t /*sensor*/, unsigned int * pval) { *pval = 89+sim_jitter+(int)(size_t)dev; return NVML_SUCCESS;}
nvmlReturn_t sim_nvmlDeviceGetTotalEccErrors(nvmlDevice_t /*dev*/, nvmlMemoryErrorType_t /*met*/, nvmlEccCounterType_t /*mec*/, unsigned long long * pval) { *pval = 0; /*sim_jitter-1+(int)dev;*/ return NVML_SUCCESS; }

int g_verbose = 0;
const char ** g_environ = NULL;
void* g_cu_handle = NULL;
// functions for runtime linking to nvcuda library
typedef void * cudev;
typedef cudaError_t (CUDACALL* cuda_uint_t)(unsigned int);
cuda_uint_t cuInit = NULL;
typedef cudaError_t (CUDACALL* cuda_dev_int_t)(cudev*,int);
cuda_dev_int_t cuDeviceGet = NULL;
typedef cudaError_t (CUDACALL* cuda_ga_t)(int*,int,cudev);
cuda_ga_t cuDeviceGetAttribute = NULL;
typedef cudaError_t (CUDACALL* cuda_name_t)(char*,int,cudev);
cuda_name_t cuDeviceGetName = NULL;
typedef cudaError_t (CUDACALL* cuda_cc_t)(int*,int*,cudev);
cuda_cc_t cuDeviceComputeCapability = NULL;
typedef cudaError_t (CUDACALL* cuda_size_t)(size_t*,cudev);
cuda_size_t cuDeviceTotalMem = NULL;
typedef cudaError_t (CUDACALL* cuda_int_t)(int*);
cuda_int_t cuDeviceGetCount = NULL;

bool cu_Init(void* cu_handle) {
	g_cu_handle = cu_handle;
	if (g_verbose) fprintf(stderr, "Warning: simulating cudart using nvcuda\n");
	cuInit = (cuda_uint_t)dlsym(cu_handle, "cuInit");
	if ( ! cuInit) return false;

	cuDeviceGet = (cuda_dev_int_t)dlsym(cu_handle, "cuDeviceGet");
	cuDeviceGetAttribute = (cuda_ga_t)dlsym(cu_handle, "cuDeviceGetAttribute");
	cuDeviceGetName = (cuda_name_t)dlsym(cu_handle, "cuDeviceGetName");
	cuDeviceComputeCapability = (cuda_cc_t)dlsym(cu_handle, "cuDeviceComputeCapability");
	cuDeviceTotalMem = (cuda_size_t)dlsym(cu_handle, "cuDeviceTotalMem");
	cuDeviceGetCount = (cuda_int_t)dlsym(cu_handle, "cuDeviceGetCount");

	cudaError ret = cuInit(0);
	if (ret != cudaSuccess) fprintf(stderr, "Error: cuInit returned %d\n", ret);
	return ret == cudaSuccess;
}

cudaError_t CUDACALL cu_cudaRuntimeGetVersion(int* pver) { 
	*pver = 5050;
	return cudaSuccess; 
}

cudaError_t CUDACALL cu_cudaGetDeviceProperties(struct cudaDeviceProp * p, int devID) {
	cudev dev;
	cudaError_t res = cuDeviceGet(&dev, devID);
	if (cudaSuccess == res) {
		cuDeviceGetName(p->name, sizeof(p->name), dev);
		cuDeviceComputeCapability(&p->major, &p->minor, dev);
		cuDeviceTotalMem(&p->totalGlobalMem, dev);
		cuDeviceGetAttribute(&p->clockRate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, dev);
		cuDeviceGetAttribute(&p->multiProcessorCount, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, dev);
	}
	return res; 
}



////////////////////////////////////////////////////////////////////////////////
// Program main
////////////////////////////////////////////////////////////////////////////////
int
main( int argc, const char** argv, const char ** envp)
{
	int deviceCount = 0;
	//int have_cuda = 1;
	int have_nvml = 0;
	int opt_static = 0;
	int opt_dynamic = 0;
	int opt_simulate = 0; // pretend to detect GPUs
	int opt_config = 0;
	int opt_slot = 0;
	int opt_nvcuda = 0; // use nvcuda rather than cudarl
	const char * opt_tag = "GPUs";
	const char * opt_pre = "CUDA";
	int i;
    int dev;

	for (i=1; i<argc && argv[i]; i++) {
		if (strcmp(argv[i],"-tag")==0) {
			if (argv[i+1]) {
				opt_tag = argv[++i];
			}
		}
		else if (strcmp(argv[i],"-prefix")==0) {
			if (argv[i+1]) {
				opt_pre = argv[++i];
			}
		}
		else if (strcmp(argv[i],"-static")==0) {
			opt_static = 1;
		}
		else if (strcmp(argv[i],"-dynamic")==0) {
			opt_dynamic = 1;
		}
		else if (strcmp(argv[i],"-verbose")==0) {
			g_verbose = 1;
		}
		else if (strncmp(argv[i],"-simulate", 9)==0) {
			opt_simulate = 1;
			const char * pcolon = strchr(argv[i],':');
			if (pcolon) {
				sim_index = atoi(pcolon+1);
				if (sim_index < 0 || sim_index >= sim_index_max) {
					fprintf(stderr, "Error: simulation must be in range of 0-%d\r\n", sim_index_max-1);
					return 0;
				}
				const char * pcomma = strchr(pcolon+1,',');
				if (pcomma) { sim_device_count = atoi(pcomma+1); }
			}
		}
		else if (strcmp(argv[i],"-nvcuda")==0) {
			// use nvcuda instead of cudart, (option is for testing...)
			opt_nvcuda = 1;
			g_environ = envp;
		} 
		else {
			fprintf(stderr, "option %s is not valid\n", argv[i]);
			return 1;
		}
	}

	// function pointers for runtime linking to cudarl stuff
	typedef cudaError_t (CUDACALL* cuda_t)(int*); //Used for DLFCN
	cuda_t cudaGetDeviceCount = NULL; 
	cuda_t cudaDriverGetVersion = NULL;
	cuda_t cudaRuntimeGetVersion = NULL;
	typedef cudaError_t (CUDACALL* cuda_DeviceProp_int)(struct cudaDeviceProp *, int);
	cuda_DeviceProp_int cudaGetDeviceProperties = NULL;

	// function pointers for the NVIDIA management layer, used for dynamic attributes.
    //nvmlReturn_t is the Return type for all of these functions
	typedef nvmlReturn_t (*nvml_void)(void);
	nvml_void nvmlInit = NULL;
	nvml_void nvmlShutdown = NULL;
	typedef nvmlReturn_t (*nvml_unsigned_int_Device)(unsigned int, nvmlDevice_t *);
	nvml_unsigned_int_Device nvmlDeviceGetHandleByIndex = NULL;
	//typedef nvmlReturn_t (*nvml_Device_EnableState)(nvmlDevice_t, nvmlEnableState_t *);
	//nvml_Device_EnableState nvmlDeviceGetDisplayMode = NULL;
	//nvml_Device_EnableState nvmlDeviceGetPersistenceMode = NULL; 
	//typedef nvmlReturn_t (*nvml_Device_EnableState_EnableState)(nvmlDevice_t, nvmlEnableState_t *, nvmlEnableState_t *);
	//nvml_Device_EnableState_EnableState nvmlDeviceGetEccMode = NULL;
	typedef nvmlReturn_t (*nvml_Device_unsigned_int)(nvmlDevice_t, unsigned int *);
	nvml_Device_unsigned_int nvmlDeviceGetFanSpeed = NULL;
	nvml_Device_unsigned_int nvmlDeviceGetPowerUsage = NULL;
	typedef nvmlReturn_t (*nvml_Device_TemperatureSensors_unsigned_int)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int *);
	nvml_Device_TemperatureSensors_unsigned_int nvmlDeviceGetTemperature = NULL;
	typedef nvmlReturn_t (*nvml_Device_MemoryErrorType_EccCounterType_unsigned_long_long)(nvmlDevice_t, nvmlMemoryErrorType_t, nvmlEccCounterType_t, unsigned long long *);
	nvml_Device_MemoryErrorType_EccCounterType_unsigned_long_long nvmlDeviceGetTotalEccErrors = NULL;

#ifdef WIN32
	HINSTANCE cuda_handle = NULL;
	HINSTANCE nvml_handle = NULL;
#else
	void* cuda_handle = NULL;
	void* nvml_handle = NULL;
#endif

    //**********************************************************************
	// Get pointers to cuda functions
    //**********************************************************************
	if (opt_simulate) {

		cudaGetDeviceCount = sim_cudaGetDeviceCount;
		cudaDriverGetVersion = sim_cudaDriverGetVersion;
		cudaRuntimeGetVersion = sim_cudaRuntimeGetVersion;
		cudaGetDeviceProperties = sim_cudaGetDeviceProperties;

		nvmlInit = sim_nvmlInit;
		nvmlShutdown = sim_nvmlInit;
		nvmlDeviceGetHandleByIndex = sim_nvmlDeviceGetHandleByIndex;
		nvmlDeviceGetFanSpeed = sim_nvmlDeviceGetFanSpeed;
		nvmlDeviceGetPowerUsage = sim_nvmlDeviceGetPowerUsage;
		nvmlDeviceGetTemperature = sim_nvmlDeviceGetTemperature;
		nvmlDeviceGetTotalEccErrors = sim_nvmlDeviceGetTotalEccErrors;
		have_nvml = 1;

	} else {

	#ifdef WIN32
		const char * cudart_library = "cudart.dll"; // "cudart32_55.dll"
		cuda_handle = LoadLibrary(cudart_library);
		if ( ! cuda_handle) {
			cuda_handle = LoadLibrary("nvcuda.dll");
			if (cuda_handle && cu_Init(cuda_handle)) {
				opt_nvcuda = 1;
				//fprintf(stderr, "using nvcuda.dll\r\n");
			} else {
				fprintf(stderr, "Error %d: Cant open library: %s\r\n", GetLastError(), cudart_library);
				fprintf(stdout, "Detected%s=0\n", opt_tag);
				return 0;
			}
		}

		const char * nvml_library = "nvml.dll";
		if (opt_dynamic) {
			nvml_handle = LoadLibrary(nvml_library);
			if ( ! nvml_handle) {
				fprintf(stderr, "Error %d: Cant open library: %s\r\n", GetLastError(), nvml_library);
			}
		}
	#else
		const char * cudart_library = "libcudart.so";
		cuda_handle = dlopen(cudart_library, RTLD_LAZY);
		if ( ! cuda_handle) {
			fprintf(stderr, "Error %s: Cant open library: %s\n", dlerror(), cudart_library);
			fprintf(stdout, "Detected%s=0\n", opt_tag);
			return 0;
		}
		dlerror(); //Reset error
		const char * nvml_library = "libnvidia-ml.so";
		if (opt_dynamic) {
			nvml_handle = dlopen(nvml_library, RTLD_LAZY);
			if ( ! nvml_handle) {
				fprintf(stderr, "Error %s: Cant open library: %s\n", dlerror(), nvml_library);
			}
			dlerror(); //Reset error
		}
	#endif

		cudaGetDeviceCount = (cuda_t) dlsym(cuda_handle, opt_nvcuda ? "cuDeviceGetCount" : "cudaGetDeviceCount");
		if ( ! cudaGetDeviceCount) {
			#ifdef WIN32
			fprintf(stderr, "Error %d: Cant find %s in library: %s\r\n", GetLastError(), "cudaGetDeviceCount", cudart_library);
			#else
			fprintf(stderr, "Error %s: Cant find %s in library: %s\n", dlerror(), "cudaGetDeviceCount", cudart_library);
			#endif
			fprintf(stdout, "Detected%s=0\n", opt_tag);
			return 0;
		}
	}

	if ((cudaGetDeviceCount(&deviceCount)) != cudaSuccess) {
		// FAILED CUDA Driver and Runtime version may be mismatched.
		// return 1;
		deviceCount = 0;
	}
    
	// If there are no devices, there is nothing more to do
    if (deviceCount == 0) {
        // There is no device supporting CUDA
		fprintf(stdout, "Detected%s=0\n", opt_tag);
		return 0;
	}

#if 0 //def TEST
    for (dev = 0; dev < deviceCount; dev++) {

		char prefix[100];
		if ( opt_slot == 0 ) {
		} else {
			if ( opt_slot - 1 != dev ) 
				continue;
		}
		sprintf(prefix,"%s%d_",opt_pre, dev);

	if (opt_static) {

		printf("%sCudaDrv=4.20\n", prefix);
		printf("%sCudaRun=4.10\n", prefix);

        	printf("%sDev=%d\n",  prefix,dev);
        	printf("%sName=\"%s\"\n", prefix, "GeForce GTX 480");
        	printf("%sCapability=%d.%d\n", prefix, 2, 0);
		printf("%sGlobalMemMB=%.0f\n", prefix,1536 );
        	printf("%sNumSMs=%d\n", prefix, 15);
        	printf("%sNumCores=%d\n", prefix, 480);
        	printf("%sClockGhz=%.2f\n", prefix, 1.40);
	} else {

		printf("%sFanSpeedPct=%u\n",prefix,44);
		printf("%sDieTempF=%u\n",prefix,52);

	}

	}

#endif

	// lookup the function pointers we will need later from the cudart library
	//
	if ( ! opt_simulate) {
		if (opt_nvcuda) {
			// if we have nvcuda loaded rather than cudart, we can simulate 
			// cudart functions from nvcuda functions. 
			cudaDriverGetVersion = (cuda_t) dlsym(cuda_handle, "cuDriverGetVersion");
			cudaRuntimeGetVersion = cu_cudaRuntimeGetVersion;
			cudaGetDeviceProperties = cu_cudaGetDeviceProperties;
		} else {
			cudaDriverGetVersion = (cuda_t) dlsym(cuda_handle, "cudaDriverGetVersion");
			cudaRuntimeGetVersion = (cuda_t) dlsym(cuda_handle, "cudaRuntimeGetVersion");
			cudaGetDeviceProperties = (cuda_DeviceProp_int) dlsym(cuda_handle, "cudaGetDeviceProperties");
		}
	}

	// load functions we will need from the nvml library, if we don't have that library
	// that's ok it just means there is no dynamic info.
	//
	if ( ! opt_simulate && nvml_handle) {
		nvmlInit = (nvml_void) dlsym(nvml_handle, "nvmlInit"); // (void)
		if ( ! nvmlInit) {
			have_nvml = 0;
		} else {
			have_nvml = 1;
			nvmlShutdown = (nvml_void) dlsym(nvml_handle, "nvmlShutdown"); // (void)
			nvmlDeviceGetHandleByIndex = (nvml_unsigned_int_Device) dlsym(nvml_handle, "nvmlDeviceGetHandleByIndex"); //(unsigned int, nvmlUnit_t *)
			//nvmlDeviceGetDisplayMode = (nvml_Device_EnableState) dlsym(nvml_handle, "nvmlDeviceGetDisplayMode"); //(nvmlDevice_t, nvmlEnableState_t *)
			//nvmlDeviceGetPersistenceMode = (nvml_Device_EnableState) dlsym(nvml_handle, "nvmlDeviceGetPersistenceMode"); //(nvmlDevice_t, nvmlEnableState_t *);
			//nvmlDeviceGetEccMode = (nvml_Device_EnableState_EnableState) dlsym(nvml_handle, "nvmlDeviceGetEccMode"); //(nvmlDevice_t, nvmlEnableState_t *, nvmlEnableState_t *);
			nvmlDeviceGetFanSpeed = (nvml_Device_unsigned_int) dlsym(nvml_handle, "nvmlDeviceGetFanSpeed"); //(nvmlDevice_t, unsigned int *);
			nvmlDeviceGetPowerUsage = (nvml_Device_unsigned_int) dlsym(nvml_handle, "nvmlDeviceGetPowerUsage"); //(nvmlDevice_t, unsigned int *);
			nvmlDeviceGetTemperature = (nvml_Device_TemperatureSensors_unsigned_int) dlsym(nvml_handle, "nvmlDeviceGetTemperature"); //(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int *);
			nvmlDeviceGetTotalEccErrors = (nvml_Device_MemoryErrorType_EccCounterType_unsigned_long_long) dlsym(nvml_handle, "nvmlDeviceGetTotalEccErrors"); //(nvmlDevice_t, nvmlMemoryErrorType_t, nvmlEccCounterType_t, unsigned long long *);
		}
	}

	nvmlReturn_t result = NVML_ERROR_NOT_FOUND;
	if (have_nvml) {
		result = nvmlInit();
		if (NVML_SUCCESS != result)
		{
			fprintf(stderr, "Warning: nvmlInit failed with error %d, dynamic information will not be available.\n", result);
			have_nvml = 0;
		}
	}

	// print out info about detected GPU resources
	//
	std::string detected_gpus;
	for (dev = 0; dev < deviceCount; dev++) {
		char prefix[100];
		sprintf(prefix,"%s%d",opt_pre,dev);
		if ( ! detected_gpus.empty()) { detected_gpus += ", "; }
		detected_gpus += prefix;
	}
	fprintf(stdout, opt_config ? "Detected%s=%s\n" : "Detected%s=\"%s\"\n", opt_tag, detected_gpus.c_str());

	// print out static and/or dynamic info about detected GPU resources
	for (dev = 0; dev < deviceCount; dev++) {

		if (opt_slot && ((opt_slot-1) != dev)) {
			continue;
		}

		char prefix[100];
		sprintf(prefix,"%s%d",opt_pre,dev);

		if ( opt_static ) {
			cudaDeviceProp deviceProp;
			int driverVersion=0, runtimeVersion=0;

			cudaGetDeviceProperties(&deviceProp, dev);

		#if CUDART_VERSION >= 2020
			cudaDriverGetVersion(&driverVersion);
			cudaRuntimeGetVersion(&runtimeVersion);
			printf("%sDriverVersion=%d.%d\n", prefix, driverVersion/1000, driverVersion%100);
			// printf("%sRuntimeVersion=%d.%d\n", prefix, runtimeVersion/1000, runtimeVersion%100);
		#endif

			//printf("%sDev=%d\n",  prefix,dev);
			printf("%sName=\"%s\"\n", prefix, deviceProp.name);
			printf("%sCapability=%d.%d\n", prefix, deviceProp.major, deviceProp.minor);
			printf("%sGlobalMemMB=%.0f\n", prefix, deviceProp.totalGlobalMem/(1024.*1024.));
		#if CUDART_VERSION >= 2000
			printf("%sNumSMs=%d\n", prefix, deviceProp.multiProcessorCount);
			printf("%sNumCores=%d\n", prefix, deviceProp.multiProcessorCount * ConvertSMVer2Cores(deviceProp.major, deviceProp.minor));
		#endif
			printf("%sClockGhz=%.2f\n", prefix, deviceProp.clockRate * 1e-6f);

		}	// end of if opt_static


		// Dynamic properties

		// need nvml to do dynamic properties
		if ( !have_nvml ) continue;
		// everything after here is dynamic properties
		if ( !opt_dynamic ) continue;

		// get a handle to this device
		nvmlDevice_t device;
		if (nvmlDeviceGetHandleByIndex(dev,&device) != NVML_SUCCESS) {
			continue;
		}

		/*
		nvmlEnableState_t state,state1;
		result = nvmlDeviceGetDisplayMode(device,&state);
		if ( result == NVML_SUCCESS ) {
			printf("%sDisplayEnabled=%s\n",prefix,state==NVML_FEATURE_ENABLED ? "true" : "false");
		}

		result = nvmlDeviceGetPersistenceMode(device,&state);
		if ( result == NVML_SUCCESS ) {
			printf("%sPersistenceEnabled=%s\n",prefix,state==NVML_FEATURE_ENABLED ? "true" : "false");
		}

		result = nvmlDeviceGetPowerCappingMode(device,&state);
		if ( result == NVML_SUCCESS ) {
			printf("%sPowerCappingEnabled=%s\n",prefix,state==NVML_FEATURE_ENABLED ? "true" : "false");
		}

		result = nvmlDeviceGetEccMode(device,&state,&state1);
		if ( result == NVML_SUCCESS ) {
			printf("%sEccEnabled=%s\n",prefix,state==NVML_FEATURE_ENABLED ? "true" : "false");
		}
		*/

		unsigned int tuint;
		result = nvmlDeviceGetFanSpeed(device,&tuint);
		if ( result == NVML_SUCCESS ) {
			printf("%sFanSpeedPct=%u\n",prefix,tuint);
		}

		result = nvmlDeviceGetPowerUsage(device,&tuint);
		if ( result == NVML_SUCCESS ) {
			printf("%sPowerUsage_mw=%u\n",prefix,tuint);
		}

#if 0 
		result = nvmlDeviceGetTemperature(device,NVML_TEMPERATURE_BOARD,&tuint);
		if ( result == NVML_SUCCESS ) {
			printf("%sBoardTempF=%u\n",prefix,tuint);
		}
#endif
		result = nvmlDeviceGetTemperature(device,NVML_TEMPERATURE_GPU,&tuint);
		if ( result == NVML_SUCCESS ) {
			printf("%sDieTempF=%u\n",prefix,tuint);
		}

		unsigned long long eccCounts;
		result = nvmlDeviceGetTotalEccErrors(device,NVML_SINGLE_BIT_ECC,NVML_VOLATILE_ECC,&eccCounts);
		if ( result == NVML_SUCCESS ) {
			printf("%sEccErrorsSingleBit=%llu\n",prefix,eccCounts);
		}
		result = nvmlDeviceGetTotalEccErrors(device,NVML_DOUBLE_BIT_ECC,NVML_VOLATILE_ECC,&eccCounts);
		if ( result == NVML_SUCCESS ) {
			printf("%sEccErrorsDoubleBit=%llu\n",prefix,eccCounts);
		}

	}


	if (have_nvml) nvmlShutdown();

#ifdef WIN32
	if (cuda_handle) FreeLibrary(cuda_handle);
	if (nvml_handle) FreeLibrary(nvml_handle);
#else
	if (cuda_handle) dlclose(cuda_handle);
	if (nvml_handle) dlclose(nvml_handle);
#endif // WIN32

    return 0;
}
