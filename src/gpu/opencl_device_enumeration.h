#ifndef   _OPENCL_DEVICE_ENUMERATION_H
#define   _OPENCL_DEVICE_ENUMERATION_H

extern int g_cl_cCuda;
extern int g_cl_ixFirstCuda;
extern int g_cl_cHip;
extern int g_cl_ixFirstHip;
extern std::vector<cl_device_id> cl_gpu_ids;

#if defined(WIN32)
#define OCL_CALL __stdcall
#else
#define OCL_CALL
#endif

clReturn OCL_CALL ocl_GetDeviceCount( int * pcount );

clReturn oclGetInfo( cl_device_id did, cl_e_device_info eInfo, std::string & val );
clReturn oclGetInfo( cl_platform_id plid, cl_e_platform_info eInfo, std::string & val );

template <class t>
clReturn oclGetInfo( cl_device_id did, cl_e_device_info eInfo, t & val );

dlopen_return_t setOCLFunctionPointers();

#endif /* _OPENCL_DEVICE_ENUMERATION_H */
