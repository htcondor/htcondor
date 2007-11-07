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


 

#ifndef fcntl_h

extern "C" {

#ifdef __fcntl_h_recursive
#include_next <fcntl.h>
#else
#define fcntl __hide_fcntl
#define open  __hide_open
#define creat __hide_creat

#define __fcntl_h_recursive
#include <_G_config.h>
#include_next <fcntl.h>

#undef fcntl
#undef open
#undef creat

#define fcntl_h 1

int       fcntl(int, int, ...);
int	  creat (const char*, mode_t);

int       open (const char*, int, ...);

#endif
}
#endif
