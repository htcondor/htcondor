/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
