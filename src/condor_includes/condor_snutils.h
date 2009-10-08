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
#pragma warning ( disable : 4028 )
int vsnprintf(char *str, size_t size, const char *format, va_list args);
#pragma warning ( default : 4028 )
#endif

int printf_length(const char *format, ...);
int vprintf_length(const char *format, va_list args);

END_C_DECLS

#endif /* CONDOR_SNUTILS_H */



