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

#ifndef _DIRECTORY_WINDOWS_H_
#define _DIRECTORY_WINDOWS_H_

/***************************************************************
* Functions
***************************************************************/

/* returns TRUE if all the directories allong the given path were 
created (or already existed); ortherwise, FALSE */
BOOL CreateSubDirectory ( PCSTR path, LPSECURITY_ATTRIBUTES sa );

/* returns TRUE when a directory is created with the user set 
as the owner and has been locked down; otherwise, FALSE. */
BOOL CreateUserDirectory ( HANDLE user_token, PCSTR directory );

/* These are used when handling Windows profiles.  They are, in a
sense, a reproduction of part of the Directory object's 
functionality, with a small exception: they will forcibly delete 
any remote handles to the files they cannot overwrite to (see 
CloseRemoteHandle() for details). There is one other small, but
significant difference, these functions also handle NTFS junctions
gracefully */

/* returns TRUE if the destination is populated with the source file
hierarchy; otherwise, FALSE */
BOOL CondorCopyDirectory ( PCSTR source, PCSTR destination );

/* returns TRUE if the directory has been removed; otherwise, FALSE */
BOOL CondorRemoveDirectory ( PCSTR directory );

#endif // _DIRECTORY_WINDOWS_H_
