/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_common.h"
#include "condor_debug.h"
#include "perm.h"
#include "domain_tools.h"
#include "Lm.h"
#include "dynuser.h"

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
	
	// This is the workaround for the broken API GetEffectiveRightsFromAcl().
	// It should be guaranteed to work on all versions of NT and 2000 but be aware
	// that nested global group permissions are not supported.
	// C. Stolley - June 2001

	ACL_SIZE_INFORMATION* acl_info = new ACL_SIZE_INFORMATION();
		// Structure contains the following members:
		//  DWORD   AceCount; 
		//  DWORD   AclBytesInUse; 
		//  DWORD   AclBytesFree; 



	// first get the number of ACEs in the ACL
		if (! GetAclInformation( pacl,		// acl to get info from
								acl_info,	// buffer to receive info
								24,			// size in bytes of buffer
								AclSizeInformation // class of info to retrieve
								) ) {
			dprintf(D_ALWAYS, "Perm::GetAclInformation failed with error %d\n", GetLastError() );
			return -1;
		}

		ACCESS_MASK allow = 0x0;
		ACCESS_MASK deny = 0x0;

		unsigned int aceCount = acl_info->AceCount;

		delete acl_info; // all we wanted was the ACE count
		
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
	
	
	DWORD resume_handle = 0;
	
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

		
		int success = GetComputerName( computerName, &nameLength );
		
		if (! success ) {
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

bool perm::init( const char *accountname, char *domain ) 
{
	SID_NAME_USE snu;
	char qualified_account[1024];
	
	if ( psid && must_freesid ) FreeSid(psid);
	must_freesid = false;
	
	psid = (PSID) &sidBuffer;

	dprintf(D_FULLDEBUG,"perm::init() starting up for account (%s) "
			"domain (%s)\n", accountname, ( domain ? domain : "NULL"));

	// copy class-wide variables for account and domain
	
	Account_name = new char[ strlen(accountname) +1 ];
	strcpy( Account_name, accountname );

	if ( domain )
	{
		Domain_name = new char[ strlen(domain) +1 ];
		strcpy( Domain_name, domain );
	
		// now concatenate our domain and account name for LookupAccountName()
		// so that we have domain\username
		if ( 0 > snprintf(qualified_account, 1023, "%s\\%s",
			   	domain, accountname) ) {
		
			dprintf(D_ALWAYS, "Perm object: domain\\account (%s\\%s) "
				"string too long!\n", domain, accountname );
			return false;
		}

	} else {

		// no domain specified, so just copy the accountname.
		Domain_name = NULL;
		strncpy(qualified_account, accountname, 1023);
	}

	if ( !LookupAccountName( NULL,			// System
		qualified_account,					// Account name
		psid, &sidBufferSize,				// Sid
		domainBuffer, &domainBufferSize,	// Domain
		&snu ) )							// SID TYPE
	{
		
		dprintf(D_ALWAYS,
			"perm::init: Lookup Account Name %s failed (err=%lu), using Everyone\n",
			accountname, GetLastError());
		
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
	// likely crash if we try all the below ACL junk on a volume which
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


#ifdef PERM_OBJ_DEBUG

extern void dprintf_config( char* subsys, FILE* logfd );
extern void set_debug_flags( char *strflags );
extern "C" FILE	*DebugFP;

// Main method for testing the Perm functions
int 
main(int argc, char* argv[]) {
	
	char *filename, *domain, *username;
	perm* foo = new perm();

	DebugFP = stdout;
	set_debug_flags( "D_ALL" );
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
