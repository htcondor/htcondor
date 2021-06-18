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
#include "cuda_device_enumeration.h"
#include "opencl_device_enumeration.h"

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

std::string
constructGPUID( const char * opt_pre, int dev, int opt_uuid, int opt_opencl, int opt_short_uuid, std::vector< BasicProps > & enumeratedDevices ) {
	// Determine the GPU ID.
	std::string gpuID = Format( "%s%i", opt_pre, dev );

	// The -uuid and -short-uuid flags don't imply -properties.
	if( opt_uuid && !opt_opencl && !enumeratedDevices[dev].uuid.empty()) {
		gpuID = gpuIDFromUUID( enumeratedDevices[dev].uuid, opt_short_uuid );
	}

	return gpuID;
}

typedef std::map<std::string, std::string> KVP;
typedef std::map<std::string, KVP> MKVP;

void
setPropertiesFromDynamicProps( KVP & props, nvmlDevice_t device ) {

	// fprintf( stderr, "printDynamicProperties(%s)\n", gpuID.c_str() );
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

	if( opt_extra ) {
		if( bp.clockRate != -1 ) { props["ClockMhz"] = Format("%.2f", bp.clockRate * 1e-3f); }
		if( bp.multiProcessorCount > 0 ) {
			props["ComputeUnits"] = Format("%u", bp.multiProcessorCount);
		}
		if( bp.ccMajor != -1 && bp.ccMinor != -1 ) {
			props["CoresPerCU"] = Format("%u", ConvertSMVer2Cores(bp.ccMajor, bp.ccMinor));
		}
	}

	if( cudaDriverGetVersion ) {
		int driverVersion = 0;
		cudaDriverGetVersion( & driverVersion );
		props["DriverVersion"] = Format("%d.%d", driverVersion/1000, driverVersion%100);
		props["MaxSupportedVersion"] = Format("%d", driverVersion);
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
	int opt_nested = 0;  // publish properties using nested ads
	int opt_uuid = -1;   // publish DetectedGPUs as a list of GPU-<uuid> rather than than CUDA<N>
	int opt_short_uuid = -1; // use shortened uuids
	std::list<int> dwl; // Device White List
	bool opt_nvcuda = false; // force use of nvcuda rather than cudart
	bool opt_cudart = false; // force use of use cudart rather than nvcuda
	int opt_opencl = 0; // prefer opencl detection
	int opt_cuda_only = 0; // require cuda detection

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
			opt_opencl = 1;

			// See comment for the -dynamic flag.
			opt_dynamic = 0;
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
						fprintf (stderr, "Error: -prefix should be less than 80 characters\n");
						return 1;
				}
			}
		}
		else if (is_dash_arg_prefix(argv[i], "device", 3)) {
			if (! argv[i+1] || '-' == *argv[i+1]) {
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
			// use nvcuda instead of cudart (option is for testing)
			opt_nvcuda = true;
		}
		else if (is_dash_arg_prefix(argv[i], "cudart", 6)) {
			// use cudart instead of nvcuda (option is for testing)
			opt_cudart = true;
		}
		else {
			fprintf(stderr, "option %s is not valid\n", argv[i]);
			usage(stderr, argv[0]);
			return 1;
		}
	}


	//
	// Load and prepare libraries.
	//
	dlopen_return_t cuda_handle = NULL;
	dlopen_return_t nvml_handle = NULL;
	dlopen_return_t ocl_handle = NULL;

	if( opt_simulate ) {
		setSimulatedCUDAFunctionPointers();
		setSimulatedNVMLFunctionPointers();
	} else {
		cuda_handle = setCUDAFunctionPointers( opt_nvcuda, opt_cudart );
		if( cuda_handle && !opt_cudart ) {
			CUresult r = cuInit(0);
			if( r != CUDA_SUCCESS ) {
				if( r == CUDA_ERROR_NO_DEVICE ) {
					print_error(MODE_DIAGNOSTIC_MSG, "diag: cuInit found no CUDA-capable devices. code=%d\n", r);
				} else {
					print_error(MODE_ERROR, "Error: cuInit returned %d\n", r);
				}

				dlclose( cuda_handle );
				cuda_handle = NULL;
			}
		}

		nvml_handle = setNVMLFunctionPointers();
		if(! nvml_handle) {
			// We should definitely warn if opt_dynamic is set, but if it
			// isn't, maybe this should only show up in -debug?
			print_error(MODE_ERROR, "Unable to load NVML; will not discover dynamic properties or MIG instances.\n" );
		} else {
			nvmlReturn_t r = nvmlInit();
			if( r != NVML_SUCCESS ) {
				print_error(MODE_ERROR, "Warning: nvmlInit() failed with error %d; will not discover dynamic properties or MIG instances.\n", r );
				dlclose( nvml_handle );
				nvml_handle = NULL;
			}
		}

		if(! opt_cuda_only) {
			ocl_handle = setOCLFunctionPointers();
		}
	}

	//
	// Determine if we can find any devices before proceeding.
	//
	if( cuDeviceGetCount && cuDeviceGetCount( & deviceCount ) != cudaSuccess ) {
		deviceCount = 0;
	}

	// We assume here that at least one CUDA device exists if any MIG
	// devices exist.

	if( ocl_handle && (deviceCount == 0 || opt_opencl) ) {
		ocl_GetDeviceCount( & deviceCount );
		opt_pre = "OCL";
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
	std::set< std::string > cudaDevices;
	std::vector< BasicProps > nvmlDevices;
	std::vector< BasicProps > enumeratedDevices;
	if(! opt_opencl) {
		if(! enumerateCUDADevices(enumeratedDevices)) {
			fprintf( stderr, "Failed to enumerate GPU devices, aborting.\n" );
			return 1;
		}

		//
		// We have to report NVML devices, because enabling MIG on any
		// GPU prevents CUDA from reporting any MIG-capable GPU, even
		// if we wouldn't otherwise report it because it hasn't enabled
		// MIG.  Since don't know if that applies to non-MIG-capable GPUs,
		// record all the UUIDs we found via CUDA and skip them when
		// reporting the devices we found via NVML.
		//
		bool shouldEnumerateNVMLDevices = true;
		for( const BasicProps & bp : enumeratedDevices ) {
			if(! bp.uuid.empty()) {
				// NVML and CUDA UUIDs differ in this prefix.
				std::string UUID = "GPU-" + bp.uuid;
				cudaDevices.insert( UUID );
				// fprintf( stderr, "Adding %s to the CUDA device list\n", UUID.c_str() );
			} else {
				fprintf( stderr, "Not enumerating NVML devices because a CUDA device has no UUID.  This usually means you should upgrade your CUDA libaries.\n" );
				shouldEnumerateNVMLDevices = false;
			}
		}

		if( shouldEnumerateNVMLDevices && nvml_handle ) {
			nvmlReturn_t r = enumerateNVMLDevices(nvmlDevices);
			if(r != NVML_SUCCESS) {
				fprintf( stderr, "Failed to enumerate MIG devices (%d: %s), aborting.\n", r, nvmlErrorString(r) );
				return 1;
			}

			// We don't want to report the parent device of any MIG instance
			// as available for use (to avoid overcommitting it), so construct
			// a list of parent devices to skip in the loop below.
			for( const BasicProps & bp : nvmlDevices ) {
				std::string parentUUID = bp.uuid;
				if( parentUUID.find( "MIG-GPU-" ) != 0 ) { continue; }
				parentUUID = parentUUID.substr( 8 );
				parentUUID.erase( parentUUID.find( "/" ), std::string::npos );
				migDevices.insert(parentUUID);
				// fprintf( stderr, "Adding %s to MIG device list for MIG instance %s\n", parentUUID.c_str(), bp.uuid.c_str() );
			}
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
		// Skip devices not on the whitelist.
		if( (!dwl.empty()) && std::find( dwl.begin(), dwl.end(), dev ) == dwl.end() ) {
			continue;
		}

		// if we are defaulting to UUID, but the device has no UUID, switch the default to -by-index
		if ((opt_uuid < 0) && enumeratedDevices[dev].uuid.empty()) {
			opt_short_uuid = opt_uuid = 0;
		}

		std::string gpuID = constructGPUID(opt_pre, dev, opt_uuid, opt_opencl, opt_short_uuid, enumeratedDevices);

		// Skip devices which have MIG instances associated with them.
		if((! opt_opencl) && nvml_handle) {
			const std::string & UUID = enumeratedDevices[dev].uuid;
			if( migDevices.find( UUID ) != migDevices.end() ) {
				// fprintf( stderr, "[CUDA dev_props] Skipping %s.\n", UUID.c_str() );
				continue;
			}
		}

		if (! detected_gpus.empty()) { detected_gpus += ", "; }
		detected_gpus += gpuID;
		++filteredDeviceCount;

		// fprintf( stderr, "[CUDA dev_props] Adding CUDA device %s\n", gpuID.c_str() );

		if(! opt_basic) { continue; }

		dev_props[gpuID].clear();
		KVP & props = dev_props.find(gpuID)->second;

		if(! opt_opencl) {
			// Report CUDA properties.
			BasicProps bp = enumeratedDevices[dev];
			setPropertiesFromBasicProps( props, bp, opt_extra );

			// Not sure what this does, actually.
			if( dev < g_cl_cCuda ) {
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
		if( migDevices.find( bp.uuid ) != migDevices.end() ) {
			// fprintf( stderr, "[nvml dev_props] Skipping MIG device parent %s.\n", bp.uuid.c_str() );
			continue;
		}

		if( cudaDevices.find( bp.uuid ) != cudaDevices.end() ) {
			// fprintf( stderr, "[nvml dev_props] Skipping CUDA device %s.\n", bp.uuid.c_str() );
			continue;
		}

		std::string gpuID = gpuIDFromUUID( bp.uuid, opt_short_uuid );
		if (! detected_gpus.empty()) { detected_gpus += ", "; }
		detected_gpus += gpuID;
		++filteredDeviceCount;

		dev_props[gpuID].clear();

		// fprintf( stderr, "[nvml dev_props] Adding NVML device %s\n", gpuID.c_str() );

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
			if ((!dwl.empty()) && std::find(dwl.begin(), dwl.end(), dev) == dwl.end()) {
				continue;
			}

			// Determine the GPU ID.
			std::string gpuID = constructGPUID(opt_pre, dev, opt_uuid, opt_opencl, opt_short_uuid, enumeratedDevices);

			const std::string & UUID = enumeratedDevices[dev].uuid;
			if (migDevices.find(UUID) != migDevices.end()) {
				// fprintf( stderr, "[dynamic CUDA properties] Skipping MIG parent device %s.\n", UUID.c_str() );
				continue;
			}

			nvmlDevice_t device;
			if (NVML_SUCCESS != findNVMLDeviceHandle(enumeratedDevices[dev].uuid, & device)) {
				continue;
			}

			KVP & props = dev_props.find(gpuID)->second;
			setPropertiesFromDynamicProps(props, device);
		}

		// Dynamic properties for NVML devices
		for (auto bp : nvmlDevices) {
			if (cudaDevices.find(bp.uuid) != cudaDevices.end()) {
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
//		"    -nested           Print properties using nested ClassAds\n"
		"    -device <N>       Include properties only for GPU device N\n"
		"    -tag <string>     use <string> as resource tag, default is GPUs\n"
		"    -prefix <string>  use <string> as property prefix, default is CUDA or OCL\n"
		"    -cuda             Detection via CUDA only, ignore OpenCL devices\n"
		"    -opencl           Prefer detection via OpenCL rather than CUDA\n"
		"    -uuid             Use GPU uuid instead of index as GPU id\n"
		"    -short-uuid       Use first 8 characters of GPU uuid as GPU id (default)\n"
		"    -by-index         Use GPU index as the GPU id\n"
		"    -simulate[:D[,N]] Simulate detection of N devices of type D\n"
		"           where D is 0 - GeForce GT 330, default N=1\n"
		"                      1 - GeForce GTX 480, default N=2\n"
		"    -config           Output in HTCondor config syntax\n"
		"    -repeat [<N>]     Repeat list of detected GPUs N (default 2) times\n"
		"                      (e.g., DetectedGPUS = \"CUDA0, CUDA1, CUDA0, CUDA1\")\n"
		"                      Divides GlobalMemoryMb attribute by N.\n"
		"    -divide [<N>]     As -repeat, but divide GlobalMemoryMb by N.\n"
		"                      With both -repeat and -divide:\n"
		"                        the last flag wins,\n"
		"                        but the default won't reset an explicit N.\n"
		"    -packed           When repeating, repeat each GPU, not the whole list\n"
		"                      (e.g., DetectedGPUs = \"CUDA0, CUDA0, CUDA1, CUDA1\")\n"
		"    -cron             Output for use as a STARTD_CRON job, use with -dynamic\n"
		"    -help             Show this screen and exit\n"
		"    -verbose          Show detection progress\n"
		//"    -diagnostic       Show detection diagnostic information\n"
		//"    -nvcuda           Use nvcuda rather than cudarl for detection\n"
		//"    -cudart           Use cudart rather than nvcuda for detection\n"
		"\n"
	);
}

