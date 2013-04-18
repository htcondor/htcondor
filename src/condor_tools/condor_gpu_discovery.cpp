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
#include <iostream>
using namespace std;

#ifndef TEST
#include <dlfcn.h> //To facilitate compiling on machines that don't have NVIDIA installed
//#include <cuda_runtime_api.h>
#include "cuda_header_doc.h"
//#include "nvml.h"
#include "nvml_stub.h"
#endif

/*
 compile with nvcc -g  -lnvidia-ml condor_nvidia.cu 

*/


#ifndef TEST
int ConvertSMVer2Cores(int major, int minor)
{
	// Defines for GPU Architecture types (using the SM version to determine the # of cores per SM
    typedef struct {
                int SM; // 0xMm (hexidecimal notation), M = SM Major version, and m = SM minor version
                int Cores;
	} sSMtoCores;

        sSMtoCores nGpuArchCoresPerSM[] = 
        { { 0x10,  8 },
          { 0x11,  8 },
          { 0x12,  8 },
          { 0x13,  8 },
          { 0x20, 32 },
          { 0x21, 48 },
          {   -1, -1 }
        };

        int index = 0;
        while (nGpuArchCoresPerSM[index].SM != -1) {
                if (nGpuArchCoresPerSM[index].SM == ((major << 4) + minor) ) {
                        return nGpuArchCoresPerSM[index].Cores;
                }
                index++;
        }
        return -1;
}
#endif 

