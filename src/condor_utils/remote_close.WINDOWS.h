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

#ifndef _REMOTE_CLOSE_WINDOWS_H_
#define _REMOTE_CLOSE_WINDOWS_H_

/***************************************************************
* Functions
***************************************************************/

/* Given the pid of a process and a handle in it's process space, this
function will attempt to close the requested handle */
DWORD CloseRemoteHandle ( DWORD pid, HANDLE handle );

/*
Filter can be any of the following:

"Directory", "SymbolicLink", "Token", "Process", "Thread", "Unknown7",
"Event", "EventPair", "Mutant", "Unknown11", "Semaphore", "Timer",
"Profile", "WindowStation", "Desktop", "Section", "Key", "Port",
"WaitablePort", "Unknown21", "Unknown22", "Unknown23", "Unknown24",
"IoCompletion", "File" 

We presently make use of only two of these filters: "Directory" 
and "File". This may change in future versions, when it may be 
appropriate to filter for a "SymbolicLink", or otherwise.
*/
DWORD RemoteFindAndCloseA ( PCSTR filename, PCSTR filter );
DWORD RemoteFindAndCloseW ( PCWSTR w_filename, PCWSTR w_filter );

/* returns TRUE if the filtered object was closed; otherwise FALSE. */
BOOL RemoteDeleteW ( PCWSTR w_filename, PCWSTR w_filter );

#endif // _REMOTE_CLOSE_WINDOWS_H_
