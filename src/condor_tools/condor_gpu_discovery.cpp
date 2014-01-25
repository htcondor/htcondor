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
#include <vector>
#include <time.h>


// we don't want to use the actual cuda headers because we want to build on machines where they are not installed.
//#include <cuda_runtime_api.h>
//#include "nvml.h"
#include "cuda_header_doc.h"
#include "opencl_header_doc.h"
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
	int ECCEnabled;
	size_t totalGlobalMem;
};
struct _simulated_cuda_config {
	int driverVer;
	int runtimeVer;
	int deviceCount;
	const struct _simulated_cuda_device * device;
};

static const struct _simulated_cuda_device GeForceGTX480 = { "GeForce GTX 480", 0x20, 1400*1000, 15, 0, 1536*1024*1024 };
static const struct _simulated_cuda_device GeForceGT330 = { "GeForce GT 330",  0x12, 1340*1000, 12,  0, 1024*1024*1024 };
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
	memset(p, 0, sizeof(*p));
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
	p->ECCEnabled = dev->ECCEnabled;
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
int g_diagnostic = 0;
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
	if (g_diagnostic) fprintf(stderr, "diag: simulating cudart using nvcuda\n");
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
	*pver = 0;
	cudaError_t ret = cudaErrorNoDevice;

#ifdef WIN32
	// On windows, the CUDA runtime isn't installed in the system directory, 
	// but we can locate it by looking in the registry.
	// TODO: handle multiple installed runtimes...
	const char * reg_key = "SOFTWARE\\NVIDIA Corporation\\GPU Computing Toolkit\\CUDA";
	const char * reg_val_name = "FirstVersionInstalled";
	HKEY hkey = NULL;
	int bitness = KEY_WOW64_64KEY; //KEY_WOW64_32KEY; KEY_WOW64_64KEY
	int res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, reg_key, 0, KEY_READ | bitness, &hkey);
	if (res != ERROR_SUCCESS) {
		fprintf(stderr, "can't open %s\n", reg_key);
		return ret;
	}
	char version[100];
	memset(version, 0, sizeof(version));
	DWORD vtype = REG_SZ, cbData = sizeof(version);
	res = RegQueryValueEx(hkey, reg_val_name, NULL, &vtype, (unsigned char*)version, &cbData);
	if (res == ERROR_SUCCESS) {
		// the value of this key is something like "v1.1", we want to strip off the v and
		// convert the 1.1 into two integers. so we can pack them back together as 0x1010
		version[sizeof(version)-1] = 0;
		const char * pv = version;
		while (*pv == ' ' || *pv == 'v' || *pv == 'V') ++pv;
		int whole=0, fract=0;
		if (2 == sscanf(pv, "%d.%d", &whole, &fract)) {
			*pver = (whole * 1000) + ((fract*10) % 100);
			ret = cudaSuccess;
		}
	}
	RegCloseKey(hkey);
#endif
	return ret; 
}

cudaError_t CUDACALL cu_cudaGetDeviceProperties(struct cudaDeviceProp * p, int devID) {
	memset(p, 0, sizeof(*p));
	cudev dev;
	cudaError_t res = cuDeviceGet(&dev, devID);
	if (cudaSuccess == res) {
		cuDeviceGetName(p->name, sizeof(p->name), dev);
		cuDeviceComputeCapability(&p->major, &p->minor, dev);
		cuDeviceTotalMem(&p->totalGlobalMem, dev);
		cuDeviceGetAttribute(&p->clockRate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, dev);
		cuDeviceGetAttribute(&p->multiProcessorCount, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, dev);
		cuDeviceGetAttribute(&p->ECCEnabled, CU_DEVICE_ATTRIBUTE_ECC_ENABLED, dev);
	}
	return res; 
}


static struct {
	clGetPlatformIDs_t GetPlatformIDs;
	clGetPlatformInfo_t GetPlatformInfo;
	clGetDeviceIDs_t GetDeviceIDs;
	clGetDeviceInfo_t GetDeviceInfo;
} ocl = { NULL, NULL, NULL, NULL };

