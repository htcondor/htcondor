/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#define STAT_SIZE sizeof(struct stat)
#define STATFS_SIZE sizeof(struct statfs)
#define GID_T_SIZE sizeof(gid_t)
#define INT_SIZE sizeof(int)
#define LONG_SIZE sizeof(long)
#define FLOAT_SIZE sizeof(float)
#define FD_SET_SIZE sizeof(fd_set)
#define TIMEVAL_SIZE sizeof(struct timeval)
#define TIMEVAL_ARRAY_SIZE (sizeof(struct timeval) * 2)
#define TIMEZONE_SIZE sizeof(struct timezone)
#define FILEDES_SIZE sizeof(struct filedes)
#define RLIMIT_SIZE sizeof(struct rlimit)
#define UTSNAME_SIZE sizeof(struct utsname)
#define POLLFD_SIZE sizeof(struct pollfd)
#define RUSAGE_SIZE sizeof(struct rusage)
#define MAX_STRING 1024
#define EIGHT 8
#define STATFS_ARRAY_SIZE (rval * sizeof(struct statfs))
#define PROC_SIZE sizeof(PROC)
#define STARTUP_INFO_SIZE sizeof(STARTUP_INFO)
#define SIZE_T_SIZE sizeof(size_t)
#define U_SHORT_SIZE sizeof(u_short)
#define U_INT_SIZE sizeof(u_int)
#define NEG_ONE -1
#define UTIMBUF_SIZE sizeof(struct utimbuf)

#if defined( HAS_64BIT_STRUCTS )
#define STAT64_SIZE sizeof(struct stat64)
#define RLIMIT64_SIZE sizeof(struct rlimit64)
#endif
