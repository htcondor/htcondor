#ifndef __dynuser_H
#define __dynuser_H

#ifdef WIN32

#include <aclapi.h>
#include "ntsecapi.h"

const int max_sid_length = 100;
const int max_domain_length = 100;

#ifndef STATUS_SUCCESS 
#define STATUS_SUCCESS              ((NTSTATUS)0x00000000L) 
#define STATUS_INVALID_PARAMETER    ((NTSTATUS)0xC000000DL) 
#endif 

class dynuser {
public:
	dynuser();
	~dynuser();

	bool createuser(char * username);
	bool deleteuser(char * username);

	HANDLE get_token();

    bool cleanup_condor_users(char* user_prefix);

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

	void update_psid();

	void createpass();
	void createaccount();
	void update_t();
	
	bool add_batch_privilege();
	bool add_users_group();

    bool del_users_group();

#if 0
	bool dump_groups();
#endif

	char	*accountname,	*password;			// ASCII versions
	wchar_t	*accountname_t,	*password_t;		// Unicode versions

	HANDLE logon_token;
};

#endif  // of ifdef WIN32

#endif  
