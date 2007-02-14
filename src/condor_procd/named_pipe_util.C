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

#ifndef _NAMED_PIPE_UTIL_H
#define _NAMED_PIPE_UTIL_H

#include "condor_common.h"
#include "condor_debug.h"

char*
named_pipe_make_addr(const char* orig_addr, pid_t pid, int serial_number)
{
	// determine buffer size and allocate it; we need enough space for:
	//   - a copy of orig_addr
	//   - a '.' (dot)
	//   - the pid, in string form
	//   - another '.' (dot)
	//   - the serial number, in string form
	//   - the zero-terminator
	//
	const int MAX_INT_STR_LEN = 10;
	int addr_len = strlen(orig_addr) +
	               1 +
	               MAX_INT_STR_LEN +
	               1 +
	               MAX_INT_STR_LEN +
	               1;
	char* addr = new char[addr_len];
	ASSERT(addr != NULL);

	// use snprintf to fill in the buffer
	//
	int ret = snprintf(addr,
	                   addr_len,
	                   "%s.%u.%u",
	                   orig_addr,
	                   pid,
	                   serial_number);
	if (ret < 0) {
		EXCEPT("snprintf error: %s (%d)", strerror(errno), errno);
	}
	if (ret >= addr_len) {
		EXCEPT("error: pid string would exceed %d chars",
		       MAX_INT_STR_LEN);
	}

	return addr;
}

#endif
