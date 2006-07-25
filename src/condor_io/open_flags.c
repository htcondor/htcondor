/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

 
#include "condor_common.h"
#include <fcntl.h>

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

#ifdef WIN32
extern "C" {
#endif

int open_flags_encode(int old_flags)
{
	int i;
	int new_flags = 0;

	for (i = 0; i < (sizeof(FlagList) / sizeof(FlagList[0])); i++) {
		if (old_flags & FlagList[i].system_flag) {
			new_flags |= FlagList[i].condor_flag;
		}
	}
	return new_flags;
}

int open_flags_decode(int old_flags)
{
	int i;
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

#ifdef WIN32
}  /* end of extern "C" */
#endif

