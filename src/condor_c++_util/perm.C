#include "condor_common.h"
#include "condor_debug.h"
#include "perm.h"
#include "Lm.h"

//
// get_permissions:  1 = yes, 0 = no, -1 = unknown/error
//
int perm::get_permissions( const char *file_name ) {
	DWORD retVal;
	TRUSTEE Trustee;
	ACCESS_MASK AccessRights;
	PACL pacl;
	
	PSECURITY_DESCRIPTOR pSD;
	DWORD pSD_length = 0;
	DWORD pSD_length_needed = 0;
	BOOL acl_present = FALSE;
	BOOL acl_defaulted = FALSE;
	
	// Do the call first to find out how much space is needed.
	
	pSD = NULL;
	
	GetFileSecurity(
		file_name,					// address of string for file name
		DACL_SECURITY_INFORMATION,	// requested information
		pSD,						// address of security descriptor
		pSD_length, 				// size of security descriptor buffer
		&pSD_length_needed			// address of required size of buffer
		);
	
	if( pSD_length_needed <= 0 ) {					// Find out how much space is needed, if <=0 then error
		if ( (GetLastError() == ERROR_FILE_NOT_FOUND) || 
			(GetLastError() == ERROR_PATH_NOT_FOUND) ) {
			
			// Here we have the tricky part of walking up the directory path
			// Typically it works like this:  If the filename exists, great, we'll
			//	 get the permissions on that.  If the filename does not exist, then
			//	 we pop the filename part off the file_name and look at the
			//	 directory.  If that directory should be (for some odd reason) non-
			//	 existant, then we just pop that off, until either we find something
			//	 thats will give us a permissions bitmask, or we run out of places
			//	 to look (which shouldn't happen since c:\ should always give us
			//	 SOMETHING...
			int i = strlen( file_name ) - 1;
			while ( i >= 0 && ( file_name[i] != '\\' && file_name[i] != '/' ) ) {
				i--;
			}
			if ( i < 0 ) {	// We've nowhere else to look, and this is bad.
				return -1;	// Its not a no, its an unknown
			}
			char *new_file_name = new char[i+1];
			strncpy(new_file_name, file_name, i);
			new_file_name[i]= '\0';
			
			// Now that we've chopped off more of the filename, call get_permissions
			// again...
			DWORD retval = get_permissions( new_file_name );
			delete[] new_file_name;
			
			// ... and return what it returns. (after deleting the string that was
			// allocated.
			
			return retval;
		}
		dprintf(D_ALWAYS, "perm::GetFileSecurity failed (err=%d)\n", GetLastError());
		return -1;
	}
	
	pSD_length = pSD_length_needed + 2; 	// Add 2 for safety.
	pSD_length_needed = 0;
	pSD = new BYTE[pSD_length];
	
	// Okay, now that we've found something, and know how large of an SD we need,
	// call the thing for real and lets get ON WITH IT ALREADY...
	
	if( !GetFileSecurity(
		file_name,					// address of string for file name
		DACL_SECURITY_INFORMATION,	// requested information
		pSD,						// address of security descriptor
		pSD_length, 				// size of security descriptor buffer
		&pSD_length_needed			// address of required size of buffer
		) ) {
		dprintf(D_ALWAYS, "perm::GetFileSecurity(%s) failed (err=%d)\n", file_name, GetLastError());
		delete pSD;
		return -1;
	}
	
	// Now, get the ACL from the security descriptor
	if( !GetSecurityDescriptorDacl(
		pSD,							// address of security descriptor
		&acl_present,					// address of flag for presence of disc. ACL
		&pacl,							// address of pointer to ACL
		&acl_defaulted					// address of flag for default disc. ACL
		) ) {
		dprintf(D_ALWAYS, "perm::GetSecurityDescriptorDacl failed (file=%s err=%d)\n", file_name, GetLastError());
		delete pSD;
		return -1;
	}
	
	// The block below won't get used anymore under NT4 since the 
	// GetEffectiveRightsFromAcl() API is screwed for a variety of reasons. 
	// Mainly it just doesn't return the right values, except if you're running
	// NT SP6a with the hotfix explained in MS KB Q258437. For Windows2000 
	// we'll assume it's ok however. We prefer to use 
	// GetEffectiveRightsFromAcl() on Win2k because it allows nested groups,
	// which would complicate things even further for our workaround.
	
	const char* OS = sysapi_opsys();
	
	
    if ( strcmp( OS, "WINNT50" ) == 0 )
	{
		
		// Fill in the Trustee thing-a-ma-jig(tm).	What's a thing-a-ma-jig(tm) you ask?
		// Frankly, I don't know.  Its a Microsoft thing...
		
		Trustee.pMultipleTrustee = NULL;
		Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
		Trustee.TrusteeForm = TRUSTEE_IS_SID;
		Trustee.TrusteeType = TRUSTEE_IS_USER;
		Trustee.ptstrName = (char *)psid;
		
		retVal = GetEffectiveRightsFromAcl( pacl,	// ACL to get trustee's rights from
			&Trustee,						// trustee to get rights for
			&AccessRights					// receives trustee's access rights
			);
		
		if( retVal != ERROR_SUCCESS ) {
			dprintf(D_ALWAYS, "perm::GetEffectiveRightsFromAcl failed (file=%s err=%d)\n", file_name, GetLastError());
			return -1;
		}
		
	} else { // if not Windows 2000, we have to do the dirty work ourselves
		
		unsigned long pEntriesCount = 0;
		EXPLICIT_ACCESS* pList;
		
		// Get explicit permissions entries for our ACL
		retVal = GetExplicitEntriesFromAcl( pacl,	// ACL to get Explicit Entries from
			&pEntriesCount, 				// pointer to number of Explicit Access Entries
			&pList							// pointer to array of EXPLICIT_ACCESS structures
			);
		
		if( retVal != ERROR_SUCCESS ) {
			dprintf(D_ALWAYS, "perm::GetExplicitEntriesFromAcl failed (file=%s err=%d)\n", file_name, GetLastError());
			LocalFree( pList );
			return -1;
		}
		
		// Walk through array of EXPLICIT_ACCESS structures
		
		ACCESS_MASK allow = 0x0;
		ACCESS_MASK deny = 0x0;
		
		int result;
		for (int i=0; i < (int) pEntriesCount; i++) {
			result = userInExplicitAccess( pList[i], Account_name, Domain_name );		
			
			if (result == 1) {
				switch ( pList[i].grfAccessMode ) {				
				case GRANT_ACCESS:
				case SET_ACCESS:
					allow |= pList[i].grfAccessPermissions;
					break;
				case DENY_ACCESS:
					deny |= pList[i].grfAccessPermissions;
					break;
				}
			}
		}
		
		AccessRights = allow;
		AccessRights &= ~deny;
		LocalFree( pList );
	}
	// And now, we have the access rights, so return those.
	return AccessRights;
}

