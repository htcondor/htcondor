/***************************************************************
 *
 * Copyright (C) 2014, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"	// has declaration for GetLastErrorString()
#include "stl_string_utils.h"  // for chomp( std::string )

/* Windows-specific function to provide strerror() like functionality for
 * Win32 and Winsock error codes. Returns an error string from a static
 * buffer (no need to free it), just like strerror.
 * errCode should come from GetLastError() or WSAGetLastError().
 * If errCode is 0 (default), then GetLastErrorString will invoke
 * GetLastError itself.
 *
 * Note: must include "condor_debug.h"
 */

const char*
GetLastErrorString(DWORD iErrCode)
{
	static std::string szError;
	LPTSTR lpMsgBuf = NULL;

	if ( iErrCode == 0 ) {
		iErrCode = GetLastError();
	}

	DWORD rc = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		(DWORD)iErrCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL );

	if ( rc > 0 && lpMsgBuf ) {
		szError =  (char *) lpMsgBuf;
		LocalFree(lpMsgBuf);  // must dealloc lpMsgBuf!
		// Get rid of the the ending /r/n we get from FormatMessage
		chomp( szError);
	} else {
		szError = "Unknown error";
	}

	return szError.c_str();
}

