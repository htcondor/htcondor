/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "login_tracker.h"

#if defined(WIN32)

// constructor on Windows simply saves the login name
//
LoginTracker::LoginTag::LoginTag(char* login) : m_login(login)
{
}

// Windows test method (where all the real work happens)
//
bool
LoginTracker::LoginTag::test(procInfo* pi)
{
	BOOL result;

	// open a handle to the process in question
	//
	HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION,
	                                    FALSE,
	                                    pi->pid);
	if (process_handle == NULL) {
		dprintf(D_ALWAYS,
		        "login_match: OpenProcess error: %u\n",
		        GetLastError());
		return false;
	}

	// now get a handle to its token
	//
	HANDLE token_handle;
	result = OpenProcessToken(process_handle,
	                          TOKEN_QUERY,
	                          &token_handle);
	if (result == FALSE) {
		dprintf(D_ALWAYS,
		        "login_match: OpenProcessToken error: %u\n",
		        GetLastError());
		CloseHandle(process_handle);
		return false;
	}

	// now get the user's SID out of the token
	//
	DWORD sid_length;
	GetTokenInformation(token_handle,
	                    TokenUser,
	                    NULL,
	                    0,
	                    &sid_length);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		dprintf(D_ALWAYS,
		        "login_match: GetTokenInformation error: %u\n",
		        GetLastError());
		CloseHandle(token_handle);
		CloseHandle(process_handle);
		return false;
	}
	TOKEN_USER* sid_buffer = (TOKEN_USER*)malloc(sid_length);
	ASSERT(sid_buffer != NULL);
	result = GetTokenInformation(token_handle,
	                             TokenUser,
	                             sid_buffer,
	                             sid_length,
	                             &sid_length);
	if (result == FALSE) {
		dprintf(D_ALWAYS,
		        "login_match: GetTokenInformation error: %u\n",
		        GetLastError());
		free(sid_buffer);
		CloseHandle(token_handle);
		CloseHandle(process_handle);
		return false;
	}

	// ok, now lookup the user name corresponding to the SID we just got
	//
	char user_buffer[1024];
	DWORD user_length = sizeof(user_buffer);
	char domain_buffer[1024];
	DWORD domain_length = sizeof(domain_buffer);
	SID_NAME_USE sid_name_use;
	result = LookupAccountSid(NULL,
	                          sid_buffer->User.Sid,
	                          user_buffer,
	                          &user_length,
	                          domain_buffer,
	                          &domain_length,
	                          &sid_name_use);
	DWORD error = GetLastError();
	free(sid_buffer);
	CloseHandle(token_handle);
	CloseHandle(process_handle);
	if (result == FALSE) {
		dprintf(D_ALWAYS,
		        "login_match: LookupAccountSid error: %u\n",
		        GetLastError());
		return false;
	}

	// TODO: should we be comparing domains here as well?
	//
	return (stricmp(user_buffer, m_login) == 0);
}

#else

// constructor on UNIX looks up the UID for the login name
//
LoginTracker::LoginTag::LoginTag(char* login)
{
	errno = 0;
	struct passwd *pwd = getpwnam(login);
	if (pwd == NULL) {
		if (errno != 0) {
			dprintf(D_ALWAYS,
			        "login_match: getpwnam error: %s (%d)\n",
			        strerror(errno),
			        errno);
		}
		else {
			dprintf(D_ALWAYS,
			        "login_match: account \"%s\" not found\n",
			        login);
		}
		m_uid = (uid_t)-1;
	}
	else {
		m_uid = pwd->pw_uid;
	}
}

// test function for UNIX
//
bool
LoginTracker::LoginTag::test(procInfo* pi)
{
	return (pi->owner == m_uid);
}

#endif