//
// returns true if account1 and account2 match
// and domain1 is null, empty, or domain1 and domain2
// match. That is, return a match if the accounts match
// and the domains match or are unspecified
//
bool perm::domainAndNameMatch( const char *account1, const char *account2, const char *domain1, const char *domain2 ) {
	
	// for debugging
	//	cout << account1 << "\\" << ( domain1 ? domain1 : "NULL" ) << " " << account2 << "\\" << domain2 << endl;
	
	return ( ( strcmp ( account1, account2 ) == 0 ) && 
		( domain1 == NULL || domain1 == "" || 
		strcmp ( domain1, domain2 ) == 0 ) );	
}

//
//  returns account and domain string from a SID pointer
//  It's just a wrapper method, but it saves time since this is a common operation
//
//  returns 0=success, -1=something bad happened
//
int perm::getAccountFromSid( LPTSTR Sid, char* &account, char* &domain ) {
	// call lookupAccountSid and get name and domain
	
	unsigned long name_buffer_size = 0;
	unsigned long domain_name_size = 0;
	SID_NAME_USE peSid;
	
	// get buffer sizes first
	int success = LookupAccountSid( NULL,	// name of local or remote computer
		Sid,							// security identifier
		account,						// account name buffer
		&name_buffer_size,				// size of account name buffer
		domain,							// domain name
		&domain_name_size,				// size of domain name buffer
		&peSid							// SID type
		);	
	
	// set buffer sizes
	account = new char[name_buffer_size];
	domain = new char[domain_name_size];
	
	// now look up the sid and get the name and domain so we can compare 
	// them to what we're searching for
	success = LookupAccountSid( NULL,		// computer to lookup on (NULL means local)
		Sid,			// security identifier
		account,							// account name buffer
		&name_buffer_size,				// size of account name buffer
		domain,						// domain name
		&domain_name_size,				// size of domain name buffer
		&peSid							// SID type
		);	
	if ( ! success ) {
		dprintf(D_ALWAYS, "perm::LookupAccountSid failed (err=%d)\n", GetLastError());
		delete[] account;
		delete[] domain;
		return -1;
	}
	
	return 0;
}

