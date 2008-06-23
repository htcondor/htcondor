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

#ifndef _PROFILE_HELPERS_WINDOWS_H_
#define _PROFILE_HELPERS_WINDOWS_H_

/***************************************************************
* Headers
***************************************************************/

#include "env.h"

/***************************************************************
* Functions
***************************************************************/

/* returns TRUE if a user profile was created; otherwise, FALSE. */
BOOL CondorCreateUserProfile ();

/* returns TRUE if a user profile was loaded; otherwise, FALSE.*/
BOOL CondorLoadUserProfile ();

/* returns TRUE if a user profile was unloaded; otherwise, FALSE.*/
BOOL CondorUnloadUserProfile ();

/* returns TRUE if a user's environment was loaded; otherwise, FALSE.*/
BOOL CondorCreateEnvironmentBlock ( Env &env );

#endif // _PROFILE_HELPERS_WINDOWS_H_