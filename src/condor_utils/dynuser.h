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

#ifndef __dynuser_H
#define __dynuser_H

#ifdef WIN32

#include <aclapi.h>
#include <ntsecapi.h>



const int max_sid_length = 100;
const int max_domain_length = 100;

#ifndef STATUS_SUCCESS 
#define STATUS_SUCCESS              ((NTSTATUS)0x00000000L) 
//#define STATUS_INVALID_PARAMETER    ((NTSTATUS)0xC000000DL) 
#endif 

#define ACCOUNT_PREFIX			"condor-run-"
#define ACCOUNT_PREFIX_REUSE	"condor-"

// get names of accounts and groups in a language-independent way
char* getWellKnownName( DWORD subAuth1, DWORD subAuth2 = 0, bool domainname=false );
// these just call getWellKnownName()
char* getSystemAccountName();
char* getUserGroupName();
char* getBuiltinDomainName();
char* getNTAuthorityDomainName();

class dynuser {
public:
	dynuser();
	~dynuser();

	bool init_user();
	bool deleteuser(char const * username);
	const char* get_accountname();

	HANDLE get_token();

    bool cleanup_condor_users(const char* user_prefix);

	void reset(); // used to be private

	const char* account_prefix() { 
		if ( reuse_account ) { return ACCOUNT_PREFIX_REUSE; }
		else { return ACCOUNT_PREFIX; }
	};

	bool reuse_accounts() { return reuse_account; }

private:

	// SID stuff.  Should be moved to a uid_t structure
	char *	_account_name;
	char *	_domain_name;

	char				sidBuffer[max_sid_length];
	PSID				psid;
	unsigned long		sidBufferSize;
	char				domainBuffer[max_domain_length];
	unsigned long		domainBufferSize;

	// End of SID stuff

	bool update_psid(); // returns true on successful

	void createpass();
	void createaccount();
	void enable_account();
	void disable_account();
	void update_t();
	
	bool hide_user();
	bool add_users_group();

    bool del_users_group();
	bool logon_user();
	
	char	*accountname,	*password;			// ASCII versions
	wchar_t	*accountname_t,	*password_t;		// Unicode versions
	bool reuse_account;	// accounts are enabled/disabled instead of created/deleted when true

	HANDLE logon_token;
};

#endif  // of ifdef WIN32

#endif  
