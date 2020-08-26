#ifndef   _CUDA_DEVICE_ENUMERATION_H
#define   _CUDA_DEVICE_ENUMERATION_H

#if defined(WIN32)
#define CUDACALL __stdcall
#else
#define CUDACALL
#endif

//
// NVML
//

typedef nvmlReturn_t (*nvml_void)(void);
typedef nvmlReturn_t (*nvml_dghbi)(unsigned int, nvmlDevice_t *);
typedef nvmlReturn_t (*nvml_dghbp)(const char *, nvmlDevice_t *);
typedef nvmlReturn_t (*nvml_unsigned_int)(unsigned int *);
typedef nvmlReturn_t (*nvml_dgs)( nvmlDevice_t, nvmlSamplingType_t, unsigned long long, nvmlValueType_t *, unsigned int *, nvmlSample_t * );
typedef nvmlReturn_t (*nvml_dm)( nvmlDevice_t, nvmlMemory_t * );
typedef const char * (*cc_nvml)( nvmlReturn_t );

#ifndef   DEFINE_GPU_FUNCTION_POINTERS
	#define GPUFP extern
#else
	#define GPUFP
#endif /* DEFINE_GPU_FUNCTION_POINTERS */

GPUFP nvml_void          nvmlInit;
GPUFP nvml_void          nvmlShutdown;
GPUFP nvml_unsigned_int  nvmlDeviceGetCount;
GPUFP nvml_dgs           nvmlDeviceGetSamples;
GPUFP nvml_dm            nvmlDeviceGetMemoryInfo;
GPUFP nvml_dghbi         nvmlDeviceGetHandleByIndex;
GPUFP nvml_dghbp         nvmlDeviceGetHandleByPciBusId;
GPUFP cc_nvml            nvmlErrorString;

dlopen_return_t setNVMLFunctionPointers( /* bool simulate = false */ );

//
// CUDA (device enumeration).
//
class BasicProps;

typedef CUresult (CUDACALL* cu_uint_t)(unsigned int);
typedef cudaError_t (CUDACALL* cuda_t)(int *);
typedef cudaError_t (CUDACALL* dev_basic_props)(int, BasicProps *);
typedef cudaError_t (CUDACALL* cuda_DevicePropBuf_int)(struct cudaDevicePropBuffer *, int);

GPUFP cu_uint_t cuInit;
GPUFP cuda_t cudaGetDeviceCount;
GPUFP cuda_t cudaDriverGetVersion;
GPUFP cuda_t cudaRuntimeGetVersion;
GPUFP dev_basic_props getBasicProps;
GPUFP cuda_DevicePropBuf_int cudaGetDevicePropertiesOfIndeterminateStructure;

typedef void * cudev;
typedef cudaError_t (CUDACALL* cuda_dev_int_t)(cudev *, int);
typedef cudaError_t (CUDACALL* cuda_name_t)(char *, int, cudev);
typedef CUresult (CUDACALL * cuda_uuid_t)(unsigned char uuid[16], cudev);
typedef CUresult (CUDACALL * cuda_pciid_t)(char *, int, cudev);
typedef cudaError_t (CUDACALL* cuda_cc_t)(int *, int *, cudev);
typedef cudaError_t (CUDACALL* cuda_size_t)(size_t *, cudev);
typedef cudaError_t (CUDACALL* cuda_ga_t)(int *, int, cudev);

GPUFP cuda_dev_int_t cuDeviceGet;
GPUFP cuda_name_t cuDeviceGetName;
GPUFP cuda_uuid_t cuDeviceGetUuid;
GPUFP cuda_pciid_t cuDeviceGetPCIBusId;
GPUFP cuda_cc_t cuDeviceComputeCapability;
GPUFP cuda_size_t cuDeviceTotalMem;
GPUFP cuda_ga_t cuDeviceGetAttribute;

dlopen_return_t setCUDAFunctionPointers( /* bool simulate = false */ );

#undef GPUFP

// basic device properties we can query from the driver
class BasicProps {
	public:
		BasicProps();

		std::string   name;
		unsigned char uuid[16];
		char          pciId[16];
		size_t        totalGlobalMem;
		int           ccMajor;
		int           ccMinor;
		int           multiProcessorCount;
		int           clockRate;
		int           ECCEnabled;

		bool hasUUID() { return uuid[0] || uuid[6] || uuid[8]; }
		const char * printUUID(char* buf, int bufsiz);
};

bool enumerateCUDADevices( std::vector< BasicProps > & devices );

#endif /* _CUDA_DEVICE_ENUMERATION_H */
