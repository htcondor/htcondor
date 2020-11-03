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

#include "condor_common.h"
#include "condor_debug.h"
#include "perm.h"
#include "domain_tools.h"
#include "Lm.h"
#include "dynuser.h"

// init from param_boolean("SEC_TRY_REMOTE_USER_IN_LOCAL_DOMAIN", true)
bool perm::try_remote_user_in_local_domain = true;

//
// get_permissions:  1 = yes, 0 = no, -1 = unknown/error
//
int perm::get_permissions( const char *file_name, ACCESS_MASK &AccessRights ) {
	DWORD retVal;
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
			int i = (int)strlen( file_name ) - 1;
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
			retVal = get_permissions( new_file_name, AccessRights );
			delete[] new_file_name;
			
			// ... and return what it returns. (after deleting the string that was
			// allocated.
			
			return retVal;
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
		delete[] pSD;
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
		delete[] pSD;
		return -1;
	}
	
	// This is the workaround for the broken API GetEffectiveRightsFromAcl().
	// It should be guaranteed to work on all versions of NT and 2000 but be aware
	// that nested global group permissions are not supported.
	// C. Stolley - June 2001

	ACL_SIZE_INFORMATION acl_info;
		// Structure contains the following members:
		//  DWORD   AceCount; 
		//  DWORD   AclBytesInUse; 
		//  DWORD   AclBytesFree; 



	// first get the number of ACEs in the ACL
		if (! GetAclInformation( pacl,		// acl to get info from
								&acl_info,	// buffer to receive info
								sizeof(acl_info),  // size in bytes of buffer
								AclSizeInformation // class of info to retrieve
								) ) {
			dprintf(D_ALWAYS, "Perm::GetAclInformation failed with error %d\n", GetLastError() );
			return -1;
		}

		ACCESS_MASK allow = 0x0;
		ACCESS_MASK deny = 0x0;

		unsigned int aceCount = acl_info.AceCount;
		
		int result;
		
		// now look at each ACE in the ACL and see if it contains the user we're looking for
		for (unsigned int i=0; i < aceCount; i++) {
			LPVOID current_ace;

			if (! GetAce(	pacl,	// pointer to ACL 
							i,		// index of ACE we want
							&current_ace	// pointer to ACE
							) ) {
				dprintf(D_ALWAYS, "Perm::GetAce() failed! Error code %d\n", GetLastError() );
				return -1;
			}

			dprintf(D_FULLDEBUG, "Calling Perm::userInAce() for %s\\%s\n",
				   (Domain_name) ? Domain_name : "NULL",
				   (Account_name) ? Account_name : "NULL" );
			result = userInAce ( current_ace, Account_name, Domain_name );		
			
			if (result == 1) {
				switch ( ( (PACE_HEADER) current_ace)->AceType ) {				
				case ACCESS_ALLOWED_ACE_TYPE:
				case ACCESS_ALLOWED_OBJECT_ACE_TYPE:
					allow |= ( (ACCESS_ALLOWED_ACE*) current_ace)->Mask;
					break;
				case ACCESS_DENIED_ACE_TYPE:
				case ACCESS_DENIED_OBJECT_ACE_TYPE:
					deny |= ( (ACCESS_DENIED_ACE*) current_ace)->Mask;
					break;
				}
			}
		}
		
		AccessRights = allow;
		AccessRights &= ~deny;

	// and now if we've made this far everything's happy so return true
	return 1;
}


