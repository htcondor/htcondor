#include <stdlib.h>

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
#include "opencl_header_doc.h"
#include "opencl_device_enumeration.h"

#include "print_error.h"

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
		if (! g_buffer) { print_error(MODE_ERROR, "ERROR: failed to allocate %d bytes\n", (int)(cb*2)); exit(-1); }
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
		if (! g_buffer) { print_error(MODE_ERROR, "ERROR: failed to allocate %d bytes\n", (int)(cb*2)); exit(-1); }
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

const char * ocl_value(int val) { static char ach[11]; snprintf(ach, sizeof(ach), "%d", val); return ach; }
const char * ocl_value(unsigned int val) { static char ach[11]; snprintf(ach, sizeof(ach), "%u", val); return ach; }
const char * ocl_value(long long val) { static char ach[22]; snprintf(ach, sizeof(ach), "%lld", val); return ach; }
const char * ocl_value(unsigned long long val) { static char ach[22]; snprintf(ach, sizeof(ach), "%llu", val); return ach; }
const char * ocl_value(double val) { static char ach[22]; snprintf(ach, sizeof(ach), "%g", val); return ach; }

template <class t>
clReturn oclGetInfo(cl_device_id did, cl_e_device_info eInfo, t & val) {
	clReturn clr = ocl.GetDeviceInfo(did, eInfo, sizeof(val), &val, NULL);
	if (clr == CL_SUCCESS) {
		print_error(MODE_VERBOSE, "\t\t%s: %s\n", ocl_name(eInfo), ocl_value(val));
	}
	return clr;
}

template clReturn oclGetInfo<int>(cl_device_id did, cl_e_device_info eInfo, int & val);
template clReturn oclGetInfo<unsigned int>(cl_device_id did, cl_e_device_info eInfo, unsigned int & val);
template clReturn oclGetInfo<unsigned long long>(cl_device_id did, cl_e_device_info eInfo, unsigned long long & val);

static int ocl_device_count = 0;
static int ocl_was_initialized = 0;
static std::vector<cl_platform_id> cl_platforms;
static cl_platform_id g_plidCuda = NULL;
static cl_platform_id g_plidHip = NULL;
static std::string g_cudaOCLVersion;

int g_cl_cCuda = 0;
int g_cl_ixFirstCuda = 0;
int g_cl_cHip = 0;
int g_cl_ixFirstHip =0 ;
std::vector<cl_device_id> cl_gpu_ids;

int ocl_Init(void) {

	print_error(MODE_DIAGNOSTIC_MSG, "diag: ocl_Init()\n");
	if (ocl_was_initialized) {
		return 0;
	}
	ocl_was_initialized = 1;

	if (! ocl.GetPlatformIDs) {
		return -1;
	}

	unsigned int cPlatforms = 0;
	clReturn clr = ocl.GetPlatformIDs(0, NULL, &cPlatforms);
	if (clr != CL_SUCCESS) {
		// if the error code is just 'no devices', we don't really want to report that as an error
		int mode = ((clr == CL_DEVICE_NOT_FOUND) || (clr == CL_PLATFORM_NOT_FOUND_KHR)) ? MODE_DIAGNOSTIC_MSG : MODE_ERROR;
		print_error(mode, "ocl.GetPlatformIDs returned error=%d and %d platforms\n", clr, cPlatforms);
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
		if (val == "AMD Accelerated Parallel Processing") g_plidHip = plid;
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
				if (plid == g_plidHip) {
					// keep track of which of the opencl GPUS are also HIP
					g_cl_ixFirstHip = ixFirst;
					g_cl_cHip = cDevs;
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

clReturn OCL_CALL ocl_GetDeviceCount(int * pcount) {
	ocl_Init();
	*pcount = ocl_device_count;
	return CL_SUCCESS;
}

dlopen_return_t
loadOpenCLLibrary() {
#if       defined(WIN32)
	const char * opencl_library = "OpenCL.dll";
#else  /* defined(WIN32) */
	const char * opencl_library = "libOpenCL.so";
#endif /* defined(WIN32) */

	dlopen_return_t opencl_handle = dlopen( opencl_library, RTLD_LAZY );
	if(! opencl_handle) {
		print_error(MODE_DIAGNOSTIC_MSG, "Can't open library '%s': '%s'\n", opencl_library, dlerror());
	}

	dlerror(); // reset error
	return opencl_handle;
}

dlopen_return_t
setOCLFunctionPointers() {
	dlopen_return_t ocl_handle = loadOpenCLLibrary();
	if(! ocl_handle) { return NULL; }

	ocl.GetPlatformIDs = (clGetPlatformIDs_t) dlsym(ocl_handle, "clGetPlatformIDs");
	ocl.GetPlatformInfo = (clGetPlatformInfo_t) dlsym(ocl_handle, "clGetPlatformInfo");
	ocl.GetDeviceIDs = (clGetDeviceIDs_t) dlsym(ocl_handle, "clGetDeviceIDs");
	ocl.GetDeviceInfo = (clGetDeviceInfo_t) dlsym(ocl_handle, "clGetDeviceInfo");

	return ocl_handle;
}
