#include "condor_common.h"
#include "dynuser.h"
#include <windows.h>
#include <lmaccess.h>
#include <lmerr.h>
#include <lmwksta.h>


////
//
// Constructor:  Set some variables up.
//
////

dynuser::dynuser() {
	psid =(PSID) &sidBuffer;
	sidBufferSize = 100;
	domainBufferSize = 80;
	logon_token = NULL;
	accountname_t = NULL;
}

////
//
// createuser:  This creates a user, adds it to the "users" group and logs it in
//
////

bool dynuser::createuser(char *username){ 
	accountname = new char[100];
	password = new char[100];
	accountname_t = new wchar_t[100];
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
		NET_API_STATUS nerr = NetUserDel(0,accountname_t);

		if (nerr != NERR_Success) {
			dprintf(D_ALWAYS,"dynuser: Removing user %s FAILED!\n",accountname);
		} else {
			dprintf(D_FULLDEBUG,"dynuser: Removed user %s\n",accountname);
		}
	}

	if ( accountname )		delete accountname;
	if ( accountname_t )	delete accountname_t;
	if ( password )			delete password;
	if ( password_t )		delete password_t;
	
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
	if (!MultiByteToWideChar( 0, MB_ERR_INVALID_CHARS, accountname, -1, accountname_t, 100)) {
		abort();
	}
	if (!MultiByteToWideChar( 0, MB_ERR_INVALID_CHARS, password, -1, password_t, 100)) {
		abort();
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

	if ( !LookupAccountName( 0,				// Domain
		accountname,						// Acocunt name
		psid, &sidBufferSize,				// Sid
		domainBuffer, &domainBufferSize,	// Domain
		&snu ) )							// SID TYPE
	{
		EXCEPT("dynuser:Lookup Account Name %s failed!",accountname);
	}
}


bool dynuser::deleteuser(char* username ) {
	accountname = new char[100];

	strcpy( accountname, username);	// Used to add condor-run-, but not anymore.
	
	this->update_t( );				// Make the accountname_t and password_t accounts

	NET_API_STATUS nerr = NetUserDel( NULL, accountname_t);
	if ( nerr != NERR_Success ) return false;
	
	return true;
}