const char * ocl_name(cl_e_platform_info eInfo) {
	switch (eInfo) {
		case CL_PLATFORM_PROFILE:    return "PROFILE";
		case CL_PLATFORM_VERSION:    return "VERSION";
		case CL_PLATFORM_NAME:       return "NAME";
		case CL_PLATFORM_VENDOR:     return "VENDOR";
		case CL_PLATFORM_EXTENSIONS: return "EXTENSIONS";
	}
	return "";
}

const char * ocl_name(cl_e_device_info eInfo) {
	switch (eInfo) {
		case CL_DEVICE_TYPE:				 return "DEVICE_TYPE";
		case CL_DEVICE_VENDOR_ID:			 return "VENDOR_ID";
		case CL_DEVICE_MAX_COMPUTE_UNITS:	 return "MAX_COMPUTE_UNITS";
		case CL_DEVICE_MAX_WORK_GROUP_SIZE:  return "MAX_WORK_GROUP_SIZE";
		case CL_DEVICE_MAX_CLOCK_FREQUENCY:	 return "MAX_CLOCK_FREQUENCY";
		case CL_DEVICE_GLOBAL_MEM_SIZE:		 return "GLOBAL_MEM_SIZE";
		case CL_DEVICE_AVAILABLE:			 return "AVAILABLE";
		case CL_DEVICE_EXECUTION_CAPABILITIES: return "EXECUTION_CAPABILITIES";
		case CL_DEVICE_ERROR_CORRECTION_SUPPORT: return "ERROR_CORRECTION_SUPPORT";
		case CL_DEVICE_NAME:				 return "NAME";
		case CL_DEVICE_VENDOR:				 return "VENDOR";
		case CL_DRIVER_VERSION:				 return "DRIVER";
		case CL_DEVICE_PROFILE:				 return "PROFILE";
		case CL_DEVICE_VERSION:				 return "VERSION";
	}
	return "";
}

// auto-growing buffer for use in fetching OpenCL info.
static char* g_buffer = NULL;
unsigned int g_cBuffer = 0;
int ocl_log = 0; // set to true to enable logging of oclGetInfo

clReturn oclGetInfo(cl_platform_id plid, cl_e_platform_info eInfo, std::string & val) {
	val.clear();
	size_t cb = 0;
	clReturn clr = ocl.GetPlatformInfo(plid, eInfo, g_cBuffer, g_buffer, &cb);
	if ((clr == CL_SUCCESS) && (g_cBuffer <= cb)) {
		if (g_buffer) free (g_buffer);
		if (cb < 120) cb = 120;
		g_buffer = (char*)malloc(cb*2);
		g_cBuffer = cb*2;
		clr = ocl.GetPlatformInfo(plid, eInfo, g_cBuffer, g_buffer, &cb);
	}
	if (clr == CL_SUCCESS) {
		g_buffer[g_cBuffer-1] = 0;
		val = g_buffer;
		if (ocl_log) { fprintf(stdout, "\t%s: \"%s\"\n", ocl_name(eInfo), val.c_str()); }
	}
	return clr;
}

clReturn oclGetInfo(cl_device_id did, cl_e_device_info eInfo, std::string & val) {
	val.clear();
	size_t cb = 0;
	clReturn clr = ocl.GetDeviceInfo(did, eInfo, g_cBuffer, g_buffer, &cb);
	if ((clr == CL_SUCCESS) && (g_cBuffer <= cb)) {
		if (g_buffer) free (g_buffer);
		if (cb < 120) cb = 120;
		g_buffer = (char*)malloc(cb*2);
		g_cBuffer = cb*2;
		clr = ocl.GetDeviceInfo(did, eInfo, g_cBuffer, g_buffer, &cb);
	}
	if (clr == CL_SUCCESS) {
		g_buffer[g_cBuffer-1] = 0;
		val = g_buffer;
		if (ocl_log) { fprintf(stdout, "\t\t%s: \"%s\"\n", ocl_name(eInfo), val.c_str()); }
	}
	return clr;
}

