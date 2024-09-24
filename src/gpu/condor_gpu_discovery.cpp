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
#include <list>
#include <map>
#include <set>
#include <algorithm>

#include <time.h>
#include <stdarg.h> // for va_start

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
#include "opencl_header_doc.h"
#include "hip_header_doc.h"
#include "cuda_device_enumeration.h"
#include "opencl_device_enumeration.h"
#include "hip_device_enumeration.h"

#include "print_error.h"

// Function taken from helper_cuda.h in the CUDA SDK
// https://github.com/NVIDIA/cuda-samples/blob/master/Common/helper_cuda.h
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
		{ 0x80, 64 }, // Ampere Generation (SM 8.0) GA100 class
		{ 0x86, 128}, // Ampere Generation (SM 8.6) GA10x class  (GeForce  RTX 3060)
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

	if (! temp) { max_temp = 4096; temp = (char*)malloc(max_temp+1); }

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

void usage(FILE* output, const char* argv0);

// trim leading and trailing whitespace from the input string and return as a std::string.
static std::string trim(const char* str) {
	while (*str && isspace(*str)) ++str; // trim leading
	std::string tmp(str);
	int end = (int)tmp.length() - 1;
	while (end >= 0 && isspace(tmp[end])) { --end; }
	return tmp.substr(0, end + 1);
}

// returns true if pre is non-empty and str is the same as pre up to pre.size()
static bool starts_with(const std::string& str, const std::string& pre) {
	size_t cp = pre.size();
	if (cp <= 0)
		return false;

	size_t cs = str.size();
	if (cs < cp)
		return false;

	for (size_t ix = 0; ix < cp; ++ix) {
		if (str[ix] != pre[ix])
			return false;
	}
	return true;
}

static bool uuid_match(const std::string & item, const std::string & uuid) {
	if (item.find("GPU-")) {
		return item.substr(4) == uuid;
	}
	return item == uuid;
}

// split the input list on , and and put the resulting items into the whitelist after trimming leading and trailing spaces
bool addItemsToSet( const char * list, std::set<std::string> & whitelist ) {
	char * tokenizer = strdup( list );
	if( tokenizer == NULL ) {
		// Deliberately unparseable.
		fprintf( stderr, "Error: device list too long\n" );
		return false;
	}
	char * next = strtok( tokenizer, "," );
	for( ; next != NULL; next = strtok( NULL, "," ) ) {
		std::string item(trim(next));
		if ( ! item.empty()) { whitelist.insert(item); }
	}
	free( tokenizer );
	return true;
}

class DeviceWhitelist {
public:
	DeviceWhitelist() {}
	DeviceWhitelist(std::set<std::string> & combined_list) {
		for (auto item : combined_list) {
			// scan the item to see if it matches the pattern ^[A-Za-z]*[0-9]+$
			// if so, capture the numeric part as an int, otherwise treat is a GPUID rather than as an index
			const char * p = item.c_str();
			while (isalpha(*p)) { ++p; }
			if (isdigit(*p)) {
				char * endp = nullptr;
				int ix = strtol(p, &endp, 10);
				if (endp && *endp == 0) {
					by_index.insert(ix);
					continue;
				}
			}
			// not an index type entry, assume it's a prefix type entry
			// TODO: distinguish between uuids and prefixes
			by_prefix.insert(item);
		}
	}

	bool empty() { return by_index.empty() && by_prefix.empty() && by_uuid.empty(); }

	bool is_whitelisted_index(int index) {
		if (by_index.empty()) return true;
		return by_index.count(index) > 0;
	}

	bool is_whitelisted(const std::string & GPUID, int index) {
		bool match = true; // assume empty lists
		for (auto pre : by_prefix) {
			if (starts_with(GPUID, pre)) return true;
			match = false;
		}
		for (auto uuid : by_uuid) {
			if (uuid_match(GPUID, uuid)) return true;
			match = false;
		}
		if (by_index.empty()) {
			return match;
		} else {
			return by_index.count(index) > 0;
		}
	}

	// use to early out iteration,
	// we can only do this when the whitelist consists entirely of device indexes
	bool ignore_device(int index) {
		if (by_prefix.empty() && by_uuid.empty()) {
			return ! is_whitelisted_index(index);
		}
		return false;
	}

	// use this to add a MIG-GPU-<uuid> prefix to the whitelist
	void add_prefix(const std::string & GPUID) {
		by_prefix.insert(GPUID);
	}
	// use this to add MIG parent GPU uuids into the whitelist
	void add_uuid(const std::string & GPUID) {
		by_uuid.insert(GPUID);
	}

private:
	std::set<int> by_index;
	std::set<std::string> by_prefix;
	std::set<std::string> by_uuid;
};

