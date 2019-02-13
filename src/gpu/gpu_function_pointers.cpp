#include <stdio.h>

#include "pi_dynlink.h"
#include "nvml_stub.h"
#include "cuda_header_doc.h"
#include "gpu_function_pointers.h"

//
// NVML
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
// CUDA
//

// Internal wrappers (sigh).
extern cudaError_t CUDACALL cu_getBasicProps( int devID, BasicProps * p );
extern cudaError_t CUDACALL cuda9_getBasicProps( int devID, BasicProps * p );
extern cudaError_t CUDACALL cuda10_getBasicProps( int devID, BasicProps * p );
extern cudaError_t CUDACALL cu_cudaRuntimeGetVersion( int * pver );

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
