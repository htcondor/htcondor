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

#ifndef CONDOR_OPEN_H_INCLUDE
#define CONDOR_OPEN_H_INCLUDE

// needed for FILE declaration, it is a typedef so an imcomplete struct will
// not work :(
#include <stdio.h>


#ifdef __cplusplus
extern "C"  {
#define SAFE_OPEN_DEFAULT_MODE_VALUE 0644
#else
#define SAFE_OPEN_DEFAULT_MODE_VALUE
#endif

#include "safe_open.h"
#include "safe_fopen.h"


#ifdef __cplusplus
}
#endif

#endif