std::string
constructGPUID( const char * opt_pre, int dev, int opt_uuid, bool has_uuid, int opt_short_uuid, std::vector< BasicProps > & enumeratedDevices ) {
	// Determine the GPU ID.
	std::string gpuID = Format( "%s%i", opt_pre, dev );

	// The -uuid and -short-uuid flags don't imply -properties.
	if( opt_uuid && has_uuid && !enumeratedDevices[dev].uuid.empty()) {
		gpuID = gpuIDFromUUID( enumeratedDevices[dev].uuid, opt_short_uuid );
	}

	return gpuID;
}

typedef std::map<std::string, std::string> KVP;
typedef std::map<std::string, KVP> MKVP;

void
setPropertiesFromDynamicProps( KVP & props, nvmlDevice_t device ) {

	unsigned int tuint;
	nvmlReturn_t result = nvmlDeviceGetFanSpeed(device,&tuint);
	if ( result == NVML_SUCCESS ) {
		props["FanSpeedPct"] = Format("%u",tuint);
	}

	result = nvmlDeviceGetPowerUsage(device,&tuint);
	if ( result == NVML_SUCCESS ) {
		props["PowerUsage_mw"] = Format("%u",tuint);
	}

	result = nvmlDeviceGetTemperature(device,NVML_TEMPERATURE_GPU,&tuint);
	if ( result == NVML_SUCCESS ) {
		props["DieTempC"] = Format("%u",tuint);
	}

	unsigned long long eccCounts;
	result = nvmlDeviceGetTotalEccErrors(device,NVML_SINGLE_BIT_ECC,NVML_VOLATILE_ECC,&eccCounts);
	if ( result == NVML_SUCCESS ) {
		props["EccErrorsSingleBit"] = Format("%llu",eccCounts);
	}
	result = nvmlDeviceGetTotalEccErrors(device,NVML_DOUBLE_BIT_ECC,NVML_VOLATILE_ECC,&eccCounts);
	if ( result == NVML_SUCCESS ) {
		props["EccErrorsDoubleBit"] = Format("%llu",eccCounts);
	}
}

void
setPropertiesFromBasicProps( KVP & props, const BasicProps & bp, int opt_extra ) {
	props["DeviceUuid"] = Format("\"%s\"", bp.uuid.c_str());
	if(! bp.name.empty()) { props["DeviceName"] = Format("\"%s\"", bp.name.c_str()); }
	if( bp.pciId[0] ) { props["DevicePciBusId"] = Format("\"%s\"", bp.pciId); }
	if( bp.ccMajor != -1 && bp.ccMinor != -1 ) { props["Capability"] = Format("%d.%d", bp.ccMajor, bp.ccMinor); }
	if( bp.ECCEnabled != -1 ) { props["ECCEnabled"] = bp.ECCEnabled ? "true" : "false"; }
	if( bp.totalGlobalMem != (size_t)-1 ) { props["GlobalMemoryMb"] = Format("%.0f", bp.totalGlobalMem / (1024.*1024.)); }
	if( bp.xNACK != -1 ) { props["xNACK"] = Format("%d", bp.xNACK); }
	if( bp.warpSize != -1 ) { props["WarpSize"] = Format("%d", bp.warpSize); }

	if( opt_extra ) {
		if( bp.clockRate != -1 ) { props["ClockMhz"] = Format("%.2f", bp.clockRate * 1e-3f); }
		if( bp.multiProcessorCount > 0 ) {
			props["ComputeUnits"] = Format("%u", bp.multiProcessorCount);
		}
		if( bp.ccMajor != -1 && bp.ccMinor != -1 ) {
			props["CoresPerCU"] = Format("%u", ConvertSMVer2Cores(bp.ccMajor, bp.ccMinor));
		}
	}

	if( bp.driverVersion != -1 ) {
		props["DriverVersion"] = Format("%d.%d", bp.driverVersion/1000, bp.driverVersion%100);
		props["MaxSupportedVersion"] = Format("%d", bp.driverVersion);
	}
}


