/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h" 
#include "condor_snutils.h"

/*
** Compatibility routine for systems that don't have an snprintf
*/

#ifdef NEED_SNPRINTF
int 
snprintf(
	char       *output, 
	int        buffer_size, 
	const char *format, 
	...)
{
	int length;

	length = condor_vsnprintf(output, buffer_size, format, args);
	return length;
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

#ifdef WIN32
	null_output = fopen("NUL", "w");
#else
	null_output = fopen("/dev/null", "w");
#endif

	if (NULL != null_output) {
		length = vfprintf(null_output, format, args);
		fclose(null_output);
	} else {
		length = -1;
	}
	return length;
}