//
// determines if user and domain matches the user and domain specifed in trustee
// 1 = yes, 0 = no, -1 = unknown/error
//
int perm::processUserTrustee( const char *account, const char *domain, const TRUSTEE *trustee ) {
	
	char *trustee_name = NULL;		// name of the trustee we're looking at
	char *trustee_domain = NULL;	// domain of the trustee we're looking at
	
	// if we have a user, we'll get the name of the user either 
	// from a string or Sid inside the trustee
	
	dprintf(D_FULLDEBUG,"in perm::processUserTrustee()\n");

	if ( trustee->TrusteeForm == TRUSTEE_IS_NAME ) // buffer that identifies Trustee is a string
	{			
		// break the trustee string down into domain and name
		
		char *trustee_str = new char[ strlen(trustee->ptstrName)+1];
		strcpy( trustee_str, trustee->ptstrName );
		getDomainAndName( trustee_str , trustee_domain, trustee_name );
		
		if ( domainAndNameMatch( account, trustee_name, domain, trustee_domain ) )
		{
			delete[] trustee_str;
			return 1;
		}
		delete[] trustee_str;
		return 0;
		
	} else if ( trustee->TrusteeForm == TRUSTEE_IS_SID ) {
		
		if ( getAccountFromSid( trustee->ptstrName, trustee_name, trustee_domain ) != 0 ) {
			// something bad went down when looking up the account sid, getAccountFromSid()
			// dprintf'd it, so we just need to get out of here and terminate with an error.
			return -1;
		}
		
		
		if ( domainAndNameMatch( account, trustee_name, domain, trustee_name ) )
		{				
			// Free buffers
			delete[] trustee_name;
			delete[] trustee_domain;
			return 1;
			
		} else {
			
			//Free Buffers
			delete[] trustee_name;
			delete[] trustee_domain;
			
			// account doesn't match anything in EXPLICIT_ACCESS structure, so return NO (0)
			return 0;
		}
		
	} else {
		
		// So the trustee is a user, but it isn't in the form of a
		// name or a SID. This case shouldn't happen, but even if it did 
		// I don't know what to do anyways, so just return error.
		
		dprintf(D_ALWAYS, "perm::userInExplicitAccess failed: Trustee object form is unrecognized");
		return -1;
	}
}

//
// Determines if user is a member of the local group specified in trustee
//
//  1 = yes, 0 = no, -1 = error
//
int perm::processLocalGroupTrustee( const char *account, const char *domain, const TRUSTEE *trustee ) {
	
	LOCALGROUP_MEMBERS_INFO_3 *buf, *cur; // group members output buffer pointers
	
	dprintf(D_FULLDEBUG,"in perm::processLocalGroupTrustee()\n");

	char *trustee_name = NULL;		// name of the trustee we're looking at
	char *trustee_domain = NULL;	// domain of the trustee we're looking at
	
	if ( trustee->TrusteeForm == TRUSTEE_IS_SID )
	{
		if ( getAccountFromSid( trustee->ptstrName, trustee_name, trustee_domain ) != 0 ) {
			// something bad went down when looking up the account sid, getAccountFromSid()
			// dprintf'd it, so we just need to get out of here and terminate with an error.
			return -1;
		}
	}
	else if ( trustee->TrusteeForm == TRUSTEE_IS_NAME )
	{
		char *trustee_str = new char[ strlen(trustee->ptstrName)+1];
		
		strcpy( trustee_str, trustee->ptstrName );
		getDomainAndName( trustee_str , trustee_domain, trustee_name );
	}
	else {
		// TRUSTEE is not a name or SID, which should never happen, but if it does then return 
		// an error because something must be really screwed
		return -1;
	}
	
	unsigned long entries_read;	
	unsigned long total_entries;
	NET_API_STATUS status;
	wchar_t *trustee_name_unicode = new wchar_t[strlen(trustee_name)+1]; 
	MultiByteToWideChar(CP_ACP, 0, trustee_name, -1, trustee_name_unicode, strlen(trustee_name)+1);
	
	DWORD resume_handle = 0;
	
	do {	 // loop until we have checked all the group members
		
		status = NetLocalGroupGetMembers ( 
			NULL,									// servername
			trustee_name_unicode,					// name of group
			3,										// information level
			(BYTE**) &buf,							// pointer to buffer that receives data
			16384,									// preferred length of data
			&entries_read,							// number of entries read
			&total_entries,							// total entries available
			&resume_handle							// resume handle 
			);	
		
		switch ( status ) {
		case ERROR_ACCESS_DENIED:
			dprintf(D_ALWAYS, "perm::NetLocalGroupGetMembers failed: ERROR_ACCESS_DENIED\n");
			NetApiBufferFree( buf );
			delete[] trustee_name_unicode;	
			delete[] trustee_domain; 
			dprintf(D_ALWAYS, "perm::NetLocalGroupGetMembers failed: (total entries: %d, entries read: %d )\n", total_entries, entries_read );
			return -1;
			break;
		case NERR_InvalidComputer:
			dprintf(D_ALWAYS, "perm::NetLocalGroupGetMembers failed: ERROR_InvalidComputer\n");
			NetApiBufferFree( buf );
			delete[] trustee_name_unicode;	
			delete[] trustee_domain; 
			dprintf(D_ALWAYS, "perm::NetLocalGroupGetMembers failed: (total entries: %d, entries read: %d )\n", total_entries, entries_read );
			return -1;
			break;
		case ERROR_NO_SUCH_ALIAS:
			dprintf(D_ALWAYS, "perm::NetLocalGroupGetMembers failed: ERROR_NO_SUCH_ALIAS\n");
			NetApiBufferFree( buf );
			delete[] trustee_name_unicode;	
			delete[] trustee_domain; 
			dprintf(D_ALWAYS, "perm::NetLocalGroupGetMembers failed: (total entries: %d, entries read: %d )\n", total_entries, entries_read );
			return -1;
			break;
		}
			

		DWORD i;

		for ( i = 0, cur = buf; i < entries_read; ++ i, ++ cur )
		{
			wchar_t* member_unicode = cur->lgrmi3_domainandname;
			// convert unicode string to ansi string
			char* member = new char[wcslen( member_unicode)+1];
			WideCharToMultiByte(CP_ACP, 0, member_unicode, -1, member, wcslen( member_unicode)+1, NULL, NULL);
			
			// compare domain and name to find a match
			char *member_name, *member_domain;
			getDomainAndName( member, member_domain, member_name );

			if ( domainAndNameMatch (account, member_name, domain, member_domain) )
			{
				delete[] member;
				delete[] trustee_name_unicode;	
				delete[] trustee_domain; 
				NetApiBufferFree( buf );
				return 1;
			}
			delete[] member;
		}
	} while ( status == ERROR_MORE_DATA );
	delete[] trustee_name_unicode;	
	delete[] trustee_domain; 
	// having exited the for loop without finding anything, we conclude
	// that the account does not exist in the explicit access structure
	
	NetApiBufferFree( buf );
	return 0;
} // end if is a local group

