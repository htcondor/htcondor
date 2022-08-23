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
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "dynuser.h"
#include "system_info.WINDOWS.h"
#include <lmaccess.h>
#include <lmerr.h>
#include <lmwksta.h>
#include <lmapibuf.h>


// language-independant way to get at the name of the Local System account
// delete[] the result!
//
char* getSystemAccountName() {
	return getWellKnownName(SECURITY_LOCAL_SYSTEM_RID);
}

// language-independant way to get at the BUILTIN\Users group name
// delete[] the result!
char* getUserGroupName() {
	return getWellKnownName(
		SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS);
}

// looks up Users group and returns the BUILTIN domain
// which for example is VORDEFINIERT on German systems
char*
getBuiltinDomainName() {
	return getWellKnownName(
		SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS, true);
}

// looks up SYSTEM account and returns the NT AUTHORITY
// domain, which for example is NT-AUTHORITAT on German systems.
char*
getNTAuthorityDomainName() {
	return getWellKnownName(SECURITY_LOCAL_SYSTEM_RID, 0, true);
}

// looks up well known SIDs and RIDs. The optional domainname,
// parameter, when true, causes it to return the domain name
// instead of the account name.
// Seems like two sub-authorities should be enough for what we need.
// delete[] the result!
char* 
getWellKnownName( DWORD subAuth1, DWORD subAuth2, bool domainname ) {
	
	PSID pSystemSID;
	SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
	char *systemName;
	char *systemDomain;
	DWORD name_size, domain_size;
	SID_NAME_USE sidUse;
	bool result;
	
	name_size = domain_size = 255;
	systemName =	new char[name_size];
	systemDomain =	new char[domain_size];

	// Create a well-known SID for the Users Group.
	
	if(! AllocateAndInitializeSid( &auth, ((subAuth2) ? 2 : 1),
		subAuth1,
		subAuth2,
		0,0, 0, 0, 0, 0,
		&pSystemSID) ) {
		printf( "AllocateAndInitializeSid Error %u\n", GetLastError() );
		return NULL;
	}
	
	// Now lookup whatever the account name is for this SID
	
	result = LookupAccountSid(
		NULL,			// System name (or NULL for loca)
		pSystemSID,		// ptr to SID to lookup
		systemName,		// ptr to buffer that receives name
		&name_size,			// size of buffer
		systemDomain,	// ptr to domain buffer
		&domain_size,			// size of domain buffer
		&sidUse
		);
	FreeSid(pSystemSID);
	
	if ( ! result ) {
		printf( "LookupAccountSid Error %u\n", GetLastError() );
		return NULL;
	} else if ( domainname ) {
		delete[] systemName;
		return systemDomain;	
	} else {
		delete[] systemDomain;
		return systemName;
	}
}

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
	reuse_account = true; // default is	don't delete accounts when done
}

////
//
// init_user: sets up an account to do our work, either by 
//				logging on an existing condor account if 
//				we have one, else create one.
////