////////////////////////////////////////////////////////////////////////////////
// Program main
////////////////////////////////////////////////////////////////////////////////
int
main( int argc, const char** argv) 
{
    int deviceCount = 0;
		//int have_cuda = 1;
	int have_nvml = 1;
	char prefix[100];
	int opt_static = 0;
	int opt_dynamic = 0;
	int opt_slot = 0;
	int i;
    int dev;

	for (i=1; i<argc && argv[i]; i++) {
		if (strcmp(argv[i],"-static")==0) {
			opt_static = 1;
		}
		if (strcmp(argv[i],"-dynamic")==0) {
			opt_dynamic = 1;
		}
		if (strcmp(argv[i],"-slot")==0) {
			i++;
			if ( i < argc ) {
				opt_slot = atoi( argv[i] );
			}
		}
	}

#ifdef TEST
	deviceCount = 2;
#else
	
	//**********************************************************************
	// DLFCN CODE
	//**********************************************************************
	//printf("DLFCN Code\n");
	void* nvml_handle = dlopen("libnvidia-ml.so", RTLD_LAZY);
	if(!nvml_handle) {
		cerr << "Error: Cannot open library: " << dlerror() << endl;
		printf("HAS_GPU = False\n");
		return 0;
	}
	dlerror(); //Reset error
	void* cuda_handle = dlopen("libcudart.so", RTLD_LAZY);
	if(!cuda_handle) {
		cerr << "Error: Cannot open library: " << dlerror() << endl;
		printf("HAS_GPU = False\n");
		return 0;
	}
	dlerror(); //Reset error

    //**********************************************************************
	// Pointers to function
    //**********************************************************************
	typedef cudaError_t (*cuda_t)(int*); //Used for DLFCN
	//typedef cuda_t (*cudaError_t)(int*); //Used for DLFCN	
		//typedef void* cuda_obj;
	cuda_t cudaGetDeviceCount = (cuda_t) dlsym(cuda_handle, "cudaGetDeviceCount");
		//cuda_obj cudaSuccess = (cuda_obj) dlsym(cuda_handle, "cudaSuccess");
		//cuda_obj nvmlReturn_t = (cuda_obj) dlsym(nvml_handle, "nvmlReturn_t");

	nvmlReturn_t result;
	if ( opt_dynamic && !opt_static && opt_slot ) {
		deviceCount = opt_slot;
	} else {
		if ((cudaGetDeviceCount(&deviceCount)) != cudaSuccess) {
			// FAILED CUDA Driver and Runtime version may be mismatched.
			// return 1;
			deviceCount = 0;
		}
	}
#endif

	if (opt_static == 0 && opt_dynamic == 0) {
		printf("%d\n",deviceCount);
		return 0;
	}
    
	// If there are no devices, there is nothing more to do
    if (deviceCount == 0) {
		printf("There are no devices\n");
        // There is no device supporting CUDA
		return 0;
	}

#ifdef TEST
    for (dev = 0; dev < deviceCount; dev++) {

		if ( opt_slot == 0 ) {
			sprintf(prefix,"GPU%d_",dev);
		} else {
			if ( opt_slot - 1 != dev ) 
				continue;
			sprintf(prefix,"GPU_");
		}

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

#else
    //**********************************************************************
	// Pointers to functions
	//**********************************************************************
	typedef cudaError_t (*cuda_DeviceProp_int)(struct cudaDeviceProp *, int);
	cuda_DeviceProp_int cudaGetDeviceProperties = (cuda_DeviceProp_int) dlsym(cuda_handle, "cudaGetDeviceProperties");
	cuda_t cudaDriverGetVersion = (cuda_t) dlsym(cuda_handle, "cudaDriverGetVersion");
	cuda_t cudaRuntimeGetVersion = (cuda_t) dlsym(cuda_handle, "cudaRuntimeGetVersion");

    //nvmlReturn_t is the Return type for all of these functions
	typedef nvmlReturn_t (*nvml_void)(void);
	nvml_void nvmlInit = (nvml_void) dlsym(nvml_handle, "nvmlInit"); // (void)
	typedef nvmlReturn_t (*nvml_unsigned_int_Device)(unsigned int, nvmlDevice_t *);
	nvml_unsigned_int_Device nvmlDeviceGetHandleByIndex = (nvml_unsigned_int_Device) dlsym(nvml_handle, "nvmlDeviceGetHandleByIndex"); //(unsigned int, nvmlUnit_t *)
	typedef nvmlReturn_t (*nvml_Device_EnableState)(nvmlDevice_t, nvmlEnableState_t *);
	nvml_Device_EnableState nvmlDeviceGetDisplayMode = (nvml_Device_EnableState) dlsym(nvml_handle, "nvmlDeviceGetDisplayMode"); //(nvmlDevice_t, nvmlEnableState_t *)
	nvml_Device_EnableState nvmlDeviceGetPersistenceMode = (nvml_Device_EnableState) dlsym(nvml_handle, "nvmlDeviceGetPersistenceMode"); //(nvmlDevice_t, nvmlEnableState_t *);
	typedef nvmlReturn_t (*nvml_Device_EnableState_EnableState)(nvmlDevice_t, nvmlEnableState_t *, nvmlEnableState_t *);
	nvml_Device_EnableState_EnableState nvmlDeviceGetEccMode = (nvml_Device_EnableState_EnableState) dlsym(nvml_handle, "nvmlDeviceGetEccMode"); //(nvmlDevice_t, nvmlEnableState_t *, nvmlEnableState_t *);
	typedef nvmlReturn_t (*nvml_Device_unsigned_int)(nvmlDevice_t, unsigned int *);
	nvml_Device_unsigned_int nvmlDeviceGetFanSpeed = (nvml_Device_unsigned_int) dlsym(nvml_handle, "nvmlDeviceGetFanSpeed"); //(nvmlDevice_t, unsigned int *);
	nvml_Device_unsigned_int nvmlDeviceGetPowerUsage = (nvml_Device_unsigned_int) dlsym(nvml_handle, "nvmlDeviceGetPowerUsage"); //(nvmlDevice_t, unsigned int *);
	typedef nvmlReturn_t (*nvml_Device_TemperatureSensors_unsigned_int)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int *);
	nvml_Device_TemperatureSensors_unsigned_int nvmlDeviceGetTemperature = (nvml_Device_TemperatureSensors_unsigned_int) dlsym(nvml_handle, "nvmlDeviceGetTemperature"); //(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int *);
	typedef nvmlReturn_t (*nvml_Device_MemoryErrorType_EccCounterType_unsigned_long_long)(nvmlDevice_t, nvmlMemoryErrorType_t, nvmlEccCounterType_t, unsigned long long *);
	nvml_Device_MemoryErrorType_EccCounterType_unsigned_long_long nvmlDeviceGetTotalEccErrors = (nvml_Device_MemoryErrorType_EccCounterType_unsigned_long_long) dlsym(nvml_handle, "nvmlDeviceGetTotalEccErrors"); //(nvmlDevice_t, nvmlMemoryErrorType_t, nvmlEccCounterType_t, unsigned long long *);
	nvml_void nvmlShutdown = (nvml_void) dlsym(nvml_handle, "nvmlShutdown"); // (void)

		/*
	cuda_obj NVML_SUCCESS = (cuda_obj) dlsym(nvml_handle, "NVML_SUCCESS");
	cuda_obj NVML_FEATURE_ENABLED = (cuda_obj) dlsym(nvml_handle, "NVML_FEATURE_ENABLED");
	cuda_obj NVML_TEMPERATURE_GPU = (cuda_obj) dlsym(nvml_handle, "NVML_TEMPERATURE_GPU");
	cuda_obj NVML_SINGLE_BIT_ECC = (cuda_obj) dlsym(nvml_handle, "NVML_SINGLE_BIT_ECC");
	cuda_obj NVML_VOLATILE_ECC = (cuda_obj) dlsym(nvml_handle, "NVML_VOLATILE_ECC");
	cuda_obj NVML_DOUBLE_BIT_ECC = (cuda_obj) dlsym(nvml_handle, "NVML_DOUBLE_BIT_ECC");
	cuda_obj nvmlEnableState_t = (cuda_obj) dlsym(nvml_handle, "nvmlEnableState_t");
		*/

	result = nvmlInit();
	if (NVML_SUCCESS != result)
	{
		printf("NVML Failure\n");
		// Failed to init NVML
		have_nvml = 0;
	}

    for (dev = 0; dev < deviceCount; dev++) {
        cudaDeviceProp deviceProp;
		int driverVersion=0, runtimeVersion=0;

		if ( opt_slot == 0 ) {
			sprintf(prefix,"GPU%d_",dev);
		} else {
			if ( opt_slot - 1 != dev ) 
				continue;
			sprintf(prefix,"GPU_");
		}

		if ( opt_static ) {
        cudaGetDeviceProperties(&deviceProp, dev);


    #if CUDART_VERSION >= 2020
		cudaDriverGetVersion(&driverVersion);
		cudaRuntimeGetVersion(&runtimeVersion);
		printf("%sCudaDrv=%d.%d\n", prefix, driverVersion/1000, driverVersion%100);
		printf("%sCudaRun=%d.%d\n", prefix, runtimeVersion/1000, runtimeVersion%100);
    #endif

        printf("%sDev=%d\n",  prefix,dev);
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

		nvmlEnableState_t state,state1;
		unsigned int tuint;
		result = nvmlDeviceGetDisplayMode(device,&state);
		if ( result == NVML_SUCCESS ) {
			printf("%sDisplayEnabled=%s\n",prefix,state==NVML_FEATURE_ENABLED ? "true" : "false");
		}

		result = nvmlDeviceGetPersistenceMode(device,&state);
		if ( result == NVML_SUCCESS ) {
			printf("%sPersistenceEnabled=%s\n",prefix,state==NVML_FEATURE_ENABLED ? "true" : "false");
		}

#if 0  // for GF11x Fermi-class products only
		result = nvmlDeviceGetPowerCappingMode(device,&state);
		if ( result == NVML_SUCCESS ) {
			printf("%sPowerCappingEnabled=%s\n",prefix,state==NVML_FEATURE_ENABLED ? "true" : "false");
		}
#endif
		result = nvmlDeviceGetEccMode(device,&state,&state1);
		if ( result == NVML_SUCCESS ) {
			printf("%sEccEnabled=%s\n",prefix,state==NVML_FEATURE_ENABLED ? "true" : "false");
		}

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

	//DLFCN function closing cuda_handle and nvml_handle
	dlclose(cuda_handle);
	dlclose(nvml_handle);
#endif

    return 0;
}