const char * ocl_value(int val) { static char ach[11]; sprintf(ach, "%d", val); return ach; }
const char * ocl_value(unsigned int val) { static char ach[11]; sprintf(ach, "%u", val); return ach; }
const char * ocl_value(long long val) { static char ach[22]; sprintf(ach, "%lld", val); return ach; }
const char * ocl_value(unsigned long long val) { static char ach[22]; sprintf(ach, "%llu", val); return ach; }
const char * ocl_value(double val) { static char ach[22]; sprintf(ach, "%g", val); return ach; }

template <class t>
clReturn oclGetInfo(cl_device_id did, cl_e_device_info eInfo, t & val) {
	clReturn clr = ocl.GetDeviceInfo(did, eInfo, sizeof(val), &val, NULL);
	if (clr == CL_SUCCESS) {
		if (ocl_log) { fprintf(stdout, "\t\t%s: %s\n", ocl_name(eInfo), ocl_value(val)); }
	}
	return clr;
}

static int ocl_device_count = 0;
static int ocl_was_initialized = 0;
static std::vector<cl_platform_id> cl_platforms;
static std::vector<cl_device_id> cl_gpu_ids;
static cl_platform_id g_plidCuda = NULL;
static std::string g_cudaOCLVersion;
static int g_cl_ixFirstCuda = 0;
static int g_cl_cCuda = 0;

int ocl_Init(void) {

	if (g_diagnostic) fprintf(stderr, "diag: ocl_Init()\n");
	if (ocl_was_initialized) {
		return 0;
	}
	ocl_was_initialized = 1;

	if ( ! ocl.GetPlatformIDs) {
		return cudaErrorInitializationError;
	}

	unsigned int cPlatforms = 0;
	clReturn clr = ocl.GetPlatformIDs(0, NULL, &cPlatforms);
	if (clr != CL_SUCCESS) {
		fprintf(stderr, "ocl.GetPlatformIDs returned error=%d and %d platforms\n", clr, cPlatforms);
	}
	if (cPlatforms > 0) {
		cl_platforms.reserve(cPlatforms);
		for (unsigned int ii = 0; ii < cPlatforms; ++ii) {
			cl_platforms.push_back(NULL);
		}
		clr = ocl.GetPlatformIDs(cPlatforms, &cl_platforms[0], &cPlatforms);
	}

	// enable logging in oclGetInfo if verbose
	ocl_log = g_verbose;

	for (unsigned int ii = 0; ii < cPlatforms; ++ii) {
		cl_platform_id plid = cl_platforms[ii];
		if (g_diagnostic) { fprintf(stdout, "ocl platform %d = %x\n", ii, (int)(size_t)plid); }
		std::string val;
		clr = oclGetInfo(plid, CL_PLATFORM_NAME, val);
		if (val == "NVIDIA CUDA") g_plidCuda = plid;
		clr = oclGetInfo(plid, CL_PLATFORM_VERSION, val);
		if (plid == g_plidCuda) g_cudaOCLVersion = val;

		// if logging is enabled, then oclGetInfo has the side effect of logging the value.
		if (ocl_log) {
			clr = oclGetInfo(plid, CL_PLATFORM_VENDOR, val);
			clr = oclGetInfo(plid, CL_PLATFORM_PROFILE, val);
			clr = oclGetInfo(plid, CL_PLATFORM_EXTENSIONS, val);
		}

		unsigned int cDevs = 0;
		cl_f_device_type f_types = CL_DEVICE_TYPE_GPU;
		clr = ocl.GetDeviceIDs(plid, f_types, 0, NULL, &cDevs);
		if (g_verbose) { fprintf(stdout, "\tDEVICES = %d\n", cDevs); }

		if (CL_SUCCESS == clr && cDevs > 0) {
			unsigned int ixFirst = cl_gpu_ids.size();
			cl_gpu_ids.reserve(ixFirst + cDevs);
			for (unsigned int jj = 0; jj < cDevs; ++jj) { cl_gpu_ids.push_back(NULL); }
			clr = ocl.GetDeviceIDs(plid, f_types, cDevs, &cl_gpu_ids[ixFirst], NULL);
			if (clr == CL_SUCCESS) {
				if (plid == g_plidCuda) {
					// keep track of which of the opencl GPUS are also CUDA
					g_cl_ixFirstCuda = ixFirst;
					g_cl_cCuda = cDevs;
				}

				// if logging is enabled, query various properties of the devices
				// to trigger the logging of those properties.
				if (ocl_log) {
					for (unsigned int jj = ixFirst; jj < ixFirst + cDevs; ++jj) {
						cl_device_id did = cl_gpu_ids[jj];
						oclGetInfo(did, CL_DEVICE_NAME, val);
						cl_f_device_type eType = CL_DEVICE_TYPE_NONE;
						oclGetInfo(did, CL_DEVICE_TYPE, eType);
						unsigned int vid = 0;
						oclGetInfo(did, CL_DEVICE_VENDOR_ID, vid);
						oclGetInfo(did, CL_DEVICE_VENDOR, val);
						oclGetInfo(did, CL_DEVICE_VERSION, val);
						oclGetInfo(did, CL_DRIVER_VERSION, val);
						unsigned int multiprocs = 0;
						oclGetInfo(did, CL_DEVICE_MAX_COMPUTE_UNITS, multiprocs);
						unsigned long long work_size = 0;
						oclGetInfo(did, CL_DEVICE_MAX_WORK_GROUP_SIZE, work_size);
						unsigned int clock_mhz;
						oclGetInfo(did, CL_DEVICE_MAX_CLOCK_FREQUENCY, clock_mhz);
						unsigned long long mem_bytes = 0;
						oclGetInfo(did, CL_DEVICE_GLOBAL_MEM_SIZE, mem_bytes);
						int has_ecc = 0;
						oclGetInfo(did, CL_DEVICE_ERROR_CORRECTION_SUPPORT, has_ecc);
					}
				}
			}
		}
	}

	ocl_log = 0;
	ocl_device_count = cl_gpu_ids.size();
	return 0;
}

