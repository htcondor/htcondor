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
#include "condor_snutils.h"
#include "condor_debug.h"

/*
** Compatibility routine for systems that don't have an snprintf
*/

#ifndef HAVE_SNPRINTF
int 
snprintf(
	char       *output, 
	size_t     buffer_size, 
	const char *format, 
	...)
{
	int length;
	va_list  args;

	va_start(args, format);
	length = condor_vsnprintf(output, (int)buffer_size, format, args);
	va_end(args);

	return length;
}


// we also use vsnprintf() in the code, and platforms that don't have
// snprintf() probably don't have this, either.
int 
vsnprintf(
	char       *output, 
	int        buffer_size, 
	const char *format, 
	va_list args )
{
	return condor_vsnprintf(output, buffer_size, format, args);
}
#endif

// This is snprintf, and it works like you expect. buffer_size is the 
// size of the buffer, and at most buffer_size-1 characters will be printed,
// and the output will also be null-termianted. The number of characters
// that would have been printed, had there been sufficient space, is
// returned. (If there was sufficient space, then it's the number of
// characters).
//
// The reason we have condor_snprintf() in addition to snprintf() is 
// because not all snprintfs() return the same thing when the output
// buffer isn't big enough. We can use this one whenever we care about
// the return value of snprintf.
int 
condor_snprintf(
	char       *output, 
	int        buffer_size, 
	const char *format, 
	...)
{
	int      length;
	va_list  args;

	va_start(args, format);
	length = condor_vsnprintf(output, buffer_size, format, args);
	va_end(args);

	return length;
}

// This is the real implementation of snprintf and condor_snprintf, it just
// takes a va_list instead of a ... argument.
int 
condor_vsnprintf(
	char        *output, 
	int         buffer_size, 
	const char  *format, 
	va_list     args)
{
	int actual_length;

	actual_length = vprintf_length(format, args);
	if (actual_length <= buffer_size - 1) { // -1 for the trailing null
		vsprintf(output, format, args);
	} else {
		char *full_output;

		full_output = malloc(actual_length + 1);
		if (full_output == NULL) {
			actual_length = -1;
		} else {
			int termination_point;
			
			vsprintf(full_output, format, args);
			
			if (buffer_size <= 0) {
				termination_point = 0;
			} else {
				termination_point = buffer_size - 1;
			}
			full_output[termination_point] = 0;
			strcpy(output, full_output);
			free(full_output);
		}
	}
	return actual_length;
}

// Returns the number of characters that a printf would return,
// without actually printing the characters anywhere.
int 
printf_length(const char *format, ...)
{
	int      length;
	va_list  args;

	va_start(args, format);
	length = vprintf_length(format, args);
	va_end(args);

	return length;
}

// Same as printf_length, but we take va_list instead of a ... argument.
int 
vprintf_length(const char *format, va_list args) 
{
	int   length;
	FILE  *null_output;

	null_output = safe_fopen_wrapper(NULL_FILE, "w", 0644);
	if(NULL == null_output) {
		/* We used to return -1 in this case, but realistically, 
		 * what is our caller going to do?  Indeed, at least some
		 * callers ignored the case and merrily stomped over memory.
		 */
		EXCEPT("Unable to open null file (%s). Needed for formatting "
			   "purposes. errno=%d (%s)", NULL_FILE, errno, strerror(errno));
	}

#ifdef va_copy
	{
	va_list copyargs;

	va_copy(copyargs, args);
	length = vfprintf(null_output, format, copyargs);
	va_end(copyargs);
	}
#else
	length = vfprintf(null_output, format, args);
#endif
	fclose(null_output);
	return length;
}
