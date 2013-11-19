/***************************************************************
 *
 * Copyright (C) 2013, Condor Team, Computer Sciences Department,
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

#ifndef HTC_DEPRECATED_H
#define HTC_DEPRECATED_H

/* Generate compiler warnings when a function marked DEPRECATED is called.  Put
 * around the declaration.  So
 *
 *    int old_code(char * s);
 *
 * becomes
 *
 *    DEPRECATED( int old_code(char * s) );
 */
#if defined _MSC_VER
#  define DEPRECATED(func) __declspec(deprecated) func
#elif defined  __GNUC__
#  define DEPRECATED(func) func __attribute__ ((deprecated))
#else
#  define DEPRECATED(func) func
#endif

#endif
