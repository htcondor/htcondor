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


#ifndef __TRUNCATE_H
#define __TRUNCATE_H

#include "condor_common.h"

#ifdef WIN32

/* gives us the truncate() and ftruncate() functions on Win32 */
#if defined(__cplusplus)
extern "C" {
#endif

int ftruncate( int file_handle, long size );
int truncate( const char *path, long size );

#if defined(__cplusplus)
}
#endif

#endif // WIN32
#endif // __TRUNCATE_H
