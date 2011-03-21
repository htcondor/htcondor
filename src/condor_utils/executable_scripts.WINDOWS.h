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

#ifndef _EXECUTABLE_SCRIPTS_H_
#define _EXECUTABLE_SCRIPTS_H_

/***************************************************************
* Functions
***************************************************************/

/** Finds the executable and parameter list associated with the
	provided extension. The arguments buffer can be NULL. */
BOOL GetExecutableAndArgumentTemplateByExtention (
	LPCSTR	extension,
	LPSTR	executable,
	LPSTR	arguments = NULL );

#endif /* _EXECUTABLE_SCRIPTS_H_ */
