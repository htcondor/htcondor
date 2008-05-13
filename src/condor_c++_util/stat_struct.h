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

#include "condor_common.h"

	// Define a "standard" StatBuf type
#if HAVE_STAT64
typedef struct stat64 StatStructType;
typedef ino_t StatStructInode;
#elif HAVE__STATI64
typedef struct _stati64 StatStructType;
typedef _ino_t StatStructInode;
#else
typedef struct stat StatStructType;
typedef ino_t StatStructInode;
#endif

#endif
