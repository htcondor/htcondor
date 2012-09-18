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

#ifndef _SECURITY_WINDOWS_H_
#define _SECURITY_WINDOWS_H_

/***************************************************************
 * Functions
 ***************************************************************/

/***************************************************************
 * File Security
 ***************************************************************/

/* Gathers a file's security attributes.  Returns TRUE on 
success; otherwise, FALSE */
BOOL GetFileSecurityAttributes ( PCWSTR w_source, PSECURITY_ATTRIBUTES *sa );

/***************************************************************
 * Token Security
 ***************************************************************/

BOOL ModifyPrivilege ( LPCTSTR privilege, BOOL enable );

/***************************************************************
 * User Security
 ***************************************************************/

/* Retrieves a user's SID. Return TRUE on success; otherwise, FALSE */
BOOL LoadUserSid ( HANDLE user_token, PSID *sid );
void UnloadUserSid ( PSID sid );

#endif // _SECURITY_WINDOWS_H_