bool dynuser::init_user() { 

	// if we don't have an accountname determined yet,
	// lets get that straight. 
 	if ( !accountname ) {
 		accountname = new char[100];
		

 		// How we initialize username: if we are the starter,
		// we set it to be condor-slotX. However, if
		// reuse_account = false we initialize it to be 
		// condor-run-<pid>.

		char *logappend = NULL;
		char* tmp = NULL;
		char slot_num[10];

		reuse_account = param_boolean_crufty("REUSE_CONDOR_RUN_ACCOUNT", true);
	 
		if ( reuse_account ) {
	
			MyString prefix;
			char* resource_prefix = param("STARTD_RESOURCE_PREFIX");
			if (resource_prefix != NULL) {
				prefix.formatstr(".%s", resource_prefix);
				free(resource_prefix);
			}
			else {
				prefix = ".slot";
			}
				
			logappend = param("STARTER_LOG");
			tmp = strstr(logappend, prefix.Value());
			
			if ( tmp ) {
				
				// copy the slot # from the log filename
	
				strncpy(slot_num, tmp+1, 10);
				slot_num[9] = '\0'; // make sure it's terminated

			} else {
	
				// must not be an smp machine, so just use slot #1
		 		
				dprintf(D_FULLDEBUG, "Dynuser: Couldn't param slot# - using 1 by default\n");
				snprintf(slot_num,10,"slot1");
		 	}
			int ret=_snprintf(accountname, 100, 
				ACCOUNT_PREFIX_REUSE "%s", slot_num);
		 	if ( ret < 0 ) {
		 		EXCEPT("account name to create too long");
		 	}
		} else { 
			// don't reuse accounts, so just name the
			// condor-run-<pid>

			// we used to call DaemonCore for the PID, but
			// this should accomplish the same thing and we
			// don't have to link in DC to everything non-DC that uses
			// Uids.C and Dynuser.C -stolley 7/2002
			int current_pid = GetCurrentProcessId();
			int ret=_snprintf(accountname, 100, 
				ACCOUNT_PREFIX "%d", current_pid);
		 	if ( ret < 0 ) {
		 		EXCEPT("account name to create too long");
		 	}
		}

		if ( logappend ) { free(logappend); logappend = NULL; tmp = NULL; }
		

	} else if ( logon_token && accountname ) {
			// account is already logged in, so just return
		return true; 
	} 

	// ok we have an account name, but its not logged in,
	// so lets take care of that.

	// generate a random password
	if (! password ) {
 		password = new char[100];
	}
	createpass();
	
	
	// logon the user with our new password
	return logon_user();
}

//
// logon_user() - carries out what is necessary to
//		log the current account in. If the user
//		doesn't exist (existing_user = false) then
//		the account is first created and then logged in.
////
bool dynuser::logon_user(){ 
	if (!accountname_t)
		accountname_t = new wchar_t[100];
	if (!password_t)
		password_t = new wchar_t[100];

	this->update_t();	// copy accountname_t and password_t from ascii 
						// versions accountname and password


	if (! this->update_psid() ) {	// returns false if user can't be found.
								// Updates our psid if its successful. 

		dprintf(D_FULLDEBUG, "update_psid() failed, so creating a new account\n");

		// since update_psid failed, create the account we want
		this->createaccount();		// Create the account
		//	this->dump_groups();	// This tests to make sure that we're reading groups correctly.

		if (! this->update_psid() ) { // now update sid 
			dprintf(D_ALWAYS, "update_psid() failed after account creation!\n");
			return false;
		}

		if (!this->add_users_group()) {	// Add this user to the users group
			dprintf(D_ALWAYS, "dynuser::logon_user() - Failed to add account to users group!\n");
		}

		if (!this->hide_user()) {	// Hide this user
			dprintf(D_ALWAYS, "dynuser::logon_user() - Failed to "
				"hide the user from the welcome screen!\n");
		}

	} else {	// if update_psid is successful, set the password
				//	on the account we're reusing


		// first enable the account
		dprintf(D_FULLDEBUG, "dynuser: Re-enabling account (%s)\n", accountname);
		enable_account();

		// set password on account to what we want it to be (shhh)
		USER_INFO_1003 ui = {
			password_t
		};
	
		if ( NERR_Success !=  NetUserSetInfo( 
			NULL, 			// servername
			accountname_t, 	// username
			1003, 			// level for setting password
			(LPBYTE) &ui,	// buf containing info
			NULL 			// # of offending parameter (ignored)
			)) {
		   	dprintf(D_ALWAYS, "Error setting password on account %s\n",
			   	accountname );
		}

	}
	// Log that user on and get a handle to the token.

	if (!LogonUser(accountname,		// accountname
		".",						// domain
		password,					// password
		LOGON32_LOGON_INTERACTIVE,	// logon type
		LOGON32_PROVIDER_DEFAULT,	// Logon provider
		&logon_token)				// And the token to stuff it in.
		) {
			dprintf(D_ALWAYS,"LogonUser(%s, ... ) failed with status %d\n",
				accountname,GetLastError());
			return false;
	}
	
	dprintf(D_FULLDEBUG,"dynuser::createuser(%s) successful\n",accountname);
	
	return true;

}

