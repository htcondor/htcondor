#include "condor_common.h"
#include "condor_debug.h"
#include "my_username.h"

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
   struct passwd  *pwd;

	int tryUid = uuid;
	if ( !tryUid ) {
		tryUid = geteuid();
	}

   if( (pwd=getpwuid(tryUid)) == NULL ) {
		return( NULL );
   }
   return strdup(pwd->pw_name);
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