//
// Determines if user is a member of the local group group_name
//
//  1 = yes, 0 = no, -1 = error
//
int perm::userInLocalGroup( const char *account, const char *domain, const char *group_name ) {
	
	LOCALGROUP_MEMBERS_INFO_3 *buf, *cur; // group members output buffer pointers
	
	dprintf(D_FULLDEBUG,"in perm::userInLocalGroup() looking at group '%s'\n", (group_name) ? group_name : "NULL");

	unsigned long entries_read;	
	unsigned long total_entries;
	NET_API_STATUS status;
	wchar_t group_name_unicode[MAX_GROUP_LENGTH+1];
	_snwprintf(group_name_unicode, MAX_GROUP_LENGTH+1, L"%S", group_name);
	group_name_unicode[MAX_GROUP_LENGTH] = 0;
	
	
	DWORD_PTR resume_handle = 0;
	
	do {	 // loop until we have checked all the group members
		
		status = NetLocalGroupGetMembers ( 
			NULL,									// servername
			group_name_unicode,					// name of group
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
			dprintf(D_ALWAYS, "perm::NetLocalGroupGetMembers failed: (total entries: %d, entries read: %d )\n", 
				total_entries, entries_read );
			return -1;
			break;
		case NERR_InvalidComputer:
			dprintf(D_ALWAYS, "perm::NetLocalGroupGetMembers failed: ERROR_InvalidComputer\n");
			NetApiBufferFree( buf );
			dprintf(D_ALWAYS, "perm::NetLocalGroupGetMembers failed: (total entries: %d, entries read: %d )\n", 
				total_entries, entries_read );
			return -1;
			break;
		case ERROR_NO_SUCH_ALIAS:
			dprintf(D_ALWAYS, "perm::NetLocalGroupGetMembers failed: ERROR_NO_SUCH_ALIAS\n");
			NetApiBufferFree( buf );
			dprintf(D_ALWAYS, "perm::NetLocalGroupGetMembers failed: (total entries: %d, entries read: %d )\n", 
				total_entries, entries_read );
			return -1;
			break;
		}
			

		DWORD i;

		for ( i = 0, cur = buf; i < entries_read; ++ i, ++ cur )
		{
			wchar_t* member_unicode = cur->lgrmi3_domainandname;
			// convert unicode string to ansi string
			char member[MAX_DOMAIN_LENGTH+MAX_ACCOUNT_LENGTH+2];  // domain+acct+slash+null
			snprintf(member, MAX_DOMAIN_LENGTH+MAX_ACCOUNT_LENGTH+2, "%S", member_unicode);
			
			// compare domain and name to find a match
			char *member_name, *member_domain;
			getDomainAndName( member, member_domain, member_name );

			if ( domainAndNameMatch (account, member_name, domain, member_domain) )
			{
				NetApiBufferFree( buf );
				return 1;
			}
		}
	} while ( status == ERROR_MORE_DATA );
	// having exited the for loop without finding anything, we conclude
	// that the account does not exist in the explicit access structure
	
	NetApiBufferFree( buf );
	return 0;
} // end if is a local group

