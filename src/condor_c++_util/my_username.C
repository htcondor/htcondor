#include "condor_common.h"
#include "condor_debug.h"
#include "my_username.h"

char *
my_username( int uuid = 0 ) {
#if defined(WIN32)
   char username[UNLEN+1];
   unsigned long usernamelen = UNLEN+1;
   if (GetUserName(username, &usernamelen) < 0) {
      EXCEPT( "GetUserName failed.\n" );
   }
   return strdup(username);
#else
   struct passwd  *pwd;

	int tryUid = uuid;
	if ( !tryUid ) {
		tryUid = getuid();
	}
//   if( (pwd=getpwuid(getuid())) == NULL ) {
   if( (pwd=getpwuid(tryUid)) == NULL ) {
      EXCEPT( "Can't get passwd entry for uid %d\n", tryUid );
   }
   return strdup(pwd->pw_name);
#endif
}

