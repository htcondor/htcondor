#ifndef __PERM_H
#define __PERM_H

#include <aclapi.h>

const int perm_max_sid_length = 100;

class perm {
public:
	perm( );
	~perm( );

	bool init ( char *username, char *domain = 0 );

	// External functions to ask permissions questions: 1 = true, 0 = false, -1 = unknown
	int read_access( const char *filename );
	int write_access( const char *filename );
	int execute_access( const char *filename );
	int set_acls( const char *location );

protected:
	int get_permissions( const char *location );
	bool volume_has_acls( const char *path );

private:
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
};


#endif