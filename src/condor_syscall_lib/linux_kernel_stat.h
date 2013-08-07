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

/* Versions of the `struct stat' data structure.  */

#ifndef _STAT_VER_LINUX_OLD
#  define _STAT_VER_LINUX_OLD 1
#endif

#ifndef _STAT_VER_KERNEL
#  define _STAT_VER_KERNEL    1
#endif

#ifndef _STAT_VER_SVR4
#  define _STAT_VER_SVR4      2
#endif

#ifndef _STAT_VER_LINUX
#  define _STAT_VER_LINUX     3
#endif



/*
  Definition of `struct stat' used in the Linux kernel.
  Taken from the GNU libc distribution.
*/

/* The linux madness with stat just never ends... Under glibc23, the fields
	labeled __unused{1,2,3} became used because the timespec structure now
	consume that space in the structure. The structure sizes between glibc22
	and glibc23 haven't changed, just the field name for a few things. */

#if defined(GLIBC23) || defined(GLIBC24) || defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)

struct kernel_stat
  {
    unsigned short int st_dev;
    unsigned short int __pad1;
#define _HAVE___PAD1
    unsigned long int st_ino;
    unsigned short int st_mode;
    unsigned short int st_nlink;
    unsigned short int st_uid;
    unsigned short int st_gid;
    unsigned short int st_rdev;
    unsigned short int __pad2;
#define _HAVE___PAD2
    unsigned long int st_size;
    unsigned long int st_blksize;
    unsigned long int st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    unsigned long int __unused4;
#define _HAVE___UNUSED4
    unsigned long int __unused5;
#define _HAVE___UNUSED5
  };

#else /* glibc22 */

struct kernel_stat
  {
    unsigned short int st_dev;
    unsigned short int __pad1;
#define _HAVE___PAD1
    unsigned long int st_ino;
    unsigned short int st_mode;
    unsigned short int st_nlink;
    unsigned short int st_uid;
    unsigned short int st_gid;
    unsigned short int st_rdev;
    unsigned short int __pad2;
#define _HAVE___PAD2
    unsigned long int st_size;
    unsigned long int st_blksize;
    unsigned long int st_blocks;
    unsigned long int st_atime;
    unsigned long int __unused1;
#define _HAVE___UNUSED1
    unsigned long int st_mtime;
    unsigned long int __unused2;
#define _HAVE___UNUSED2
    unsigned long int st_ctime;
    unsigned long int __unused3;
#define _HAVE___UNUSED3
    unsigned long int __unused4;
#define _HAVE___UNUSED4
    unsigned long int __unused5;
#define _HAVE___UNUSED5
  };
#endif
