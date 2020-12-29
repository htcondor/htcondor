#ifndef   _CUDA_DEVICE_ENUMERATION_H
#define   _CUDA_DEVICE_ENUMERATION_H

char hex_digit( unsigned char n );
char * print_uuid( char * buf, int bufsize, const unsigned char uuid[16] );

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
typedef nvmlReturn_t (*nvml_dghbc)(const char *, nvmlDevice_t *);
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
GPUFP nvml_dghbc         nvmlDeviceGetHandleByUUID;
GPUFP cc_nvml            nvmlErrorString;


typedef nvmlReturn_t (*nvml_device_uint)( nvmlDevice_t, unsigned int * );
typedef nvmlReturn_t (*nvml_dgt)( nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int * );
typedef nvmlReturn_t (*nvml_dgtee)( nvmlDevice_t, nvmlMemoryErrorType_t, nvmlEccCounterType_t, unsigned long long * );
typedef nvmlReturn_t (*nvml_dgu)( nvmlDevice_t, char *, unsigned int );

GPUFP nvml_device_uint  nvmlDeviceGetFanSpeed;
GPUFP nvml_device_uint  nvmlDeviceGetPowerUsage;
GPUFP nvml_dgt          nvmlDeviceGetTemperature;
GPUFP nvml_dgtee        nvmlDeviceGetTotalEccErrors;

GPUFP nvml_dgu          nvmlDeviceGetUUID;

dlopen_return_t setNVMLFunctionPointers();
void setSimulatedNVMLFunctionPointers();

//
// findNVMLDeviceHandle() is current a convenience function that converts
// a BasicProp's UUID into the string that NVML wants.  Later, it will
// also scan the MIG devices for that UUID, since as of the latest NVML
// documentation (date June 2020), nvmlDeviceGetHandleByUUID() won't
// return MIG device handles.
//

typedef nvmlReturn_t (* fndh)(unsigned char uuid[16], nvmlDevice_t * device);
GPUFP fndh findNVMLDeviceHandle;

nvmlReturn_t nvml_findNVMLDeviceHandle(unsigned char uuid[16], nvmlDevice_t * device);

//
// CUDA (device enumeration).
//

class BasicProps;

typedef CUresult (CUDACALL* cu_uint_t)(unsigned int);
typedef cudaError_t (CUDACALL* cuda_t)(int *);
typedef cudaError_t (CUDACALL* dev_basic_props)(int, BasicProps *);
typedef cudaError_t (CUDACALL* cuda_DevicePropBuf_int)(struct cudaDevicePropBuffer *, int);

GPUFP cu_uint_t                 cuInit;
GPUFP cuda_t                    cuDeviceGetCount;
GPUFP cuda_t                    cudaDriverGetVersion;
GPUFP cuda_t                    cudaRuntimeGetVersion;

GPUFP dev_basic_props           getBasicProps;
GPUFP cuda_DevicePropBuf_int    cudaGetDevicePropertiesOfIndeterminateStructure;

typedef void * cudev;
typedef cudaError_t (CUDACALL* cuda_dev_int_t)(cudev *, int);
typedef cudaError_t (CUDACALL* cuda_name_t)(char *, int, cudev);
typedef CUresult (CUDACALL * cuda_uuid_t)(unsigned char uuid[16], cudev);
typedef CUresult (CUDACALL * cuda_pciid_t)(char *, int, cudev);
typedef cudaError_t (CUDACALL* cuda_cc_t)(int *, int *, cudev);
typedef cudaError_t (CUDACALL* cuda_size_t)(size_t *, cudev);
typedef cudaError_t (CUDACALL* cuda_ga_t)(int *, int, cudev);

GPUFP cuda_dev_int_t            cuDeviceGet;
GPUFP cuda_uuid_t               cuDeviceGetUuid;
GPUFP cuda_name_t               cuDeviceGetName;
GPUFP cuda_size_t               cuDeviceTotalMem;
GPUFP cuda_pciid_t              cuDeviceGetPCIBusId;
GPUFP cuda_ga_t                 cuDeviceGetAttribute;
GPUFP cuda_cc_t                 cuDeviceComputeCapability;

dlopen_return_t setCUDAFunctionPointers( bool force_nvcuda = false, bool force_cudart = false );
void setSimulatedCUDAFunctionPointers();

#undef GPUFP

//
// We can simulate CUDA device enumeration for testing purposes.
//

extern int sim_index;
extern int sim_device_count;
extern const int sim_index_max;

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
