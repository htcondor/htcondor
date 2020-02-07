/*
ALL NVIDIA DESIGN SPECIFICATIONS, REFERENCE BOARDS, FILES, DRAWINGS, 
DIAGNOSTICS, LISTS, AND OTHER DOCUMENTS (TOGETHER AND SEPARATELY, "MATERIALS") 
ARE BEING PROVIDED "AS IS." NVIDIA MAKES NO WARRANTIES, EXPRESSED, IMPLIED, 
STATUTORY, OR OTHERWISE WITH RESPECT TO THE MATERIALS, AND EXPRESSLY DISCLAIMS 
ALL IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A 
PARTICULAR PURPOSE.


Information furnished is believed to be accurate and reliable. However, NVIDIA 
Corporation assumes no responsibility for the consequences of use of such 
information or for any infringement of patents or other rights of third parties 
that may result from its use. No license is granted by implication of otherwise 
under any patent rights of NVIDIA Corporation. Specifications mentioned in this 
publication are subject to change without notice. This publication supersedes 
and replaces all other information previously supplied. NVIDIA Corporation 
products are not authorized as critical components in life support devices or 
systems without express written approval of NVIDIA Corporation. 

NVIDIA and the NVIDIA logo are trademarks or registered trademarks of NVIDIA 
Corporation in the U.S. and other countries. Other company and product names 
may be trademarks of the respective companies with which they are associated.

© 2007-2012 NVIDIA Corporation. All rights reserved.
*/

#define CUDART_VERSION  5000

