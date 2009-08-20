/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


 

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