//
// Determines if user is a member of the global group specified in trustee
//
//  1 = yes, 0 = no, -1 = error
//
int perm::processGlobalGroupTrustee( const char *account, const char *domain, const TRUSTEE *trustee ) {
	
	char *trustee_name = NULL;		// name of the trustee we're looking at
	char *trustee_domain = NULL;	// domain of the trustee we're looking at

	dprintf(D_FULLDEBUG,"in perm::processGlobalGroupTrustee()\n");

	if ( trustee->TrusteeForm == TRUSTEE_IS_SID )
	{
		if ( getAccountFromSid( trustee->ptstrName, trustee_name, trustee_domain ) != 0 ) {
			// something bad went down when looking up the account sid, getAccountFromSid()
			// dprintf'd it, so we just need to get out of here and terminate with an error.
			return -1;
		}
		
	} else if ( trustee->TrusteeForm == TRUSTEE_IS_NAME ) {
		
		char *trustee_str = new char[ strlen(trustee->ptstrName)+1];
		strcpy( trustee_str, trustee->ptstrName );
		getDomainAndName( trustee_str , trustee_domain, trustee_name );
		
	} else {
		// TrusteeForm is not name or SID, so return error
		return -1;
	}
	
	unsigned char* BufPtr; // buffer pointer
	wchar_t* trustee_domain_unicode = new wchar_t[strlen(trustee_domain)+1];
	wchar_t* trustee_name_unicode = new wchar_t[strlen(trustee_name)+1];
	MultiByteToWideChar(CP_ACP, 0, trustee_domain, -1, trustee_domain_unicode, strlen(trustee_domain)+1);
	MultiByteToWideChar(CP_ACP, 0, trustee_name, -1, trustee_name_unicode, strlen(trustee_name)+1);
	delete[] trustee_domain;
	
	GROUP_USERS_INFO_0* group_members;
	unsigned long entries_read, total_entries;
	NET_API_STATUS status;
	
	// get domain controller name for the domain in question
	status = NetGetDCName( NULL,	// servername
		trustee_domain_unicode,		// domain to lookup
		&BufPtr						// pointer to buffer containing the name (Unicode string) of the Domain Controller
		);
	
	if (status == NERR_DCNotFound ) {
		dprintf(D_ALWAYS, "perm::NetGetDCName() failed: DCNotFound (domain looked up: %s)", trustee_domain);
		NetApiBufferFree( BufPtr );
		delete[] trustee_domain_unicode;
		delete[] trustee_name_unicode;
		
		return -1;
	} else if ( status == ERROR_INVALID_NAME ) {
		dprintf(D_ALWAYS, "perm::NetGetDCName() failed: Error Invalid Name (domain looked up: %s)", trustee_domain);
		NetApiBufferFree( BufPtr );
		delete[] trustee_domain_unicode;
		delete[] trustee_name_unicode;
		return -1;
	}
	
	wchar_t* DomainController = (wchar_t*) BufPtr;
	
	do {
		
		status = NetGroupGetUsers( DomainController,	// domain controller name
			trustee_name_unicode,						// domain to query
			0,											// level of info
			&BufPtr,									// pointer to buffer containing group members
			16384,										// preferred size of buffer
			&entries_read,								// # entries read
			&total_entries,								// total # of entries
			NULL										// resume pointer
			);
		
		group_members = (GROUP_USERS_INFO_0*) BufPtr;
		
		switch ( status ) {
		case NERR_Success:
		case ERROR_MORE_DATA:
			break;
		case ERROR_ACCESS_DENIED:
		case NERR_InvalidComputer:
		case NERR_GroupNotFound:
			char* DCname = new char[ wcslen( DomainController )+1 ];
			wsprintf(DCname, "%ws", DomainController);
			dprintf(D_ALWAYS, "perm::NetGroupGetUsers failed: (domain: %s, domain controller: %s, total entries: %d, entries read: %d, err=%d)", trustee_domain, DCname, total_entries, entries_read, GetLastError());
			delete[] DCname;
			delete[] trustee_domain_unicode;
			delete[] trustee_name_unicode;
			NetApiBufferFree( BufPtr );
			NetApiBufferFree( DomainController );
			return -1;
		}
		
		DWORD i;
		GROUP_USERS_INFO_0* cur;
		
		for ( i = 0, cur = group_members; i < entries_read; ++ i, ++ cur )			{
			
			char* t_domain;
			char* t_name;
			char *t_str = new char[ strlen(trustee->ptstrName)+1];
			strcpy( t_str, (char*)group_members->grui0_name );
			getDomainAndName( t_str, t_domain, t_name);	
			
			if ( domainAndNameMatch( account, t_name, domain, t_domain ) )
			{
				delete[] trustee_domain_unicode;
				delete[] trustee_name_unicode;
				delete[] t_str;
				NetApiBufferFree( BufPtr );
				NetApiBufferFree( DomainController );
				return 1;
			}
		}
	}while ( status == ERROR_MORE_DATA ); // loop if there's more group members to look at
	
	// exiting the for loop means we didn't find anything
	delete[] trustee_domain_unicode;
	delete[] trustee_name_unicode;
	NetApiBufferFree( BufPtr );
	NetApiBufferFree( DomainController );
	return 0;			
}

