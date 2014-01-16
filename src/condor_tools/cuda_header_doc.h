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

enum cudaDeviceAttr
{
    CU_DEVICE_ATTRIBUTE_CLOCK_RATE = 13,                        /**< Typical clock frequency in kilohertz */
    CU_DEVICE_ATTRIBUTE_TEXTURE_ALIGNMENT = 14,                 /**< Alignment requirement for textures */
    CU_DEVICE_ATTRIBUTE_GPU_OVERLAP = 15,                       /**< Device can possibly copy memory and execute a kernel concurrently. Deprecated. Use instead CU_DEVICE_ATTRIBUTE_ASYNC_ENGINE_COUNT. */
    CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT = 16,              /**< Number of multiprocessors on device */
};

struct cudaDeviceProp {
	char name[256];
	size_t totalGlobalMem;
	size_t sharedMemPerBlock;
	int regsPerBlock;
	int warpSize;
	size_t memPitch;
	int maxThreadsPerBlock;
	int maxThreadsDim[3];
	int maxGridSize[3];
	int clockRate;
	size_t totalConstMem;
	int major;
	int minor;
	size_t textureAlignment;
	size_t texturePitchAlignment;
	int deviceOverlap;
	int multiProcessorCount;
	int kernelExecTimeoutEnabled;
	int integrated;
	int canMapHostMemory;
	int computeMode;
	int maxTexture1D;
	int maxTexture1DMipmap;
	int maxTexture1DLinear;
	int maxTexture2D[2];
	int maxTexture2DMipmap[2];
	int maxTexture2DLinear[3];
	int maxTexture2DGather[2];
	int maxTexture3D[3];
#if CUDART_VERSION > 5000
    int    maxTexture3DAlt[3];         /**< Maximum alternate 3D texture dimensions */
#endif
	int maxTextureCubemap;
	int maxTexture1DLayered[2];
	int maxTexture2DLayered[3];
	int maxTextureCubemapLayered[2];
	int maxSurface1D;
	int maxSurface2D[2];
	int maxSurface3D[3];
	int maxSurface1DLayered[2];
	int maxSurface2DLayered[3];
	int maxSurfaceCubemap;
	int maxSurfaceCubemapLayered[2];
	size_t surfaceAlignment;
	int concurrentKernels;
	int ECCEnabled;
	int pciBusID;
	int pciDeviceID;
	int pciDomainID;
	int tccDriver;
	int asyncEngineCount;
	int unifiedAddressing;
	int memoryClockRate;
	int memoryBusWidth;
	int l2CacheSize;
	int maxThreadsPerMultiProcessor;
#if CUDART_VERSION > 5000
    int    streamPrioritiesSupported;  /**< Device supports stream priorities */
#endif

	// HTCondor defined extra space, to leave room for the CUDA runtime
	// to return new fields in later versions
	int htcondor_reserved[100];
};

typedef enum cudaError cudaError_t;


//#if defined(__cplusplus)
}
//#endif /* __cplusplus */
