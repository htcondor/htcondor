#ifndef   _HIP_DEVICE_ENUMERATION_H
#define   _HIP_DEVICE_ENUMERATION_H

#if defined(WIN32)
#define HIP_CALL __stdcall
#else
#define HIP_CALL
#endif

#include "BasicProps.h"


int HIP_CALL hip_GetDeviceCount( int * pcount );
bool enumerateHIPDevices( std::vector< BasicProps > & devices );

dlopen_return_t setHIPFunctionPointers();

#endif /* _HIP_DEVICE_ENUMERATION_H */
