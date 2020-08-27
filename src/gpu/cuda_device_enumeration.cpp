#include <string.h>
#include <stdlib.h>

#include <string>
#include <vector>

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

bool
enumerateCUDADevices( std::vector< BasicProps > & devices ) {
	int deviceCount = 0;

	if( (cudaGetDeviceCount(&deviceCount) != cudaSuccess) ) {
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

static char hex_digit(unsigned char n) { return n + ((n < 10) ? '0' : ('a' - 10)); }
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

BasicProps::BasicProps() : totalGlobalMem(0), ccMajor(0), ccMinor(0), multiProcessorCount(0), clockRate(0), ECCEnabled(0) {
	memset( uuid, 0, sizeof(uuid) );
	memset( pciId, 0, sizeof(pciId) );
}

const char * BasicProps::printUUID(char* buf, int bufsiz) {
	return print_uuid(buf, bufsiz, uuid);
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
	cudaDevicePropStrings * dps = (cudaDevicePropStrings *)(&buf);
	cudaDevicePropInts * dpi = (cudaDevicePropInts *)(&buf.props[ints_offset]);
	memcpy(p->uuid, dps->uuid, sizeof(dps->uuid));
	return basicPropsFromCudaProps(dps, dpi, p);
}

cudaError_t CUDACALL cu_getBasicProps(int devID, BasicProps * p) {
	cudev dev;
	cudaError_t res = cuDeviceGet(&dev, devID);
	if (cudaSuccess == res) {
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
		cuDeviceGetAttribute(&p->clockRate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, dev);
		cuDeviceGetAttribute(&p->multiProcessorCount, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, dev);
		cuDeviceGetAttribute(&p->ECCEnabled, CU_DEVICE_ATTRIBUTE_ECC_ENABLED, dev);
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
		fprintf( stderr, "Unable to load %s.\n", nvml_library );
	}
	return nvml_handle;
}
#else  /* defined(WIN32) */
dlopen_return_t
loadNVMLLibrary() {
	const char * nvml_library = "libnvidia-ml.so.1";
	dlopen_return_t nvml_handle = dlopen( nvml_library, RTLD_LAZY );
	if(! nvml_handle) {
		const char * nvml_fallback_library = "libnvidia-ml.so";
		nvml_handle = dlopen( nvml_fallback_library, RTLD_LAZY );
		fprintf( stderr, "Unable to load %s or %s.\n", nvml_library, nvml_fallback_library );
	}
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
	nvmlDeviceGetHandleByPciBusId =
		(nvml_dghbp)dlsym( nvml_handle, "nvmlDeviceGetHandleByPciBusId_v2" );
	nvmlErrorString =
		(cc_nvml)dlsym( nvml_handle, "nvmlErrorString" );

	return nvml_handle;
}

//
// CUDA function pointers.
//

dlopen_return_t
setCUDAFunctionPointers() {
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
	dlopen_return_t cudart_handle = dlopen( cudart_library, RTLD_LAZY );

	if( cuda_handle ) {
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

		cudaGetDeviceCount =
			(cuda_t)dlsym( cuda_handle, "cuDeviceGetCount" );
		cudaDriverGetVersion =
			(cuda_t)dlsym( cuda_handle, "cudaDriverGetVersion" );
#if defined(WIN32)
		cudaRuntimeGetVersion = cu_cudaRuntimeGetVersion;
#else
		cudaRuntimeGetVersion =
			(cuda_t)dlsym( cuda_handle, "cudaRuntimeGetVersion" );
#endif

		getBasicProps = cu_getBasicProps;

		return cuda_handle;
	} else if( cudart_handle ) {
		cudaGetDeviceCount =
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
			fprintf( stderr, "Unable to find cudaRuntimGetVersion().\n" );
			dlclose( cudart_handle );
			return NULL;
		}

		return cudart_handle;
	} else {
		return NULL;
	}
}