////
//
// reset() - clears/closes all current account information and
//			handle pointers, etc.
//
////
void
dynuser::reset() {

	reuse_account = true;

	if ( accountname ) {
		delete [] accountname;
		accountname = NULL;
	}
	if ( accountname_t ) {
		delete [] accountname_t;
		accountname_t = NULL;
	}
	if ( password ) {
		delete [] password;
		password = NULL;
	}
	if ( password_t ) {
		delete [] password_t;
		password_t = NULL;
	}	
	if ( logon_token ) {
		CloseHandle ( logon_token );
		logon_token = NULL;
	}
}


////
//
//  Destructor::  This deletes the user (if desired) and logs it out.
//
////

dynuser::~dynuser() {

	if (!reuse_account) {
	   	deleteuser(accountname);
   	} else {
		// just disable the account
		
		disable_account();
	}

	// free our memory and such
	reset();
}

// unsets the 'DISABLED' flag on current account
void dynuser::enable_account() {
	
	if (! accountname_t ) { return; }

	USER_INFO_1008 ui = {
		UF_PASSWD_CANT_CHANGE | UF_DONT_EXPIRE_PASSWD
	};
	
	if ( NERR_Success !=  NetUserSetInfo( 
		NULL, 			// servername
		accountname_t, 	// username
		1008, 			// level for setting password
		(LPBYTE) &ui,	// buf containing info
		NULL 			// # of offending parameter (ignored)
		)) {
	   	dprintf(D_ALWAYS, "Error enabling account %s\n",
		   	accountname );
	}
}


// sets the 'DISABLED' flag on the current account
void dynuser::disable_account() {

	NET_API_STATUS rval;

	if (! accountname_t ) { return; }

	USER_INFO_1008 ui = {
		UF_ACCOUNTDISABLE | UF_PASSWD_CANT_CHANGE | UF_DONT_EXPIRE_PASSWD
	};
	
	rval = NetUserSetInfo( 
		NULL, 			// servername
		accountname_t, 	// username
		1008, 			// level for setting password
		(LPBYTE) &ui,	// buf containing info
		NULL 			// # of offending parameter (ignored)
		);
	if ( NERR_Success == rval ) {
	   	return;
	} else if ( ERROR_ACCESS_DENIED == rval ) {
		dprintf(D_ALWAYS, "Error disabling account %s (ACCESS DENIED)\n",
		   	accountname );
	} else if (ERROR_INVALID_PARAMETER == rval) {
			dprintf(D_ALWAYS, "Error disabling account %s (INVALID PARAMETER)\n",
		   	accountname );
	} else if (NERR_UserNotFound == rval) {
			dprintf(D_ALWAYS, "Error disabling account %s (User Not Found)\n",
		   	accountname );
	} else {
			dprintf(D_ALWAYS, "Error disabling account %s (Unknown error)\n",
		   	accountname );
	}
}



void dynuser::createpass() {
	
	ASSERT( password != NULL );
		
	const int password_len = 32;

	//                     0 2 4 6
	const char groups[] = "09AZ!/az";

	// generate a random printable ASCII password
	// with specific character groups salted into it to insure
	// that stupid complexity meters see it as "complex"
	int ix = 0;
	while (ix < password_len) {
		int c = 32 + (rand() % 95);
		if (!(ix & 3)) { // every 4th character we will restrict to a char subset
			int ig = (ix/2) & 6;
			if (c < groups[ig] || c > groups[ig+1]) {
				c = groups[ig] + (c % (groups[ig+1] - groups[ig]));
			}
		}
		password[ix++] = (char)c;
	}

	password[ix++] = 0;
}