//
// check if given account and domain are in the specified EXPLICIT_ACCESS structure
// This could be granted or denied access, we don't care at this point
//
// return 0 = No, 1 = yes, -1 = unknown/error
//
int perm::userInExplicitAccess( const EXPLICIT_ACCESS &EAS, const char *account, const char *domain ) {
	
	char *trustee_name;		// name of the trustee we're looking at
	char *trustee_domain;	// domain of the trustee we're looking at
	
	if ( EAS.Trustee.TrusteeType == TRUSTEE_IS_USER ) 
	{
		return processUserTrustee( account, domain, &EAS.Trustee );
	} 
	else if ( ( EAS.Trustee.TrusteeType == TRUSTEE_IS_GROUP ) ||
		( EAS.Trustee.TrusteeType == TRUSTEE_IS_ALIAS ) ||
		( EAS.Trustee.TrusteeType == TRUSTEE_IS_WELL_KNOWN_GROUP ) )	// the trustee is a group, not a specific user
	{
		// Determine whether group is local or global
		
		char *trustee_str = new char[ strlen(EAS.Trustee.ptstrName)+1];
		strcpy( trustee_str, EAS.Trustee.ptstrName );
		getDomainAndName( trustee_str , trustee_domain, trustee_name );
		
		// for debugging
		//cout << "Name of trustee found is: " << trustee_name << endl;
		
		char computerName[MAX_COMPUTERNAME_LENGTH+1];
		unsigned long nameLength = MAX_COMPUTERNAME_LENGTH+1;
		
		int success = GetComputerName( computerName, &nameLength );
		
		if (! success ) {
			dprintf(D_ALWAYS, "perm::GetComputerName failed: (Err: %d)", GetLastError());
			delete[] trustee_str;
			return -1;
		}
		
		
		if ( strcmp( trustee_name, "Everyone" ) == 0 ) // if file is in group Everyone, we're done.
		{
			delete[] trustee_str;
			return 1;
		} else if ( trustee_domain == NULL || trustee_domain == "" || // if domain is local to this machine
			( strcmp(trustee_domain, "BUILTIN") == 0 ) ||
			( strcmp(trustee_domain, "NT AUTHORITY") == 0 ) ||
			( strcmp(trustee_domain, computerName ) == 0 ) ) {
			
			delete[] trustee_str;			
			return processLocalGroupTrustee( account, domain, &EAS.Trustee );			
		}
		else { // if group is global
			
			delete[] trustee_str;			
			return processGlobalGroupTrustee( account, domain, &EAS.Trustee );
		}
		
	} // is group
	else {
		// If it's not a user and not a group, I don't know what it is, so return error
		return -1;
	}	
} 

perm::perm() {
	psid =(PSID) &sidBuffer;
	sidBufferSize = 100;
	domainBufferSize = 80;
	must_freesid = false;
	/*	These shouldn't be needed...
	perm_read = FILE_GENERIC_READ;
	perm_write = FILE_GENERIC_WRITE;
	perm_execute = FILE_GENERIC_EXECUTE;
	*/
}

