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
#include <map>
#include <list>
#include <algorithm>

#include <time.h>
#include <stdarg.h> // for va_start

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

static char hex_digit(unsigned char n) { return n + ((n < 10) ? '0' : ('a' - 10)); }
static char printable_digit(unsigned char n) { return (n >= ' ' && n <= '~') ? n : '.'; }
static const char * print_uuid(char* buf, int bufsiz, const unsigned char uuid[16]) {
	char *p = buf;
	char *endp = buf + bufsiz -1;
	for (int ix = 0; ix < 16; ++ix) {
		if (ix == 4 || ix == 6 || ix == 8 || ix == 10) *p++ = '-';
		unsigned char ch = uuid[ix];
		*p++ = hex_digit((ch >> 4) & 0xF);
		*p++ = hex_digit(ch & 0xF);
		if (p >= endp) break;
	}
	*p = 0;
	return buf;
}

// because on linux, these become gnu_dev_major and gnu_dev_minor (sigh)
// basic device properties we can query from the driver
class BasicProps {
public:
	BasicProps() : totalGlobalMem(0), ccMajor(0), ccMinor(0), multiProcessorCount(0), clockRate(0), ECCEnabled(0) {
		memset(uuid, 0, sizeof(uuid));
		memset(pciId, 0, sizeof(pciId));
	}

	std::string   name;                 /**< ASCII string identifying device */
	unsigned char uuid[16];             // uuids are big-endian 32 lower case hex digits shown as: "xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx" where M indicates version and N indicates sub-version
	char          pciId[16];            // null terminated string in form [domain]:[bus]:[device].[function], each field is hex. max of 12 chars, [domain}: or .[function] may be omitted
	size_t        totalGlobalMem;       /**< Global memory available on device in bytes */
	int           ccMajor;              /**< Major compute capability */
	int           ccMinor;              /**< Minor compute capability */
	int           multiProcessorCount;  /**< Number of multiprocessors on device */
	int           clockRate;            /**< Clock frequency in kilohertz */
	int           ECCEnabled;           /**< Device has ECC support enabled */

	bool hasUuid() { return uuid[0] || uuid[6] || uuid[8]; }
	const char * printUuid() {
		static char buf[32 + 8];
		return print_uuid(buf, sizeof(buf), uuid);
	}
};

typedef cudaError_t (CUDACALL* dev_basic_props)(int, BasicProps *);
typedef cudaError_t(CUDACALL* cuda_DevicePropBuf_int)(struct cudaDevicePropBuffer *, int);
cuda_DevicePropBuf_int cudaGetDevicePropertiesOfIndeterminateStructure = NULL;

void hex_dump(FILE* out, const unsigned char * buf, size_t cb, int offset)
{
	char line[6 + 16 * 3 + 2 + 16 + 2 + 2];
	for (size_t lx = 0; lx < cb && lx < 0xFFFF; lx += 16) {
		memset(line, ' ', sizeof(line));
		line[sizeof(line) - 1] = 0;
		sprintf(line, "%04X:", (int)lx);
		char * pb = line + 5;
		*pb++ = ' ';
		char * pa = pb + 16 * 3 + 2;
		for (size_t ix = 0; ix < 16; ++ix) {
			if (lx + ix >= cb) break;
			unsigned char ch = buf[lx + ix];
			*pa++ = printable_digit(ch);
			*pb++ = hex_digit((ch >> 4) & 0xF);
			*pb++ = hex_digit(ch & 0xF);
			*pb++ = ' ';
		}
		fprintf(out, "%s\n", line);
		if ((int)lx > offset && (int)lx <= offset+16) fprintf(out, "strings: %04X\n", offset);
	}
}

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
        { 0x32, 192}, // Kepler Generation (SM 3.2) GK20A class
        { 0x35, 192}, // Kepler Generation (SM 3.5) GK11x class
        { 0x37, 192}, // Kepler Generation (SM 3.7) GK21x class
        { 0x50, 128}, // Maxwell Generation (SM 5.0) GM10x class
        { 0x52, 128}, // Maxwell Generation (SM 5.2) GM20x class
        { 0x53, 128}, // Maxwell Generation (SM 5.3) GM20x class
        { 0x60, 64 }, // Pascal Generation (SM 6.0) GP100 class
        { 0x61, 128}, // Pascal Generation (SM 6.1) GP10x class
        { 0x62, 128}, // Pascal Generation (SM 6.2) GP10x class
        { 0x70, 64 }, // Volta Generation (SM 7.0) GV100 class
        { 0x72, 64 }, // Volta Generation (SM 7.2) GV10B class
        { 0x75, 64 }, // Turing Generation (SM 7.5) TU1xx class
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

// because we don't have access to condor_utils, make a limited local version
const std::string & Format(const char * fmt, ...) {
	static std::string buffer;
	static char * temp = NULL;
	static int    max_temp = 0;

	if ( ! temp) { max_temp = 4096; temp = (char*)malloc(max_temp+1); }

	va_list args;
	va_start(args, fmt);
	int cch = vsnprintf(temp, max_temp, fmt, args);
	va_end (args);
	if (cch > max_temp) {
		free(temp);
		max_temp = cch+100;
		temp = (char*)malloc(max_temp+1); temp[0] = 0;
		va_start(args, fmt);
		vsnprintf(temp, max_temp, fmt, args);
		va_end (args);
	}

	temp[max_temp] = 0;
	buffer = temp;
	return buffer;
}

#ifndef MATCH_PREFIX
bool
is_arg_prefix(const char * parg, const char * pval, int must_match_length /*= 0*/) 
{
	// no matter what, at least 1 char must match
	// this also protects us from the case where parg is ""
	if (!*pval || (*parg != *pval)) return false;

	// do argument matching based on a minimum prefix. when we run out
	// of characters in parg we must be at a \0 or no match and we
	// must have matched at least must_match_length characters or no match
	int match_length = 0;
	while (*parg == *pval) {
		++match_length;
		++parg; ++pval;
		if (!*pval) break;
	}
	if (*parg) return false;
	if (must_match_length < 0) return (*pval == 0);
	return match_length >= must_match_length;
}

