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
#include "link.h"

#if defined(WIN32)

int
link(const char* oldpath, const char* newpath)
{
	if (CreateHardLink(newpath, oldpath, NULL)) {
		return 0;
	}
	else {
		DWORD error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND) {
			errno = ENOENT;
		}
		else {
			// for anything but the ENOENT-equivalent, we'll
			// log the error and set errno to something generic-
			// sounding ("operation not permitted")
			dprintf(D_ALWAYS,
			        "link: CreateHardLink error: %u\n",
			        (unsigned)error);
			errno = EPERM;
		}
		return -1;
	}
}

int
link_count(const char* path)
{
	HANDLE handle = CreateFile(path,
	                           GENERIC_READ,
	                           FILE_SHARE_READ |
	                               FILE_SHARE_WRITE |
	                               FILE_SHARE_DELETE,
	                           NULL,
	                           OPEN_EXISTING,
	                           0,
	                           NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		dprintf(D_ALWAYS,
		        "link_count: CreateFile error: %u\n",
		        (unsigned)GetLastError());
		return -1;
	}
	BY_HANDLE_FILE_INFORMATION bhfi;
	BOOL ret = GetFileInformationByHandle(handle, &bhfi);
	DWORD err = GetLastError();
	CloseHandle(handle);
	if (!ret) {
		dprintf(D_ALWAYS,
		        "link_count: GetFileInformationByHandle error: %u\n",
		        (unsigned)err);
		return -1;
	}
	return (int)bhfi.nNumberOfLinks;
}

#else

int
link_count(const char* path)
{
	struct stat st;
	if (stat(path, &st) == -1) {
		dprintf(D_ALWAYS,
		        "link_count: stat error on %s: %s\n",
		        path,
		        strerror(errno));
		return -1;
	}
	return st.st_nlink;
}

#endif