void dynuser::update_t() {
	if ( accountname && accountname_t ) {
		if (!MultiByteToWideChar( CP_ACP, MB_ERR_INVALID_CHARS, 
					accountname, -1, accountname_t, 100)) {
			dprintf(D_ALWAYS, "DynUser: MultiByteToWideChar() failed "
					"error=%li\n", GetLastError());
			EXCEPT("Unexpected failure in dynuser:update_t");
		}
	}
	if ( password && password_t ) {
		/* In some (Asian, maybe other) locales, MBTWC fails a lot. We're
		 * trying to copy an ascii string to a Unicode string,
		 * and in some languages, not all the characters that 
		 * are printable in ascii have mappings to a Unicode 
		 * equivalent given the standard ANSI codepage. So,
		 * first we try the default ANSI codepage, then we try 
		 * the current thread's default ANSI codepage, as set by
		 * the current locale.
		 */

		if (MultiByteToWideChar( CP_ACP, MB_ERR_INVALID_CHARS, 
			password, -1, password_t, 100)) {
				// success
				return;
		} else if (MultiByteToWideChar( CP_THREAD_ACP, MB_ERR_INVALID_CHARS, 
			password, -1, password_t, 100)) { 
				// success
				return;
		} 

		dprintf(D_ALWAYS, "DynUser: MultiByteToWideChar() failed "
				"error=%li\n", GetLastError());
		EXCEPT("Unexpected failure in dynuser:update_t");
	}
}


HANDLE dynuser::get_token() {
	return this->logon_token;
}

const char*
dynuser::get_accountname() {
	if (logon_token && accountname )
		return accountname;
	else
		return NULL;
}

void InitString( UNICODE_STRING &us, wchar_t *psz ) {
	size_t cch = wcslen( psz );
	us.Length = (USHORT)(cch * sizeof(wchar_t));
	us.MaximumLength = (USHORT)((cch + 1) * sizeof (wchar_t));
	us.Buffer = psz;
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

	// this is a bad idea! We shouldn't be deleting accounts
	// willy nilly like this?!	-stolley 6/2002
	//
	// Remove the account, if it exists...
	//	NetUserDel(0,accountname_t);


	NET_API_STATUS nerr = NetUserAdd (0,		// Machine
						 1,						// struct version
						 (BYTE *)&userInfo,
						 &nParam);

	if ( NERR_Success == nerr ) {
		dprintf(D_FULLDEBUG, "Account %s created successfully\n",accountname);
	} else if ( NERR_UserExists == nerr ) {
		dprintf(D_ALWAYS, "Account %s already exists!\n",accountname);
//		EXCEPT("createaccount: User %s already exists",accountname);
	} else if (NERR_PasswordTooShort == nerr) {
		dprintf(D_ALWAYS, "Account %s creation failed! (err=%d) generated password %s does not meet complexity requirement.\n",accountname,password);
	} else {
		dprintf(D_ALWAYS, "Account %s creation failed! (err=%d)\n",accountname,nerr);
	}

}


////
//
// dynuser::hide_user:  inhibit the user from being displayed in the
//						Windows welcome screen
//
// I believe, unfortunately, that this setting is only observed after
// a reboot.  Not really a big deal, just a comment to answer the
// inevitable RUST ticket asking about slot users not actually being
// hidden :)
//
////