//#if defined(__cplusplus)
extern "C" {
//#endif /* __cplusplus */

enum cudaError
{
	cudaSuccess = 0,
	// TJ pulled these from the 5.5 SDK for Windows.
	cudaErrorInitializationError = 3, // CUDA driver and runtime could not be initialized
	cudaErrorInvalidDevice = 10,	  // device ordinal is not valid
	cudaErrorInvalidValue = 11,		  // one or more params to an API call are not valid
	cudaErrorInsufficientDriver = 35, // the driver is older than the CUDA runtime
	cudaErrorNoDevice = 38,			  // no CUDA capable devices were detected
	cudaErrorDevicesUnavailable = 46, //  all CUDA devices are busy or unavailable
	cudaErrorNotPermitted = 70,		  // the attempted operation is not permitted
	cudaErrorNotSupported = 71,		  // the attempted operation is not supported on the current system or device

};

// driver errors
typedef enum cudaError_enum {
	/**
	 * The API call returned with no errors. In the case of query calls, this
	 * also means that the operation being queried is complete (see
	 * ::cuEventQuery() and ::cuStreamQuery()).
	 */
	CUDA_SUCCESS = 0,

	/**
	 * This indicates that one or more of the parameters passed to the API call
	 * is not within an acceptable range of values.
	 */
	CUDA_ERROR_INVALID_VALUE = 1,

	/**
	 * The API call failed because it was unable to allocate enough memory to
	 * perform the requested operation.
	 */
	CUDA_ERROR_OUT_OF_MEMORY = 2,

	/**
	 * This indicates that the CUDA driver has not been initialized with
	 * ::cuInit() or that initialization has failed.
	 */
	CUDA_ERROR_NOT_INITIALIZED = 3,

	/**
	 * This indicates that no CUDA-capable devices were detected by the installed
	 * CUDA driver.
	 */
	CUDA_ERROR_NO_DEVICE = 100,

	/**
	 * This indicates that the device ordinal supplied by the user does not
	 * correspond to a valid CUDA device.
	 */
	CUDA_ERROR_INVALID_DEVICE = 101,

	// there are more, but these are the ones of interest to gpu discovery
} CUresult;

enum cudaDeviceAttr
{
    CU_DEVICE_ATTRIBUTE_CLOCK_RATE = 13,                        /**< Typical clock frequency in kilohertz */
    CU_DEVICE_ATTRIBUTE_TEXTURE_ALIGNMENT = 14,                 /**< Alignment requirement for textures */
    CU_DEVICE_ATTRIBUTE_GPU_OVERLAP = 15,                       /**< Device can possibly copy memory and execute a kernel concurrently. Deprecated. Use instead CU_DEVICE_ATTRIBUTE_ASYNC_ENGINE_COUNT. */
    CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT = 16,              /**< Number of multiprocessors on device */
    CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY = 19,               /**< Device can map host memory into CUDA address space */
    CU_DEVICE_ATTRIBUTE_ECC_ENABLED = 32,                       /**< Device has ECC support enabled */
    CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR = 39,    /**< Maximum resident threads per multiprocessor */

};

// for CUDA 10, the cudeDeviceProp structure is VERY incompatible with previous versions!!
// so we split it into 2 parts.  the Strings, and the Ints

struct cudaDevicePropStrings
{
	char         name[256];                  /**< ASCII string identifying device */

	// fields below this marker will exist only for CUDA 10 and later
	unsigned char uuid[16];                   /**< 16-byte unique identifier */
	char          luid[8];                    /**< 8-byte locally unique identifier. Value is undefined on TCC and non-Windows platforms */
	unsigned int  luidDeviceNodeMask;         /**< LUID device node mask. Value is undefined on TCC and non-Windows platforms */
};

struct cudaDevicePropInts
{
	size_t       totalGlobalMem;             /**< Global memory available on device in bytes */
	size_t       sharedMemPerBlock;          /**< Shared memory available per block in bytes */
	int          regsPerBlock;               /**< 32-bit registers available per block */
	int          warpSize;                   /**< Warp size in threads */
	size_t       memPitch;                   /**< Maximum pitch in bytes allowed by memory copies */
	int          maxThreadsPerBlock;         /**< Maximum number of threads per block */
	int          maxThreadsDim[3];           /**< Maximum size of each dimension of a block */
	int          maxGridSize[3];             /**< Maximum size of each dimension of a grid */
	int          clockRate;                  /**< Clock frequency in kilohertz */
	size_t       totalConstMem;              /**< Constant memory available on device in bytes */
	int          ccMajor;                    /**< Major compute capability */
	int          ccMinor;                    /**< Minor compute capability */
	size_t       textureAlignment;           /**< Alignment requirement for textures */
	size_t       texturePitchAlignment;      /**< Pitch alignment requirement for texture references bound to pitched memory */
	int          deviceOverlap;              /**< Device can concurrently copy memory and execute a kernel. Deprecated. Use instead asyncEngineCount. */
	int          multiProcessorCount;        /**< Number of multiprocessors on device */
	int          kernelExecTimeoutEnabled;   /**< Specified whether there is a run time limit on kernels */
	int          integrated;                 /**< Device is integrated as opposed to discrete */
	int          canMapHostMemory;           /**< Device can map host memory with cudaHostAlloc/cudaHostGetDevicePointer */
	int          computeMode;                /**< Compute mode (See ::cudaComputeMode) */
	int          maxTexture1D;               /**< Maximum 1D texture size */
	int          maxTexture1DMipmap;         /**< Maximum 1D mipmapped texture size */
	int          maxTexture1DLinear;         /**< Maximum size for 1D textures bound to linear memory */
	int          maxTexture2D[2];            /**< Maximum 2D texture dimensions */
	int          maxTexture2DMipmap[2];      /**< Maximum 2D mipmapped texture dimensions */
	int          maxTexture2DLinear[3];      /**< Maximum dimensions (width, height, pitch) for 2D textures bound to pitched memory */
	int          maxTexture2DGather[2];      /**< Maximum 2D texture dimensions if texture gather operations have to be performed */
	int          maxTexture3D[3];            /**< Maximum 3D texture dimensions */
	int          maxTexture3DAlt[3];         /**< Maximum alternate 3D texture dimensions */
	int          maxTextureCubemap;          /**< Maximum Cubemap texture dimensions */
	int          maxTexture1DLayered[2];     /**< Maximum 1D layered texture dimensions */
	int          maxTexture2DLayered[3];     /**< Maximum 2D layered texture dimensions */
	int          maxTextureCubemapLayered[2];/**< Maximum Cubemap layered texture dimensions */
	int          maxSurface1D;               /**< Maximum 1D surface size */
	int          maxSurface2D[2];            /**< Maximum 2D surface dimensions */
	int          maxSurface3D[3];            /**< Maximum 3D surface dimensions */
	int          maxSurface1DLayered[2];     /**< Maximum 1D layered surface dimensions */
	int          maxSurface2DLayered[3];     /**< Maximum 2D layered surface dimensions */
	int          maxSurfaceCubemap;          /**< Maximum Cubemap surface dimensions */
	int          maxSurfaceCubemapLayered[2];/**< Maximum Cubemap layered surface dimensions */
	size_t       surfaceAlignment;           /**< Alignment requirements for surfaces */
	int          concurrentKernels;          /**< Device can possibly execute multiple kernels concurrently */
	int          ECCEnabled;                 /**< Device has ECC support enabled */
	int          pciBusID;                   /**< PCI bus ID of the device */
	int          pciDeviceID;                /**< PCI device ID of the device */
	int          pciDomainID;                /**< PCI domain ID of the device */
	int          tccDriver;                  /**< 1 if device is a Tesla device using TCC driver, 0 otherwise */
	int          asyncEngineCount;           /**< Number of asynchronous engines */
	int          unifiedAddressing;          /**< Device shares a unified address space with the host */
	int          memoryClockRate;            /**< Peak memory clock frequency in kilohertz */
	int          memoryBusWidth;             /**< Global memory bus width in bits */
	int          l2CacheSize;                /**< Size of L2 cache in bytes */
	int          maxThreadsPerMultiProcessor;/**< Maximum resident threads per multiprocessor */
	int          streamPrioritiesSupported;  /**< Device supports stream priorities */

	// fields below this line only exist on CUDA10 and later

	int          globalL1CacheSupported;     /**< Device supports caching globals in L1 */
	int          localL1CacheSupported;      /**< Device supports caching locals in L1 */
	size_t       sharedMemPerMultiprocessor; /**< Shared memory available per multiprocessor in bytes */
	int          regsPerMultiprocessor;      /**< 32-bit registers available per multiprocessor */
	int          managedMemory;              /**< Device supports allocating managed memory on this system */
	int          isMultiGpuBoard;            /**< Device is on a multi-GPU board */
	int          multiGpuBoardGroupID;       /**< Unique identifier for a group of devices on the same multi-GPU board */
	int          hostNativeAtomicSupported;  /**< Link between the device and the host supports native atomic operations */
	int          singleToDoublePrecisionPerfRatio; /**< Ratio of single precision performance (in floating-point operations per second) to double precision performance */
	int          pageableMemoryAccess;       /**< Device supports coherently accessing pageable memory without calling cudaHostRegister on it */
	int          concurrentManagedAccess;    /**< Device can coherently access managed memory concurrently with the CPU */
	int          computePreemptionSupported; /**< Device supports Compute Preemption */
	int          canUseHostPointerForRegisteredMem; /**< Device can access host registered memory at the same virtual address as the CPU */
	int          cooperativeLaunch;          /**< Device supports launching cooperative kernels via ::cudaLaunchCooperativeKernel */
	int          cooperativeMultiDeviceLaunch; /**< Device can participate in cooperative kernels launched via ::cudaLaunchCooperativeKernelMultiDevice */
	size_t       sharedMemPerBlockOptin;     /**< Per device maximum shared memory per block usable by special opt in */
	int          pageableMemoryAccessUsesHostPageTables; /**< Device accesses pageable memory via the host's page tables */
	int          directManagedMemAccessFromHost; /**< Host can directly access managed memory on the device without migration. */
};


// this is the buffer we pass to cudaGetDeviceProperties, because we don't know how big a buffer to send
struct cudaDevicePropBuffer {
	unsigned char props[sizeof(cudaDevicePropStrings) + sizeof(cudaDevicePropInts) + 100];
};

#define CUDA_9_AND_10 1

typedef enum cudaError cudaError_t;


//#if defined(__cplusplus)
}
//#endif /* __cplusplus */
