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


 
#include "condor_common.h"
#include <fcntl.h>
#include "utilfns.h"

// Ugh. On Windows, this file is compiled as C++ (since that's a
// per-project setting). We want to make sure to get the linkage
// right.
//
#if defined(WIN32) && defined(__cplusplus)
#define OPEN_FLAGS_LINKAGE extern "C"
#else
#define OPEN_FLAGS_LINKAGE
#endif

//OPEN_FLAGS_LINKAGE int open_flags_encode(int old_flags);
//OPEN_FLAGS_LINKAGE int open_flags_decode(int old_flags);

#define CONDOR_O_RDONLY 0x0000
#define CONDOR_O_WRONLY 0x0001
#define CONDOR_O_RDWR	0x0002
#define CONDOR_O_CREAT	0x0100
#define CONDOR_O_TRUNC	0x0200
#define CONDOR_O_EXCL	0x0400
#define CONDOR_O_NOCTTY	0x0800
#define CONDOR_O_APPEND 0x1000

static struct {
	int		system_flag;
	int		condor_flag;
} FlagList[] = {
	{O_RDONLY, CONDOR_O_RDONLY},
	{O_WRONLY, CONDOR_O_WRONLY},
	{O_RDWR, CONDOR_O_RDWR},
	{O_CREAT, CONDOR_O_CREAT},
	{O_TRUNC, CONDOR_O_TRUNC},
#ifndef WIN32
	{O_NOCTTY, CONDOR_O_NOCTTY},
#endif
	{O_EXCL, CONDOR_O_EXCL},
	{O_APPEND, CONDOR_O_APPEND}
};

OPEN_FLAGS_LINKAGE int open_flags_encode(int old_flags)
{
	unsigned int i;
	int new_flags = 0;

	for (i = 0; i < (sizeof(FlagList) / sizeof(FlagList[0])); i++) {
		if (old_flags & FlagList[i].system_flag) {
			new_flags |= FlagList[i].condor_flag;
		}
	}
	return new_flags;
}

OPEN_FLAGS_LINKAGE int open_flags_decode(int old_flags)
{
	unsigned int i;
	int new_flags = 0;

	for (i = 0; i < (sizeof(FlagList) / sizeof(FlagList[0])); i++) {
		if (old_flags & FlagList[i].condor_flag) {
			new_flags |= FlagList[i].system_flag;
		}
	}
#if defined(WIN32)
	new_flags |= _O_BINARY;	// always open in binary mode
#endif
	return new_flags;
}


