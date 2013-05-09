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
    cudaSuccess = 0
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
};

typedef enum cudaError cudaError_t;


//#if defined(__cplusplus)
}
//#endif /* __cplusplus */
