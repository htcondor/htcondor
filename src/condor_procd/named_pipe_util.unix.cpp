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
#include "condor_debug.h"
#include "named_pipe_util.unix.h"

char*
named_pipe_make_client_addr(const char* orig_addr,
                            pid_t pid,
                            int serial_number)
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

static const char WATCHDOG_SUFFIX[] = ".watchdog";

char*
named_pipe_make_watchdog_addr(const char* orig_path)
{
	size_t orig_path_len = strlen(orig_path);
	char* watchdog_path = new char[orig_path_len + sizeof(WATCHDOG_SUFFIX)];
	ASSERT(watchdog_path != NULL);
	strcpy(watchdog_path, orig_path);
	strcpy(watchdog_path + orig_path_len, WATCHDOG_SUFFIX);

	return watchdog_path;
}

bool
named_pipe_create(const char* name, int& read_fd, int& write_fd)
{
	int read_fd_tmp;
	int write_fd_tmp;

	// ensure the FIFO doesn't already exist
	//
	// FIXME: we need to verify before we do this that the condor user
	// would be able to do this!
	//
	unlink(name);

	// make the FIFO node in the filesystem
	//
	if (mkfifo(name, 0600) == -1) {
		dprintf(D_ALWAYS,
		        "mkfifo of %s error: %s (%d)\n",
		        name,
		        strerror(errno),
		        errno);
		return false;
	}

	// open the pipe end that we'll be reading requests from
	// (we do this with O_NONBLOCK because otherwise we'd deadlock
	// waiting for someone to open the pipe for writing)
	//
	read_fd_tmp = safe_open_wrapper(name, O_RDONLY | O_NONBLOCK);
	if (read_fd_tmp == -1) {
		dprintf(D_ALWAYS,
		        "open for read-only of %s failed: %s (%d)\n",
		        name,
		        strerror(errno),
		        errno);
		return false;
	}

	// set the pipe back into blocking mode now that we have it open
	//
	int flags = fcntl(read_fd_tmp, F_GETFL);
	if (flags == -1) {
		dprintf(D_ALWAYS,
		        "fcntl error: %s (%d)\n",
		        strerror(errno),
		        errno);
		close(read_fd_tmp);
		return false;
	}
	if (fcntl(read_fd_tmp, F_SETFL, flags & ~O_NONBLOCK) == -1) {
		dprintf(D_ALWAYS,
		        "fcntl error: %s (%d)\n",
		        strerror(errno),
		        errno);
		close(read_fd_tmp);
		return false;
	}

	// open our FIFO for writing as well; we won't actually ever write
	// to this FD, but we keep this open to prevent EOF from ever being
	// read from read_fd
	//
	write_fd_tmp = safe_open_wrapper(name, O_WRONLY);
	if (write_fd_tmp == -1) {
		dprintf(D_ALWAYS,
		        "open for write-only of %s failed: %s (%d)\n",
		        name,
		        strerror(errno),
		        errno);
		close(read_fd_tmp);
		return false;
	}

	// looks like it all worked; return the read and write FDs
	//
	read_fd = read_fd_tmp;
	write_fd = write_fd_tmp;

	return true;
}
