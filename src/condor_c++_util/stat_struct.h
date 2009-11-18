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

#ifndef STAT_STRUCT_H
#define STAT_STRUCT_H

#include "config.h"

	// Define a common "struct stat" type
#if HAVE_STAT64
typedef struct stat64 StatStructType;
#elif HAVE__STATI64	/* Win32 */
typedef struct _stati64 StatStructType;
#else
typedef struct stat StatStructType;
#endif

	// Types of individual elements in the "struct stat" elements
#if HAVE__STATI64	/* Win32 */
typedef _ino_t StatStructInode;
typedef _dev_t StatStructDev;
typedef unsigned short StatStuctMode;
typedef short StatStuctNlink;
typedef short StatStuctUID;
typedef short StatStuctGID;
typedef _off_t StatStructOff;
#undef STAT_STRUCT_HAVE_BLOCK_SIZE
#undef STAT_STRUCT_HAVE_BLOCK_COUNT
typedef time_t StatStructTime;

#else	/* UNIX & variants */
typedef ino_t StatStructInode;
typedef dev_t StatStructDev;
typedef mode_t StatStructMode;
typedef nlink_t StatStructNlink;
typedef uid_t StatStructUID;
typedef gid_t StatStructGID;
typedef off_t StatStructOff;
#define STAT_STRUCT_HAVE_BLOCK_SIZE		1
typedef blksize_t StatStructBlockSize;
#define STAT_STRUCT_HAVE_BLOCK_COUNT	0
typedef blkcnt_t StatStructBlockCount;
typedef time_t StatStructTime;
#endif

#endif