int
main( int argc, const char** argv)
{
	int deviceCount = 0;
	int opt_basic = 0; // publish basic GPU properties
	int opt_extra = 0; // publish extra GPU properties.
	int opt_dynamic = 0; // publish dynamic GPU properties
	int opt_cron = 0;  // publish in a form usable by STARTD_CRON
	int opt_hetero = 0;  // don't assume properties are homogeneous
	int opt_simulate = 0; // pretend to detect GPUs
	int opt_config = 0;
	int opt_nested = 1;  // publish properties using nested ads
	int opt_uuid = -1;   // publish DetectedGPUs as a list of GPU-<uuid> rather than than CUDA<N>
	int opt_short_uuid = -1; // use shortened uuids
	std::set<std::string> whitelist; // combined whitelist from CUDA_VISIBLE_DEVICES, GPU_DEVICE_ORDINAL and/or -devices arg
	bool opt_nvcuda = false; // force use of nvcuda rather than cudart
	bool opt_cudart = false; // force use of use cudart rather than nvcuda
	bool clean_environ = true; // clean the environment before enumerating
	int opt_opencl = 0; // prefer opencl detection
	int opt_hip = 0; // prefer hip detection
	int opt_dash_cuda = 0; // prefer cuda detection
	int opt_cuda_only = 0; // require cuda detection
	int detect_order = 0;  // counter used to set detection order, of 0 use legacy detection order

	const char * opt_tag = "GPUs";
	const char * opt_pre = "CUDA";
	const char * opt_pre_arg = NULL;
	int opt_repeat = 0;
	int opt_divide = 0;
	int opt_packed = 0;
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
		if(! addItemsToSet( cvd, whitelist )) { return 1; }
	}

	// Ditto for GPU_DEVICE_ORDINAL.  If we ever handle dissimilar GPUs
	// properly (right now, the negotiator can't tell them apart), we'll
	// have to change this program to filter for specific types of GPUs.
	char * gdo = getenv( "GPU_DEVICE_ORDINAL" );
	if( gdo != NULL ) {
		if(! addItemsToSet( gdo, whitelist )) { return 1; }
	}

	//
	// Argument parsing.
	//
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
		else if (is_dash_arg_prefix(argv[i], "nested", 2)) {
			opt_nested = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "not-nested", 6) || is_dash_arg_prefix(argv[i], "no-nested", 5)) {
			opt_nested = 0;
		}
		else if (is_dash_arg_prefix(argv[i], "dynamic", 3)) {
			// We need the basic properties in order to determine the
			// dynamic ones (most noticeably, the PCI bus ID).
			opt_basic = 1;
			opt_dynamic = 1;

			// Even if it made sense to ask NVML about OpenCL devices, we
			// don't get the PCI bus ID from OpenCL, so we can't determine
			// which device the dynamic properties apply to.
			opt_opencl = 0;
		}
		else if (is_dash_arg_prefix(argv[i], "cron", 4)) {
			opt_cron = 1;
		} else if (is_dash_arg_prefix(argv[i], "mixed", 3) || is_dash_arg_prefix(argv[i], "hetero", 3)) {
			opt_hetero = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "opencl", -1)) {
			opt_opencl = ++detect_order;

			// See comment for the -dynamic flag.
			opt_dynamic = 0;
		}
		else if (is_dash_arg_prefix(argv[i], "cuda", -1)) {
			opt_dash_cuda = ++detect_order;
		}
		else if (is_dash_arg_prefix(argv[i], "hip", -1)) {
			opt_hip = ++detect_order;
		}
		else if (is_dash_arg_prefix(argv[i], "verbose", 4)) {
			g_verbose = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "diagnostic", 4)) {
			g_diagnostic = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "repeat", -1)) {
			if (! argv[i+1] || '-' == *argv[i+1]) {
			    // This preserves the value from -divide.
			    if( opt_repeat == 0 ) { opt_repeat = 2; }
			} else {
				opt_repeat = atoi(argv[++i]);
			}
            opt_divide = 0;
		}
		else if (is_dash_arg_prefix(argv[i], "divide", -1)) {
			if (! argv[i+1] || '-' == *argv[i+1]) {
			    // This preserves the setting from -repeat.
			    if( opt_repeat == 0 ) { opt_repeat = 2; }
			} else {
				opt_repeat = atoi(argv[++i]);
			}
			opt_divide = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "packed", -1)) {
			opt_packed = 1;
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
		else if (is_dash_arg_prefix(argv[i], "by-index", 4)) {
			opt_short_uuid = opt_uuid = 0;
		}
		else if (is_dash_arg_prefix(argv[i], "uuid", 2)) {
			opt_uuid = 1;
			opt_short_uuid = 0;
		}
		else if (is_dash_arg_prefix(argv[i], "short-uuid", -1)) {
			opt_uuid = 1;
			opt_short_uuid = 1;
		}
		else if (is_dash_arg_prefix(argv[i], "prefix", 3)) {
			if (argv[i+1]) {
				opt_pre_arg = argv[++i];
				if (strlen(opt_pre_arg) > 80) {
						// Deliberately unparseable.
						fprintf (stderr, "Error: -prefix should be less than 80 characters\n");
						return 1;
				}
			}
		}
		else if (is_dash_arg_prefix(argv[i], "device", 3)) {
			if (! argv[i+1] || '-' == *argv[i+1]) {
				// Deliberately unparseable.
				fprintf (stderr, "Error: -device requires an argument\n");
				usage(stderr, argv[0]);
				return 1;
			}
			addItemsToSet(argv[++i], whitelist);
		}
		else if (is_dash_arg_colon_prefix(argv[i], "simulate", &pcolon, 3)) {
			opt_simulate = 1;
			if ( ! setupSimulatedDevices(pcolon ? pcolon+1 : nullptr)) {
				// print an error message that can't be parsed as classad
				fprintf(stderr, "Error: simulation must be in range of 0-%d\r\n", sim_index_max);
				usage(stderr, argv[0]);
				return 1;
			}
		}
		else if (is_dash_arg_prefix(argv[i], "nvcuda",-1)) {
			// use nvcuda instead of cudart (option is for testing)
			opt_nvcuda = true;
		}
		else if (is_dash_arg_prefix(argv[i], "cudart", 6)) {
			// use cudart instead of nvcuda (option is for testing)
			opt_cudart = true;
		}
		else if (is_dash_arg_prefix(argv[i], "dirty-environment", 4)) {
			clean_environ = false;
		}
		else {
			fprintf(stderr, "option %s is not valid\n", argv[i]);
			usage(stderr, argv[0]);
			return 1;
		}
	}

	if (clean_environ) {
		print_error(MODE_DIAGNOSTIC_MSG, "diag: clearing environment before device enumeration\n");
	#if defined(WINDOWS)
		_putenv( "CUDA_VISIBLE_DEVICES=" );
		_putenv( "GPU_DEVICE_ORDINAL=" );
		_putenv( "HIP_VISIBLE_DEVICES=" );
		_putenv( "ROCR_VISIBLE_DEVICES=" );
	#else
		unsetenv( "CUDA_VISIBLE_DEVICES" );
		unsetenv( "GPU_DEVICE_ORDINAL" );
		unsetenv( "HIP_VISIBLE_DEVICES" );
		unsetenv( "ROCR_VISIBLE_DEVICES" );
	#endif
	}

	// if we got the -cuda flag, and not also -opencl or -hip
	// treat that as -cuda-only, otherwise the values of the flags indicate
	// preference
	if (opt_dash_cuda && ! opt_opencl && ! opt_hip) {
		opt_cuda_only = true;
	}

	DeviceWhitelist dwl(whitelist);

	//
	// Load and prepare libraries.
	//
	dlopen_return_t cuda_handle = NULL;
	dlopen_return_t nvml_handle = NULL;
	dlopen_return_t ocl_handle = NULL;
	dlopen_return_t hip_handle = NULL;
	bool canEnumerateNVMLDevices = false;

	if( opt_simulate ) {
		setSimulatedCUDAFunctionPointers();
		canEnumerateNVMLDevices = setSimulatedNVMLFunctionPointers();
		opt_cuda_only = true;
		if( cuDeviceGetCount( & deviceCount ) != cudaSuccess ) {
			deviceCount = 0;
		}
	} else if (0 == detect_order || opt_dash_cuda) { // default detection order or cuda detection enabled
		cuda_handle = setCUDAFunctionPointers( opt_nvcuda, opt_cudart, false );
		if( cuda_handle && !opt_cudart ) {
			if( cuInit ) {
				CUresult r = cuInit(0);
				if( r != CUDA_SUCCESS ) {
					if( r == CUDA_ERROR_NO_DEVICE ) {
						print_error(MODE_DIAGNOSTIC_MSG, "diag: cuInit found no CUDA-capable devices. code=%d\n", r);
					} else {
						print_error(MODE_ERROR, "Error: cuInit returned %d\n", r);
					}

					dlclose( cuda_handle );
					cuda_handle = NULL;
					cuDeviceGetCount = NULL; // no longer safe to call this.
				}
			}
		}

		nvml_handle = setNVMLFunctionPointers();
		if(! nvml_handle) {
			// this is a failure if -dynamic is set, but not otherwise
			print_error(opt_dynamic ? MODE_ERROR : MODE_DIAGNOSTIC_MSG,
				"# Unable to load NVML; will not discover dynamic properties or MIG instances.\n" );
		} else {
			nvmlReturn_t r = nvmlInit();
			if( r != NVML_SUCCESS ) {
				print_error(MODE_ERROR, "Warning: nvmlInit() failed with error %d; will not discover dynamic properties or MIG instances.\n", r );
				dlclose( nvml_handle );
				nvml_handle = NULL;
			}
		}
		canEnumerateNVMLDevices = nvml_handle != NULL;

		// check if there are any cuda devices.
		if( cuDeviceGetCount && cuDeviceGetCount( & deviceCount ) != cudaSuccess ) {
			deviceCount = 0;
		}
		// if we found any cuda devices and neither -opencl nor -hip was not passed, do only cuda detection
		if (deviceCount != 0 && ! opt_hip) {
			opt_cuda_only = true;
		}
	}

	//
	// Determine if we can find any devices before proceeding. we already checked for CUDA devices
	// We assume here that at least one CUDA device exists if any MIG
	// devices exist.

	if( ! opt_cuda_only && (opt_hip || (deviceCount == 0 && ! opt_opencl)) ) {
		int hipDeviceCount = 0;
		hip_handle = setHIPFunctionPointers();
		if (hip_handle) {
			hip_GetDeviceCount( & hipDeviceCount );
			deviceCount+=hipDeviceCount;
			opt_pre = "GPU";
		}
	}
	// we can't do both cuda/hip and opencl detection
	// so we only do opencl if we have found no devices
	// and we have not been asked to do HIP detection
	if( ! opt_cuda_only && deviceCount == 0 && (opt_opencl || ! opt_hip) ) {
		ocl_handle = setOCLFunctionPointers();
		if (ocl_handle) {
			ocl_GetDeviceCount( & deviceCount );
			opt_pre = "OCL";
			// opencl devices have no uuid
			if (opt_uuid < 0) { opt_short_uuid = opt_uuid = 0; }
		}
	}

	if( deviceCount == 0 ) {
		if (! opt_cron) fprintf(stdout, "Detected%s=0\n", opt_tag);
		if (opt_config) fprintf(stdout, "NUM_DETECTED_%s=0\n", opt_tag);
		return 0;
	}


	// Set the device prefix.
	if( opt_pre_arg ) { opt_pre = opt_pre_arg; }


	// We're doing it this way so that we can share the device-enumeration
	// logic between condor_gpu_discovery and condor_gpu_utilization.
	std::set< std::string > migDevices;
	std::map< std::string, int > cudaDevices;
	std::vector< BasicProps > nvmlDevices;
	std::vector< BasicProps > enumeratedDevices;

	bool enumeratedCUDADevices = false;
	if(! opt_opencl) {
		if (cuDeviceGetCount && enumerateCUDADevices(enumeratedDevices)) {
			enumeratedCUDADevices = true;
		}
	}

	if(! enumeratedCUDADevices) {
		if( opt_cuda_only ) {
			const char * problem = "Failed to enumerate GPU devices";
			fprintf( stderr, "# %s, aborting.\n", problem );
			fprintf( stdout, "condor_gpu_discovery_error = \"%s\"\n", problem );
			return 1;
		}
	} else {

		//
		// We have to report NVML devices, because enabling MIG on any
		// GPU prevents CUDA from reporting any MIG-capable GPU, even
		// if we wouldn't otherwise report it because it hasn't enabled
		// MIG.  Since don't know if that applies to non-MIG-capable GPUs,
		// we will record all the UUIDs we found via CUDA and skip them when
		// reporting the devices we found via NVML.
		//
		bool shouldEnumerateNVMLDevices = true;
		for (dev = 0; dev < (int)enumeratedDevices.size(); ++dev) {
			if (enumeratedDevices[dev].uuid.empty()) {
				if (opt_uuid) {
					const char * problem = "Not enumerating NVML devices because a CUDA device has no UUID.  This usually means you should upgrade your CUDA libaries.";
					fprintf( stderr, "# %s\n", problem );
					// print this warning to stdout as a key=value pair so it parses as a classad attribute
					fprintf(stdout, "condor_gpu_discovery_error = \"%s\"\n", problem);
				}
				// if we are defaulting to UUID, but the device has no UUID, switch the default to -by-index
				if (opt_uuid < 0) {
					opt_short_uuid = opt_uuid = 0;
				}
				shouldEnumerateNVMLDevices = false;
			} else {
				// for later NVML enumeration, we need a map of cuda device uuid to index
				cudaDevices[enumeratedDevices[dev].uuid] = dev;
			}
		}

		if( shouldEnumerateNVMLDevices && canEnumerateNVMLDevices ) {
			nvmlReturn_t r = enumerateNVMLDevices(nvmlDevices);
			if(r != NVML_SUCCESS) {
				const char * problem = "Failed to enumerate MIG devices";
				fprintf( stderr, "# %s (%d: %s)\n", problem, r, nvmlErrorString(r) );
				fprintf( stdout, "condor_gpu_discovery_error = \"%s\"\n", problem );
				return 1;
			}

			if( NVML_SUCCESS != getMIGParentDeviceUUIDs( migDevices ) ) {
				const char * problem = "Failed to enumerate MIG parent devices";
				fprintf( stderr, "# %s (%d: %s)\n", problem, r, nvmlErrorString(r) );
				fprintf( stdout, "condor_gpu_discovery_error = \"%s\"\n", problem );
				return 1;
			}

			for( auto parentUUID : migDevices ) {
				print_error(MODE_DIAGNOSTIC_MSG, "diag: MIG parent uuid: %s\n", parentUUID.c_str());
			}
		}

		if(opt_hip){
			enumerateHIPDevices(enumeratedDevices);
		}
	}

	//
	// we will build an array of key=value pairs to hold properties of each device
	// we build the whole list before showing anything so we can decide whether
	// we are looking at a homogenous or heterogeneous pool
	//
	MKVP dev_props;
	KVP common; // props that have a common value for all GPUS will be set in this collection

	std::string detected_gpus;
	int filteredDeviceCount = 0;
	for( dev = 0; dev < deviceCount; ++dev ) {

		bool has_uuid = dev < (int)enumeratedDevices.size() && ! enumeratedDevices[dev].uuid.empty();

		// Skip devices not on the whitelist, this skips nothing when the whitelist is not index based
		if( dwl.ignore_device(dev) ) {
			print_error(MODE_DIAGNOSTIC_MSG, "diag: skipping %d uuid=%s during CUDA enumeration because of index\n",
				dev, !has_uuid ? "<none>" : enumeratedDevices[dev].uuid.c_str());
			continue;
		}

		// check device against the uuid whitelist
		if (has_uuid) {
			std::string id = gpuIDFromUUID( enumeratedDevices[dev].uuid, false );
			if ( ! dwl.is_whitelisted(id, dev)) {
				print_error(MODE_DIAGNOSTIC_MSG, "diag: skipping %d GPUid=%s during CUDA enumeration because of whitelist\n",
					dev, id.c_str());
				continue;
			}
		}

		std::string gpuID = constructGPUID(opt_pre, dev, opt_uuid, has_uuid, opt_short_uuid, enumeratedDevices);

		// Skip devices which have MIG instances associated with them.
		if(!migDevices.empty() && has_uuid) {
			// The "UUIDs" from NVML have "GPU-" as a prefix.
			std::string id = "GPU-" + enumeratedDevices[dev].uuid;
			if( migDevices.find( id ) != migDevices.end() ) {
				print_error(MODE_DIAGNOSTIC_MSG, "diag: skipping %d GPUid=%s during CUDA enumeration because it is MIG parent\n",
					dev, id.c_str());
				if ( ! dwl.empty() && dwl.is_whitelisted_index(dev)) {
					// if there is a whitelist, and this is a GPU whitelisted by index
					// add it to the whitelist by uuid and by prefix
					// prefix is so that MIG devices of type MIG-GPU-<uuid>/<x>/<y> are whitelisted
					std::string migpre = "MIG-" + id;
					print_error(MODE_DIAGNOSTIC_MSG, "diag: adding %s as prefix to whitelist\n", migpre.c_str());
					dwl.add_prefix(migpre);
					dwl.add_uuid(enumeratedDevices[dev].uuid);
				}
				continue;
			}
		}

		if (! detected_gpus.empty()) { detected_gpus += ", "; }
		detected_gpus += gpuID;
		++filteredDeviceCount;

		if(! opt_basic) { continue; }

		dev_props[gpuID].clear();

		KVP & props = dev_props.find(gpuID)->second;

		if(! enumeratedDevices.empty() && dev < (int)enumeratedDevices.size() ) {
			// Report CUDA properties.
			BasicProps bp = enumeratedDevices[dev];
			setPropertiesFromBasicProps( props, bp, opt_extra );

			// Not sure what this does, actually.
			if( dev < g_cl_cCuda  && dev < g_cl_cHip) {
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
		} else {
			// Report OpenCL properties.
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
			} /* end if reporting extra OpenCL properties */
		} /* end if reporting OpenCL properties */
	} /* end enumerated device loop */

	//
	// Construct the properties for NVML devices.  This includes MIG devices
	// and any non-MIG NVML device we found that isn't either (a) a parent of
	// a MIG device or (b) a CUDA device we've already included.
	//
	for( const BasicProps & bp : nvmlDevices ) {

		// NVML "uuid" field is not actually a uuid.  depending on the driver version it may be
		// GPU-<uuid>,  MIG-GPU-<uuid>/<migid>/<cid>, or MIG-<uuid>
		// the first type of "uuid" should be a match for a GPU we already enumerated via CUDA
		// the second two are various forms of MIG devices,
		// the first form for driver version < 470, the second form for driver version >= 470
		std::string uuid(bp.uuid);
		if (uuid.find("MIG-") == 0) { uuid = uuid.substr(4); }
		if (uuid.find("GPU-") == 0) { uuid = uuid.substr(4); }
		// skip devices we already enumerated as cuda devices (even if we skipped them because of a whitelist)
		if( cudaDevices.find( uuid ) != cudaDevices.end() ) {
			print_error(MODE_DIAGNOSTIC_MSG, "diag: skipping uuid=%s during nvml enumeration because it matches CUDA%d\n",
				bp.uuid.c_str(), cudaDevices[uuid]);
			continue;
		}
		// TODO: track parent UUID of MIG devices and whitelist them if the parent is whitelisted.
		// this is needed for MIG devices that have their own uuid (driver 470 and later)
		// TODO: handle the case where whitelist is of the form <X>:<Y> where X is GPU index and Y is MIG index
		if ( ! dwl.is_whitelisted(bp.uuid, -1)) {
			print_error(MODE_DIAGNOSTIC_MSG, "diag: skipping uuid=%s during nvml enumeration because of whitelist\n", bp.uuid.c_str());
			continue;
		}

		// skip devices known to be parents of MIGs since these devices can't actually be used
		if( migDevices.find( bp.uuid ) != migDevices.end() ) {
			print_error(MODE_DIAGNOSTIC_MSG, "diag: skipping MIG parent %s during nvml enumeration\n", bp.uuid.c_str());
			continue;
		}

		print_error(MODE_DIAGNOSTIC_MSG, "diag: adding device uuid=%s which was not found during CUDA enum\n", bp.uuid.c_str());

		std::string gpuID = gpuIDFromUUID( bp.uuid, opt_short_uuid );
		if (! detected_gpus.empty()) { detected_gpus += ", "; }
		detected_gpus += gpuID;
		++filteredDeviceCount;

		dev_props[gpuID].clear();
		if(! opt_basic) { continue; }

		KVP & props = dev_props.find(gpuID)->second;
		setPropertiesFromBasicProps( props, bp, opt_extra );
	}

	// check for device homogeneity so we can print out simpler
	// config/device attributes. we do this before we set dynamic props
	// so that dynamic props never end up in the common collection
	if (opt_basic) {
		if (! opt_hetero && ! dev_props.empty()) {
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
					if (! all_match) break;
				}
				if (all_match) {
					common[it->first] = it->second;
				}
			}
		}
	}

	//
	// Dynamic properties
	//
	if (opt_dynamic) {

		// Dynamic properties for CUDA devices
		for (dev = 0; dev < deviceCount; ++dev) {
			if( dwl.ignore_device(dev) ) {
				continue;
			}

			bool has_uuid = dev < (int)enumeratedDevices.size() && ! enumeratedDevices[dev].uuid.empty();

			// in case we have a whitelist of GPUid's or uuids, construct the full uuid
			// and check it against the whitelist
			if( opt_uuid && has_uuid ) {
				std::string id = gpuIDFromUUID( enumeratedDevices[dev].uuid, false );
				if ( ! dwl.is_whitelisted(id, dev)) {
					continue;
				}
			}

			// skip devices that have active MIG children
			if (has_uuid) {
				// The "UUIDs" from NVML have "GPU-" as a prefix.
				std::string id = "GPU-" + enumeratedDevices[dev].uuid;
				if( migDevices.find( id ) != migDevices.end() ) {
					continue;
				}
			}

			// Determine the GPU ID.
			std::string gpuID = constructGPUID(opt_pre, dev, opt_uuid, has_uuid, opt_short_uuid, enumeratedDevices);

			nvmlDevice_t device;
			if (NVML_SUCCESS != findNVMLDeviceHandle(enumeratedDevices[dev].uuid, & device)) {
				continue;
			}

			KVP & props = dev_props.find(gpuID)->second;
			setPropertiesFromDynamicProps(props, device);
		}

		// Dynamic properties for NVML devices
		for (auto bp : nvmlDevices) {
			std::string uuid(bp.uuid);
			if (uuid.find("MIG-") == 0) { uuid = uuid.substr(4); }
			if (uuid.find("GPU-") == 0) { uuid = uuid.substr(4); }
			if (cudaDevices.find(uuid) != cudaDevices.end()) {
				// fprintf( stderr, "[dynamic NVML properties] Skipping CUDA device %s.\n", bp.uuid.c_str() );
				continue;
			}

			nvmlDevice_t device;
			if (NVML_SUCCESS != findNVMLDeviceHandle(bp.uuid, & device)) {
				// fprintf( stderr, "[dynamic NVML properties] Skipping NVML device %s because I can't find its handle.\n", bp.uuid.c_str() );
				continue;
			}

			std::string gpuID = gpuIDFromUUID(bp.uuid, opt_short_uuid);
			auto gt = dev_props.find(gpuID);
			if (gt != dev_props.end()) {
				KVP & props = gt->second;
				setPropertiesFromDynamicProps(props, device);
			}
		}
	}

	//
	// If the user requested it, adjust detected_gpus.
	//
	if( opt_repeat ) {
		std::string original_detected_gpus = detected_gpus;

		if( opt_packed == 0 ) {
			for( int i = 1; i < opt_repeat; ++i ) {
				detected_gpus += ", " + original_detected_gpus;
			}
		} else {
			// We could try to do this via the data structures, but we don't
			// actually record the UUIDs anywhere other than a data structure
			// keyed by the UUID...
			detected_gpus.clear();

			size_t left = 0, delim = 0;
			do {
				delim = original_detected_gpus.find( ", ", left );

				// If delim is std::string::npos, this code will break if
				// original_detected_gpus is almost 2^64 characters long.
				// I'm not worried.
				std::string id = original_detected_gpus.substr( left, delim - left );
				if(! detected_gpus.empty()) { detected_gpus += ", "; }
				detected_gpus += id;
				for( int i = 1; i < opt_repeat; ++i ) {
					detected_gpus += ", " + id;
				}

				if( delim == std::string::npos ) { break; }
				left = delim + 2;
			} while( true );
		}

		// ... and the amount of reported memory per device.
		if( opt_divide ) {
			for (MKVP::iterator mit = dev_props.begin(); mit != dev_props.end(); ++mit) {
				if (mit->second.empty()) continue;

				int global_memory = 0;
				for (KVP::iterator it = mit->second.begin(); it != mit->second.end(); ++it) {
					if( it->first == "GlobalMemoryMb" ) {
						global_memory = std::stoi( it->second );
						it->second = std::to_string(global_memory / opt_repeat);
					}
				}
				if(global_memory != 0) {
					mit->second["DeviceMemoryMb"] = std::to_string(global_memory);
				}
			}
		}
	}


	if (! opt_cron) {
		fprintf(stdout, opt_config ? "Detected%s=%s\n" : "Detected%s=\"%s\"\n", opt_tag, detected_gpus.c_str());
	}

	if (opt_config) {
		fprintf(stdout, "NUM_DETECTED_%s=%d\n", opt_tag, filteredDeviceCount);
	}


	// now print out device properties.
	if (opt_basic) {
		if (! common.empty()) {
			if (opt_nested) {
				printf("Common=[ ");
				for (KVP::const_iterator it = common.begin(); it != common.end(); ++it) {
					printf("%s=%s; ", it->first.c_str(), it->second.c_str());
				}
				printf("]\n");
			} else
			for (KVP::const_iterator it = common.begin(); it != common.end(); ++it) {
				printf("%s%s=%s\n", opt_pre, it->first.c_str(), it->second.c_str());
			}
		}
		for (MKVP::const_iterator mit = dev_props.begin(); mit != dev_props.end(); ++mit) {
			if (mit->second.empty()) continue;
			std::string attrpre(mit->first.c_str());
			std::replace(attrpre.begin(), attrpre.end(), '-', '_');
			std::replace(attrpre.begin(), attrpre.end(), '/', '_');
			if (opt_nested) {
				printf("%s=[ id=\"%s\"; ", attrpre.c_str(), mit->first.c_str());
				for (KVP::const_iterator it = mit->second.begin(); it != mit->second.end(); ++it) {
					if (common.find(it->first) != common.end()) continue;
					printf("%s=%s; ", it->first.c_str(), it->second.c_str());
				}
				printf("]\n");
			} else
			for (KVP::const_iterator it = mit->second.begin(); it != mit->second.end(); ++it) {
				if (common.find(it->first) != common.end()) continue;
				printf("%s%s=%s\n", attrpre.c_str(), it->first.c_str(), it->second.c_str());
			}
		}
	}

	//
	// Clean up on the way out.
	//
	if( nvml_handle ) {
		nvmlShutdown();
		dlclose( nvml_handle );
	}
	if( cuda_handle ) { dlclose( cuda_handle ); }

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
		"    -nested           Print properties using nested ClassAds. (default)\n"
		"    -not-nested       Print properties using prefixed attributes. Disables -nested\n"
		"    -device <N>       Include properties only for GPU device N\n"
		"    -tag <string>     use <string> as resource tag, default is GPUs\n"
		"    -prefix <string>  use <string> as -not-nested property prefix, default is CUDA or OCL\n"
		"    -cuda             Detection via CUDA\n"
		"    -hip              Detection via HIP 6\n"
		"    -opencl           Detection via OpenCL\n"
		"    -uuid             Use GPU uuid instead of index as GPU id\n"
		"    -short-uuid       Use first 8 characters of GPU uuid as GPU id (default)\n"
		"    -by-index         Use GPU index as the GPU id\n"
		"    -simulate[:D[,N[,D2...]] Simulate detection of N devices of type D\n"
		"           where D is 0 - GeForce GT 330\n"
		"                      1 - GeForce GTX 480, default N=2\n"
		"                      2 - Tesla V100-PCIE-16GB\n"
		"                      3 - TITAN RTX\n"
		"                      4 - A100-SXM4-40GB\n"
		"                      5 - NVIDIA A100-SXM4-40GB MIG 1g.5gb\n"
		"                      6 - NVIDIA A100-SXM4-40GB MIG 3g.20gb\n"
		"    -config           Output in HTCondor config syntax\n"
		"    -repeat [<N>]     Repeat list of detected GPUs N (default 2) times\n"
		"                      (e.g., DetectedGPUS = \"CUDA0, CUDA1, CUDA0, CUDA1\")\n"
		"    -divide [<N>]     As -repeat, but divide GlobalMemoryMb by N.\n"
		"                      With both -repeat and -divide:\n"
		"                        the last flag wins,\n"
		"                        but the default won't reset an explicit N.\n"
		"    -packed           When repeating, repeat each GPU, not the whole list\n"
		"                      (e.g., DetectedGPUs = \"CUDA0, CUDA0, CUDA1, CUDA1\")\n"
		"    -cron             Output for use as a STARTD_CRON job, use with -dynamic\n"
		"    -help             Show this screen and exit\n"
		"    -verbose          Show detection progress\n"
		"    -diagnostic       Show detection diagnostic information\n"
		"    -nvcuda           Use nvcuda libraries for -diagnostic detection\n"
		"    -cudart           Use cudart libraries for -diagnostic detection\n"
		"\n"
	);
}