//
// Determines if user is a member of the global group group_name on domain group_domain
//
//  1 = yes, 0 = no, -1 = error
//
int perm::userInGlobalGroup( const char *account, const char *domain, const char* group_name, const char* group_domain ) {
	
	dprintf(D_FULLDEBUG,"in perm::processGlobalGroupTrustee() looking at group '%s\\%s'\n", 
		(group_domain) ? group_domain : "NULL", (group_name) ? group_name : "NULL" );

	unsigned char* BufPtr; // buffer pointer
	wchar_t group_domain_unicode[MAX_DOMAIN_LENGTH+1];	// computer names restricted to 254 chars
	wchar_t group_name_unicode[MAX_GROUP_LENGTH+1];	// groups limited to 256 chars
	_snwprintf(group_domain_unicode, MAX_DOMAIN_LENGTH+1, L"%S", group_domain);
	_snwprintf(group_name_unicode, MAX_GROUP_LENGTH+1, L"%S", group_name);
	group_domain_unicode[MAX_DOMAIN_LENGTH] = 0;
	group_name_unicode[MAX_GROUP_LENGTH] = 0;
	
	GROUP_USERS_INFO_0 *group_members;
	unsigned long entries_read, total_entries;
	NET_API_STATUS status;
	
	// get domain controller name for the domain in question
	status = NetGetDCName( NULL,	// servername
		group_domain_unicode,		// domain to lookup
		&BufPtr						// pointer to buffer containing the name (Unicode string) of the Domain Controller
		);
	
	if (status == NERR_DCNotFound ) {
		dprintf(D_ALWAYS, "perm::NetGetDCName() failed: DCNotFound (domain looked up: %s)\n", group_domain);
		NetApiBufferFree( BufPtr );
		return -1;
	} else if ( status == ERROR_INVALID_NAME ) {
		dprintf(D_ALWAYS, "perm::NetGetDCName() failed: Error Invalid Name (domain looked up: %s)", group_domain);
		NetApiBufferFree( BufPtr );
		return -1;
	}
	
	wchar_t* DomainController = (wchar_t*) BufPtr;
	
	do {
		
		status = NetGroupGetUsers( DomainController,	// domain controller name
			group_name_unicode,							// domain to query
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
			dprintf(D_ALWAYS, "perm::NetGroupGetUsers failed: (domain: %s, domain controller: %s, total entries: %d, entries read: %d, err=%d)",
				group_domain, DCname, total_entries, entries_read, GetLastError());
			delete[] DCname;
			NetApiBufferFree( BufPtr );
			NetApiBufferFree( DomainController );
			return -1;
		}
		
		DWORD i;
				
		for ( i = 0; i < entries_read; i++ )			{
			
			char t_name[MAX_ACCOUNT_LENGTH+1]; // account names are restricted to 20 chars, but I'm 
								// gonna be safe and say 256.
							 
			snprintf(t_name, MAX_ACCOUNT_LENGTH+1, "%S", group_members[i].grui0_name);
			dprintf(D_FULLDEBUG, "GlobalGroupMember: %s\n", t_name);
			//getDomainAndName( t_str, t_domain, t_name);	
			
			if ( domainAndNameMatch( account, t_name, domain, group_domain ) )
			{
				//delete[] t_str;
				NetApiBufferFree( BufPtr );
				NetApiBufferFree( DomainController );
				return 1;
			}
		
		}
	}while ( status == ERROR_MORE_DATA ); // loop if there's more group members to look at
	
	// exiting the for loop means we didn't find anything
	NetApiBufferFree( BufPtr );
	NetApiBufferFree( DomainController );
	return 0;			
}

//
// check if given account and domain are in the specified ACE
// This could be granted or denied access, we don't care at this point
//
// return 0 = No, 1 = yes, -1 = unknown/error
//
int perm::userInAce ( const LPVOID cur_ace, const char *account, const char *domain ) {
	
	char *trustee_name = NULL;		// name of the trustee we're looking at
	char *trustee_domain =NULL;	// domain of the trustee we're looking at
	unsigned long name_buffer_size = 0;
	unsigned long domain_name_size = 0;
	SID_NAME_USE peSid;
	int result;

	LPVOID ace_sid = &((ACCESS_ALLOWED_ACE*) cur_ace)->SidStart;
	
	// lookup the ACE's SID
	// get buffer sizes first
	int success = LookupAccountSid( NULL,	// name of local or remote computer
		ace_sid,							// security identifier
		trustee_name ,						// account name buffer
		&name_buffer_size,					// size of account name buffer
		trustee_domain,						// domain name
		&domain_name_size,					// size of domain name buffer
		&peSid								// SID type
		);	
	
	// set buffer sizes
	trustee_name = new char[name_buffer_size];
	trustee_domain = new char[domain_name_size];
	
	// now look up the sid and get the name and domain so we can compare 
	// them to what we're searching for
	success = LookupAccountSid( NULL,		// computer to lookup on (NULL means local)
		ace_sid,								// security identifier
		trustee_name,						// account name buffer
		&name_buffer_size,					// size of account name buffer
		trustee_domain,						// domain name
		&domain_name_size,					// size of domain name buffer
		&peSid								// SID type
		);	

	if ( ! success ) {
		dprintf(D_ALWAYS, "perm::LookupAccountSid failed (err=%d)\n", GetLastError());
		if (trustee_name) { delete[] trustee_name; trustee_name = NULL; }
		if (trustee_domain) { delete[] trustee_domain; trustee_domain = NULL; }
		return -1;
	} else {
		dprintf(D_FULLDEBUG, "perm::UserInAce: Checking %s\\%s\n", 
			trustee_domain, trustee_name);
	}

	if ( peSid == SidTypeUser ) 
	{
		result = domainAndNameMatch( account, trustee_name, domain, trustee_domain );
	} 
	else if ( ( peSid == SidTypeGroup ) ||
		( peSid == SidTypeAlias ) ||
		( peSid == SidTypeWellKnownGroup ) )	// the trustee is a group, not a specific user
	{
		// Determine whether group is local or global
		
		char computerName[MAX_COMPUTERNAME_LENGTH+1];
		unsigned long nameLength = MAX_COMPUTERNAME_LENGTH+1;
		char* builtin = getBuiltinDomainName();
		char* nt_authority = getNTAuthorityDomainName();

		BOOL bSuccess = GetComputerName( computerName, &nameLength );
		if ( ! bSuccess ) {
			// this should never happen
			dprintf(D_ALWAYS, "perm::GetComputerName failed: (Err: %d)", GetLastError());
			result = -1; // failure
		} else if ( strcmp( trustee_name, "Everyone" ) == 0 ) { 
			// if file is in group Everyone, we're done.
			result = 1; 
		} else if ((trustee_domain == NULL) || 
			( strcmp( trustee_domain, "" ) == 0 ) ||
			( strcmp(trustee_domain, builtin) == 0 ) ||
			( strcmp(trustee_domain, nt_authority) == 0 ) ||
			( strcmp(trustee_domain, computerName ) == 0 ) ) {
			
			result = userInLocalGroup( account, domain, trustee_name );			
		
		} else { // if group is global
			result = userInGlobalGroup( account, domain, trustee_name, trustee_domain );
		}

		delete[] builtin;
		delete[] nt_authority;
	} // is group
	else {
		// If it's not a user and not a group, I don't know what it is, so return error
		result = -1;
	}	

	if (trustee_name) { delete[] trustee_name; trustee_name = NULL; }
	if (trustee_domain) { delete[] trustee_domain; trustee_domain = NULL; }

	return result;
} 

