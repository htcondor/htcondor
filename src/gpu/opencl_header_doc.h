/*
 * Copyright (c) 2008-2009 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.

The contents of this file are excerpts form cl.h, from the OpenCL SDK
They are the minimum definitions necessary to dynamically link to OpenCL.so
and query for devices in the condor_gpu_discovery application.

*/

#ifndef __OPENCL_HEADER_DOC_H__
#define __OPENCL_HEADER_DOC_H__

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#ifdef WIN32
#define CL_API_CALL __stdcall
#else
#define CL_API_CALL
#endif


typedef struct _cl_platform_id * cl_platform_id;
typedef int clReturn;
enum {
	CL_SUCCESS                                =  0,
	CL_DEVICE_NOT_FOUND                       = -1,
	CL_DEVICE_NOT_AVAILABLE                   = -2,
	CL_COMPILER_NOT_AVAILABLE                 = -3,
	CL_PLATFORM_NOT_FOUND_KHR                 = -1001,
};
typedef clReturn (CL_API_CALL* clGetPlatformIDs_t)(unsigned int, cl_platform_id *, unsigned int *);
enum cl_e_platform_info {
	CL_PLATFORM_PROFILE = 0x900,
	CL_PLATFORM_VERSION = 0x901,
	CL_PLATFORM_NAME    = 0x902,
	CL_PLATFORM_VENDOR	= 0x903,
	CL_PLATFORM_EXTENSIONS = 0x904,
};
typedef clReturn (CL_API_CALL* clGetPlatformInfo_t)(cl_platform_id, cl_e_platform_info, size_t, void*, size_t*);
typedef struct _cl_device_id * cl_device_id;
enum { // used 
	CL_DEVICE_TYPE_NONE = 0,
	CL_DEVICE_TYPE_DEFAULT     = (1 << 0),
	CL_DEVICE_TYPE_CPU         = (1 << 1),
	CL_DEVICE_TYPE_GPU         = (1 << 2),
	CL_DEVICE_TYPE_ACCELERATOR = (1 << 3),
	CL_DEVICE_TYPE_ALL         = 0xFFFFFFFF
};
typedef unsigned long long cl_device_type; // device type is type cl_bitfield which is cl_ulong which is unsigned long long
typedef clReturn (CL_API_CALL* clGetDeviceIDs_t)(cl_platform_id, cl_device_type, unsigned int, cl_device_id*, unsigned int *);
enum cl_e_device_info {
	CL_DEVICE_TYPE                =  0x1000, // enum cl_device_type
	CL_DEVICE_VENDOR_ID           =  0x1001, // uint vendor unique id
	CL_DEVICE_MAX_COMPUTE_UNITS   =  0x1002, // uint
	CL_DEVICE_MAX_WORK_GROUP_SIZE =  0x1004, // size_t
	CL_DEVICE_MAX_CLOCK_FREQUENCY =  0x100C, // uint
	CL_DEVICE_GLOBAL_MEM_SIZE     =  0x101F, // size_t
	CL_DEVICE_AVAILABLE           =  0x1027, // bool
	CL_DEVICE_EXECUTION_CAPABILITIES = 0x1029, // enum CL_EXEC_KERNEL | CL_EXEC_NATIVE_KERNEL
	CL_DEVICE_ERROR_CORRECTION_SUPPORT = 0x1024, // bool
	CL_DEVICE_NAME    =  0x102B,			   // string
	CL_DEVICE_VENDOR  =  0x102C,			   // string
	CL_DRIVER_VERSION =  0x102D,			   // string of form "major.minor"
	CL_DEVICE_PROFILE =  0x102E,			   // string 
	CL_DEVICE_VERSION =  0x102F,			   // string of form "OpenCL m.n vend" where m & n are ints, and vend is anything

};
typedef clReturn (CL_API_CALL* clGetDeviceInfo_t)(cl_device_id, cl_e_device_info, size_t, void*, size_t*);


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif
