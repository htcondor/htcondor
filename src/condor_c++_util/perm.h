#ifndef __PERM_H
#define __PERM_H

#ifdef WIN32
#include <aclapi.h>
#endif

const int perm_max_sid_length = 100;

class perm {
public:
	perm( );
	~perm( );

#ifndef WIN32
	
	// On Unix, make everything a stub saying sucesss

	bool init ( const char *username, char *domain = 0 ) { return true;}
	int read_access( const char *filename ) { return 1; }
	int write_access( const char *filename ) { return 1; }
	int execute_access( const char *filename ) { return 1; }
	int set_acls( const char *location ) { return 1; }

#else

	// Windows
	
	bool init ( const char *username, char *domain = 0 );

	// External functions to ask permissions questions: 1 = true, 0 = false, -1 = unknown
	int read_access( const char *filename );
	int write_access( const char *filename );
	int execute_access( const char *filename );
	int set_acls( const char *location );

protected:
	int get_permissions( const char *location, ACCESS_MASK &rights );
	bool volume_has_acls( const char *path );
	int userInAce( const LPVOID cur_ace, const char *account, const char *domain );

private:

		// used by get_permissions 
	char * Account_name;
	char * Domain_name;
	
	bool domainAndNameMatch( const char *account1, const char *account2, const char *domain1, const char *domain2 );
	int getAccountFromSid( LPTSTR Sid, char* &account, char* &domain );
	
		// takes string of the form <DOMAIN_NAME>\<ACCOUNT_NAME> and chops it into two strings
	void getDomainAndName( char* &namestr, char* &domain, char* &name ) {
		char* nameptr = strrchr ( namestr, '\\' ); 
		if ( nameptr != NULL ) {
			domain = namestr;
			*nameptr = '\0';
			name = nameptr+1;
		} else {
			name = namestr;
			domain = NULL;
		}
	};
	
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