cudaError_t CUDACALL ocl_GetDeviceCount(int * pcount) {
	ocl_Init();
	*pcount = ocl_device_count;
	return cudaSuccess;
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
	//int opt_rdp = 0;
	int opt_slot = 0;
	int opt_nvcuda = 0; // use nvcuda rather than cudarl
	int opt_opencl = 0;
	const char * opt_tag = "GPUs";
	const char * opt_pre = "CUDA";
	const char * opt_pre_arg = NULL;
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
				opt_pre_arg = argv[++i];
			}
		}
		else if (strcmp(argv[i],"-static")==0) {
			opt_static = 1;
		}
		else if (strcmp(argv[i],"-dynamic")==0) {
			opt_dynamic = 1;
		}
		else if (strcmp(argv[i],"-opencl")==0) {
			opt_opencl = 1;
		}
		else if (strcmp(argv[i],"-verbose")==0) {
			g_verbose = 1;
		}
		else if (strcmp(argv[i],"-diagnostic")==0) {
			g_diagnostic = 1;
		}
		else if (strcmp(argv[i],"-config")==0) {
			opt_config = 1;
		}
		//else if (strcmp(argv[i],"-rdp")==0) {
		//	opt_rdp = 1;
		//}
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


    //ciErrNum = clGetPlatformInfo (clSelectedPlatformID, CL_PLATFORM_NAME, sizeof(cBuffer), cBuffer, NULL);
	//clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 100, devices, &devices_n)
	//clGetDeviceInfo(devices[i], CL_DEVICE_NAME, sizeof(buffer), buffer, NULL));


#ifdef WIN32
	HINSTANCE cuda_handle = NULL;
	HINSTANCE ocl_handle = NULL;
	HINSTANCE nvml_handle = NULL;
