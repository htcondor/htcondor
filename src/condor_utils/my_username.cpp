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
#include "condor_debug.h"
#include "my_username.h"
#include "condor_uid.h"

#if ! defined(WIN32)
#include "passwd_cache.unix.h"
#endif /* ! WIN32 */

char *
my_username() {
#if defined(WIN32)
   char username[UNLEN+1];
   unsigned long usernamelen = UNLEN+1;
   if (GetUserName(username, &usernamelen) < 0) {
		return( NULL );
   }
   return strdup(username);
#else
   passwd_cache * my_cache = pcache();
   ASSERT(my_cache);
   char * name = 0;
   if( ! my_cache->get_user_name(geteuid(), name) ) {
	   free(name); // Just in case;
	   name = 0;
   }
   return name;
#endif
}

char* my_domainname( ) {
#ifndef WIN32
	return NULL;
}
#else
	// WINDOWS

	// note that this code returns the domain of the
	// current thread's owner.

	HANDLE hAccessToken = NULL;
	UCHAR InfoBuffer[1000];
	char szAccountName[200], szDomainName[200];
	PTOKEN_USER pTokenUser = (PTOKEN_USER)InfoBuffer;
	DWORD dwInfoBufferSize,dwAccountSize = 200, dwDomainSize = 200;
	SID_NAME_USE snu;

	hAccessToken = my_usertoken();

	GetTokenInformation(hAccessToken,TokenUser,InfoBuffer,
		1000, &dwInfoBufferSize);

	szAccountName[0] = '\0';
	szDomainName[0] = '\0';
	LookupAccountSid(NULL, pTokenUser->User.Sid, szAccountName,
		&dwAccountSize,szDomainName, &dwDomainSize, &snu);
	
	if ( hAccessToken )
		CloseHandle(hAccessToken);

	return strdup(szDomainName);
}

// return the current thread (or process) user token.
// if the current thread isn't impersonating, its the
// current process's token that's returned.
HANDLE my_usertoken() {
	HANDLE hAccessToken = NULL;
	if (! OpenThreadToken(GetCurrentThread(), TOKEN_READ, TRUE,
			   	&hAccessToken) ) {
		OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &hAccessToken);
	}
	return hAccessToken;
}
#endif