perm::perm() {
	psid =(PSID)sidBuffer;
	sidBufferSize = COUNTOF(sidBuffer);
	domainBufferSize = COUNTOF(domainBuffer);
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

bool perm::init( const char *accountname, const char *domain ) 
{
	SID_NAME_USE snu;
	char qualified_account[1024];
	
	if ( psid && must_freesid ) FreeSid(psid);
	must_freesid = false;
	
	psid = (PSID) &sidBuffer;
	sidBufferSize = sizeof(sidBuffer);

	dprintf(D_FULLDEBUG,"perm::init() starting up for account (%s) "
			"domain (%s)\n", accountname, ( domain ? domain : "NULL"));

	// copy class-wide variables for account and domain
	
	Account_name = new char[ strlen(accountname) +1 ];
	strcpy( Account_name, accountname );

	if ( domain  && (strcmp(domain, ".") != 0) )
	{
		Domain_name = new char[ strlen(domain) +1 ];
		strcpy( Domain_name, domain );
	
		// now concatenate our domain and account name for LookupAccountName()
		// so that we have domain\username
		if ( 0 > _snprintf(qualified_account, 1023, "%s\\%s",
			   	domain, accountname) ) {
		
			dprintf(D_ALWAYS, "Perm object: domain\\account (%s\\%s) "
				"string too long!\n", domain, accountname );
			return false;
		}

	} else {

		// no domain specified, so just copy the accountname.
		Domain_name = NULL;
		strncpy(qualified_account, accountname, 1024);
		qualified_account[1024-1] = 0;
	}

	domainBufferSize = COUNTOF(domainBuffer);
	if ( !LookupAccountName( NULL,			// System
		qualified_account,					// Account name
		psid, &sidBufferSize,				// Sid
		domainBuffer, &domainBufferSize,	// Domain
		&snu ) )							// SID TYPE
	{
		// if we can't find the account using the given domain
		// try without a domain
		if (Domain_name && try_remote_user_in_local_domain) 
		{
			dprintf(D_ALWAYS,
				   "perm::init: Lookup Account Name %s failed (err=%lu), trying %s\n",
				   qualified_account, GetLastError(), accountname);

			delete[] Domain_name;
			Domain_name = NULL;
			sidBufferSize = COUNTOF(sidBuffer);
			domainBufferSize = COUNTOF(domainBuffer);
			domainBuffer[0] = 0;

			if (LookupAccountName( NULL, // System
			    accountname,
			    psid, &sidBufferSize,
			    domainBuffer, &domainBufferSize,
			    &snu ))
			{
				dprintf(D_ALWAYS,"perm::init: Using Account Name %s (in domain %s)\n",
				       accountname, domainBuffer);
				return true;
			}
		}
		dprintf(D_ALWAYS,
			"perm::init: Lookup Account Name %s failed (err=%lu), using Everyone\n",
			qualified_account, GetLastError());
		
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
	
	ACCESS_MASK rights = NULL;

	int p = get_permissions( filename, rights );
	
	if ( p < 0 ) return -1;
	return ( rights & FILE_GENERIC_READ ) == FILE_GENERIC_READ;
}

int perm::write_access( const char * filename ) {
	if ( !volume_has_acls(filename) ) {
		return 1;
	}
	
	ACCESS_MASK rights = NULL;

	int p = get_permissions( filename, rights );

	if ( p < 0 ) return -1;
	
/*	Just some harmless debugging output

	printf("Rights mask: 0x%08x\n", rights);
	printf("FILE_GENERIC_WRITE mask: 0x%08x\n", FILE_GENERIC_WRITE);
	printf("GENERIC_WRITE mask: 0x%08x\n", GENERIC_WRITE);
	rights = ( rights & FILE_GENERIC_WRITE );
	printf("Logical AND result mask: 0x%08x\n", rights);

	bool test = ( rights == FILE_GENERIC_WRITE );
*/
	return (rights & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE;
}

int perm::execute_access( const char * filename ) {
	if ( !volume_has_acls(filename) ) {
		return 1;
	}
	
	ACCESS_MASK rights = NULL;

	int p = get_permissions( filename, rights );

	if ( p < 0 ) return -1;
	return ( rights & FILE_GENERIC_EXECUTE ) == FILE_GENERIC_EXECUTE;
}


// volume_has_acls() returns true if the filename exists on an NTFS
// volume or some other file system which preserves ACLs on files.	
// false if otherwise (i.e. FAT, FAT32, CDFS, HPFS, etc) or error.
bool perm::volume_has_acls( const char *filename )
{
	char *root_path;
	DWORD foo,fsflags;

	if ( !filename || !filename[0] ) {
		dprintf(D_ALWAYS, "perm::volume_has_acls(): NULL or zero-length input\n");
		return false;
	}

	if ( filename[1] == ':' ) {
		root_path = (char*)malloc(4);
		if (root_path == NULL) {
			EXCEPT("Out of memory!");
		}
		root_path[0] = filename[0];
		root_path[1] = ':';
		root_path[2] = '\\';
		root_path[3] = '\0';
	}
	else if (filename[0] == '\\' && filename[1] == '\\') {
		// UNC path: strip down to the form: "\\SERVER\SHARE\"
		char const *next_backslash;
		next_backslash = strchr(&filename[2], '\\');
		if (!next_backslash) {
			dprintf(D_ALWAYS,
			        "perm::volume_had_acls(): incomplete UNC volume spec: %s\n",
			        filename);
			return false;
		}
		next_backslash = strchr(next_backslash + 1, '\\');
		if (next_backslash) {
			// found backslash after share name; use everything up to it (inclusive)
			int len_to_copy = next_backslash - filename + 1;
			root_path = (char*)malloc(len_to_copy + 1);
			if (root_path == NULL) {
				EXCEPT("Out of memory!");
			}
			memcpy(root_path, filename, len_to_copy);
			root_path[len_to_copy] = '\0';
		}
		else {
			// found no fourth backslash; copy the whole string and tack one on
			int len_to_copy = (int)strlen(filename);
			root_path = (char*)malloc(len_to_copy + 2);
			if (root_path == NULL) {
				EXCEPT("Out of memory!");
			}
			memcpy(root_path, filename, len_to_copy);
			root_path[len_to_copy] = '\\';
			root_path[len_to_copy + 1] = '\0';
		}
	}
	else {
		// use current working volume
		root_path = NULL;
	}
	
	if ( !GetVolumeInformation(root_path,NULL,0,NULL,&foo,&fsflags,
		NULL,0) ) 
	{
		dprintf(D_ALWAYS,
		        "perm: GetVolumeInformation on volume %s FAILED err=%d\n",
				root_path ? root_path : "(null)",
		        GetLastError());
		if (root_path) {
			free(root_path);
		}
		return false;
	}
	if (root_path) {
		free(root_path);
	}
	
	if ( fsflags & FS_PERSISTENT_ACLS ) 	// note: single & (bit comparison)
		return true;
	else {
		
		dprintf(D_FULLDEBUG,"perm: file %s on volume with no ACLS\n",filename);
		return false;
	}
}

// set_acls sets the acls on the filename (typically a directory).
// This is currently set to add GENERIC_ALL (otherwise known
// in NT-land as Full) permissions.

bool
perm::set_acls( const char *filename )
{
	PACL newDACL, oldDACL;
	PSECURITY_DESCRIPTOR pSD;
	DWORD err;
	EXPLICIT_ACCESS ea;
	PEXPLICIT_ACCESS entryList;
	ULONG entryCount;
	unsigned int i;
	
	pSD = NULL;
	newDACL = oldDACL = NULL;
	
	
	// If this is not on an NTFS volume, we're done.  In fact, we'll
	// likely crash if we try all the below ACL junk on a volume which
	// does not support ACLs. Dooo!
	if ( !volume_has_acls(filename) ) 
	{
		dprintf(D_FULLDEBUG, "perm::set_acls(%s): volume has no ACLS\n",
				filename);
		// return true (success) here, so upper layers do not consider
		// this a fatal error --- thus allowing us to run on FAT32.
		return true;
	}
	
	// Make sure we have the sid.
	
	if ( psid == NULL ) 
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): do not have SID for user\n",
				filename);
		return false;
	}

	// first get the file's old DACL so we can copy it into the new one.
	// NOTE: oldDACL will be a pointer into the pSD structure - the code below
	// will eventually call LocalFree(pSD), but should not call LocalFree(oldDACL) because
	// the oldDACL is deallocated when the pSD is deallocated.
	err = GetNamedSecurityInfo((char*)filename, SE_FILE_OBJECT,
			DACL_SECURITY_INFORMATION, NULL, NULL, &oldDACL, NULL, &pSD);

	if ( ERROR_SUCCESS != err ) {
		// this is intentionally D_FULLDEBUG, since this error often occurs
		// if the caller doesn't have WRITE_DAC access to the path. The 
		// remedy in that case is to call set_owner() on the path first.
		dprintf(D_FULLDEBUG, "perm::set_acls(%s): failed to get security info. "
				"err=%d\n", filename, err);
		return false;
	}

	// now, check to make sure we don't already have an entry in ACL
	// that matches the one we're about to insert.
	err = GetExplicitEntriesFromAcl(oldDACL, &entryCount, &entryList); 

	if ( ERROR_SUCCESS != err ) {
		dprintf(D_ALWAYS, "perm::set_acls(%s): failed to get entries from ACL. "
				"err=%d\n", filename, err);
		LocalFree(pSD);
		return false;
	}

	for (i=0; i<entryCount; i++) {
		if (	( entryList[i].grfAccessPermissions == GENERIC_ALL ) &&
				( entryList[i].grfAccessMode == GRANT_ACCESS ) &&
				( entryList[i].Trustee.TrusteeForm == TRUSTEE_IS_SID ) &&
				( EqualSid(entryList[i].Trustee.ptstrName, psid) ) ) {

			// MATCH - the ACE is already in the ACL,
			// so just return success.
			
			dprintf(D_FULLDEBUG, "set_acls() found a matching ACE already "
					"in the ACL, so skipping the add\n");

			LocalFree(entryList);
			LocalFree(pSD);
			return true;
		}
	}
	
	// didn't find the ACE in there already, so proceed to add it.
	LocalFree(entryList);

	// now set up an EXPLICIT_ACCESS structure for the new ACE
	
	ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
	ea.grfAccessPermissions = GENERIC_ALL;
	ea.grfAccessMode        = GRANT_ACCESS;
	ea.grfInheritance       = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
	ea.Trustee.pMultipleTrustee = NULL;
	ea.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
	ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
	ea.Trustee.ptstrName = (char*)psid;

	// create the new ACL with the new ACE
	err = SetEntriesInAcl(1, &ea, oldDACL, &newDACL);

	if ( ERROR_SUCCESS != err ) {
		dprintf(D_ALWAYS, "perm::set_acls(%s): failed to add new ACE "
				"(err=%d)\n", filename, err);

		LocalFree(pSD);
		return false;
	}

	// Attach new ACL to the file

	// PROTECTED_DACL_SECURITY_INFORMATION causes the function to NOT
	// inherit its parent's ACL. I believe this is what we want.
	err = SetNamedSecurityInfo((char*)filename, SE_FILE_OBJECT,
		DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
		NULL,NULL,newDACL,NULL); 
	
	if ( err == ERROR_ACCESS_DENIED ) {
		// set the SE_SECURITY_NAME privilege and try again.
		
		dprintf(D_FULLDEBUG, "SetFileSecurity() failed; "
				"adding SE_SECURITY_NAME priv and trying again.\n");

		HANDLE hToken = NULL;

		if (!OpenProcessToken(GetCurrentProcess(), 
			TOKEN_ADJUST_PRIVILEGES, &hToken)) {

          dprintf(D_ALWAYS, "perm: OpenProcessToken failed: %u\n",
				  GetLastError()); 
       } else {

	    	// Enable the SE_SECURITY_NAME privilege.
    		if (!SetPrivilege(hToken, SE_SECURITY_NAME, TRUE)) {
	   			dprintf(D_ALWAYS, "perm: can't set SE_SECURITY_NAME privs "
					   "to set ACLs.\n");
    		} else { 
				err = SetNamedSecurityInfo((char*)filename, SE_FILE_OBJECT,
				DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
				NULL,NULL,newDACL,NULL); 
			}
			CloseHandle(hToken);
		}
	}

	// clean up our memory.
	LocalFree(pSD);
	LocalFree(newDACL);

	if (err != ERROR_SUCCESS)
	{
		dprintf(D_ALWAYS, "perm::set_acls(%s): Unable to set file ACL "
				"(err=%d).\n", filename,GetLastError() );
		return false;
	}
	
	return true;
}