#else
	void* cuda_handle = NULL;
	void* ocl_handle = NULL;
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
		const char * opencl_library = "OpenCL.dll";
		ocl_handle = LoadLibrary(opencl_library);
		if ( ! ocl_handle && opt_opencl) {
			fprintf(stderr, "Error %d: Cant open library: %s\r\n", GetLastError(), opencl_library);
		}

		const char * cudart_library = "cudart.dll"; // "cudart32_55.dll"
		cuda_handle = LoadLibrary(cudart_library);
		if ( ! cuda_handle) {
			cuda_handle = LoadLibrary("nvcuda.dll");
			if (cuda_handle && cu_Init(cuda_handle)) {
				opt_nvcuda = 1;
				if (g_diagnostic) { fprintf(stderr, "using nvcuda.dll to simulate cudart\n"); }
			} else if (ocl_handle) {
				// if no cuda, fall back to OpenCL detection
			} else {
				if (g_diagnostic) { fprintf(stderr, "Error %d: Cant open library: %s\r\n", GetLastError(), cudart_library); }
				fprintf(stdout, "Detected%s=0\n", opt_tag);
				return 0;
			}
		}

		// if the NVIDIA management library is available, we can use it to get dynamic info
		const char * nvml_library = "nvml.dll";
		if (opt_dynamic) {
			nvml_handle = LoadLibrary(nvml_library);
			if ( ! nvml_handle) {
				fprintf(stderr, "Error %d: Cant open library: %s\r\n", GetLastError(), nvml_library);
			}
		}
	#else
		const char * opencl_library = "libOpenCL.so";
		ocl_handle = dlopen(opencl_library, RTLD_LAZY);
		if ( ! ocl_handle && opt_opencl) {
			fprintf(stderr, "Error %s: Cant open library: %s\n", dlerror(), opencl_library);
		}
		dlerror(); //Reset error
		const char * cudart_library = "libcudart.so";
		cuda_handle = dlopen(cudart_library, RTLD_LAZY);
		if ( ! cuda_handle) {
			cuda_handle = dlopen("libnvcuda.so", RTLD_LAZY);
			if (cuda_handle && cu_Init(cuda_handle)) {
				opt_nvcuda = 1;
				if (g_diagnostic) { fprintf(stderr, "using libnvcuda.so to simulate libcudart\n"); }
			} else if (ocl_handle) {
				// if no cuda, fall back to OpenCL detection
			} else {
				if (g_diagnostic) { fprintf(stderr, "Error %s: Cant open library: %s\n", dlerror(), cudart_library); }
				fprintf(stdout, "Detected%s=0\n", opt_tag);
				return 0;
			}
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

		if (ocl_handle) {
			ocl.GetPlatformIDs = (clGetPlatformIDs_t) dlsym(ocl_handle, "clGetPlatformIDs");
			ocl.GetPlatformInfo = (clGetPlatformInfo_t) dlsym(ocl_handle, "clGetPlatformInfo");
			ocl.GetDeviceIDs = (clGetDeviceIDs_t) dlsym(ocl_handle, "clGetDeviceIDs");
			ocl.GetDeviceInfo = (clGetDeviceInfo_t) dlsym(ocl_handle, "clGetDeviceInfo");
		}

		if (cuda_handle) {
			cudaGetDeviceCount = (cuda_t) dlsym(cuda_handle, opt_nvcuda ? "cuDeviceGetCount" : "cudaGetDeviceCount");
		} else if (ocl_handle && ocl.GetDeviceIDs) {
			cudaGetDeviceCount = ocl_GetDeviceCount;
			opt_pre = "OCL";
		}
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

	/*
	if (opt_rdp) {
		deviceCount = deviceCount ? deviceCount : 1;
	}
	*/
	if (opt_opencl) {
		int cd = 0;
		ocl_GetDeviceCount(&cd);
		if ( ! deviceCount) {
			deviceCount = cd;
			opt_pre = "OCL";
		}
	}
    
	// If there are no devices, there is nothing more to do
    if (deviceCount == 0) {
        // There is no device supporting CUDA
		fprintf(stdout, "Detected%s=0\n", opt_tag);
		return 0;
	}


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

	if (opt_pre_arg) { opt_pre = opt_pre_arg; }

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

		if (opt_static && cudaGetDeviceProperties) {
			cudaDeviceProp deviceProp;
			int driverVersion=0, runtimeVersion=0;

			cudaGetDeviceProperties(&deviceProp, dev);

			//printf("%sDev=%d\n",  prefix,dev);

		#if CUDART_VERSION >= 2020
			cudaDriverGetVersion(&driverVersion);
			cudaRuntimeGetVersion(&runtimeVersion);
			printf("%sDriverVersion=%d.%d\n", prefix, driverVersion/1000, driverVersion%100);
			printf("%sRuntimeVersion=%d.%d\n", prefix, runtimeVersion/1000, runtimeVersion%100);
		#endif

			if (dev < g_cl_cCuda) {
				cl_device_id did = cl_gpu_ids[g_cl_ixFirstCuda + dev];
				std::string fullver;
				oclGetInfo(did, CL_DEVICE_VERSION, fullver);
				size_t ix = fullver.find_first_of(' '); // skip OpenCL
				std::string vervend = fullver.substr(ix+1);
				ix = vervend.find_first_of(' '); 
				std::string ver = vervend.substr(0, ix); // split version from vendor info.
				printf("%sOpenCLVersion=%s\n", prefix, ver.c_str());

				//unsigned long long work_size = 0;
				//oclGetInfo(did, CL_DEVICE_MAX_WORK_GROUP_SIZE, work_size);
				//printf("%sWork=%llu\n", prefix, work_size);
			}

			printf("%sDeviceName=\"%s\"\n", prefix, deviceProp.name);
			printf("%sCapability=%d.%d\n", prefix, deviceProp.major, deviceProp.minor);
			printf("%sECCEnabled=%s\n", prefix, deviceProp.ECCEnabled ? "true" : "false");
			printf("%sGlobalMemoryMb=%.0f\n", prefix, deviceProp.totalGlobalMem/(1024.*1024.));
			printf("%sClockMhz=%.2f\n", prefix, deviceProp.clockRate * 1e-3f);
			printf("%sMultiprocessors=%u\n", prefix, deviceProp.multiProcessorCount);
			//printf("%sCores=%u\n", prefix, deviceProp.multiProcessorCount * ConvertSMVer2Cores(deviceProp.major, deviceProp.minor));

		} else if (opt_static && ocl_handle) {
			cl_device_id did = cl_gpu_ids[dev];
			std::string val;
			oclGetInfo(did, CL_DEVICE_NAME, val);
			printf("%sDeviceName=\"%s\"\n", prefix, val.c_str());
			//oclGetInfo(did, CL_DEVICE_VENDOR, val);
			oclGetInfo(did, CL_DEVICE_VERSION, val);
			size_t ix = val.find_first_of(' '); // skip OpenCL
			std::string vervend = val.substr(ix+1);
			ix = vervend.find_first_of(' '); 
			std::string ver = vervend.substr(0, ix); // split version from vendor info.
			printf("%sOpenCLVersion=%s\n", prefix, ver.c_str());

			unsigned int multiprocs = 0;
			oclGetInfo(did, CL_DEVICE_MAX_COMPUTE_UNITS, multiprocs);
			printf("%sMultiprocessors=%u\n", prefix, multiprocs);
			//unsigned int work_size = 0;
			//oclGetInfo(did, CL_DEVICE_MAX_WORK_GROUP_SIZE, work_size);
			//printf("%sWork=%u\n", prefix, work_size);
			unsigned int clock_mhz;
			oclGetInfo(did, CL_DEVICE_MAX_CLOCK_FREQUENCY, clock_mhz);
			printf("%sClockMhz=%u\n", prefix, clock_mhz);
			unsigned long long mem_bytes = 0;
			oclGetInfo(did, CL_DEVICE_GLOBAL_MEM_SIZE, mem_bytes);
			printf("%sGlobalMemoryMb=%.0f\n", prefix, mem_bytes/(1024.*1024.));
			//printf("%sCapability=%d.%d\n", prefix, deviceProp.major, deviceProp.minor);
			int ecc_enabled = 0;
			oclGetInfo(did, CL_DEVICE_ERROR_CORRECTION_SUPPORT, ecc_enabled);
			printf("%sECCEnabled=%s\n", prefix, ecc_enabled ? "true" : "false");
		}



		
		// end of if opt_static


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
