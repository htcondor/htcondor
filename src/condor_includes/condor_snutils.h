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

#ifndef CONDOR_SNUTILS_H
#define CONDOR_SNUTILS_H

/* Some platforms don't have a proper snprintf(). On Windows, _snprintf()
 * doesn't NULL-terminate the buffer if the formated string is too large
 * to fit. On HP-UX, snprintf() returns -1 if the formated string is too
 * large to fit in the buffer.
 *
 * For these platforms, we provide our own snprintf(). Thus, on all
 * platforms, we guarantee that snprintf() NULL-terminates the destination
 * buffer in all cases and returns the number of bytes (excluding the
 * terminating NULL) that would have been written if the buffer had been
 * large enough.
 *
 * We also provide [v]printf_length(), which returns the number of bytes
 * (excluding the terminating NULL) necessary to hold the formatted
 * string. vprintf_length() uses a copy of the va_list passed in as a
 * convenience to the calling function.
 */

BEGIN_C_DECLS

#ifdef WIN32
int snprintf(char *str, size_t size, const char *format, ...);
/**	Disable the warning about the number of formal parameters 
	differing from a previous declaration */
//#pragma warning ( disable : 4273 ) // inconsistent dll linkage
__declspec(dllimport) int __cdecl vsnprintf(char *str, size_t size, const char *format, va_list args);
//#pragma warning ( default : 4273 )
#endif

int printf_length(const char *format, ...);
int vprintf_length(const char *format, va_list args);

/*
 * sprintf_realloc
 - Calls sprintf to write to *buf at position *bufpos.
 - Reallocs *buf if necessary; the caller must free() the buffer when finished.
 - It is fine for *buf to be NULL and *buflen to be 0.
 - On success, returns the number of characters written, not counting
   the terminating '\0' and updates *bufpos to be the index of the
   terminating '\0'.  On error, returns -1 and sets errno.  On error, the
   caller should not assume that the buffer is null terminated, but the caller
   should still free *buf.

   This function does not abort on any errors except on some platforms
   with broken sprintf() implementations, it could abort due to
   failure to open /dev/null.
 */
int sprintf_realloc( char **buf, int *bufpos, int *buflen, const char *format, ...) CHECK_PRINTF_FORMAT(4,5);


/* vsprintf_realloc
 * This function is just like sprintf_realloc except it takes the arguments
 * as a va_list.
 *
 * The caller of this function should not assume that args can be
 * used again on systems where va_copy is required.  If the caller
 * wishes to use args again, the caller should therefore pass in
 * a copy of the args.
 */
int vsprintf_realloc( char **buf, int *bufpos, int *buflen, const char *format, va_list args);

/* strcpy_len
 * Truncating strcpy. will not go past the end of the output buffer (specifed by len)
 * but unlike strncpy, it gurantees that the output is null terminated.
 *
 * returns the number of characters copied (not including the terminating null) unless
 * the input did not fit in the output buffer, in that case it returns the size of the output buffer.
 */

int strcpy_len(char * out, const char * in, int len);

END_C_DECLS

#endif /* CONDOR_SNUTILS_H */



