#include "condor_common.h"
#include "dynuser.h"
#include <windows.h>
#include <lmaccess.h>
#include <lmerr.h>
#include <lmwksta.h>
#include <lmapibuf.h>


////
//
// Constructor:  Set some variables up.
//
////

dynuser::dynuser() {
	psid =(PSID) &sidBuffer;
	sidBufferSize = max_sid_length;
	domainBufferSize = max_domain_length;
	logon_token = NULL;
	accountname = NULL;
	password = NULL;
	accountname_t = NULL;
	password_t = NULL;
}

////
//
// createuser:  This creates a user, adds it to the "users" group and logs it in
//
////

bool dynuser::createuser(char *username){ 
	if (!accountname) 
		accountname = new char[100];
	if (!password)
		password = new char[100];
	if (!accountname_t)
		accountname_t = new wchar_t[100];
	if (!password_t)
		password_t = new wchar_t[100];
	this->createpass();

	strcpy(accountname, username);  // This used to add condor-run- to the username, but
									// now it doesn't.

	this->update_t();				// Make the accountname_t and password_t accounts
	this->createaccount();			// Create the account
	this->update_psid();			// Updates our psid;
//	this->dump_groups();			// This tests to make sure that we're reading groups correctly.
	if (!this->add_users_group()) {	// Add this user to the users group
		return false;
	}
	if (!this->add_batch_privilege() ) {	// Give this user the right to run as a batch job. (SIDE EFFECT = sets the PSID)
		return false;
	}


	// Log that user on and get a handle to the token.

	if (!LogonUser(accountname,		// accountname
		".",						// domain
		password,					// password
		LOGON32_LOGON_BATCH,		// logon type
		LOGON32_PROVIDER_DEFAULT,	// Logon provider
		&logon_token)				// And the token to stuff it in.
		) {
		dprintf(D_ALWAYS,"LogonUser(%s, ... ) failed with status %d",
			accountname,GetLastError());
		
		return false;
	}
	
	dprintf(D_FULLDEBUG,"dynuser::createuser(%s) successful\n",accountname);
	
	return true;

}


////
//
//  Destructor::  This deletes the user and logs it out.
//
////

dynuser::~dynuser() {

	if ( accountname_t ) {

		bool success;
		if ( accountname ) {
			success = deleteuser(accountname);
		} else {
			// should never happen
			success = false;
		}

		if (success) {
			dprintf(D_FULLDEBUG,"dynuser: Removed user %s\n",accountname);
		} else {
			dprintf(D_ALWAYS,"dynuser: Removing user %s FAILED!\n",accountname);
		}
	}

	if ( accountname )		delete [] accountname;
	if ( accountname_t )	delete [] accountname_t;
	if ( password )			delete [] password;
	if ( password_t )		delete [] password_t;
	
	if ( logon_token )		CloseHandle ( logon_token );
}



void dynuser::createpass() {
	if ( !password ) abort();
	
	for ( int i = 0; i < 14; i++ ) {
		char c = (char) ( rand() % 256 );

		if ( !isprint( c ) ) { // For sanity.  This leaves many characters to chose from.
			i--;
		} else {
			password[i] = c;
		}
	}

	password[14] = 0;
	
	// cout << "Shhhh... the password is "<<password<<endl;
}


void dynuser::update_t() {
	if ( accountname && accountname_t ) {
		if (!MultiByteToWideChar( 0, MB_ERR_INVALID_CHARS, accountname, -1, accountname_t, 100)) {
			abort();
		}
	}
	if ( password && password_t ) {
		if (!MultiByteToWideChar( 0, MB_ERR_INVALID_CHARS, password, -1, password_t, 100)) {
			abort();
		}
	}
}


HANDLE dynuser::get_token() {
	return this->logon_token;
}

void InitString( UNICODE_STRING &us, wchar_t *psz ) {
	USHORT cch = wcslen( psz );
	us.Length = cch * sizeof(wchar_t);
	us.MaximumLength = (cch + 1) * sizeof (wchar_t);
	us.Buffer = psz;
}

