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

#ifndef __PERM_H
#define __PERM_H

#ifdef WIN32
#include <aclapi.h>

bool SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege);
#endif

#define MAX_DOMAIN_LENGTH 254	// max string length 
#define MAX_ACCOUNT_LENGTH 256	// max string length
#define MAX_GROUP_LENGTH 256	// max string length

const int perm_max_sid_length = 100;

class perm {
public:
	perm( );
	~perm( );

#ifndef WIN32
	
	// On Unix, make everything a stub saying sucesss

	bool init ( const char *username, const char *domain = 0 ) { return true;}
	int read_access( const char *filename ) { return 1; }
	int write_access( const char *filename ) { return 1; }
	int execute_access( const char *filename ) { return 1; }
	int set_acls( const char *location ) { return 1; }

#else

	// Windows
	
	bool init ( const char *username, const char *domain = 0 );

	// External functions to ask permissions questions: 1 = true, 0 = false, -1 = unknown
	int read_access( const char *filename );
	int write_access( const char *filename );
	int execute_access( const char *filename );

	// returns true on success
	bool set_acls( const char *location );
	bool set_owner( const char *location );

protected:
	int get_permissions( const char *location, ACCESS_MASK &rights );
	bool volume_has_acls( const char *path );
	int userInAce( const LPVOID cur_ace, const char *account, const char *domain );

private:

		// used by get_permissions 
	char * Account_name;
	char * Domain_name;
	
	int userInLocalGroup( const char *account, const char *domain, const char* group_name );
	int userInGlobalGroup( const char *account, const char *domain, const char* group_name, const char* group_domain );
	
	
	// SID stuff.  Should one day be moved to a uid_t structure
	// char *	_account_name;
	// char *	_domain_name;

	char				sidBuffer[perm_max_sid_length];
	PSID				psid;
	unsigned long		sidBufferSize;
	char				domainBuffer[80];
	unsigned long		domainBufferSize;

	// End of SID stuff

	bool must_freesid;

	/* This insanity shouldn't be needed anymore...
	// insanity
	DWORD perm_read, perm_write, perm_execute;
	*/

	
#endif /* of ifdef WIN32 */
};


#endif

