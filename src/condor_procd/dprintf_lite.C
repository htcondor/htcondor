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
char*	_EXCEPT_File;
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