bool dynuser::add_batch_privilege() {
	bool retval = true;
	wchar_t *priv = L"SeBatchLogonRight"; //SeLogonBatch

	UNICODE_STRING machine;
	{
		PWKSTA_INFO_100 pwkiWorkstationInfo;
		DWORD netret = NetWkstaGetInfo( NULL, 100, 
			(LPBYTE *)&pwkiWorkstationInfo);
		if ( netret != NERR_Success ) {
			EXCEPT("dynuser::add_batch_privilege(): Cannot determine workstation name\n");
		}
		InitString( machine, 
			(wchar_t *)pwkiWorkstationInfo->wki100_computername);
	}


	LSA_HANDLE hPolicy = 0;
	LSA_OBJECT_ATTRIBUTES oa;//	= { sizeof oa };
	ZeroMemory(&oa, sizeof(oa));
	if (LsaOpenPolicy(&machine,		// Computer Name (NULL == this machine?)
		&oa,						// Object Attributes
		POLICY_LOOKUP_NAMES | POLICY_CREATE_ACCOUNT,	// Type of access required
		&hPolicy ) != STATUS_SUCCESS)					// Pointer to the policy handles
	{
		dprintf(D_ALWAYS,"dynuser::add_batch_priv() LsaOpenPolicy failed\n");
		retval = false;
	}

	UNICODE_STRING usPriv; InitString( usPriv, priv );

	SetLastError(0);

	if (retval && LsaAddAccountRights( hPolicy, psid, &usPriv, 1 ) != STATUS_SUCCESS) {
		dprintf(D_ALWAYS,"dynuser::add_batch_priv() LsaAddAccountRights failed\n");
		retval = false;
	}

	LsaClose( hPolicy );
	return retval;
}

////
//
// dynuser::createaccount() -- make the account
//
////

void dynuser::createaccount() {
	USER_INFO_1 userInfo = { accountname_t, password_t, 0,				// Name / Password
							 USER_PRIV_USER,							// Priv Level 
							 L"",										// Home Dir
							 L"Dynamically created Condor account.",	// Comment
							 UF_SCRIPT,									// flags (req'd)
							 L"" };										// script path
	DWORD nParam = 0;

	// Remove the account, if it exists...
	NetUserDel(0,accountname_t);


	NET_API_STATUS nerr = NetUserAdd (0,		// Machine
						 1,						// struct version
						 (BYTE *)&userInfo,
						 &nParam);

	if ( NERR_Success == nerr ) {
		dprintf(D_FULLDEBUG, "Account %s created successfully\n",accountname);
	} else if ( NERR_UserExists == nerr ) {
		EXCEPT("createaccount: User %s already exists",accountname);
	} else {
		EXCEPT("Account %s creation failed! (err=%d)",accountname,nerr);
	}
}


////
//
// dynuser::add_users_group:  add the user to the "users" group
//
////

bool dynuser::add_users_group() {
	// Add this user to group "users"
	LOCALGROUP_MEMBERS_INFO_0 lmi;

	lmi.lgrmi0_sid = this->psid;

	NET_API_STATUS nerr = NetLocalGroupAddMembers(
	  NULL,				// LPWSTR servername,      
	  L"Users",			// LPWSTR LocalGroupName,  
	  0,				// DWORD level,            
	  (LPBYTE) &lmi,	// LPBYTE buf,             
	  1				// DWORD membercount       
	);

	if ( NERR_Success == nerr ) {
		return true;
	}
	else if ( nerr == ERROR_ACCESS_DENIED ) {
		EXCEPT("User %s not added to \"Users\" group, access denied.",accountname);
	}

	// Any other error...
	EXCEPT("Cannot add %s to \"Users\" group, unknown error (err=%d).",accountname,nerr);
	
	return false;
}


////
//
// dynuser::del_users_group:  remove the user from the "users" group
//
////

