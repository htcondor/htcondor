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