// sets the owner of the file. Unfortunately, this 
// function will fail if you're not running with 
// Administrator priviledges. Also, this function
// can set the owner on a local or remote file or 
// directory on a NTFS file system, Windows NT network 
// sharename, registry key, semaphore, event, mutex, 
// file mapping, or waitable timer. Phew!
bool perm::set_owner( const char *location ) {
	PSID owner_SID;
	DWORD size=0, d_size=0;
	SID_NAME_USE usage;
	char qualified_name[1024];

	domainBufferSize = COUNTOF(domainBuffer);
	sidBufferSize = COUNTOF(sidBuffer);

	owner_SID = (PSID)sidBuffer;

	if ( Domain_name ) {

		if ( 0 > _snprintf(qualified_name, 1023, "%s\\%s",
		   	Domain_name, Account_name) ) {
		
			dprintf(D_ALWAYS, "Perm: domain\\account (%s\\%s) "
				"string too long!\n", Domain_name, Account_name );
			return false;
		}
	} else {
		strncpy(qualified_name, Account_name, 1023);
	}

	if ( !LookupAccountName( NULL,			// System
		qualified_name,						// Account name
		owner_SID, &sidBufferSize,			// Sid
		domainBuffer, &domainBufferSize,	// Domain
		&usage) ) {							// SID TYPE
		dprintf(D_ALWAYS, "perm: LookupAccountName(%s, size) failed "
				"(err=%d)\n", qualified_name, GetLastError());
		return false;
	}

	DWORD err = SetNamedSecurityInfo((char*)location,
		SE_FILE_OBJECT,
		OWNER_SECURITY_INFORMATION,
		owner_SID,
		NULL,
		NULL,
		NULL);

	if ( err == ERROR_ACCESS_DENIED ) {

		// We have to enable the SE_TAKE_OWNERSHIP_NAME priv
		// for our access token. So do that, and try 
		// SetNamedSecurityInfo() again.

		HANDLE hToken = NULL;

		if (!OpenProcessToken(GetCurrentProcess(), 
			TOKEN_ADJUST_PRIVILEGES, &hToken)) 
       {
          dprintf(D_ALWAYS, "perm: OpenProcessToken failed: %u\n",
				  GetLastError()); 
       } else {

	    	// Enable the SE_TAKE_OWNERSHIP_NAME privilege.
    		if (!SetPrivilege(hToken, SE_TAKE_OWNERSHIP_NAME, TRUE)) {
	   			dprintf(D_ALWAYS, "perm: lacking Administrator privs "
					   "to set owner.\n");
    		} else { 
				err = SetNamedSecurityInfo((char*)location,
				SE_FILE_OBJECT,
				OWNER_SECURITY_INFORMATION,
				owner_SID,
				NULL,
				NULL,
				NULL);
			}
			CloseHandle(hToken);
		}
	}

	if ( err != ERROR_SUCCESS ) {
		dprintf(D_ALWAYS, "perm: SetNamedSecurityInfo(%s) failed (err=%d)\n",
				location, err);
		return false; 
	}

	dprintf(D_FULLDEBUG, "perm: successfully set owner on %s to %s\n",
		   	location, qualified_name);
	return true;
}