bool dynuser::del_users_group() {
	// Add this user to group "users"
	LOCALGROUP_MEMBERS_INFO_0 lmi;

	lmi.lgrmi0_sid = this->psid;

	NET_API_STATUS nerr = NetLocalGroupDelMembers(
	  NULL,				// LPWSTR servername,      
	  L"Users",			// LPWSTR LocalGroupName,  
	  0,				// DWORD level,            
	  (LPBYTE) &lmi,	// LPBYTE buf,             
	  1				// DWORD membercount       
	);

	if ( NERR_Success == nerr ) {
		return true;
	}

	dprintf(D_ALWAYS,"dynuser::del_users_group() NetLocalGroupDelMembers failed\n");	
	return false;
}

#if 0
// Dump the groups...
bool dynuser::dump_groups() {

	LOCALGROUP_INFO_0 *lgz;
	unsigned long numentries;
	unsigned long totalentries;

	NET_API_STATUS nerr = NetLocalGroupEnum(
		NULL,								//LPWSTR servername,    
		(DWORD) 0,							//DWORD level,          
		(BYTE **) &lgz,						//LPBYTE *bufptr,       
		100 * sizeof(LOCALGROUP_INFO_0),	//DWORD prefmaxlen,     
		&numentries,						//LPDWORD entriesread,  
		&totalentries,						//LPDWORD totalentries, 
		NULL								//LPDWORD resumehandle  
	);

	if ( nerr == NERR_Success ) {
		cout << "Got the dump of "<<numentries<<" things!"<<endl;
		for (int i = 0; i < numentries; i++) {
			cout << "Group "<<i<<":\t";
			wcout << lgz[i].lgrpi0_name;
			cout << "\t";

			cout << lgz[i].lgrpi0_name << endl;
		}

		return true;
	}

	cout << "Didn't get the dump!"<<endl;

	//NetApiBufferFree(ndg);  Without this, this leaks RAM

	return false;

}
#endif 

void dynuser::update_psid() {
	SID_NAME_USE snu;

	sidBufferSize = max_sid_length;
	domainBufferSize = max_domain_length;

	if ( !LookupAccountName( 0,				// Domain
		accountname,						// Acocunt name
		psid, &sidBufferSize,				// Sid
		domainBuffer, &domainBufferSize,	// Domain
		&snu ) )							// SID TYPE
	{
		dprintf(D_ALWAYS,"dynuser::update_psid() LookupAccountName(%s) failed!\n", accountname);
		// EXCEPT("dynuser:Lookup Account Name %s failed!",accountname);
	}
}


bool dynuser::deleteuser(char* username ) {

	bool retval = true;

	// allocate working buffers if needed
	if (!accountname) 
		accountname = new char[100];
	if (!accountname_t)
		accountname_t = new wchar_t[100];

	strcpy( accountname, username);	// Used to add condor-run-, but not anymore.
	
	this->update_t( );				// Make the accountname_t and password_t accounts
	this->update_psid();			// Updates our psid;


	// get machine name
	UNICODE_STRING machine;
	{
		PWKSTA_INFO_100 pwkiWorkstationInfo;
		DWORD netret = NetWkstaGetInfo( NULL, 100, 
			(LPBYTE *)&pwkiWorkstationInfo);
		if ( netret != NERR_Success ) {
			EXCEPT("dynuser::deleteuser(): Cannot determine workstation name\n");
		}
		InitString( machine, 
			(wchar_t *)pwkiWorkstationInfo->wki100_computername);
	}

	// open a policy
	LSA_HANDLE hPolicy = 0;
	LSA_OBJECT_ATTRIBUTES oa;//	= { sizeof oa };
	ZeroMemory(&oa, sizeof(oa));
	if (LsaOpenPolicy(&machine,		// Computer Name (NULL == this machine?)
		&oa,						// Object Attributes
		POLICY_LOOKUP_NAMES | POLICY_CREATE_ACCOUNT,	// Type of access required
		&hPolicy ) != STATUS_SUCCESS)					// Pointer to the policy handles
	{
		dprintf(D_ALWAYS,"dynuser::deleteuser() LsaOpenPolicy failed\n");
		retval = false;
	}

	SetLastError(0);

	// according to microsoft docs, LsaRemoveAccountRights will remove
	// the account and all rights if the 3rd param is TRUE, so NetUserDel
	// doesn't need to be called.
	NTSTATUS result;
	result = LsaRemoveAccountRights( hPolicy, psid, 1, 0, 0 );
	if (retval && result != STATUS_SUCCESS) {
        NET_API_STATUS nerr = NetUserDel( NULL, accountname_t );
        if ( nerr != NERR_Success ) {			
			dprintf(D_ALWAYS,"dynuser::deleteuser() failed!\n", result);
			retval = false;
		} else {
			dprintf(D_FULLDEBUG,"dynuser::deleteuser() Successfully deleted NT user %s\n", username);
		}
	} else {
		dprintf(D_FULLDEBUG,"dynuser::deleteuser() Successfully removed NT user %s\n", username);
	}

	LsaClose( hPolicy );

	// Delete accountname_t so destructor does not try to remove the account again
	delete [] accountname_t;
	accountname_t = NULL;
	
	return retval;

}