bool dynuser::hide_user() {

	HKEY	subkey		= NULL;
	LPCTSTR	subkey_name = "SOFTWARE\\Microsoft\\Windows NT\\"
		"CurrentVersion\\Winlogon\\SpecialAccounts\\UserList";
	DWORD	hide_user = 0;
	LONG	ok			= ERROR_SUCCESS;
	REGSAM rsAccessMask=0;
	DWORD dwDisposition=0;

	//if (SystemInfoUtils::Is64BitWindows())
		rsAccessMask = KEY_WOW64_64KEY;
	
	__try {

		/* create or open the registry sub-key for removing users
			from the Windows */

		ok = RegCreateKeyEx (
			HKEY_LOCAL_MACHINE,
			subkey_name,
			0, // reserved
			NULL, 
			REG_OPTION_NON_VOLATILE, 
			KEY_READ | KEY_WRITE | rsAccessMask,
			NULL,
			&subkey,
			&dwDisposition );

		if ( ERROR_SUCCESS != ok || NULL == subkey ) {
			dprintf ( D_FULLDEBUG,"dynuser::hide_user() "
				"RegCreateKey(HKEY_LOCAL_MACHINE, %s) failed "
				"(error=%d)\n", subkey_name, GetLastError () );
			__leave;
		}
		
		/* tell Windows to hide this user from the Welcome Screen */
		ok = RegSetValueEx (
			subkey,
			accountname,
			0, // Reserved
			REG_DWORD,
			(LPBYTE) &hide_user,
			sizeof ( DWORD ) );

		if ( ERROR_SUCCESS != ok ) {
			dprintf ( D_FULLDEBUG,"dynuser::hide_user() "
				"RegSetValueEx(%s, hide_user=0)) failed "
				"(error=%d)\n", accountname, GetLastError () );
			__leave;
		}

		/* the subkey will be closed regardless of success of failure,
			so we don't do it here explicitly */
		
	}
	__finally {
		
		if ( NULL != subkey ) {
			RegCloseKey ( subkey );
		}
		
	}

	if ( ERROR_SUCCESS != ok ) {
		dprintf ( D_FULLDEBUG,"dynuser::hide_user() failed "
			"to hide user \"%s\" from the Windows Welcom Screen "
			"(error=%d)\n", accountname, GetLastError () );
		return false;
	}

	return true;

}

////
//
// dynuser::add_users_group:  add the user to the "users" group
//
////