perm::~perm() {
	if ( psid && must_freesid ) FreeSid(psid);
	
	if ( Account_name ) {
		delete[] Account_name;
	}

	if ( Domain_name ) {
		delete[] Domain_name;
	}
}

bool perm::init( char *accountname, char *domain ) 
{
	SID_NAME_USE snu;
	
	if ( psid && must_freesid ) FreeSid(psid);
	must_freesid = false;
	
	psid = (PSID) &sidBuffer;

	dprintf(D_FULLDEBUG,"perm::init() starting up for account (%s) domain (%s)\n", accountname, domain);
	
	Account_name = new char[ strlen(accountname) +1 ];
	strcpy( Account_name, accountname );

	if ( domain )
	{
		Domain_name = new char[ strlen(domain) +1 ];
		strcpy( Domain_name, domain );
	}
	
	if ( !LookupAccountName( domain,		// Domain
		accountname,						// Account name
		psid, &sidBufferSize,				// Sid
		domainBuffer, &domainBufferSize,	// Domain
		&snu ) )							// SID TYPE
	{
		dprintf(D_ALWAYS,
			"perm::init: Lookup Account Name %s failed, using Everyone\n",
			accountname);
		
		// SID_IDENTIFIER_AUTHORITY  NTAuth = SECURITY_NT_AUTHORITY;
		SID_IDENTIFIER_AUTHORITY  NTAuth = SECURITY_WORLD_SID_AUTHORITY;
		if ( !AllocateAndInitializeSid(&NTAuth,1,
			SECURITY_WORLD_RID,
			0,
			0,0,0,0,0,0,&psid) ) 
		{	
			EXCEPT("Failed to find group EVERYONE");
		}
		must_freesid = true;
	} else {
		dprintf(D_FULLDEBUG,"perm::init: Found Account Name %s\n",
			accountname);
	}
	
	return true;
}


// perm:[read|write|execute]_access.  These functions return:
// -1 == unknown (error), 0 == no, 1 == yes.

int perm::read_access( const char * filename ) {
	if ( !volume_has_acls(filename) ) {
		return 1;
	}
	
	int p = get_permissions( filename );
	
	if ( p < 0 ) return -1;
	return ( p & FILE_GENERIC_READ ) == FILE_GENERIC_READ;
}

int perm::write_access( const char * filename ) {
	if ( !volume_has_acls(filename) ) {
		return 1;
	}
	
	int p = get_permissions( filename );
	
	if ( p < 0 ) return -1;
	
	return ( p & FILE_GENERIC_WRITE ) == FILE_GENERIC_WRITE;
}

int perm::execute_access( const char * filename ) {
	if ( !volume_has_acls(filename) ) {
		return 1;
	}
	
	int p = get_permissions( filename );
	
	if ( p < 0 ) return -1;
	return ( p & FILE_GENERIC_EXECUTE ) == FILE_GENERIC_EXECUTE;
}


// volume_has_acls() returns true if the filename exists on an NTFS
// volume or some other file system which preserves ACLs on files.	
// false if otherwise (i.e. FAT, FAT32, CDFS, HPFS, etc) or error.
bool perm::volume_has_acls( const char *filename )
{
	char path_buf[5];
	char *root_path = path_buf;
	DWORD foo,fsflags;
	
	path_buf[0] = '\0';
	
	// This function will not work with UNC paths... yet.
	if ( !filename || (filename[0]=='\\' && filename[1]=='\\') ) {
		EXCEPT("UNC pathnames not supported yet");
	}
	
	if ( filename[1] == ':' ) {
		root_path[0] = filename[0];
		root_path[1] = ':';
		root_path[2] = '\\';
		root_path[3] = '\0';
	} else {
		root_path = NULL;
	}
	
	if ( !GetVolumeInformation(root_path,NULL,0,NULL,&foo,&fsflags,
		NULL,0) ) 
	{
		dprintf(D_ALWAYS,
			"perm: GetVolumeInformation on volume %s FAILED err=%d\n",
			path_buf,
			GetLastError());
		return false;
	}
	
	if ( fsflags & FS_PERSISTENT_ACLS ) 	// note: single & (bit comparison)
		return true;
	else {
		
		dprintf(D_FULLDEBUG,"perm: volume %s does not have ACLS\n",path_buf);
		return false;
	}
}

// set_acls sets the acls on the filename (typically a directory).
// This is currently set to add GENERIC_ALL (otherwise known
// in NT-land as Full) permissions.

// This code is based off the book that Todd gave me.

