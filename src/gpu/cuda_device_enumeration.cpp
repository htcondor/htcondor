#include <string.h>
#include <stdlib.h>

#include <string>
#include <vector>
#include <map>
#include <set>

// For pi_dynlink.h
#if       defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOSERVICE
#define NOMCX
#define NOIME
#include <Windows.h>
#endif /* defined(WIN32) */

#include "nvml_stub.h"
#include "cuda_header_doc.h"
#include "pi_dynlink.h"

#define DEFINE_GPU_FUNCTION_POINTERS
#include "cuda_device_enumeration.h"

#include "print_error.h"

static char printable_digit(unsigned char n) { return (n >= ' ' && n <= '~') ? n : '.'; }

void hex_dump(FILE* out, const unsigned char * buf, size_t cb, int offset)
{
	char line[6 + 16 * 3 + 2 + 16 + 2 + 2];
	for (size_t lx = 0; lx < cb && lx < 0xFFFF; lx += 16) {
		memset(line, ' ', sizeof(line));
		line[sizeof(line) - 1] = 0;
		snprintf(line, sizeof(line), "%04X:", (int)lx);
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

bool
enumerateCUDADevices( std::vector< BasicProps > & devices ) {
	int deviceCount = 0;

	if( (cuDeviceGetCount(&deviceCount) != cudaSuccess) ) {
		return false;
	}

	for( int dev = 0; dev < deviceCount; ++dev ) {
		BasicProps bp;
		if( cudaSuccess == getBasicProps( dev, &bp ) ) {
			devices.push_back(bp);
		}
	}

	return true;
}

char hex_digit(unsigned char n) { return n + ((n < 10) ? '0' : ('a' - 10)); }
char * print_uuid(char* buf, int bufsiz, const unsigned char uuid[16]) {
	char *p = buf;
	char *endp = buf + bufsiz - 1;
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

BasicProps::BasicProps() {
	memset( pciId, 0, sizeof(pciId) );
}

void
BasicProps::setUUIDFromBuffer( const unsigned char uuid[16] ) {
	char buffer[NVML_DEVICE_UUID_V2_BUFFER_SIZE];
	print_uuid(buffer, NVML_DEVICE_UUID_V2_BUFFER_SIZE, uuid);
	this->uuid = buffer;
}

// ----------------------------------------------------------------------------

#if defined(WIN32)

cudaError_t
CUDACALL cu_cudaRuntimeGetVersion( int * pver ) {
	if(! pver) { return cudaErrorInvalidValue; }
	* pver = 0;
	cudaError_t ret = cudaErrorNoDevice;

	// On windows, the CUDA runtime isn't installed in the system directory,
	// but we can locate it by looking in the registry.
	// TODO: handle multiple installed runtimes...
	const char * reg_key = "SOFTWARE\\NVIDIA Corporation\\GPU Computing Toolkit\\CUDA";
	const char * reg_val_name = "FirstVersionInstalled";
	HKEY hkey = NULL;
	int bitness = KEY_WOW64_64KEY; //KEY_WOW64_32KEY; KEY_WOW64_64KEY
	int res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, reg_key, 0, KEY_READ | bitness, &hkey);
	if (res != ERROR_SUCCESS) {
		print_error(MODE_DIAGNOSTIC_MSG, "diag: cuInit found no CUDA-capable devices. code=%d\n", ret);
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

	return ret;
}

#endif /* defined(WIN32) */

// ----------------------------------------------------------------------------

cudaError_t basicPropsFromCudaProps(cudaDevicePropStrings * dps, cudaDevicePropInts * dpi, BasicProps * p)
{
	p->name = dps->name;
	p->totalGlobalMem = dpi->totalGlobalMem;
	p->ccMajor = dpi->ccMajor;
	p->ccMinor = dpi->ccMinor;
	p->clockRate = dpi->clockRate;
	p->multiProcessorCount = dpi->multiProcessorCount;
	p->ECCEnabled = dpi->ECCEnabled;
	p->setUUIDFromBuffer( dps->uuid );
	if (dpi->pciBusID || dpi->pciDeviceID) {
		snprintf(p->pciId, sizeof(p->pciId), "%04X:%02X:%02X.0", dpi->pciDomainID, dpi->pciBusID, dpi->pciDeviceID);
	}
	cudaDriverGetVersion(&p->driverVersion);
	return cudaSuccess;
}

cudaError_t CUDACALL cuda9_getBasicProps(int devID, BasicProps * p) {
	cudaDevicePropBuffer buf;
	memset(buf.props, 0, sizeof(buf.props));
	cudaError_t err = cudaGetDevicePropertiesOfIndeterminateStructure(&buf, devID);
	if (cudaSuccess != err)
		return err;

	const int ints_offset = 256;
	if( g_diagnostic ) {
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
	if( g_diagnostic ) {
		print_error(MODE_DIAGNOSTIC_MSG, "diag: cudaDviceProperties 10 buffer:\n");
		hex_dump(stdout, &buf.props[0], sizeof(buf), ints_offset);
	}
	cudaDevicePropStrings * dps = (cudaDevicePropStrings *)(&buf);
	cudaDevicePropInts * dpi = (cudaDevicePropInts *)(&buf.props[ints_offset]);
	p->setUUIDFromBuffer(dps->uuid);
	return basicPropsFromCudaProps(dps, dpi, p);
}

cudaError_t CUDACALL cu_getBasicProps(int devID, BasicProps * p) {
	cudev dev;
	cudaError_t res = cuDeviceGet(&dev, devID);
	if (cudaSuccess == res) {
		if( g_diagnostic ) {
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
		if( cuDeviceGetUuid ) {
			unsigned char buffer[16];
			cuDeviceGetUuid( buffer, dev );
			p->setUUIDFromBuffer( buffer );
		}
		if (cuDeviceGetPCIBusId) cuDeviceGetPCIBusId(p->pciId, sizeof(p->pciId), dev);
		cuDeviceComputeCapability(&p->ccMajor, &p->ccMinor, dev);
		cudaError_t r = cuDeviceTotalMem(&p->totalGlobalMem, dev);
		print_error(MODE_DIAGNOSTIC_MSG, "# cuDeviceTotalMem(%p) returns %d, value = %llu\n", dev, r, (unsigned long long)p->totalGlobalMem);
		cuDeviceGetAttribute(&p->clockRate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, dev);
		cuDeviceGetAttribute(&p->multiProcessorCount, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, dev);
		cuDeviceGetAttribute(&p->ECCEnabled, CU_DEVICE_ATTRIBUTE_ECC_ENABLED, dev);
		cudaDriverGetVersion(&p->driverVersion);

		char driver[80];
		if (nvmlSystemGetDriverVersion) {
			int res = nvmlSystemGetDriverVersion(driver, 80);
			if( NVML_SUCCESS == res ) {
				p->driver = driver;
			}
		}

	}
	return res;
}

//
// NVML function pointers.
//

#if       defined(WIN32)
dlopen_return_t
loadNVMLLibrary() {
	const char * nvml_library = "nvml.dll";
	dlopen_return_t nvml_handle = dlopen( nvml_library, RTLD_LAZY );
	if(! nvml_handle) {
		print_error(MODE_DIAGNOSTIC_MSG, "Can't open library '%s': '%s'\n", nvml_library, dlerror());
	}

	dlerror(); // reset error
	return nvml_handle;
}
#else  /* defined(WIN32) */
dlopen_return_t
loadNVMLLibrary() {
	const char * nvml_library = "libnvidia-ml.so.1";
	dlopen_return_t nvml_handle = dlopen( nvml_library, RTLD_LAZY );
	if(! nvml_handle) {
		print_error(MODE_DIAGNOSTIC_MSG, "Can't open library '%s': '%s'\n", nvml_library, dlerror());
		dlerror(); // reset error

		const char * nvml_fallback_library = "libnvidia-ml.so";
		nvml_handle = dlopen( nvml_fallback_library, RTLD_LAZY );
		print_error(MODE_DIAGNOSTIC_MSG, "Can't open library '%s': '%s'\n", nvml_fallback_library, dlerror());
	}

	dlerror(); // reset error
	return nvml_handle;
}
#endif /* defined(WIN32) */

dlopen_return_t
setNVMLFunctionPointers() {
	dlopen_return_t nvml_handle = loadNVMLLibrary();
	if(! nvml_handle) { return NULL; }

	nvmlInit =
		(nvml_void)dlsym( nvml_handle, "nvmlInit_v2" );
	nvmlShutdown =
		(nvml_void)dlsym( nvml_handle, "nvmlShutdown" );
	nvmlDeviceGetCount =
		(nvml_unsigned_int)dlsym( nvml_handle, "nvmlDeviceGetCount_v2" );
	nvmlDeviceGetSamples =
		(nvml_dgs)dlsym( nvml_handle, "nvmlDeviceGetSamples" );
	nvmlDeviceGetMemoryInfo =
		(nvml_dm)dlsym( nvml_handle, "nvmlDeviceGetMemoryInfo" );
	nvmlDeviceGetHandleByIndex =
		(nvml_dghbi)dlsym( nvml_handle, "nvmlDeviceGetHandleByIndex_v2" );
	nvmlDeviceGetHandleByUUID =
		(nvml_dghbc)dlsym( nvml_handle, "nvmlDeviceGetHandleByUUID" );
	nvmlErrorString =
		(cc_nvml)dlsym( nvml_handle, "nvmlErrorString" );

	nvmlDeviceGetFanSpeed =
		(nvml_get_uint)dlsym( nvml_handle, "nvmlDeviceGetFanSpeed" );
	nvmlDeviceGetPowerUsage =
		(nvml_get_uint)dlsym( nvml_handle, "nvmlDeviceGetPowerUsage" );
	nvmlDeviceGetTemperature =
		(nvml_dgt)dlsym( nvml_handle, "nvmlDeviceGetTemperature" );
	nvmlDeviceGetTotalEccErrors =
		(nvml_dgtee)dlsym( nvml_handle, "nvmlDeviceGetTotalEccErrors" );

	nvmlDeviceGetPciInfo_v3 =
		(nvml_get_pci)dlsym( nvml_handle, "nvmlDeviceGetPciInfo_v3" );
	nvmlDeviceGetGpuInstanceId =
		(nvml_get_uint)dlsym( nvml_handle, "nvmlDeviceGetGpuInstanceID" );
	nvmlDeviceGetComputeInstanceId =
		(nvml_get_uint)dlsym( nvml_handle, "nvmlDeviceGetComputeInstanceID" );
	nvmlDeviceGetMaxMigDeviceCount =
		(nvml_get_uint)dlsym( nvml_handle, "nvmlDeviceGetMaxMigDeviceCount" );
	nvmlDeviceGetName =
		(nvml_get_char)dlsym( nvml_handle, "nvmlDeviceGetName" );
	nvmlDeviceGetUUID =
		(nvml_get_char)dlsym( nvml_handle, "nvmlDeviceGetUUID" );
	nvmlDeviceGetMigDeviceHandleByIndex =
		(nvml_get_dhbi)dlsym( nvml_handle, "nvmlDeviceGetMigDeviceHandleByIndex" );
	nvmlDeviceGetCudaComputeCapability =
		(nvml_get_int_int)dlsym( nvml_handle, "nvmlDeviceGetCudaComputeCapability" );
	nvmlDeviceGetMaxClockInfo =
		(nvml_get_clock)dlsym( nvml_handle, "nvmlDeviceGetMaxClockInfo" );
	nvmlDeviceGetAttributes =
		(nvml_get_attrs)dlsym( nvml_handle, "nvmlDeviceGetAttributes" );
	nvmlDeviceGetEccMode =
		(nvml_get_eccm)dlsym( nvml_handle, "nvmlDeviceGetEccMode" );

	nvmlSystemGetDriverVersion =
		(nvml_system_get_driver_version)dlsym( nvml_handle, "nvmlSystemGetDriverVersion" );
	return nvml_handle;
}

//
// CUDA function pointers.
//

dlopen_return_t
setCUDAFunctionPointers( bool force_nvcuda, bool force_cudart, bool must_load ) {
	//
	// We try to load and initialize the nvcuda library; if that fails,
	// we load the cudart library, instead.
	//

#if defined(WIN32)
	const char * cuda_library = "nvcuda.dll";
	const char * cudart_library = "cudart.dll";
#else
	const char * cuda_library = "libcuda.so";
	const char * cudart_library = "libcudart.so";
#endif

	dlopen_return_t cuda_handle = dlopen( cuda_library, RTLD_LAZY );
	if(! cuda_handle) { print_error(MODE_DIAGNOSTIC_MSG, "Can't open library '%s': '%s'\n", cuda_library, dlerror()); }
	dlerror(); // reset error

	dlopen_return_t cudart_handle = dlopen( cudart_library, RTLD_LAZY );
	if(! cudart_handle) { print_error(MODE_DIAGNOSTIC_MSG, "Can't open library '%s': '%s'\n", cudart_library, dlerror()); }
	dlerror(); // reset error

	if( cuda_handle && ! force_cudart ) {
		print_error(MODE_DIAGNOSTIC_MSG, "diag: using nvcuda for gpu discovery\n");

		cuInit =
			(cu_uint_t)dlsym( cuda_handle, "cuInit" );

		cuDeviceGet =
			(cuda_dev_int_t)dlsym( cuda_handle, "cuDeviceGet" );
		cuDeviceGetAttribute =
			(cuda_ga_t)dlsym( cuda_handle, "cuDeviceGetAttribute" );
		cuDeviceGetName =
			(cuda_name_t)dlsym( cuda_handle, "cuDeviceGetName" );
		cuDeviceGetUuid =
			(cuda_uuid_t)dlsym( cuda_handle, "cuDeviceGetUuid" );
		cuDeviceGetPCIBusId =
			(cuda_pciid_t)dlsym( cuda_handle, "cuDeviceGetPCIBusId" );
		cuDeviceComputeCapability =
			(cuda_cc_t)dlsym( cuda_handle, "cuDeviceComputeCapability" );
		cuDeviceTotalMem =
			(cuda_size_t)dlsym( cuda_handle, "cuDeviceTotalMem_v2" );
		cuDeviceGetCount =
			(cuda_t)dlsym( cuda_handle, "cuDeviceGetCount" );

		cudaDriverGetVersion =
			(cuda_t)dlsym( cuda_handle, "cuDriverGetVersion" );
#if defined(WIN32)
		cudaRuntimeGetVersion = cu_cudaRuntimeGetVersion;
#else
		cudaRuntimeGetVersion =
			(cuda_t)dlsym( cuda_handle, "cudaRuntimeGetVersion" );
#endif

		getBasicProps = cu_getBasicProps;

		findNVMLDeviceHandle = nvml_findNVMLDeviceHandle;
		return cuda_handle;
	} else if( cudart_handle && ! force_nvcuda ) {
		print_error(MODE_DIAGNOSTIC_MSG, "diag: using cudart for gpu discovery\n");

		cuDeviceGetCount =
			(cuda_t)dlsym( cudart_handle, "cudaGetDeviceCount" );
		cudaDriverGetVersion =
			(cuda_t)dlsym( cudart_handle, "cudaDriverGetVersion" );
		cudaRuntimeGetVersion =
			(cuda_t)dlsym( cudart_handle, "cudaRuntimeGetVersion" );

		if( cudaRuntimeGetVersion ) {
			int runtimeVersion = 0;
			cudaRuntimeGetVersion(& runtimeVersion);
			if( runtimeVersion >= 10 * 1000 ) {
				getBasicProps = cuda10_getBasicProps;
			} else {
				getBasicProps = cuda9_getBasicProps;
			}
			cudaGetDevicePropertiesOfIndeterminateStructure =
				(cuda_DevicePropBuf_int)dlsym( cudart_handle, "cudaGetDeviceProperties" );
		} else {
			fprintf( stderr, "# Unable to find cudaRuntimGetVersion().\n" );
			dlclose( cudart_handle );
			return NULL;
		}

		findNVMLDeviceHandle = nvml_findNVMLDeviceHandle;
		return cudart_handle;
	} else if (must_load) {
		fprintf( stderr, "# Unable to load a CUDA library (%s or %s).\n",
			cuda_library, cudart_library );
	} else {
		print_error(MODE_DIAGNOSTIC_MSG, "# Unable to load a CUDA library (%s or %s).\n",
			cuda_library, cudart_library);
	}
	if (cuda_handle) {
		dlclose(cuda_handle);
	}
	return NULL;
}


nvmlReturn_t
getMIGParentDeviceUUIDs( std::set< std::string > & parentDeviceUUIDs ) {
	// if this version of the library doesn't have a nvmlDeviceGetMaxMigDeviceCount function
	// then there can be no MIG parent devices, trivial success
	if (! nvmlDeviceGetMaxMigDeviceCount) { return NVML_SUCCESS; }

	unsigned int deviceCount = 0;
	nvmlReturn_t r = nvmlDeviceGetCount(& deviceCount);
	if( NVML_SUCCESS != r ) { return r; }

	for( unsigned int i = 0; i < deviceCount; ++i ) {
		nvmlDevice_t device;
		r = nvmlDeviceGetHandleByIndex( i, & device );
		if( NVML_SUCCESS != r ) { return r; }

		unsigned int maxMigDeviceCount = 0;
		r = nvmlDeviceGetMaxMigDeviceCount( device, & maxMigDeviceCount );
		if( NVML_SUCCESS == r && maxMigDeviceCount != 0 ) {
			for( unsigned int index = 0; index < maxMigDeviceCount; ++index ) {
				nvmlDevice_t migDevice;
				r = nvmlDeviceGetMigDeviceHandleByIndex( device, index, & migDevice );
				if( NVML_SUCCESS != r ) { continue; }

				char uuid[NVML_DEVICE_UUID_V2_BUFFER_SIZE];
				r = nvmlDeviceGetUUID( device, uuid, NVML_DEVICE_UUID_V2_BUFFER_SIZE );
				if( NVML_SUCCESS != r ) { return r; }

				parentDeviceUUIDs.insert( uuid );
				break;
			}
		}
	}

	return NVML_SUCCESS;
}


nvmlReturn_t
getUUIDToMIGDeviceHandleMap( std::map< std::string, nvmlDevice_t > & map ) {
	unsigned int deviceCount = 0;
	nvmlReturn_t r = nvmlDeviceGetCount(& deviceCount);
	if( NVML_SUCCESS != r ) { return r; }

	// if this version of the library doesn't have a nvmlDeviceGetMaxMigDeviceCount function
	// then we behave as if the mig device count was 0, trivial success.
	if (! nvmlDeviceGetMaxMigDeviceCount) { return NVML_SUCCESS; }

	for( unsigned int i = 0; i < deviceCount; ++i ) {
		nvmlDevice_t device;
		r = nvmlDeviceGetHandleByIndex( i, & device );
		if( NVML_SUCCESS != r ) { return r; }

		unsigned int maxMigDeviceCount = 0;
		r = nvmlDeviceGetMaxMigDeviceCount( device, & maxMigDeviceCount );
		if( NVML_SUCCESS == r && maxMigDeviceCount != 0 ) {
			for( unsigned int index = 0; index < maxMigDeviceCount; ++index ) {
				nvmlDevice_t migDevice;
				r = nvmlDeviceGetMigDeviceHandleByIndex( device, index, & migDevice );
				// It is possible to enable MIG on a MIG-capable device but
				// not create any compute instances.  Since nobody can use
				// those devices, ignore them.
				if( NVML_SUCCESS != r ) { continue; }

				char uuid[NVML_DEVICE_UUID_V2_BUFFER_SIZE];
				r = nvmlDeviceGetUUID( migDevice, uuid, NVML_DEVICE_UUID_V2_BUFFER_SIZE );
				if( NVML_SUCCESS != r ) { return r; }
				map[std::string(uuid)] = migDevice;
			}
		}
	}

	return NVML_SUCCESS;
}

nvmlReturn_t
nvml_findNVMLDeviceHandle(const std::string & uuid, nvmlDevice_t * device) {
	if( uuid.find("MIG-") == 0 ) {
		// Unfortunately, nvmlDeviceGetHandleByUUID() doesn't work when
		// passed a MIG instance's UUID, so we have to scan them all.
		nvmlReturn_t r;
		static bool mapInitialized = false;
		static std::map< std::string, nvmlDevice_t > uuidsToHandles;
		if(! mapInitialized) {
			r = getUUIDToMIGDeviceHandleMap( uuidsToHandles );
			if( NVML_SUCCESS != r ) { return r;	}
		}

		auto iter = uuidsToHandles.find(uuid);
		if( iter != uuidsToHandles.end() ) {
			* device = iter->second;
			return NVML_SUCCESS;
		} else {
			return NVML_ERROR_NOT_FOUND;
		}
	} else if( uuid.find("GPU-") == 0 ) {
		return nvmlDeviceGetHandleByUUID( uuid.c_str(), device );
	} else {
		// Assume it's a CUDA UUID.
		std::string nvmlUUID = "GPU-" + uuid;
		return nvmlDeviceGetHandleByUUID( nvmlUUID.c_str(), device );
	}
}

nvmlReturn_t
nvml_getBasicProps( nvmlDevice_t migDevice, BasicProps * p ) {
	nvmlReturn_t r;

	char name[NVML_DEVICE_NAME_BUFFER_SIZE];
	r = nvmlDeviceGetName( migDevice, name, NVML_DEVICE_NAME_BUFFER_SIZE );
	if( NVML_SUCCESS == r ) {
		p->name = name;
	}

	nvmlPciInfo_t pci;
	if(nvmlDeviceGetPciInfo_v3) {
		r = nvmlDeviceGetPciInfo_v3( migDevice, & pci );
		if( NVML_SUCCESS == r ) {
			strncpy( p->pciId, pci.busIdLegacy, NVML_DEVICE_PCI_BUS_ID_BUFFER_V2_SIZE );
		}
	}

	nvmlMemory_t memory;
	r = nvmlDeviceGetMemoryInfo( migDevice, & memory );
	if( NVML_SUCCESS == r ) {
		p->totalGlobalMem = memory.total;
	}

	int ccMajor = 0, ccMinor = 0;
	if(nvmlDeviceGetCudaComputeCapability) {
		r = nvmlDeviceGetCudaComputeCapability( migDevice, & ccMajor, & ccMinor );
		if( NVML_SUCCESS == r ) {
			p->ccMajor = ccMajor;
			p->ccMinor = ccMinor;
		}
	}

	// This is not always the same as cuDeviceGetAttribute(CLOCK_RATE).
	unsigned int clock = 0;
	r = nvmlDeviceGetMaxClockInfo( migDevice, NVML_CLOCK_GRAPHICS, & clock );
	if( NVML_SUCCESS == r ) {
		// in MHz from NVML but KHz from CUDA
		p->clockRate = clock * 1000;
	}

	// This only exists for MIG devices.
	nvmlDeviceAttributes_t attributes;
	if(nvmlDeviceGetAttributes) {
		r = nvmlDeviceGetAttributes( migDevice, & attributes );
		if( NVML_SUCCESS == r ) {
			p->multiProcessorCount = attributes.multiprocessorCount;
		}
	}

	nvmlEnableState_t current, pending;
	r = nvmlDeviceGetEccMode( migDevice, & current, & pending );
	if( NVML_SUCCESS == r ) {
		p->ECCEnabled = (current == NVML_FEATURE_ENABLED);
	}

	return NVML_SUCCESS;
}

nvmlReturn_t
enumerateNVMLDevices( std::vector< BasicProps > & devices ) {
	nvmlReturn_t r;
	static bool mapInitialized = false;
	static std::map< std::string, nvmlDevice_t > uuidsToHandles;
	if(! mapInitialized) {
		r = getUUIDToMIGDeviceHandleMap( uuidsToHandles );
		if( NVML_SUCCESS != r ) { return r; }
	}

	for( auto i = uuidsToHandles.begin(); i != uuidsToHandles.end(); ++i ) {
		nvmlDevice_t migDevice = i->second;

		BasicProps bp;
		bp.uuid = i->first;
		r = nvml_getBasicProps( migDevice, & bp );
		print_error(MODE_DIAGNOSTIC_MSG, "# nvml_getBasicProps() for MIG %s returns %d\n", bp.uuid.c_str(), r);
		if( NVML_SUCCESS != r ) { return r; }

		devices.push_back( bp );
	}

	//
	// Enumerate non-MIG NVML devices.
	//
	unsigned int deviceCount = 0;
	r = nvmlDeviceGetCount(& deviceCount);
	if( NVML_SUCCESS != r ) { return r; }

	for( unsigned int i = 0; i < deviceCount; ++i ) {
		nvmlDevice_t device;
		r = nvmlDeviceGetHandleByIndex( i, & device );
		if( NVML_SUCCESS != r ) { return r; }

		unsigned int maxMigDeviceCount = 0;
		if (nvmlDeviceGetMaxMigDeviceCount) {
			r = nvmlDeviceGetMaxMigDeviceCount(device, & maxMigDeviceCount);
		}
		if( NVML_SUCCESS == r && maxMigDeviceCount == 0 ) {
			char uuid[NVML_DEVICE_UUID_V2_BUFFER_SIZE];
			r = nvmlDeviceGetUUID( device, uuid, NVML_DEVICE_UUID_V2_BUFFER_SIZE );
			if( NVML_SUCCESS != r ) { return r; }

			BasicProps bp;
			bp.uuid = uuid;
			r = nvml_getBasicProps( device, & bp );
			print_error(MODE_DIAGNOSTIC_MSG, "# nvml_getBasicProps() for %s returns %d\n", bp.uuid.c_str(), r);
			if( NVML_SUCCESS != r ) { return r; }

			devices.push_back( bp );
		}
	}

	return NVML_SUCCESS;
}

std::string
gpuIDFromUUID( const std::string & uuid, int opt_short_uuid ) {
	std::string gpuID = uuid;

	//
	// UUIDs from CUDA are _just_ the UUID.  UUIDs from NVML may include
	// a leading "GPU-", or, as of driver version 470, a leading "MIG-".
	//
	// However, the GPU IDs with a leading "MIG-" don't work if shortened,
	// so never do that.
	//

	if( gpuID.find( "MIG-" ) == 0 ) {
		return gpuID;
	}

	//
	// By a "GPU ID", we mean an identifier that can be used in
	// CUDA_VISIBLE_DEVICES.  Oddly, the bare UUID returned from CUDA
	// can't; the UUID must be prefixed with "GPU-", to make it look
	// like an NVML "UUID".
	//

	if( gpuID.find( "GPU-" ) == std::string::npos ) {
		gpuID = "GPU-" + gpuID;
	}

	//
	// GPU IDs don't have to include the entire UUID, just enough of
	// a prefix to uniquely identify the device.
	//

	if( opt_short_uuid ) {
		// MIG-GPU-<UUID>/x/y
		if( gpuID.find("MIG-") == 0 ) {
			size_t first_dash_in_uuid = gpuID.find( "-", 8 );
			size_t first_slash = gpuID.find( "/" );
			gpuID.replace( first_dash_in_uuid, first_slash - first_dash_in_uuid, "" );
		} else if( gpuID.find("GPU-") == 0 ) {
			gpuID.erase( 12 );
		}
	}

	return gpuID;
}
