/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "my_username.h"
#include "condor_uid.h"

#if ! defined(WIN32)
#include "passwd_cache.h"
#endif /* ! WIN32 */

char *
my_username( int uuid ) {
#if defined(WIN32)
   char username[UNLEN+1];
   unsigned long usernamelen = UNLEN+1;
   if (GetUserName(username, &usernamelen) < 0) {
		return( NULL );
   }
   return strdup(username);
#else
   if( uuid < 0 ) {
		uuid = geteuid();
   }
   passwd_cache * my_cache = pcache();
   ASSERT(my_cache);
   char * name = 0;
   if( ! my_cache->get_user_name(uuid, name) ) {
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
	if (! OpenThreadToken(GetCurrentThread(), TOKEN_READ, FALSE,
			   	&hAccessToken) ) {
		OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &hAccessToken);
	}
	return hAccessToken;
}
#endif