// this function will remove all accounts starting with user_prefix

bool dynuser::cleanup_condor_users(char* user_prefix) {

	LPUSER_INFO_10 pBuf = NULL;
	LPUSER_INFO_10 pTmpBuf;
	DWORD dwEntriesRead = 0;
	DWORD dwTotalEntries = 0;
	DWORD dwResumeHandle = 0;
	DWORD dwTotalCount = 0;
	NET_API_STATUS nStatus;

	bool retval = true;


    // check for valid user_prefix
	if (!user_prefix || !*user_prefix) {
		dprintf(D_ALWAYS,"dynuser::cleanup_condor_users() called with no user_prefix\n");
		return false;
	}

	do {
		nStatus = NetUserEnum(NULL,                     // NULL means local server
								10,                     // accounts, names, and comments
								FILTER_NORMAL_ACCOUNT,  // global users
								(LPBYTE*)&pBuf,
								MAX_PREFERRED_LENGTH,   // buffer size
								&dwEntriesRead,
								&dwTotalEntries,
								&dwResumeHandle);

		// If the call succeeds,
		if ((nStatus == NERR_Success) || (nStatus == ERROR_MORE_DATA)) {
			if ((pTmpBuf = pBuf) != NULL) {
		
				// Loop through the entries.
				for (DWORD i = 0; (i < dwEntriesRead); i++) {
					assert(pTmpBuf != NULL);

					if (pTmpBuf == NULL) {
						dprintf(D_ALWAYS,"dynuser::cleanup_condor_users() Access violation\n");
						retval = false;
						break;
					}

					// convert from unicode to ascii
					char buf[1024]; // this is the max size sprintf will print into anyways
					wsprintf(buf, "%ws", pTmpBuf->usri10_name);

					
					if (strnicmp( buf, user_prefix, strlen(user_prefix)) == 0) {
						dprintf(D_FULLDEBUG,"dynuser::cleanup_condor_users() Found orphaned account: %s\n", buf);
						{
							dynuser du;
							retval = retval && du.deleteuser(buf);
						}
					}

					pTmpBuf++;
					dwTotalCount++;
				}
			}
		} else {
			dprintf(D_ALWAYS,"dynuser::cleanup_condor_users() Got bad status (%d) from NetUserEnum\n", nStatus);
			retval = false;
		}

		// Free the allocated buffer.
		if (pBuf != NULL) {
			NetApiBufferFree(pBuf);
			pBuf = NULL;
		}
	} while (nStatus == ERROR_MORE_DATA);


	// Check again for allocated memory.
	if (pBuf != NULL) {
		NetApiBufferFree(pBuf);
		pBuf = NULL;
	}

	return retval;
}