bool
is_arg_colon_prefix(const char * parg, const char * pval, const char ** ppcolon, int must_match_length /*= 0*/) 
{
	if (ppcolon) *ppcolon = NULL;

	// no matter what, at least 1 char must match
	// this also protects us from the case where parg is ""
	if (!*pval || (*parg != *pval)) return false;

	// do argument matching based on a minimum prefix. when we run out
	// of characters in parg we must be at a \0 or no match and we
	// must have matched at least must_match_length characters or no match
	int match_length = 0;
	while (*parg == *pval) {
		++match_length;
		++parg; ++pval;
		if (*parg == ':') {
			if (ppcolon) *ppcolon = parg;
			break;
		}
		if (!*pval) break;
	}
	if (*parg && *parg != ':') return false;
	if (must_match_length < 0) return (*pval == 0);
	return match_length >= must_match_length;
}

bool
is_dash_arg_prefix(const char * parg, const char * pval, int must_match_length /*= 0*/)
{
	if (*parg != '-') return false;
	++parg;
	// if arg begins with --, then we require an exact match for pval.
	if (*parg == '-') { ++parg; must_match_length = -1; }
	return is_arg_prefix(parg, pval, must_match_length);
}

bool
is_dash_arg_colon_prefix(const char * parg, const char * pval, const char ** ppcolon, int must_match_length /*= 0*/)
{
	if (*parg != '-') return false;
	++parg;
	// if arg begins with --, then we require an exact match for pval.
	if (*parg == '-') { ++parg; must_match_length = -1; }
	return is_arg_colon_prefix(parg, pval, ppcolon, must_match_length);
}
#endif // MATCH_PREFIX


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
	if (sim_index < 0 || sim_index > sim_index_max)
		return cudaErrorInvalidValue;
	if (sim_index == sim_index_max) {
		*pdevs = sim_index_max;
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
cudaError_t CUDACALL sim_cudaRuntimeGetVersion(int* pver) {
	if (sim_index < 0 || sim_index > sim_index_max)
		return cudaErrorInvalidValue;
	int ix = sim_index < sim_index_max ? sim_index : 0;
	*pver = aSimConfig[ix].runtimeVer;
	return cudaSuccess; 
}

cudaError_t CUDACALL sim_getBasicProps(int devID, BasicProps * p) {
	if (sim_index < 0 || sim_index > sim_index_max)
		return cudaErrorNoDevice;

	const struct _simulated_cuda_device * dev;
	if (sim_index == sim_index_max) {
		dev = aSimConfig[devID].device;
	} else {
		int cDevs = sim_device_count ? sim_device_count : aSimConfig[sim_index].deviceCount;
		if (devID < 0 || devID > cDevs)
			return cudaErrorInvalidDevice;
		dev = aSimConfig[sim_index].device;
	}

	p->name = dev->name;
	unsigned char uuid[16] = {0x01,0x22,0x33,0x34,  0x44,0x45, 0x56,0x67, 0x89,0x9a, 0xab,0xbc,0xcd,0xde,0xef,0xf0 };
	uuid[0] = (unsigned char)devID;
	memcpy(p->uuid, uuid, 16);
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
sim_nvmlDeviceGetHandleByPciBusId(const char * pciBusId, nvmlDevice_t * pdev) {
	unsigned int devID;
	if( sscanf(pciBusId, "0000:%02x:00.0", & devID) != 1 ) {
		return NVML_ERROR_NOT_FOUND;
	}
	devID -= 0x40;
	* pdev = (nvmlDevice_t)(size_t)(devID+1);
	return NVML_SUCCESS;
};



int g_verbose = 0;
int g_diagnostic = 0;
int g_config_syntax = 0;
int g_config_fail_on_error = 0;

#define MODE_ERROR          1
#define MODE_VERBOSE        2
#define MODE_DIAGNOSTIC_MSG 4 // diagnostic to stdout
#define MODE_DIAGNOSTIC_ERR 5 // diagnostic to stderr
int print_error(int mode, const char * fmt, ...) {
	char temp[4096];
	char * ptmp = temp;
	int max_temp = (int)(sizeof(temp)/sizeof(temp[0]));

	FILE * out = g_config_syntax ? stdout : stderr;
	bool is_error = true;
	switch (mode) {
	case MODE_VERBOSE:
		if ( ! g_verbose) return 0;
		is_error = false;
		out = stdout;
		break;
	case MODE_DIAGNOSTIC_MSG:
		out = stdout;
		is_error = false;
		// fall through
	case MODE_DIAGNOSTIC_ERR:
		if ( ! g_diagnostic) return 0;
		break;
	}

	// if config syntax is desired, turn error messages into comments
	// unless g_config_fail_on_error is set.
	if (g_config_syntax && ( !is_error || ! g_config_fail_on_error)) {
		*ptmp++ = '#';
		--max_temp;
	}

	va_list args;
	va_start(args, fmt);
	int cch = vsnprintf(ptmp, max_temp, fmt, args);
	va_end (args);

	if (cch < 0 || cch >= max_temp) {
		// TJ: I don't think its possible to get where when g_config_syntax is on
		// because all known inputs will be < 4k in size.
		// but if we do, just suppress this error message
		if ( ! g_config_syntax) {
			fprintf(stderr, "internal error, %d is not enough space to format, value will be truncated.\n", max_temp);
		}
		temp[max_temp-1] = 0;
	}

	return fputs(temp, out);
}


void* g_cu_handle = NULL;
// functions for runtime linking to nvcuda library
typedef void * cudev;
typedef CUresult (CUDACALL* cu_uint_t)(unsigned int);
cu_uint_t cuInit = NULL;
typedef cudaError_t (CUDACALL* cuda_dev_int_t)(cudev*,int);
cuda_dev_int_t cuDeviceGet = NULL;
typedef cudaError_t (CUDACALL* cuda_ga_t)(int*,int,cudev);
cuda_ga_t cuDeviceGetAttribute = NULL;
typedef cudaError_t (CUDACALL* cuda_name_t)(char*,int,cudev);
cuda_name_t cuDeviceGetName = NULL;
typedef CUresult (CUDACALL * cuda_uuid_t)(unsigned char uuid[16], cudev);
cuda_uuid_t cuDeviceGetUuid = NULL;
typedef CUresult (CUDACALL * cuda_luid_t)(char *, unsigned int *, cudev);
cuda_luid_t cuDeviceGetLuid = NULL;
typedef CUresult (CUDACALL * cuda_pciid_t)(char *, int, cudev);
cuda_pciid_t cuDeviceGetPCIBusId = NULL; // buffer should have room for 12 characters, 13 with a null terminator
typedef cudaError_t (CUDACALL* cuda_cc_t)(int*,int*,cudev);
cuda_cc_t cuDeviceComputeCapability = NULL;
typedef cudaError_t (CUDACALL* cuda_size_t)(size_t*,cudev);
cuda_size_t cuDeviceTotalMem = NULL; // "useCuDeviceTotalMem_v2"
typedef cudaError_t (CUDACALL* cuda_int_t)(int*);
cuda_int_t cuDeviceGetCount = NULL;

bool cu_Init(void* cu_handle) {
	g_cu_handle = cu_handle;
	print_error(MODE_DIAGNOSTIC_MSG, "diag: using nvcuda for gpu discovery\n");
	cuInit = (cu_uint_t)dlsym(cu_handle, "cuInit");
	if ( ! cuInit) return false;

	cuDeviceGet = (cuda_dev_int_t)dlsym(cu_handle, "cuDeviceGet");
	cuDeviceGetAttribute = (cuda_ga_t)dlsym(cu_handle, "cuDeviceGetAttribute");
	cuDeviceGetName = (cuda_name_t)dlsym(cu_handle, "cuDeviceGetName");
	cuDeviceGetPCIBusId = (cuda_pciid_t)dlsym(cu_handle, "cuDeviceGetPCIBusId");
	cuDeviceGetUuid = (cuda_uuid_t)dlsym(cu_handle, "cuDeviceGetUuid");
	cuDeviceGetLuid = (cuda_luid_t)dlsym(cu_handle, "cuDeviceGetLuid");
	cuDeviceComputeCapability = (cuda_cc_t)dlsym(cu_handle, "cuDeviceComputeCapability");
	cuDeviceTotalMem = (cuda_size_t)dlsym(cu_handle, "cuDeviceTotalMem_v2");
	if ( ! cuDeviceTotalMem) {
		print_error(MODE_DIAGNOSTIC_MSG, "diag: dlsym failed for cuDeviceTotalMem_v2, a max of 4GB Memory will be reported\n");
		cuDeviceTotalMem = (cuda_size_t)dlsym(cu_handle, "cuDeviceTotalMem");
	}
	cuDeviceGetCount = (cuda_int_t)dlsym(cu_handle, "cuDeviceGetCount");

	CUresult ret = cuInit(0);
	if (ret != CUDA_SUCCESS) {
		if (ret == CUDA_ERROR_NO_DEVICE) {
			print_error(MODE_DIAGNOSTIC_MSG, "diag: cuInit found no CUDA-capable devices. code=%d\n", ret);
		} else {
			print_error(MODE_ERROR, "Error: cuInit returned %d\n", ret);
		}
	}
	return ret == CUDA_SUCCESS;
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
		print_error(MODE_ERROR, "can't open %s\n", reg_key);
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
#else
	void* handle = dlopen("libnvcuda.so", RTLD_LAZY);
	if (handle) {
		typedef cudaError_t (CUDACALL* cuda_t)(int*);
		cuda_t cudaRuntimeGetVersion = (cuda_t)dlsym(handle, "cudaRuntimeGetVersion");
		if (cudaRuntimeGetVersion) {
			ret = cudaRuntimeGetVersion(pver);
		}
		dlclose(handle);
	}
#endif
	return ret; 
}

cudaError_t CUDACALL cu_getBasicProps(int devID, BasicProps * p) {
	cudev dev;
	cudaError_t res = cuDeviceGet(&dev, devID);
	if (cudaSuccess == res) {

	if (g_diagnostic) {
		print_error(MODE_DIAGNOSTIC_MSG, "# querying ordinal:%d, dev:%p using cuDevice* API\n", devID, dev);
		size_t mem = 0;
		cudaError_t er = cuDeviceTotalMem(&mem, (cudev)(size_t)devID);
		print_error(MODE_DIAGNOSTIC_MSG, "# cuDeviceTotalMem(%d) returns %d, value = %llu\n", devID, er, (unsigned long long)mem);
	}

		// for some reason PowerPC was having trouble with a stack buffer for cuGetDeviceName
		// so we switch to malloc and a larger buffer, and we overdo the null termination.
		char * name = (char*)malloc(1028);
		memset(name, 0, 1028);
		cuDeviceGetName(name, 1024, dev);
		name[1024] = 0; // this shouldn't be necessary, but it can't hurt.
		p->name = name;
		free(name);
		if (cuDeviceGetUuid) cuDeviceGetUuid(p->uuid, dev);
		if (cuDeviceGetPCIBusId) cuDeviceGetPCIBusId(p->pciId, sizeof(p->pciId), dev);
		cuDeviceComputeCapability(&p->ccMajor, &p->ccMinor, dev);
		cudaError_t er = cuDeviceTotalMem(&p->totalGlobalMem, dev);
		print_error(MODE_DIAGNOSTIC_MSG, "# cuDeviceTotalMem(%p) returns %d, value = %llu\n", dev, er, (unsigned long long)p->totalGlobalMem);
		cuDeviceGetAttribute(&p->clockRate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, dev);
		cuDeviceGetAttribute(&p->multiProcessorCount, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, dev);
		cuDeviceGetAttribute(&p->ECCEnabled, CU_DEVICE_ATTRIBUTE_ECC_ENABLED, dev);
	}
	return res; 
}

cudaError_t basicPropsFromCudaProps(cudaDevicePropStrings * dps, cudaDevicePropInts * dpi, BasicProps * p)
{
	p->name = dps->name;
	p->totalGlobalMem = dpi->totalGlobalMem;
	p->ccMajor = dpi->ccMajor;
	p->ccMinor = dpi->ccMinor;
	p->clockRate = dpi->clockRate;
	p->multiProcessorCount = dpi->multiProcessorCount;
	p->ECCEnabled = dpi->ECCEnabled;
	memcpy(p->uuid, dps->uuid, sizeof(p->uuid));
	if (dpi->pciBusID || dpi->pciDeviceID) {
		sprintf(p->pciId, "%04x:%02x:%02x.0", dpi->pciDomainID, dpi->pciBusID, dpi->pciDeviceID);
	}
	return cudaSuccess;
}
cudaError_t CUDACALL cuda9_getBasicProps(int devID, BasicProps * p) {
	cudaDevicePropBuffer buf;
	memset(buf.props, 0, sizeof(buf.props));
	cudaError_t err = cudaGetDevicePropertiesOfIndeterminateStructure(&buf, devID);
	if (cudaSuccess != err)
		return err;

	const int ints_offset = 256;
	if (g_diagnostic) {
		print_error(MODE_DIAGNOSTIC_MSG, "diag: cudaDviceProperties 9 buffer:\n");
		hex_dump(stdout, &buf.props[0], sizeof(buf), ints_offset);
	}

	cudaDevicePropStrings * dps = (cudaDevicePropStrings *)(&buf);
	cudaDevicePropInts * dpi = (cudaDevicePropInts *)(&buf.props[ints_offset]);

	return basicPropsFromCudaProps(dps, dpi, p);
}
cudaError_t CUDACALL cuda10_getBasicProps(int devID, BasicProps * p) {
	cudaDevicePropBuffer buf;
	memset(buf.props, 0, sizeof(buf.props));
	cudaError_t err = cudaGetDevicePropertiesOfIndeterminateStructure(&buf, devID);
	if (cudaSuccess != err)
		return err;

	// skip over the integers, and align on a size_t boundary
	const int ints_offset = (sizeof(cudaDevicePropStrings) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);

	if (g_diagnostic) {
		print_error(MODE_DIAGNOSTIC_MSG, "diag: cudaDviceProperties 10 buffer:\n");
		hex_dump(stdout, &buf.props[0], sizeof(buf), ints_offset);
	}

	cudaDevicePropStrings * dps = (cudaDevicePropStrings *)(&buf);
	cudaDevicePropInts * dpi = (cudaDevicePropInts *)(&buf.props[ints_offset]);
	memcpy(p->uuid, dps->uuid, sizeof(dps->uuid));
	return basicPropsFromCudaProps(dps, dpi, p);
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
		if ( ! g_buffer) { print_error(MODE_ERROR, "ERROR: failed to allocate %d bytes\n", (int)(cb*2)); exit(-1); }
		g_cBuffer = (unsigned int)(cb*2);
		clr = ocl.GetPlatformInfo(plid, eInfo, g_cBuffer, g_buffer, &cb);
	}
	if (clr == CL_SUCCESS) {
		g_buffer[g_cBuffer-1] = 0;
		val = g_buffer;
		if (ocl_log) { print_error(MODE_VERBOSE, "\t%s: \"%s\"\n", ocl_name(eInfo), val.c_str()); }
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
		if ( ! g_buffer) { print_error(MODE_ERROR, "ERROR: failed to allocate %d bytes\n", (int)(cb*2)); exit(-1); }
		g_cBuffer = (unsigned int)(cb*2);
		clr = ocl.GetDeviceInfo(did, eInfo, g_cBuffer, g_buffer, &cb);
	}
	if (clr == CL_SUCCESS) {
		g_buffer[g_cBuffer-1] = 0;
		val = g_buffer;
		if (ocl_log) { print_error(MODE_VERBOSE, "\t\t%s: \"%s\"\n", ocl_name(eInfo), val.c_str()); }
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
		print_error(MODE_VERBOSE, "\t\t%s: %s\n", ocl_name(eInfo), ocl_value(val));
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

	print_error(MODE_DIAGNOSTIC_MSG, "diag: ocl_Init()\n");
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
		print_error(MODE_ERROR, "ocl.GetPlatformIDs returned error=%d and %d platforms\n", clr, cPlatforms);
	}
	if (cPlatforms > 0) {
		cl_platforms.reserve(cPlatforms);
		for (unsigned int ii = 0; ii < cPlatforms; ++ii) {
			cl_platforms.push_back(NULL);
		}
		ocl.GetPlatformIDs(cPlatforms, &cl_platforms[0], &cPlatforms);
	}

	// enable logging in oclGetInfo if verbose
	ocl_log = g_verbose;

	for (unsigned int ii = 0; ii < cPlatforms; ++ii) {
		cl_platform_id plid = cl_platforms[ii];
		print_error(MODE_DIAGNOSTIC_MSG, "ocl platform %d = %x\n", ii, (int)(size_t)plid);
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
		cl_device_type f_types = CL_DEVICE_TYPE_GPU;
		clr = ocl.GetDeviceIDs(plid, f_types, 0, NULL, &cDevs);
		print_error(MODE_VERBOSE, "\tDEVICES = %d\n", cDevs);

		if (CL_SUCCESS == clr && cDevs > 0) {
			unsigned int ixFirst = (unsigned int)cl_gpu_ids.size();
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
						cl_device_type eType = CL_DEVICE_TYPE_NONE;
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
	ocl_device_count = (int)cl_gpu_ids.size();
	return 0;
}

cudaError_t CUDACALL ocl_GetDeviceCount(int * pcount) {
	ocl_Init();
	*pcount = ocl_device_count;
	return cudaSuccess;
}

void usage(FILE* output, const char* argv0);

bool addToDeviceWhiteList( char * list, std::list<int> & dwl ) {
	char * tokenizer = strdup( list );
	if( tokenizer == NULL ) {
		fprintf( stderr, "Error: device list too long\n" );
		return false;
	}
	char * next = strtok( tokenizer, "," );
	for( ; next != NULL; next = strtok( NULL, "," ) ) {
		dwl.push_back( atoi( next ) );
	}
	free( tokenizer );
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Program main
////////////////////////////////////////////////////////////////////////////////
int
main( int argc, const char** argv)
{
	int deviceCount = 0;
	//int have_cuda = 1;
	int have_nvml = 0;
	int opt_basic = 0; // publish basic GPU properties
	int opt_extra = 0; // publish extra GPU properties.
	int opt_dynamic = 0; // publish dynamic GPU properties
	int opt_cron = 0;  // publish in a form usable by STARTD_CRON
	int opt_hetero = 0;  // don't assume properties are homogeneous
	int opt_simulate = 0; // pretend to detect GPUs
	int opt_config = 0;
	//int opt_rdp = 0;
	std::list<int> dwl; // Device White List
	int opt_nvcuda = 0; // use nvcuda rather than cudarl
	int opt_cudarl = 0; // force use of use cudarl rather than nvcuda
	int opt_opencl = 0; // prefer opencl detection
	int opt_cuda_only = 0; // require cuda detection
	const char * opt_tag = "GPUs";
	const char * opt_pre = "CUDA";
	const char * opt_pre_arg = NULL;
	const char * pcolon;
	int i;
    int dev;

	//
	// When run with CUDA_VISIBLE_DEVICES set, only report the visible
	// devices, but do so with the machine-level device indices.  The
	// easiest way to do the latter is by unsetting CUDA_VISIBLE_DEVICES.
	//
	char * cvd = getenv( "CUDA_VISIBLE_DEVICES" );
	if( cvd != NULL ) {
		if(! addToDeviceWhiteList( cvd, dwl )) { return 1; }
	}
#if defined(WINDOWS)
	_putenv( "CUDA_VISIBLE_DEVICES=" );
#else
	unsetenv( "CUDA_VISIBLE_DEVICES" );
#endif

	// Ditto for GPU_DEVICE_ORDINAL.  If we ever handle dissimilar GPUs
	// properly (right now, the negotiator can't tell them apart), we'll
	// have to change this program to filter for specific types of GPUs.
	char * gdo = getenv( "GPU_DEVICE_ORDINAL" );
	if( gdo != NULL ) {
		if(! addToDeviceWhiteList( gdo, dwl )) { return 1; }
	}
#if defined( WINDOWS)
	_putenv( "GPU_DEVICE_ORDINAL=" );
#else
	unsetenv( "GPU_DEVICE_ORDINAL" );
#endif

	for (i=1; i<argc && argv[i]; i++) {
		if (is_dash_arg_prefix(argv[i], "help", 1)) {
			opt_basic = 1; // publish basic GPU properties
			usage(stdout, argv[0]);
			return 0;
		}
		else if (is_dash_arg_prefix(argv[i], "properties", 1)) {
			opt_basic = 1; // publish basic GPU properties
		}
		else if (is_dash_arg_prefix(argv[i], "extra", 3)) {
			opt_basic = 1; // publish basic GPU properties
			opt_extra = 1; // publish extra GPU properties
		}
		else if (is_dash_arg_prefix(argv[i], "dynamic", 3)) {
			opt_dynamic = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "cron", 4)) {
			opt_cron = 1;
		} else if (is_dash_arg_prefix(argv[i], "mixed", 3) || is_dash_arg_prefix(argv[i], "hetero", 3)) {
			opt_hetero = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "opencl", -1)) {
			opt_opencl = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "cuda", -1)) {
			opt_cuda_only = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "verbose", 4)) {
			g_verbose = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "diagnostic", 4)) {
			g_diagnostic = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "config", -1)) {
			g_config_syntax = 1;
			opt_config = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "tag", 3)) {
			if (argv[i+1]) {
				opt_tag = argv[++i];
			}
		}
		else if (is_dash_arg_prefix(argv[i], "prefix", 3)) {
			if (argv[i+1]) {
				opt_pre_arg = argv[++i];
				if (strlen(opt_pre_arg) > 80) {
						fprintf (stderr, "Error: -prefix should be less than 80 characters\n");
						return 1;
				}
			}
		}
		else if (is_dash_arg_prefix(argv[i], "device", 3)) {
			if ( ! argv[i+1] || '-' == *argv[i+1]) {
				fprintf (stderr, "Error: -device requires an argument\n");
				usage(stderr, argv[0]);
				return 1;
			}
			dwl.push_back( atoi(argv[++i]) );
		}
		else if (is_dash_arg_colon_prefix(argv[i], "simulate", &pcolon, 3)) {
			opt_simulate = 1;
			if (pcolon) {
				sim_index = atoi(pcolon+1);
				if (sim_index < 0 || sim_index > sim_index_max) {
					fprintf(stderr, "Error: simulation must be in range of 0-%d\r\n", sim_index_max);
					usage(stderr, argv[0]);
					return 1;
				}
				const char * pcomma = strchr(pcolon+1,',');
				if (pcomma) { sim_device_count = atoi(pcomma+1); }
			}
		}
		else if (is_dash_arg_prefix(argv[i], "nvcuda",-1)) {
			// use nvcuda instead of cudart, (option is for testing...)
			opt_nvcuda = 1;
		} 
		else if (is_dash_arg_prefix(argv[i], "rl", 2) || is_dash_arg_prefix(argv[i], "cudarl", 6)) {
			opt_cudarl = 1;
		}
		else {
			fprintf(stderr, "option %s is not valid\n", argv[i]);
			usage(stderr, argv[0]);
			return 1;
		}
	}

	// function pointers for runtime linking to cudarl stuff
	typedef cudaError_t (CUDACALL* cuda_t)(int*); //Used for DLFCN
	cuda_t cudaGetDeviceCount = NULL; 
	cuda_t cudaDriverGetVersion = NULL;
	dev_basic_props getBasicProps = NULL;

	// function pointers for the NVIDIA management layer, used for dynamic attributes.
    //nvmlReturn_t is the Return type for all of these functions
	typedef nvmlReturn_t (*nvml_void)(void);
	nvml_void nvmlInit = NULL;  // use "nvmlInit_v2"
	nvml_void nvmlShutdown = NULL;
	typedef nvmlReturn_t (*nvml_charptr_Device)(const char *, nvmlDevice_t *);
	nvml_charptr_Device nvmlDeviceGetHandleByPciBusId = NULL;  // use "nvmlDeviceGetHandleByPciBusId_v2"
	typedef nvmlReturn_t (*nvml_Device_unsigned_int)(nvmlDevice_t, unsigned int *);
	nvml_Device_unsigned_int nvmlDeviceGetFanSpeed = NULL;
	nvml_Device_unsigned_int nvmlDeviceGetPowerUsage = NULL;
	typedef nvmlReturn_t (*nvml_Device_TemperatureSensors_unsigned_int)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int *);
	nvml_Device_TemperatureSensors_unsigned_int nvmlDeviceGetTemperature = NULL;
	typedef nvmlReturn_t (*nvml_Device_MemoryErrorType_EccCounterType_unsigned_long_long)(nvmlDevice_t, nvmlMemoryErrorType_t, nvmlEccCounterType_t, unsigned long long *);
	nvml_Device_MemoryErrorType_EccCounterType_unsigned_long_long nvmlDeviceGetTotalEccErrors = NULL;


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
		getBasicProps = sim_getBasicProps;

		nvmlInit = sim_nvmlInit;
		nvmlShutdown = sim_nvmlInit;
		nvmlDeviceGetHandleByPciBusId = sim_nvmlDeviceGetHandleByPciBusId;
		nvmlDeviceGetFanSpeed = sim_nvmlDeviceGetFanSpeed;
		nvmlDeviceGetPowerUsage = sim_nvmlDeviceGetPowerUsage;
		nvmlDeviceGetTemperature = sim_nvmlDeviceGetTemperature;
		nvmlDeviceGetTotalEccErrors = sim_nvmlDeviceGetTotalEccErrors;
		have_nvml = 1;

	} else {

	#ifdef WIN32
		if ( ! opt_cuda_only) {
			const char * opencl_library = "OpenCL.dll";
			ocl_handle = LoadLibrary(opencl_library);
			if ( ! ocl_handle && opt_opencl) {
				print_error(MODE_ERROR, "Error %d: Cant open library: %s\r\n", GetLastError(), opencl_library);
			}
		}

		const char * cudart_library = "cudart.dll"; // "cudart32_55.dll"
		if ( ! opt_cudarl) cuda_handle = LoadLibrary("nvcuda.dll");
		if (cuda_handle && cu_Init(cuda_handle)) {
			opt_nvcuda = 1;
			cudart_library = "nvcuda.dll";
		} else {
			if ( ! cuda_handle) {
				if ( ! opt_cudarl) print_error(MODE_DIAGNOSTIC_MSG, "can't open nvcuda.dll. Error %d\r\n", GetLastError());
			} else {
				FreeLibrary(cuda_handle);
				cuda_handle = NULL;
			}
			cuda_handle = LoadLibrary(cudart_library);
			if (cuda_handle) {
				opt_nvcuda = 0;
			} else if (ocl_handle) {
				// if no cuda, fall back to OpenCL detection
			} else {
				print_error(MODE_DIAGNOSTIC_ERR, "Error %d: Can't open library: %s\r\n", GetLastError(), cudart_library);
				if ( ! opt_cron) fprintf(stdout, "Detected%s=0\n", opt_tag);
				if (opt_config) fprintf(stdout, "NUM_DETECTED_%s=0\n", opt_tag);
				return 0;
			}
		}

		// if the NVIDIA management library is available, we can use it to get dynamic info
		const char * nvml_library = "nvml.dll";
		if (opt_dynamic) {
			nvml_handle = LoadLibrary(nvml_library);
			if ( ! nvml_handle) {
				print_error(MODE_ERROR, "Error %d: Can't open library: %s\r\n", GetLastError(), nvml_library);
			}
		}
	#else
		if ( ! opt_cuda_only) {
			const char * opencl_library = "libOpenCL.so";
			ocl_handle = dlopen(opencl_library, RTLD_LAZY);
			if ( ! ocl_handle && opt_opencl) {
				print_error(MODE_ERROR, "Error %s: Can't open library: %s\n", dlerror(), opencl_library);
			}
			dlerror(); //Reset error
		}

		const char * cudart_library = "libcudart.so";
		const char * nvcuda_library = "libcuda.so";
		if ( ! opt_cudarl) cuda_handle = dlopen(nvcuda_library, RTLD_LAZY);
		if (cuda_handle && cu_Init(cuda_handle)) {
			opt_nvcuda = 1;
			cudart_library = nvcuda_library;
		} else {
			if ( ! cuda_handle) {
				if ( ! opt_cudarl) print_error(MODE_DIAGNOSTIC_MSG, "Can't open %s. Error %s\n", nvcuda_library, dlerror());
			} else {
				dlclose(cuda_handle);
				cuda_handle = NULL;
			}
			cuda_handle = dlopen(cudart_library, RTLD_LAZY);
			if (cuda_handle) {
				opt_nvcuda = 0;
			} else if (ocl_handle) {
				// if no cuda, fall back to OpenCL detection
			} else {
				print_error(MODE_DIAGNOSTIC_ERR, "Error %s: Can't open library: %s\n", dlerror(), cudart_library);
				if ( ! opt_cron) fprintf(stdout, "Detected%s=0\n", opt_tag);
				if (opt_config) fprintf(stdout, "NUM_DETECTED_%s=0\n", opt_tag);
				return 0;
			}
		}
		dlerror(); //Reset error
		const char * nvml_library = "libnvidia-ml.so.1";
		if (opt_dynamic) {
			nvml_handle = dlopen(nvml_library, RTLD_LAZY);
			if ( ! nvml_handle) {
				nvml_library = "libnvidia-ml.so";
				nvml_handle = dlopen(nvml_library, RTLD_LAZY);
			}
			if ( ! nvml_handle) {
				print_error(MODE_ERROR, "Error %s: Cant open library: %s\n", dlerror(), nvml_library);
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
			print_error(MODE_ERROR, "Error %d: Cant find %s in library: %s\r\n", GetLastError(), "cudaGetDeviceCount", cudart_library);
			#else
			print_error(MODE_ERROR, "Error %s: Cant find %s in library: %s\n", dlerror(), "cudaGetDeviceCount", cudart_library);
			#endif
			if ( ! opt_cron) fprintf(stdout, "Detected%s=0\n", opt_tag);
			if (opt_config) fprintf(stdout, "NUM_DETECTED_%s=0\n", opt_tag);
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
		if ( ! opt_cron) fprintf(stdout, "Detected%s=0\n", opt_tag);
		if (opt_config) fprintf(stdout, "NUM_DETECTED_%s=0\n", opt_tag);
		return 0;
	}


	// lookup the function pointers we will need later from the cudart library
	//
	if ( !opt_simulate && cuda_handle) {
		cuda_t cudaRuntimeGetVersion = NULL;
		if (opt_nvcuda) {
			// if we have nvcuda loaded rather than cudart, we can simulate 
			// cudart functions from nvcuda functions. 
			cudaDriverGetVersion = (cuda_t) dlsym(cuda_handle, "cuDriverGetVersion");
			cudaRuntimeGetVersion = cu_cudaRuntimeGetVersion;
			getBasicProps = cu_getBasicProps;
		} else {
			cudaDriverGetVersion = (cuda_t) dlsym(cuda_handle, "cudaDriverGetVersion");
			cudaRuntimeGetVersion = (cuda_t) dlsym(cuda_handle, "cudaRuntimeGetVersion");
			if (cudaRuntimeGetVersion) {
				int runtimeVersion = 0;
				cudaRuntimeGetVersion(&runtimeVersion);
				// cudaGetDeviceProperties fills out an unversioned structure that changed between cuda9 and cuda 10
				getBasicProps = (runtimeVersion >= 10 * 1000) ? cuda10_getBasicProps : cuda9_getBasicProps;
				cudaGetDevicePropertiesOfIndeterminateStructure = (cuda_DevicePropBuf_int)dlsym(cuda_handle, "cudaGetDeviceProperties");
			}
		}
	}

	// load functions we will need from the nvml library, if we don't have that library
	// that's ok it just means there is no dynamic info.
	//
	if ( ! opt_simulate && nvml_handle) {
		nvmlInit = (nvml_void) dlsym(nvml_handle, "nvmlInit_v2"); // (void)
		if ( ! nvmlInit) {
			have_nvml = 0;
		} else {
			have_nvml = 1;
			nvmlShutdown = (nvml_void) dlsym(nvml_handle, "nvmlShutdown"); // (void)
			nvmlDeviceGetHandleByPciBusId = (nvml_charptr_Device) dlsym(nvml_handle, "nvmlDeviceGetHandleByPciBusId_v2");
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
			print_error(MODE_ERROR, "Warning: nvmlInit failed with error %d, dynamic information will not be available.\n", result);
			have_nvml = 0;
		}
	}

	if (opt_pre_arg) { opt_pre = opt_pre_arg; }

	// we will build an array of key=value pairs to hold properties of each device
	// we build the whole list before showing anything so we can decide whether
	// we are looking at a homogenous or heterogeneous pool
	typedef std::map<std::string, std::string> KVP;
	typedef std::map<std::string, KVP> MKVP;
	MKVP dev_props;

	// print out info about detected GPU resources
	//
	std::string detected_gpus;
	int filteredDeviceCount = 0;
	for (dev = 0; dev < deviceCount; dev++) {
		if( (!dwl.empty()) && std::find( dwl.begin(), dwl.end(), dev ) == dwl.end() ) {
			continue;
		}

		char prefix[100];
		sprintf(prefix,"%s%d",opt_pre,dev);
		dev_props[prefix].clear(); // this has the side effect of creating the KVP for prefix.
		if ( ! detected_gpus.empty()) { detected_gpus += ", "; }
		detected_gpus += prefix;
		++filteredDeviceCount;
	}

	if ( ! opt_cron) {
		fprintf(stdout, opt_config ? "Detected%s=%s\n" : "Detected%s=\"%s\"\n", opt_tag, detected_gpus.c_str());
	}
	if (opt_config) {
		fprintf(stdout, "NUM_DETECTED_%s=%d\n", opt_tag, filteredDeviceCount);
	}

	// print out static and/or dynamic info about detected GPU resources
	for (dev = 0; dev < deviceCount; dev++) {

		if( (!dwl.empty()) && std::find( dwl.begin(), dwl.end(), dev ) == dwl.end() ) {
			continue;
		}

		char prefix[100];
		sprintf(prefix,"%s%d",opt_pre,dev);
		KVP& props = dev_props.find(prefix)->second;

		if (opt_basic && getBasicProps) {
			int driverVersion=0;

			if (cudaDriverGetVersion) {
				cudaDriverGetVersion(&driverVersion);
				props["DriverVersion"] = Format("%d.%d", driverVersion/1000, driverVersion%100);
				props["MaxSupportedVersion"] = Format("%d", driverVersion);
			}

			if (dev < g_cl_cCuda) {
				cl_device_id did = cl_gpu_ids[g_cl_ixFirstCuda + dev];
				std::string fullver;
				if (CL_SUCCESS == oclGetInfo(did, CL_DEVICE_VERSION, fullver)) {
					size_t ix = fullver.find_first_of(' '); // skip OpenCL
					std::string vervend = fullver.substr(ix+1);
					ix = vervend.find_first_of(' '); 
					std::string ver = vervend.substr(0, ix); // split version from vendor info.
					props["OpenCLVersion"] = ver;
				}
			}

			BasicProps bp;
			if (cudaSuccess == getBasicProps(dev, &bp)) {
				props["DeviceName"] = Format("\"%s\"", bp.name.c_str());
				/*if (bp.hasUuid())*/ props["DeviceUuid"] = Format("\"%s\"", bp.printUuid());
				if (bp.pciId[0]) props["DevicePciBusId"] = Format("\"%s\"", bp.pciId);
				props["Capability"] = Format("%d.%d", bp.ccMajor, bp.ccMinor);
				props["ECCEnabled"] = bp.ECCEnabled ? "true" : "false";
				props["GlobalMemoryMb"] = Format("%.0f", bp.totalGlobalMem / (1024.*1024.));
				if (opt_extra) {
					props["ClockMhz"] = Format("%.2f", bp.clockRate * 1e-3f);
					props["ComputeUnits"] = Format("%u", bp.multiProcessorCount);
					if (bp.ccMajor <= 6) {
						props["CoresPerCU"] = Format("%u", ConvertSMVer2Cores(bp.ccMajor, bp.ccMinor));
					} else {
						// CoresPerCU not meaningful for Architecture 7 and later
					}
				}
			}
		} else if (opt_basic && ocl_handle) {
			cl_device_id did = cl_gpu_ids[dev];

			std::string val;
			if (CL_SUCCESS == oclGetInfo(did, CL_DEVICE_NAME, val)) {
				props["DeviceName"] = Format("\"%s\"", val.c_str());
			}

			unsigned long long mem_bytes = 0;
			if (CL_SUCCESS == oclGetInfo(did, CL_DEVICE_GLOBAL_MEM_SIZE, mem_bytes)) {
				props["GlobalMemoryMb"] = Format("%.0f", mem_bytes/(1024.*1024.));
			}
			int ecc_enabled = 0;
			if (CL_SUCCESS == oclGetInfo(did, CL_DEVICE_ERROR_CORRECTION_SUPPORT, ecc_enabled)) {
				props["ECCEnabled"] = ecc_enabled ? "true" : "false";
			}
			//printf("%sCapability=%d.%d\n", prefix, deviceProp.major, deviceProp.minor);

			if (CL_SUCCESS == oclGetInfo(did, CL_DEVICE_VERSION, val)) {
				size_t ix = val.find_first_of(' '); // skip OpenCL
				std::string vervend = val.substr(ix+1);
				ix = vervend.find_first_of(' '); 
				std::string ver = vervend.substr(0, ix); // split version from vendor info.
				props["OpenCLVersion"] = ver;
			}

			if (opt_extra) {

				unsigned int cus = 0;
				if (CL_SUCCESS == oclGetInfo(did, CL_DEVICE_MAX_COMPUTE_UNITS, cus)) {
					props["ComputeUnits"] = Format("%u", cus);
				}

				unsigned int clock_mhz;
				if (CL_SUCCESS == oclGetInfo(did, CL_DEVICE_MAX_CLOCK_FREQUENCY, clock_mhz)) {
					props["ClockMhz"] = Format("%u", clock_mhz);
				}

				//if (CL_SUCCESS == oclGetInfo(did, CL_DEVICE_VENDOR, val)) { props["Vendor"] = ver; }
			}
		}

	}

	// check for device homogeneity so we can print out simpler
	// config/device attributes 
	if (opt_basic) {
		KVP common;
		if ( ! opt_hetero && ! dev_props.empty()) {
			const KVP & dev0 = dev_props.begin()->second;
			for (KVP::const_iterator it = dev0.begin(); it != dev0.end(); ++it) {
				bool all_match = true;
				MKVP::const_iterator mit = dev_props.begin();
				while (++mit != dev_props.end()) {
					const KVP & devN = mit->second;
					KVP::const_iterator pair = devN.find(it->first);
					if (pair == devN.end() || pair->second != it->second) {
						all_match = false;
					}
					if ( ! all_match) break;
				}
				if (all_match) {
					common[it->first] = it->second;
				}
			}
		}

		// now print out device properties.
		if ( ! common.empty()) {
			for (KVP::const_iterator it = common.begin(); it != common.end(); ++it) {
				printf("%s%s=%s\n", opt_pre, it->first.c_str(), it->second.c_str());
			}
		}
		for (MKVP::const_iterator mit = dev_props.begin(); mit != dev_props.end(); ++mit) {
			for (KVP::const_iterator it = mit->second.begin(); it != mit->second.end(); ++it) {
				if (common.find(it->first) != common.end()) continue;
				printf("%s%s=%s\n", mit->first.c_str(), it->first.c_str(), it->second.c_str());
			}
		}
	}


	// Dynamic properties
	if ( !opt_dynamic ) return 0;

	// need nvml to do dynamic properties
	if ( !have_nvml ) return 1;

	// everything after here is dynamic properties
	for (dev = 0; dev < deviceCount; dev++) {
		if( (!dwl.empty()) && std::find( dwl.begin(), dwl.end(), dev ) == dwl.end() ) {
			continue;
		}

		// get a handle to this device
		nvmlDevice_t device;
		char prefix[100];
		sprintf(prefix,"%s%d",opt_pre,dev);
		KVP& props = dev_props.find(prefix)->second;
		std::string unquoted = props["DevicePciBusId"].substr( 1, props["DevicePciBusId"].length() - 2 );
		result = nvmlDeviceGetHandleByPciBusId( unquoted.c_str(), & device );
		if( result != NVML_SUCCESS ) {
			continue;
		}

		unsigned int tuint;
		result = nvmlDeviceGetFanSpeed(device,&tuint);
		if ( result == NVML_SUCCESS ) {
			printf("%sFanSpeedPct=%u\n",prefix,tuint);
		}

		result = nvmlDeviceGetPowerUsage(device,&tuint);
		if ( result == NVML_SUCCESS ) {
			printf("%sPowerUsage_mw=%u\n",prefix,tuint);
		}

		result = nvmlDeviceGetTemperature(device,NVML_TEMPERATURE_GPU,&tuint);
		if ( result == NVML_SUCCESS ) {
			printf("%sDieTempC=%u\n",prefix,tuint);
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

void usage(FILE* out, const char * argv0)
{
	fprintf(out, "Usage: %s [options]\n", argv0);
	fprintf(out, "Where options are:\n"
		"    -properties       Include basic GPU device properties useful for matchmaking\n"
		"    -extra            Include extra GPU device properties\n"
		"    -dynamic          Include dynamic GPU device properties\n"
		"    -mixed            Assume mixed GPU configuration\n"
		"    -device <N>       Include properties only for GPU device N\n"
		"    -tag <string>     use <string> as resource tag, default is GPUs\n"
		"    -prefix <string>  use <string> as property prefix, default is CUDA or OCL\n"
		"    -cuda             Detection via CUDA only, ignore OpenCL devices\n"
		"    -opencl           Prefer detection via OpenCL rather than CUDA\n"
		//"    -nvcuda           Use nvcuda rather than cudarl for detection\n"
		"    -simulate[:D[,N]] Simulate detection of N devices of type D\n"
		"           where D is 0 - GeForce GT 330, default N=1\n"
		"                      1 - GeForce GTX 480, default N=2\n"
		"    -config           Output in HTCondor config syntax\n"
		"    -cron             Output for use as a STARTD_CRON job, use with -dynamic\n"
		"    -help             Show this screen and exit\n"
		"    -verbose          Show detection progress\n"
		//"    -diagnostic       Show detection diagnostic information\n"
		"\n"
	);
}

