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
#ifdef WIN32
        #define __ATTRIBUTE__WEAK__
#else
        #define __ATTRIBUTE__WEAK__ __attribute__ ((weak))
#endif


/* Dummy definition of SetSyscalls to be included in Condor libraries
   where needed. */
extern "C" {

int __ATTRIBUTE__WEAK__ SetSyscalls( int mode ) { return mode; }

}