int perm::set_acls( const char *filename )
{
	BOOL ret;
	LONG err;
	SECURITY_DESCRIPTOR *sdData;
	SECURITY_DESCRIPTOR absSD;
	PACL pacl;
	PACL pNewACL;
	DWORD newACLSize;
	BOOL byDef;
	BOOL haveDACL;
	DWORD sizeRqd;
	UINT x;
	ACL_SIZE_INFORMATION aclSize;
	ACCESS_ALLOWED_ACE *pace,*pace2;
	
	// If this is not on an NTFS volume, we're done.  In fact, we'll
	// likely crash if we try all the below ACL crap on a volume which
	// does not support ACLs. Dooo!
	if ( !volume_has_acls(filename) ) 
	{
		dprintf(D_FULLDEBUG, "perm::set_acls(%s): volume has no ACLS\n",filename);
		return 1;
	}
	
	// Make sure we have the sid.
	
	if ( psid == NULL ) 
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): do not have SID for user\n",filename);
		return -1;
	}
	
	// ----- Get a copy of the SD/DACL -----
	
	// find out how much mem is needed
	// to hold existing SD w/DACL
	sizeRqd=0;
	
	if (GetFileSecurity( filename, DACL_SECURITY_INFORMATION, NULL, 0, &sizeRqd )) 
	{
		dprintf(D_ALWAYS,"perm::set_acls() Unable to get SD size.\n");
		return -1;
	}
	
	err = GetLastError();
	
	
	if ( err != ERROR_INSUFFICIENT_BUFFER ) 
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to get SD size.\n", filename);
		return -1;
	}
	
	// allocate that memory
	sdData=( SECURITY_DESCRIPTOR * ) malloc( sizeRqd );
	
	if ( sdData == NULL ) 
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to allocate memory.\n",filename);
		return -1;
	}
	
	// actually get the SD info
	if (!GetFileSecurity( filename, DACL_SECURITY_INFORMATION,sdData, sizeRqd, &sizeRqd )) 
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to get SD info.\n", filename);
		free(sdData);
		return -1;
	}
	
	// ----- Create a new absolute SD and DACL -----
	
	// initialize absolute SD
	ret=InitializeSecurityDescriptor(&absSD,SECURITY_DESCRIPTOR_REVISION);
	if (!ret)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to init new SD.\n", filename);
		free(sdData);
		return -1;
	}
	
	// get the DACL info
	ret=GetSecurityDescriptorDacl(sdData,&haveDACL, &pacl, &byDef);
	if (!ret)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to get DACL info.\n", filename);
		free(sdData);
		return -1;
	}
	
	if (!haveDACL)
	{
		// compute size of new DACL
		//
		// Modified: 11/1/1999 J.Drews
		// The new code requires TWO ACL's to be added to work correctly
		//
		newACLSize= (sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(psid) - sizeof(DWORD))*2;
	}
	else
	{
		// get size info about existing DACL
		ret=GetAclInformation(pacl, &aclSize, sizeof(ACL_SIZE_INFORMATION), AclSizeInformation);
		
		// compute size of new DACL
		//
		// Modified: 11/1/1999 J.Drews
		// The new code requires TWO ACL's to be added to work correctly
		//
		newACLSize = aclSize.AclBytesInUse + (sizeof(ACCESS_ALLOWED_ACE) +
			GetLengthSid(psid) - sizeof(DWORD))*2;
	}
	
	// allocate memory
	//	pNewACL=(PACL) GlobalAlloc(GPTR, newACLSize);
	pNewACL=(PACL) malloc(newACLSize);
	if (pNewACL == NULL)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to allocate memory.\n",filename);
		free(sdData);
		return -1;
	}
	
	// initialize the new DACL
	ret=InitializeAcl(pNewACL, newACLSize, ACL_REVISION);
	if (!ret)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to init new DACL.\n",filename);
		free(pNewACL);
		free(sdData);
		return -1;
	}
	
	// ----- Copy existing DACL into new DACL -----
	
	// BUG Notice by J.Drews
	// Not sure what NT will do if the DACL we are going to add already
	// exists. For condor this shouldn't happen, but it would be a good
	// thing to check to see if any of the DACLs we are copying are for
	// the SID we are adding. If so, don't add them. Will leave to the
	// condor team to fix that if they desire.
	if (haveDACL)
	{
		// copy ACEs from existing DACL
		// to new DACL
		for (x=0; x<aclSize.AceCount; x++)
		{
			ret=GetAce(pacl, x, (LPVOID *) &pace);
			if (!ret)
			{
				dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to get ACE.\n", filename);
				free(pNewACL);
				free(sdData);
				return -1;
			}
			
			ret=AddAce(pNewACL, ACL_REVISION, MAXDWORD, pace, pace->Header.AceSize);
			if (!ret)
			{
				dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to add ACE.\n", filename);
				free(pNewACL);
				free(sdData);
				return -1;
			}
		}
	}
	
	// ----- Add the new ACE to the new DACL -----
	
	// add access allowed ACE to new DACL
	// Removed 11/1/1999 J.Drews, we can't use this as it doesn't set
	// the flags correctly. Have to build the ACE by hand.
	// ret=AddAccessAllowedAce(pNewACL, ACL_REVISION, GENERIC_ALL, psid);
	// if (!ret)
	// {
	//	dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to add access allowed ACE.\n", filename);
	//	free(pNewACL);
	//	free(sdData);
	//	return -1;
	// }
	
	//
	// Added 11/1/1999 J.Drews
	// We have to build the ACE ourselves to get the flags and such
	// set correctly. The old way only set the "special directory access"
	// bits and not the "special file access" bits. This prevented condor
	// from working as expected (could not modify any files copied over by
	// the condor system).
	DWORD acelen = GetLengthSid(psid) +sizeof(ACCESS_ALLOWED_ACE) -sizeof(DWORD);
	pace = (ACCESS_ALLOWED_ACE *)  malloc(acelen);
	
	if (pace == NULL)
	{
		dprintf(D_ALWAYS,"perm::set_acls(%s): unable to allocate memory",filename);
		free(pNewACL);
		free(sdData);
		return -1;
	}
	
	// Fill in ACCESS_ALLOWED_ACE structure.
	pace->Mask = GENERIC_ALL; /* 0x10000000 */
	pace->Header.AceType = ACCESS_ALLOWED_ACE_TYPE; 
	pace->Header.AceFlags = INHERIT_ONLY_ACE|OBJECT_INHERIT_ACE; /*0x09*/
	pace->Header.AceSize = (WORD) acelen; 
	memcpy(&(pace->SidStart),psid,GetLengthSid(psid) ); 	 
	ret = AddAce(pNewACL, ACL_REVISION, MAXDWORD, pace, pace->Header.AceSize);
	if(!ret)
	{
		// Handle AddAce error.
		dprintf(D_ALWAYS,"perm::set_acls(%s): unable to add accesses allowed ACE. (%d)\n",filename,ret);
		free(pNewACL);
		free(sdData);
		free(pace);
		return -1;
	}
	
	pace2 = (ACCESS_ALLOWED_ACE *)	malloc(acelen);
	if (pace2 == NULL)
	{
		dprintf(D_ALWAYS,"perm::set_acls(%s): unable to allocate memory",filename);
		free(pNewACL);
		free(sdData);
		return -1;
	}
	
	// Fill in ACCESS_ALLOWED_ACE structure.
	
	pace2->Mask = STANDARD_RIGHTS_ALL | 0x000001ff; /*0x001f01ff*/
	/* couldn't find a define for the lower end bits - J.Drews */
	pace2->Header.AceType = ACCESS_ALLOWED_ACE_TYPE; 
	pace2->Header.AceFlags = CONTAINER_INHERIT_ACE; /*0x02*/
	pace2->Header.AceSize = (WORD) acelen;
	memcpy(&(pace2->SidStart),psid,GetLengthSid(psid) );	  
	ret = AddAce(pNewACL, ACL_REVISION, MAXDWORD, pace2, pace2->Header.AceSize);
	if(!ret)
	{
		ret = GetLastError();
		// Handle AddAce error.
		dprintf(D_ALWAYS,"perm::set_acls(%s): unable to add accesses allowed ACE.(%d)\n",filename,ret);
		free(pNewACL);
		free(sdData);
		free(pace);
		free(pace2);
		return -1;
	}
	
	
	// set the new DACL
	// in the absolute SD
	ret=SetSecurityDescriptorDacl(&absSD, TRUE, pNewACL, FALSE);
	if (!ret)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to install DACL.\n", filename);
		free(pNewACL);
		free(sdData);
		free(pace);
		free(pace2);
		return -1;
	}
	
	// check the new SD
	ret=IsValidSecurityDescriptor(&absSD);
	if (!ret)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): SD invalid.\n", filename);
		free(pNewACL);
		free(sdData);
		free(pace);
		free(pace2);
		return -1;
	}
	
	// ----- Install the new DACL -----
	
	// install the updated SD
	err=SetFileSecurity( filename, DACL_SECURITY_INFORMATION, &absSD);
	if (!err)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to set file SD (err=%d).\n", filename,GetLastError() );
		free(pNewACL);
		free(sdData);
		free(pace);
		free(pace2);
		return 0;
	}
	
	// release memory
	free(pNewACL);
	free(sdData);
	free(pace);
	free(pace2);
	
	return 1;
}

#ifdef WANT_FULL_DEBUG
// Main method for testing the Perm functions
int 
main(int argc, char* argv[]) {
	
	perm* foo = new perm();
	//char p_ntdomain[80];
	//char buf[100];
	
	if (argc < 2) { 
		cout << "Enter a file name" << endl;
		return (1);
	}
	
	foo->init("stolley", "OWL");
	
	cout << "Checking write access for " << argv[1] << endl;
	
	int result = foo->write_access(argv[1]);
	
	if ( result == 1) {
		cout << "You can write to " << argv[1] << endl;
	}
	else if (result == 0) {
		cout << "You are not allowed to write to " << argv[1] << endl;
	}
	else if (result == -1) {
		cout << "An error has occured\n";
	}
	else {
		cout << "Ok you're screwed" << endl;
	}
	delete foo;
	return(0);
}

#endif