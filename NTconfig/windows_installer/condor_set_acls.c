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

#define UNICODE
#define _UNICODE

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>

#define BUFFER_SIZE 256

TCHAR administrators_buf[BUFFER_SIZE];
TCHAR everyone_buf[BUFFER_SIZE];

int
get_names(void)
{
	SID* sid;
	DWORD user_sz;
	TCHAR domain_buf[BUFFER_SIZE];
	DWORD domain_sz;
	SID_NAME_USE sid_name_use;
	SID_IDENTIFIER_AUTHORITY everyone_auth = SECURITY_WORLD_SID_AUTHORITY;
	SID_IDENTIFIER_AUTHORITY administrators_auth = SECURITY_NT_AUTHORITY;

	// first lookup the SID S-1-1-0 ("Everyone" on English systems)
	if (!AllocateAndInitializeSid(&everyone_auth,
	                              1,
	                              SECURITY_WORLD_RID,
	                              0, 0, 0, 0, 0, 0, 0,
	                              &sid)
	) {
		_ftprintf(stderr, TEXT("AllocateAndInitializeSid error: %u\n"), GetLastError());
		return 1;
	}
	user_sz = domain_sz = BUFFER_SIZE;
	if (!LookupAccountSid(NULL,
	                      sid,
	                      everyone_buf,
	                      &user_sz,
	                      domain_buf,
	                      &domain_sz,
	                      &sid_name_use)
	) {
		_ftprintf(stderr, TEXT("LookupAccountSid error: %u\n"), GetLastError());
		return 1;
	}
	FreeSid(sid);

	// now lookup the SID S-1-5-32-544 ("Administrators" on English systems)
	if (!AllocateAndInitializeSid(&administrators_auth,
	                              2,
	                              SECURITY_BUILTIN_DOMAIN_RID,
	                              DOMAIN_ALIAS_RID_ADMINS,
	                              0, 0, 0, 0, 0, 0,
	                              &sid)
	) {
		_ftprintf(stderr, TEXT("AllocateAndInitializeSid error: %u\n"), GetLastError());
		return 1;
	}
	user_sz = domain_sz = BUFFER_SIZE;
	if (!LookupAccountSid(NULL,
	                      sid,
	                      administrators_buf,
	                      &user_sz,
	                      domain_buf,
	                      &domain_sz,
	                      &sid_name_use)
	) {
		_ftprintf(stderr, TEXT("LookupAccountSid error: %u\n"), GetLastError());
		return 1;
	}
	FreeSid(sid);

	return 0;
}

int
_tmain(int argc, TCHAR *argv[])
{
	TCHAR cmd_buf[1024];

	if (argc != 2) {
		_ftprintf(stderr, TEXT("usage: condor_set_acls <directory>\n"));
		return EXIT_FAILURE;
	}

	// get the names for "Everyone" (S-1-1-0) and "Administrators" (S-1-5-32-544)
	get_names();

	_stprintf(cmd_buf, TEXT("echo y|cacls \"%s\" /t /g \"%s:F\" \"%s:R\""),
			  argv[1], administrators_buf, everyone_buf);
	if (_tsystem(cmd_buf)) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
