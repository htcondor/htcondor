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
#ifndef __dynuser_H
#define __dynuser_H

#ifdef WIN32

#include <aclapi.h>
#include <ntsecapi.h>



const int max_sid_length = 100;
const int max_domain_length = 100;

#ifndef STATUS_SUCCESS 
#define STATUS_SUCCESS              ((NTSTATUS)0x00000000L) 
#define STATUS_INVALID_PARAMETER    ((NTSTATUS)0xC000000DL) 
#endif 

#define ACCOUNT_PREFIX			"condor-run-"
#define ACCOUNT_PREFIX_REUSE	"condor-reuse-"

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
	bool deleteuser(char * username);
	const char* get_accountname();

	HANDLE get_token();

    bool cleanup_condor_users(char* user_prefix);

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
	
	bool add_batch_privilege();
	bool add_users_group();

    bool del_users_group();
	bool logon_user();
	
#if 0
	bool dump_groups();
#endif

	char	*accountname,	*password;			// ASCII versions
	wchar_t	*accountname_t,	*password_t;		// Unicode versions
	bool reuse_account;	// accounts are enabled/disabled instead of created/deleted when true

	HANDLE logon_token;
};

#endif  // of ifdef WIN32

#endif  