bool dynuser::add_users_group() {
	// Add this user to group "users"
	LOCALGROUP_MEMBERS_INFO_0 lmi;
	wchar_t UserGroupName[255];
	char* tmp;
	MyString friendly_group_name("Users");

	tmp = param("DYNAMIC_RUN_ACCOUNT_LOCAL_GROUP");
	if (tmp) {
		friendly_group_name = tmp;
		swprintf_s(UserGroupName, COUNTOF(UserGroupName), L"%S", tmp);
		free(tmp);
	} else {
		tmp = getUserGroupName();
		swprintf_s(UserGroupName, COUNTOF(UserGroupName), L"%S", tmp);
		delete[] tmp;
	}
	tmp = NULL;

	lmi.lgrmi0_sid = this->psid;

	NET_API_STATUS nerr = NetLocalGroupAddMembers(
	  NULL,				// LPWSTR servername,      
	  UserGroupName,			// LPWSTR LocalGroupName,  
	  0,				// DWORD level,            
	  (LPBYTE) &lmi,	// LPBYTE buf,             
	  1				// DWORD membercount       
	);

	if ( NERR_Success == nerr ) {
		return true;
	}
	else if ( nerr == ERROR_ACCESS_DENIED ) {
		EXCEPT("User %s not added to \"%s\" group, access denied.",
			accountname, friendly_group_name.Value());
	}

	// Any other error...
	EXCEPT("Cannot add %s to \"%s\" group, unknown error (err=%d).",
		accountname, friendly_group_name.Value(), nerr);
	
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
	wchar_t UserGroupName[255];
	char* tmp;

	tmp = param("DYNAMIC_RUN_ACCOUNT_LOCAL_GROUP");
	if (tmp) {
		swprintf_s(UserGroupName, COUNTOF(UserGroupName), L"%S", tmp);
		free(tmp);
	} else {
		tmp = getUserGroupName();
		swprintf_s(UserGroupName, COUNTOF(UserGroupName), L"%S", tmp);
		delete[] tmp;
	}
	tmp = NULL;

	lmi.lgrmi0_sid = this->psid;

	NET_API_STATUS nerr = NetLocalGroupDelMembers(
	  NULL,				// LPWSTR servername,      
	  UserGroupName,	// LPWSTR LocalGroupName,  
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

bool dynuser::update_psid() {
	SID_NAME_USE snu;
	bool result;
	
	result = TRUE;
	
	sidBufferSize = max_sid_length;
	domainBufferSize = max_domain_length;

	if ( !LookupAccountName( 0,				// Domain
		accountname,						// Account name
		psid, &sidBufferSize,				// Sid
		domainBuffer, &domainBufferSize,	// Domain
		&snu ) )							// SID TYPE
	{
		// not necessarily bad, since we could be using this method to
		// check if a user exists, else we create it
		dprintf(D_FULLDEBUG, "dynuser::update_psid() LookupAccountName(%s)"
			" failed!\n", accountname);

		result = FALSE;
	} else {
		result = TRUE;
	}

	return result;
}


bool dynuser::deleteuser(char const * username ) {

	bool retval = true;

	if (!username) {
		return false;
	}

	// as a sanity check, check the prefix of the username.  this
	// check really shouldn't be here in terms of code structure, but
	// Todd is paranoid about deleting some user's account.  -Todd T, 11/01
	const char *prefix = ACCOUNT_PREFIX;
	if ( strncmp(prefix,username,strlen(prefix)) != 0 ) {
		dprintf(D_ALWAYS,
			"Yikes! Asked to delete account %s - ig!\n",
			username);
		return false;  
	}

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
			EXCEPT("dynuser::deleteuser(): Cannot determine workstation name");
		}
		InitString( machine, 
			(wchar_t *)pwkiWorkstationInfo->wki100_computername);
	}

	// open a policy
	LSA_HANDLE hPolicy = 0;
	LSA_OBJECT_ATTRIBUTES oa;//	= { sizeof oa };
	ZeroMemory(&oa, sizeof(oa));
	if (LsaOpenPolicy(NULL,		// Computer Name (NULL == this machine?)
		&oa,						// Object Attributes
		POLICY_LOOKUP_NAMES | POLICY_CREATE_ACCOUNT | POLICY_ALL_ACCESS, // Type of access required
		&hPolicy ) != STATUS_SUCCESS)					// Pointer to the policy handles
	{
		dprintf(D_ALWAYS,"dynuser::deleteuser() LsaOpenPolicy failed\n");
		retval = false;
	}

	SetLastError(0);

	// according to microsoft docs, LsaRemoveAccountRights will remove
	// the account and all rights if the 3rd param is TRUE, so NetUserDel
	// doesn't need to be called.  But "only from a certain perspective", says
	// Richter in "Programming Server-Side Applications for Microsoft Windows 2000".
	// Confusing, eh?  So we call NetUserDel as well after we blow away the rights.
	NTSTATUS result;
	result = LsaRemoveAccountRights( hPolicy, psid, 1, 0, 0 );
	if (!retval || result != STATUS_SUCCESS) {
		// Failed to remove all Account Rights

		dprintf(D_ALWAYS,"dynuser: LsaRemoveAccountRights Failed winerr=%ul\n",
			LsaNtStatusToWinError(result));

		// We do NOT do a NetUserDel here, since we want to either remove
		// all traces of the account or nothing at all.  We don't want to remove
		// just the visible part and leave policy junk bloating the registry 
		// behind the scenes.

	} else {

		// Able to remove all Account Rights, so remove account as well.

        NET_API_STATUS nerr = NetUserDel( NULL, accountname_t );
        if ( nerr != NERR_Success && nerr != NERR_UserNotFound ) {			
			dprintf(D_ALWAYS,"dynuser::deleteuser() failed! nerr=%d\n", nerr);
			retval = false;
		} else {
			dprintf(D_FULLDEBUG,
				"dynuser::deleteuser() Successfully deleted user %s\n", 
				 username);
		}


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

