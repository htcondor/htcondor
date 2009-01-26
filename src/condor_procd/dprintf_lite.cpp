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

FILE* debug_fp = NULL;

extern "C" void
dprintf(int, const char* format, ...)
{
	if (debug_fp != NULL) {
		va_list ap;
		va_start(ap, format);
		vfprintf(debug_fp, format, ap);
		va_end(ap);
		fflush(debug_fp);
	}
}

int	_EXCEPT_Line;
const char*	_EXCEPT_File;
int	_EXCEPT_Errno;

extern "C" void
_EXCEPT_(const char* format, ...)
{
	if (debug_fp) {
		fprintf(debug_fp, "ERROR: ");
		va_list ap;
		va_start(ap, format);
		vfprintf(debug_fp, format, ap);
		va_end(ap);
		fprintf(debug_fp, "\n");
		fflush(debug_fp);
	}
	exit(1);
}
