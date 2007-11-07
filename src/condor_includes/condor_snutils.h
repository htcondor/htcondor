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

/*  not every platform has an snprintf, so if we're one of the unlucky few,
	we need to provide it. Currently that seems only to be SOLARIS251
	epaulson 6/18/2002 */

BEGIN_C_DECLS

#ifndef HAVE_SNPRINTF
int snprintf(char *output, size_t buffer_size, const char *format, ...);
#endif

int condor_snprintf(char *output, int buffer_size, const char *format, ...);
int condor_vsnprintf(char *output, int buffer_size, const char *format, 
					 va_list args);
int printf_length(const char *format, ...);
int vprintf_length(const char *format, va_list args);

END_C_DECLS

#endif /* CONDOR_SNUTILS_H */



