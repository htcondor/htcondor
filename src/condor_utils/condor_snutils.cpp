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

#ifdef WIN32
#if _MSC_VER < 1900
int 
snprintf(
	char       *str, 
	size_t     size, 
	const char *format, 
	...)
{
	int length;
	va_list  args;

	va_start( args, format );
	length = vsnprintf( str, size, format, args );
	va_end( args );

	return length;
}

// va_list is passed-by-value on Windows, so we don't need to copy it
// for the two different calls.
int
vsnprintf(
	char       *str, 
	size_t     size, 
	const char *format, 
	va_list    args)
{
	int length = -1;

	if ( str != NULL ) {
		length = _vsnprintf_s( str, size, _TRUNCATE, format, args );
	}
	if ( length < 0 ) {
		length = _vscprintf( format, args );
	}

	return length;
}
#endif

// Returns the number of characters that a printf would return,
// without actually printing the characters anywhere.
int 
printf_length(const char *format, ...)
{
	int      length;
	va_list  args;

	va_start(args, format);
	length = _vscprintf(format, args);
	va_end( args );

	return length;
}

// Same as printf_length, but we take va_list instead of a ... argument.
// va_list is passed-by-value on Windows, so we don't need to make a
// copy of it like we do on unix.
int 
vprintf_length(const char *format, va_list args) 
{
	return _vscprintf(format, args);
}

#else /* ifdef WIN32 */

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
// As a convenience to callers who want to call vprintf_length() and then
// call another v...() function, we copy the va_list.
int 
vprintf_length(const char *format, va_list args) 
{
	char c;
	int length;

#ifdef va_copy
	{
	va_list copyargs;

	va_copy(copyargs, args);
	length = vsnprintf(&c, 1, format, copyargs);
	va_end(copyargs);
	}
#else
	length = vsnprintf(&c, 1, format, args);
#endif
	return length;
}

#endif /* ifdef WIN32 */

int vsprintf_realloc( char **buf, int *bufpos, int *buflen, const char *format, va_list args)
{
	int append_len,written;

	if( !buf || !bufpos || !buflen || !format ) {
		errno = EINVAL;
		return -1;
	}

    append_len = vprintf_length(format,args);

	if( append_len < 0 ) {
		if( errno == 0 ) {
			errno = EINVAL;
		}
		return -1;
	}

	if( *bufpos + append_len + 1 > *buflen || *buf == NULL ) {
		char *realloc_buf;
		int realloc_buflen = *bufpos + append_len + 1;
		realloc_buf = (char *)realloc(*buf,realloc_buflen);
		if( !realloc_buf ) {
			errno = ENOMEM;
			return -1;
		}
		*buf = realloc_buf;
		*buflen = realloc_buflen;
    }

	written = vsnprintf(*buf + *bufpos, append_len + 1, format, args);

	if( written != append_len ) {
		if( errno == 0 ) {
			errno = EINVAL;
		}
		return -1;
	}

	*bufpos += append_len;

    return append_len;
}

int sprintf_realloc( char **buf, int *bufpos, int *buflen, const char *format, ...)
{
	va_list args;
	int retval;

	va_start(args, format);
	retval = vsprintf_realloc(buf,bufpos,buflen,format,args);
	va_end(args);

	return retval;
}

/* strcpy_len
 * Truncating strcpy. will not go past the end of the output buffer (specifed by len)
 * but unlike strncpy, it gurantees that the output is null terminated.
 *
 * returns the number of characters copied (not including the terminating null) unless
 * the input did not fit in the output buffer, in that case it returns the size of the output buffer.
 */
int strcpy_len(char * out, const char * in, int len) 
{
	if (len <= 0)
		return 0;

	char * p = out;
	int ix = 0;
	while (ix < len) {
		if ((*p++ = *in++))
			++ix;
		else
			return ix;	   // non-truncating return
	}

	// if we get to here we are truncating.
	// make sure to null terminate the output buffer
	out[len-1] = 0;

	return len; // return len when we truncate
}