// Adapted from code in MSDN
bool SetPrivilege(
    HANDLE hToken,          // access token handle
    LPCTSTR lpszPrivilege,  // name of privilege to enable/disable
    BOOL bEnablePrivilege   // to enable or disable privilege
    ) 
{
	TOKEN_PRIVILEGES tp;
	LUID luid;

	if ( !LookupPrivilegeValue( 
		NULL,            // lookup privilege on local system
		lpszPrivilege,   // privilege to lookup 
		&luid ) )        // receives LUID of privilege
	{
		dprintf(D_ALWAYS, "LookupPrivilegeValue error: %u\n", GetLastError() ); 
		return false; 
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	if (bEnablePrivilege) {
	    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	} else {
	    tp.Privileges[0].Attributes = 0;
	}

	// Enable the privilege or disable all privileges.

	if ( !AdjustTokenPrivileges(
		hToken, 
		FALSE, 
		&tp, 
		sizeof(TOKEN_PRIVILEGES), 
		(PTOKEN_PRIVILEGES) NULL, 
		(PDWORD) NULL) )
	{ 
		dprintf(D_ALWAYS, "AdjustTokenPrivileges error: %u\n", GetLastError()); 
		return false; 
	} 

	return true;
}


#ifdef PERM_OBJ_DEBUG

extern void set_debug_flags( const char *strflags, int flags );
extern "C" FILE	*DebugFP;

// Main method for testing the Perm functions
int 
main(int argc, char* argv[]) {
	
	char *filename, *domain, *username;
	perm* foo = new perm();

	DebugFP = stdout;
	set_debug_flags( "D_ALL", 0 );
	//char p_ntdomain[80];
	//char buf[100];
	
	if (argc < 4) { 
		cout << "Usage:\nperm filename domain username" << endl;
		return (1);
	}
	filename = argv[1];
	domain = argv[2];
	username = argv[3];
	
	foo->init(username, domain);
	
	cout << "Checking write access for " << filename << endl;
	
	int result = foo->write_access(filename);
	
	if ( result == 1) {
		cout << "You can write to " << filename << endl;
	}
	else if (result == 0) {
		cout << "You are not allowed to write to " << filename << endl;
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
