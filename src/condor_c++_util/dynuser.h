#ifndef __dynuser_H
#define __dynuser_H

#include <aclapi.h>
#include "ntsecapi.h"

const int max_sid_length = 100;
const int max_name_length = 100;

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

private:

	// SID stuff.  Should be moved to a uid_t structure
	char *	_account_name;
	char *	_domain_name;

	char				sidBuffer[max_sid_length];
	PSID				psid;
	unsigned long		sidBufferSize;
	char				domainBuffer[80];
	unsigned long		domainBufferSize;

	// End of SID stuff

	void update_psid();

	void createpass();
	void createaccount();
	void update_t();
	
	bool add_batch_privilege();
	bool add_users_group();

#if 0
	bool dump_groups();
#endif

	char	*accountname,	*password;			// ASCII versions
	wchar_t	*accountname_t,	*password_t;		// Unicode versions

	HANDLE logon_token;
};


#endif