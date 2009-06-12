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
#include "truncate.h"

#ifdef WIN32

/* gives us the truncate() and ftruncate() functions on Win32 */

int ftruncate( int file_handle, long size ) {

	if ( filelength(file_handle) > size ) {
		return chsize(file_handle, size);
	} else {
		return 0;
	}
}

int truncate( const char *path, long size ) {
	int fh, ret;
	
	if ( fh = safe_open_wrapper(path, O_RDWR | O_CREAT, S_IREAD | S_IWRITE) != -1 ) {
		ret = ftruncate(fh, size);
		close(fh);
		return ret;
	} else {
		return -1; // couldn't open file, and _open has set errno.
	}
}

#endif /* WIN32 */
