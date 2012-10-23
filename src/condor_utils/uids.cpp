/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_syscall_mode.h"
#include "condor_uid.h"
#include "condor_config.h"
#include "condor_environ.h"
#include "condor_distribution.h"
#include "my_username.h"
#include "daemon.h"
#include "store_cred.h"

/* See condor_uid.h for description. */
static char* CondorUserName = NULL;
static char* RealUserName = NULL;
static int SwitchIds = TRUE;
static int UserIdsInited = FALSE;
static int OwnerIdsInited = FALSE;
#ifdef WIN32
// set this to false to base the result of can_switch_ids() on token privs rather than account name.
static bool only_SYSTEM_can_switch_ids = true;
#endif
/*
   On Unix, the current uid is process wide.  On Win32, it is specific to each
   thread.  So on Win32 only, priv_state is TLS.
*/
#ifdef WIN32
THREAD_LOCAL_STORAGE static priv_state CurrentPrivState = PRIV_UNKNOWN;
#else
static priv_state CurrentPrivState = PRIV_UNKNOWN;
#endif

priv_state
get_priv_state(void)
{
	return CurrentPrivState;
}

#if !defined(WIN32)
/*
   supplementary group used to track process families. if nonzero,
   this group id will be inserted into the group list when we switch
   into USER_PRIV_FINAL
*/
static gid_t TrackingGid = 0;
#endif

/* must be listed in the same order as enum priv_state in condor_uid.h */
static const char *priv_state_name[] = {
	"PRIV_UNKNOWN",
	"PRIV_ROOT",
	"PRIV_CONDOR",
	"PRIV_CONDOR_FINAL",
	"PRIV_USER",
	"PRIV_USER_FINAL",
	"PRIV_FILE_OWNER"
};


/* Start Common Bits */
#define HISTORY_LENGTH 32

static struct {
	time_t		timestamp;
	priv_state	priv;
	const char	*file;
	int			line;
} priv_history[HISTORY_LENGTH];
static int ph_head=0, ph_count=0;

const char*
priv_to_string( priv_state p )
{
	if( p < _priv_state_threshold ) {
		return priv_state_name[p];
	}
	return "PRIV_INVALID";
}

void
log_priv(priv_state prev, priv_state new_priv, const char *file, int line)
{
	dprintf(D_PRIV, "%s --> %s at %s:%d\n",	priv_state_name[prev],
			priv_state_name[new_priv], file, line);
	priv_history[ph_head].timestamp = time(NULL);
	priv_history[ph_head].priv = new_priv;
	priv_history[ph_head].file = file; /* should be a constant - no alloc */
	priv_history[ph_head].line = line;
	ph_head = (ph_head + 1) % HISTORY_LENGTH;
	if (ph_count < HISTORY_LENGTH) ph_count++;
}


void
display_priv_log(void)
{
	int i, idx;
	if (can_switch_ids()) {
		dprintf(D_ALWAYS, "running as root; privilege switching in effect\n");
	} else {
		dprintf(D_ALWAYS, "running as non-root; no privilege switching\n");
	}		
	for (i=0; i < ph_count && i < HISTORY_LENGTH; i++) {
		idx = (ph_head-i-1+HISTORY_LENGTH)%HISTORY_LENGTH;
		dprintf(D_ALWAYS, "--> %s at %s:%d %s",
				priv_state_name[priv_history[idx].priv],
				priv_history[idx].file, priv_history[idx].line,
				ctime(&priv_history[idx].timestamp));
	}
}


priv_state
get_priv()
{
	return CurrentPrivState;
}

// 
#ifdef WIN32

// SIDs of some well known users and groups
//
#pragma pack(push, 1)
static const struct { 
   SID LocalSystem;
   SID LocalService;
   SID AuthUser;
   SID Admins;    DWORD AdminsRID;
   SID Users;     DWORD UsersRID;
   } sids = {
      {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_LOCAL_SYSTEM_RID}},
      {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_LOCAL_SERVICE_RID}},
      {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_AUTHENTICATED_USER_RID}},
      {SID_REVISION, 2, SECURITY_NT_AUTHORITY, {SECURITY_BUILTIN_DOMAIN_RID}}, DOMAIN_ALIAS_RID_ADMINS,
      {SID_REVISION, 2, SECURITY_NT_AUTHORITY, {SECURITY_BUILTIN_DOMAIN_RID}}, DOMAIN_ALIAS_RID_USERS,
   };
#pragma pack(pop, 1)

// return a copy of the SID of the owner of the current process
//
const PSID my_user_Sid() 
{
    PSID psid = NULL;

	HANDLE hToken = NULL;
    if ( ! OpenProcessToken(GetCurrentProcess(),TOKEN_READ,&hToken)) {
       dprintf(D_ALWAYS, "my_user_Sid: OpenProcessToken failed error = %d", GetLastError());
       return NULL;
    }

	BYTE buf[256];
	DWORD cbBuf = sizeof(buf);
	if ( ! GetTokenInformation(hToken, TokenUser, &buf, cbBuf, &cbBuf)) {
       dprintf(D_ALWAYS, "my_user_Sid: GetTokenInformation failed error = %d", GetLastError());
	} else {
	   TOKEN_USER * ptu = (TOKEN_USER*)buf;
	   DWORD cbSid = GetLengthSid(ptu->User.Sid);
	   psid = malloc(cbSid);
	   if (psid) {
	      CopySid(cbSid, psid, ptu->User.Sid);
	   }
	}
	CloseHandle(hToken);

	return psid;
} 
#endif 

int
can_switch_ids( void )
{
#ifdef WIN32
   static bool HasChecked = false;
   // can't switch users if we're not root/SYSTEM
   if ( ! HasChecked) {

      // begin by assuming we can't switch ID's
      SwitchIds = FALSE;

      // if we are running as LocalSystem, then we really Shouldn't 
      // run jobs without switching users first, and we can be certain
      // that this account has all of the needed privs.
      PSID psid = my_user_Sid();
      if (psid) {
         if (EqualSid(psid, const_cast<SID*>(&sids.LocalSystem)))
            SwitchIds = TRUE;
         free(psid);
      }

      // if SwitchIds is FALSE, then we know we aren't the system account, 
      // So if we allow non-system accounts to switch ids
      // set SwitchIds to true if we have the necessary privileges
      //
      if ( ! SwitchIds && ! only_SYSTEM_can_switch_ids) {

         static const LPCTSTR needed[] = {
            SE_INCREASE_QUOTA_NAME, //needed by CreateProcessAsUser
            //SE_TCB_NAME,            //needed on Win2k to CreateProcessAsUser
            SE_PROF_SINGLE_PROCESS_NAME, //needed?? to get CPU% and Memory/Disk useage for our children
            SE_CREATE_GLOBAL_NAME,  //needed to create named shared memory
            SE_CHANGE_NOTIFY_NAME,  //needed by CreateProcessAsUser
            SE_SECURITY_NAME,       //needed to change file ACL's
            SE_TAKE_OWNERSHIP_NAME, //needed to take ownership of files
            //SE_CREATE_TOKEN_NAME,   //needed?
            //SE_ASSIGNPRIMARYTOKEN_NAME //needed?
            //SE_IMPERSONATE_NAME,    //needed?
            };

         struct {
            PRIVILEGE_SET       set;
            LUID_AND_ATTRIBUTES a[COUNTOF(needed)];
            } privs = { 0, PRIVILEGE_SET_ALL_NECESSARY };

         LUID_AND_ATTRIBUTES * pla = &privs.set.Privilege[0];
         for (int ii = 0; ii < COUNTOF(needed); ++ii) {
            LookupPrivilegeValue(NULL, needed[0], &pla->Luid);
            pla->Attributes = SE_PRIVILEGE_ENABLED;
            ++pla;
         }
         privs.set.PrivilegeCount = (pla - &privs.set.Privilege[0]);

         HANDLE hToken = NULL;
         if ( ! OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hToken)) {
            dprintf(D_ALWAYS, "can_switch_ids: OpenProcessToken failed error = %d", GetLastError());
         } else {
            BOOL fEnabled = false;
            if ( ! PrivilegeCheck(hToken, &privs.set, &fEnabled)) {
               dprintf(D_ALWAYS, "can_switch_ids: PrivilegeCheck failed error = %d", GetLastError());
            } else {
               SwitchIds = fEnabled;
            }
            CloseHandle(hToken);
         }
      }

      HasChecked = true;
   }
#else // *NIX
	static bool HasCheckedIfRoot = false;

	// can't switch users if we're not root/SYSTEM
	if (!HasCheckedIfRoot) {
		if (!is_root()) {
			SwitchIds = FALSE;
		}
		HasCheckedIfRoot = true;
	}
#endif

	return SwitchIds;
}


/* End Common Bits */



#if defined(WIN32)

#include "dynuser.h"
#include "lsa_mgr.h"
#include "token_cache.h"

// Lots of functions just stubs on Win NT for now....
void init_condor_ids() {}
int set_user_ids(uid_t uid, gid_t gid) { return FALSE; }
uid_t get_my_uid() { return 999999; }
gid_t get_my_gid() { return 999999; }
int set_file_owner_ids( uid_t uid, gid_t gid ) { return FALSE; }
void uninit_file_owner_ids() {}


// Cover our getuid...
uid_t getuid() { return get_my_uid(); }

// Static/Global objects
extern dynuser *myDynuser; 	// the "system wide" dynuser object

static HANDLE CurrUserHandle = NULL;
static char *UserLoginName = NULL; // either a "nobody" account or the submitting user
static char *UserDomainName = NULL; // user's domain

static token_cache cached_tokens; // we cache tokens to save time

void uninit_user_ids() 
{
	// just reset the "current" pointers.
	// this doesn't affect the cache, but
	// makes it behave as though there is 
	// no user to set_user_priv() on.
	if ( UserLoginName ) {
	   	free(UserLoginName);
	}
	if ( UserDomainName ) {
		free(UserDomainName);
	}
	UserLoginName = NULL;
	UserDomainName= NULL;
	CurrUserHandle = NULL;
	UserIdsInited = false;
}

HANDLE priv_state_get_handle()
{
	return CurrUserHandle;
}

const char *get_user_loginname() {
    return UserLoginName;
}

const char *get_user_domainname() {
    return UserDomainName;
}

// this is in uids.windows.credd.cpp
extern bool 
get_password_from_credd (
   const char * credd_host,
   const char  username[],
   const char  domain[],
   char * pw,
   int    cb); // sizeof pw buffer (in bytes)

bool 
cache_credd_locally (
	const char  username[],
	const char  domain[],
	const char * pw);

int
init_user_ids(const char username[], const char domain[]) 
{
	int retval = 0;

	if (!username || !domain) {
		dprintf(D_ALWAYS, "WARNING: init_user_ids() called with"
			   " NULL arguments!\n");
	   	return 0;
   	}

	// we have to be very careful calling dprintf in this function, because dprintf
	// will try and _set_priv to root and then back to what it was.  so we need to
	// make sure that our global variables are always in a state where that doesn't
	// except.

	// TJ:2010: Can't do this here, UserIdsInited must never be true 
	// while CurrUserHandle is NULL or dprintf will EXCEPT.
	// UserIdsInited = true;
	
	// see if we already have a user handle for the requested user.
	// if so, just return 1. 
	// TODO: cache multiple user handles to save time.

	dprintf(D_FULLDEBUG, "init_user_ids: want user '%s@%s', "
			"current is '%s@%s'\n",
		username, domain, UserLoginName, UserDomainName);
	
	if ( CurrUserHandle = cached_tokens.getToken(username, domain)) {
		UserIdsInited = true; // do this before we call dprintf
		// when we uninit_user_ids we can end up CurrUserHandle in the cache
		// but UserLoginName and UserDomainName not set, if that happens
		// we need to set them here.
		if ( ! UserLoginName) UserLoginName = strdup(username);
		if ( ! UserDomainName) UserDomainName = strdup(domain);
		dprintf(D_FULLDEBUG, "init_user_ids: Already have handle for %s@%s,"
			" so returning.\n", username, domain);
		return 1;
	} else {
		char* myusr = my_username();
		char* mydom = my_domainname();

		// we cleared CurrUserHandle, so we aren't inited.
		UserIdsInited = false;

		// see if our calling thread matches the user and domain
		// we want a token for. This happens if we're submit for example.
		if ( strcmp( myusr, username ) == 0 &&
			 strcasecmp( mydom, domain ) == 0 ) { // domain is case insensitive

			dprintf(D_FULLDEBUG, "init_user_ids: Calling thread has token "
					"we want, so returning.\n");
			CurrUserHandle = my_usertoken();
			if (CurrUserHandle) {
				UserIdsInited = true;
			} else {
				dprintf(D_FULLDEBUG, "init_user_ids: handle is null!\n");
			}
			
			if (UserLoginName) {
				free(UserLoginName);
				UserLoginName = NULL;
			}
			if (UserDomainName) {
				free(UserDomainName);
				UserDomainName = NULL;
			}
			
			// these are strdup'ed, so we can just stash their pointers
			UserLoginName = myusr;
			UserDomainName = mydom;

			// don't forget to drop it in the cache too
			cached_tokens.storeToken(UserLoginName, UserDomainName,
				   		CurrUserHandle);
			return 1;
		}
	}

	// at this point UserIdsInited should be false.
	ASSERT( ! UserIdsInited);

	// anything beyond this requires user switching
	if (!can_switch_ids()) {
		dprintf(D_ALWAYS, "init_user_ids: failed because user switching is disabled\n");
		return 0;
	}

	if ( strcmp(username,"nobody") != 0 ) {
		// here we call routines to deal with passwords. Hopefully we're
		// running as LocalSystem here, otherwise we can't get at the 
		// password stash.
		lsa_mgr lsaMan;
		char pw[255];
		char user[255];
		char dom[255];
		wchar_t w_fullname[255];
		wchar_t *w_pw;
		bool got_password = false;
		bool got_password_from_credd = false;

		// these should probably be snprintfs
		swprintf(w_fullname, L"%S@%S", username, domain);
		sprintf(user, "%s", username);
		sprintf(dom, "%s", domain);
		
		// make sure we're SYSTEM when we do this
		w_pw = lsaMan.query(w_fullname);
		if ( w_pw ) {
			// copy password into a char buffer
			sprintf(pw, "%S", w_pw);			
			// we don't need the wide char pw anymore, so clean it up
			SecureZeroMemory(w_pw, wcslen(w_pw)*sizeof(wchar_t));
			delete[](w_pw);
			w_pw = NULL;

			// now that we got a password, see if it is good.
			retval = LogonUser(
				user,						// user name
				dom,						// domain or server - local for now
				pw,							// password
				LOGON32_LOGON_INTERACTIVE,	// type of logon operation. 
											// LOGON_BATCH doesn't seem to work right here.
				LOGON32_PROVIDER_DEFAULT,	// logon provider
				&CurrUserHandle				// receive tokens handle
			);
			
			if ( !retval ) {
				dprintf(D_FULLDEBUG,"Locally stored credential for %s@%s is stale\n",
					user,dom);
				// Set handle to NULL to make certain we recall LogonUser again below
				CurrUserHandle = NULL;	
			} else {
				got_password = true;	// so we don't bother going to a credd
			}
		}

		// if we don't have the password from our local stash, try to fetch
		// it from a credd. this tiny function is in a separate file so
        // that we don't pull in all of daemon core when we link to the utils library.

		char *credd_host = param("CREDD_HOST");
		if (credd_host && got_password==false) {
#if 1
           got_password = get_password_from_credd(
              credd_host,
              username,
              domain,
              pw,
              sizeof(pw));
           got_password_from_credd = got_password;
#else
			dprintf(D_FULLDEBUG, "trying to fetch password from credd: %s\n", credd_host);
			Daemon credd(DT_CREDD);
			Sock * credd_sock = credd.startCommand(CREDD_GET_PASSWD,Stream::reli_sock,10);
			if ( credd_sock ) {
				credd_sock->put((char*)username);	// send user
				credd_sock->put((char*)domain);		// send domain
				credd_sock->end_of_message();
				credd_sock->decode();
				pw[0] = '\0';
				int my_stupid_sizeof_int_for_damn_cedar = sizeof(pw);
				char *my_buffer = pw;
				if ( credd_sock->code(my_buffer,my_stupid_sizeof_int_for_damn_cedar) && pw[0] ) {
					got_password = true;
					got_password_from_credd = true;
				} else {
					dprintf(D_FULLDEBUG,
							"credd (%s) did not have info for %s@%s\n",
							credd_host, username,domain);
				}
				delete credd_sock;
				credd_sock = NULL;
			} else {
				dprintf(D_FULLDEBUG,"Failed to contact credd %s: %s\n",
					credd_host,credd.error() ? credd.error() : "");
			}
#endif
		}
		if (credd_host) free(credd_host);

		if ( ! got_password ) {
			dprintf(D_ALWAYS, "ERROR: Could not locate valid credential for user "
				"'%s@%s'\n", username, domain);
			return 0;
		} else {
			dprintf(D_FULLDEBUG, "Found credential for user '%s'\n", username);

			// If we have not yet called LogonUser, then CurrUserHandle is NULL,
			// and we need to call it here.
			if ( CurrUserHandle == NULL ) {
				retval = LogonUser(
					user,						// user name
					dom,						// domain or server - local for now
					pw,							// password
					LOGON32_LOGON_INTERACTIVE,	// type of logon operation. 
												// LOGON_BATCH doesn't seem to work right here.
					LOGON32_PROVIDER_DEFAULT,	// logon provider
					&CurrUserHandle				// receive tokens handle
				);
			} else {
				// we already have a good user handle from calling LogonUser to check to
				// see if our stashed credential was stale or not, so set retval to success
				retval = 1;	// LogonUser returns nonzero value on success
			}

			dprintf(D_FULLDEBUG, "LogonUser completed.\n");

			if (UserLoginName) {
				free(UserLoginName);
				UserDomainName = NULL;
			}
			if (UserDomainName) {
				free(UserDomainName);
				UserDomainName = NULL;
			}
			UserLoginName = strdup(username);
			UserDomainName = strdup(domain);

			if ( !retval ) {
				dprintf(D_ALWAYS, "init_user_ids: LogonUser failed with NT Status %ld\n", 
					GetLastError());
				retval =  0;	// return of 0 means FAILURE
			} else {
				// stash the new token in our cache
				cached_tokens.storeToken(UserLoginName, UserDomainName,
					   	CurrUserHandle);
				UserIdsInited = true;

				// if we got the password from the credd, and the admin wants passwords stashed
				// locally on this machine, then do it.
				if ( got_password_from_credd &&
                     param_boolean("CREDD_CACHE_LOCALLY",false) ) {
                    cache_credd_locally(username, domain, pw);
                }
				
				retval = 1;	// return of 1 means SUCCESS
			}

			// clear pw from memory
			SecureZeroMemory(pw, 255);

			return retval;
		}
		
	} else {
		///
		// Here's where we use a nobody account
		//
		
		dprintf(D_FULLDEBUG, "Using dynamic user account.\n");

		myDynuser->reset();
		// at this point, we know we want a user nobody, so
		// generate a dynamic user and stash the handle

				
		if ( !myDynuser->init_user() ) {
			// This is indicative of a serious problem.
			EXCEPT("Failed to create a user nobody");
		}
		
		if (UserLoginName) {
			free(UserLoginName);
			UserDomainName = NULL;
		}
		if (UserDomainName) {
			free(UserDomainName);
			UserDomainName = NULL;
		}

		UserLoginName = strdup( myDynuser->get_accountname() );
		UserDomainName = strdup( "." );

		// we created a new user, now just stash the token
		CurrUserHandle = myDynuser->get_token();

		// drop the handle in our cache too
		cached_tokens.storeToken(UserLoginName, UserDomainName,
			   		CurrUserHandle);
		UserIdsInited = true;
		return 1;
	}
}

priv_state
_set_priv(priv_state s, const char *file, int line, int dologging)
{
	priv_state PrevPrivState = CurrentPrivState;

	/* NOTE  NOTE   NOTE  NOTE
	 * This function is called from deep inside dprintf.  To
	 * avoid potentially nasty recursive situations, ONLY call
	 * dprintf() from inside of this function if the 
	 * dologging parameter is non-zero.  
	 */

	// On NT, PRIV_CONDOR = PRIV_ROOT, but we let it switch so daemoncore's
	// priv state checking won't get totally confused.
	if ( ( s == PRIV_CONDOR  || s == PRIV_ROOT ) &&
		 ( CurrentPrivState == PRIV_CONDOR || CurrentPrivState == PRIV_ROOT ) )
	{
		goto logandreturn;
	}

	// If this is PRIV_USER or PRIV_USER_FINAL, this isn't redundant
	if (s == CurrentPrivState) {
		goto logandreturn;
	}

	if (CurrentPrivState == PRIV_USER_FINAL) {
		if ( dologging ) {
			dprintf(D_ALWAYS,
				"warning: attempted switch out of PRIV_USER_FINAL\n");
		}
		return PRIV_USER_FINAL;
	}

	if (CurrentPrivState == PRIV_CONDOR_FINAL) {
		if ( dologging ) {
			dprintf(D_ALWAYS,
				"warning: attempted switch out of PRIV_CONDOR_FINAL\n");
		}
		return PRIV_CONDOR_FINAL;
	}
	CurrentPrivState = s;
	if (can_switch_ids()) {
		switch (s) {
		case PRIV_ROOT:
		case PRIV_CONDOR:
		case PRIV_CONDOR_FINAL:
			RevertToSelf();
			break;
		case PRIV_USER:
		case PRIV_USER_FINAL:
			if ( dologging ) {
				dprintf(D_FULLDEBUG, 
						"TokenCache contents: \n%s", 
						cached_tokens.cacheToString().Value());
			}
			if ( CurrUserHandle ) {
				if ( PrevPrivState == PRIV_UNKNOWN ) {
					// make certain we're back to 'condor' before impersonating
					RevertToSelf();
				}
				if ( ! ImpersonateLoggedOnUser(CurrUserHandle)) {
					dprintf(D_ALWAYS, "ImpersonateLoggedOnUser() failed, err=%d\n", GetLastError());
				}
			} else {
				// we don't want to exit here because reusing the shadow in 7.5.4
				// ends up here because of a dprintf inside uninit_user_ids
				if ( ! UserIdsInited) {
					if (dologging) {
						dprintf(D_ALWAYS, "set_user_priv() called when UserIds not inited!\n");
					}
				}  else { 
					// Tried to set_user_priv() but it failed, so bail out!
					EXCEPT("set_user_priv() failed!");
				}
			}
			break;
		case PRIV_UNKNOWN:		/* silently ignore */
			break;
		default:
			if ( dologging ) {
				dprintf( D_ALWAYS, "set_priv: Unknown priv state %d\n", (int)s);
			}
		}
	}

logandreturn:
	if (dologging) {
		log_priv(PrevPrivState, CurrentPrivState, file, line);
	}
	return PrevPrivState;
}	


// This implementation of get_condor_username() for WinNT really 
// returns the username of the current user.  Until we finish porting
// over all the priv_state code, root=condor=user, so we may as well
// just return the identity of the current user.
const char* get_condor_username() 
{
	HANDLE hProcess = NULL;
	HANDLE hAccessToken = NULL;
	UCHAR InfoBuffer[1000];
	char szAccountName[200], szDomainName[200];
	PTOKEN_USER pTokenUser = (PTOKEN_USER)InfoBuffer;
	DWORD dwInfoBufferSize,dwAccountSize = 200, dwDomainSize = 200;
	SID_NAME_USE snu;
	int length;

	if ( CondorUserName )
		return CondorUserName;

	hProcess = GetCurrentProcess();

	OpenProcessToken(hProcess,TOKEN_READ,&hAccessToken);

	GetTokenInformation(hAccessToken,TokenUser,InfoBuffer,
		1000, &dwInfoBufferSize);

	szAccountName[0] = '\0';
	szDomainName[0] = '\0';
	LookupAccountSid(NULL, pTokenUser->User.Sid, szAccountName,
		&dwAccountSize,szDomainName, &dwDomainSize, &snu);

	length = strlen(szAccountName) + strlen(szDomainName) + 4;
	CondorUserName = (char *) malloc(length);
	if (CondorUserName == NULL) {
		EXCEPT("Out of memory. Aborting.");
	}
	sprintf(CondorUserName, "%s/%s",szDomainName,szAccountName);

	if ( hProcess )
		CloseHandle(hProcess);
	if ( hAccessToken )
		CloseHandle(hAccessToken);

	return CondorUserName;
} 


#include "my_username.h"
int
is_root( void ) 
{
	int root = 0;
#if 1
    PSID psid = my_user_Sid();
	if( ! psid ) {
		dprintf( D_ALWAYS, 
				 "ERROR in is_root(): my_user_Sid() returned NULL\n" );
		return 0;
	}
    root = EqualSid(psid, const_cast<SID*>(&sids.LocalSystem));
    free (psid);
#else
	char* user = my_username( 0 );
	if( ! user ) {
		dprintf( D_ALWAYS, 
				 "ERROR in is_root(): my_username() returned NULL\n" );
		return 0;
	}
	if( !strcasecmp(user, "SYSTEM") ) {
		root = 1;
	}
	free( user );
#endif
	return root;
}


const char*
get_real_username( void )
{
	if( ! RealUserName ) {
		RealUserName = strdup( "system" );
	}
	return RealUserName;
}

void
clear_passwd_cache() {
	// no-op on Windows
}

void
delete_passwd_cache() {
	// no-op on Windows
}

#else  // end of ifdef WIN32, now below starts Unix-specific code

#include <grp.h>

#if defined(AIX31) || defined(AIX32)
#include <sys/types.h>
#include <sys/id.h>
#define SET_EFFECTIVE_UID(id) setuidx(ID_REAL|ID_EFFECTIVE,id)
#define SET_REAL_UID(id) setuidx(ID_SAVED|ID_REAL|ID_EFFECTIVE,id)
#else
#define SET_EFFECTIVE_UID(id) seteuid(id)
#define SET_REAL_UID(id) setuid(id)
#define SET_EFFECTIVE_GID(id) setegid(id)
#define SET_REAL_GID(id) setgid(id)
#define GET_EFFECTIVE_UID() geteuid()
#define GET_REAL_UID() getuid()
#define GET_EFFECTIVE_GID() getegid()
#define GET_REAL_GID() getgid()
#endif

#include "condor_debug.h"
#include "passwd_cache.unix.h"


#ifndef FALSE
#define FALSE 0
#endif /* FALSE */

#ifndef TRUE
#define TRUE 1
#endif /* TRUE */

#define ROOT 0

static uid_t CondorUid, UserUid, RealCondorUid, OwnerUid;
static gid_t CondorGid, UserGid, RealCondorGid, OwnerGid;
static int CondorIdsInited = FALSE;
static char* UserName = NULL;
static char* OwnerName = NULL;

static int set_condor_euid();
static int set_condor_egid();
static int set_user_euid();
static int set_user_egid();
static int set_owner_euid();
static int set_owner_egid();
static int set_user_ruid();
static int set_user_rgid();
static int set_root_euid();
static int set_root_egid();
static int set_condor_ruid();
static int set_condor_rgid();

/* We don't use EXCEPT here because this file is used in
   condor_syscall_lib.a.  -Jim B. */

/* We use the pcache() function to give us our "global" passwd_cache
   instead of declaring a global, static passwd_cache. We want
   to avoid declaring the passwd_cache globally, since it's contructor
   calls param(), which we shouldn't do until the config file has 
   been parsed. */
static passwd_cache *pcache_ptr = NULL;

passwd_cache* 
pcache(void) {
	if ( pcache_ptr == NULL ) {
		pcache_ptr = new passwd_cache();
	}
	return pcache_ptr;
}

void
clear_passwd_cache() {
	pcache()->reset();
}

void
delete_passwd_cache() {
	delete pcache_ptr;
	pcache_ptr = NULL;
}

void
init_condor_ids()
{
	int scm;
	bool result;
	char* env_val = NULL;
	char* config_val = NULL;
	char* val = NULL;
	uid_t envCondorUid = INT_MAX;
	gid_t envCondorGid = INT_MAX;

        /*
        ** N.B. if we are using the yellow pages, system calls which are
        ** not supported by either remote system calls or file descriptor
        ** mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
        */
	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );

	uid_t MyUid = get_my_uid();
	gid_t MyGid = get_my_gid();
	
		/* if either of the following get_user_*() functions fail,
		 * the default is INT_MAX */
	RealCondorUid = INT_MAX;
	RealCondorGid = INT_MAX;
	pcache()->get_user_uid( myDistro->Get(), RealCondorUid );
	pcache()->get_user_gid( myDistro->Get(), RealCondorGid );

	const char	*envName = EnvGetName( ENV_UG_IDS ); 
	if( (env_val = getenv(envName)) ) {
		val = env_val;
	} else if( (config_val = param_without_default(envName)) ) {
		// I had to change this to param_without_default because there's no way
		// to put a default value of condor.condor in the default value table.
		// In the future, there should be a way to call a function to find out
		// the default value for a parameter, but for now this should work.
		val = config_val;
	}
	if( val ) {  
		if( sscanf(val, "%d.%d", &envCondorUid, &envCondorGid) != 2 ) {
			fprintf( stderr, "ERROR: badly formed value in %s ", envName );
			fprintf( stderr, "%s variable (%s).\n",
					 env_val ? "environment" : "config file", val );
			fprintf( stderr, "Please set %s to ", envName );
			fprintf( stderr, "the '.' seperated uid, gid pair that\n" );
			fprintf( stderr, "should be used by %s.\n", myDistro->Get() );
			exit(1);
		}
		if( CondorUserName != NULL ) {
			free( CondorUserName );
			CondorUserName = NULL;
		}
		result = pcache()->get_user_name( envCondorUid, CondorUserName );

		if( ! result ) {

				/* failure to get username */

			fprintf( stderr, "ERROR: the uid specified in %s ", envName );
			fprintf( stderr, "%s variable (%d)\n", 
					 env_val ? "environment" : "config file", envCondorUid );
			fprintf(stderr, "does not exist in your password information.\n" );
			fprintf(stderr, "Please set %s to ", envName);
			fprintf(stderr, "the '.' seperated uid, gid pair that\n");
			fprintf(stderr, "should be used by %s.\n", myDistro->Get() );
			exit(1);
		}
	}
	if( config_val ) {
		free( config_val );
		config_val = NULL;
		val = NULL;
	}

	/* If we're root, set the Condor Uid and Gid to the value
	   specified in the "CONDOR_IDS" environment variable */
	if( can_switch_ids() ) {
		const char	*enviName = EnvGetName( ENV_UG_IDS ); 
		if( envCondorUid != INT_MAX ) {	
			/* CONDOR_IDS are set, use what it said */
				CondorUid = envCondorUid;
				CondorGid = envCondorGid;
		} else {
			/* No CONDOR_IDS set, use condor.condor */
			if( RealCondorUid != INT_MAX ) {
				CondorUid = RealCondorUid;
				CondorGid = RealCondorGid;
				if( CondorUserName != NULL ) {
					free( CondorUserName );
					CondorUserName = NULL;
				}
				CondorUserName = strdup( myDistro->Get() );
				if (CondorUserName == NULL) {
					EXCEPT("Out of memory. Aborting.");
				}
			} else {
				fprintf( stderr,
						 "Can't find \"%s\" in the password file and "
						 "%s not defined in %s_config or as an "
						 "environment variable.\n", myDistro->Get(),
						 enviName, myDistro->Get() );
				exit(1);
			}
		}
			/* We'd like to dprintf() here, but we can't.  Since this 
			   function is called from the initial time we try to
			   enter Condor priv, if we dprintf() here, we'll still be
			   in root priv when we service this dprintf(), and we'll
			   have major problems.  -Derek Wright 12/21/98 */
			/* dprintf(D_PRIV, "running as root; privilege switching in effect\n"); */
	} else {
		/* Non-root.  Set the CondorUid/Gid to our current uid/gid */
		CondorUid = MyUid;
		CondorGid = MyGid;
		if( CondorUserName != NULL ) {
			free( CondorUserName );
			CondorUserName = NULL;
		}
		result = pcache()->get_user_name( CondorUid, CondorUserName );
		if( !result ) {
			/* Cannot find an entry in the passwd file for this uid */
			CondorUserName = strdup("Unknown");
			if (CondorUserName == NULL) {
				EXCEPT("Out of memory. Aborting.");
			}
		}

		/* If CONDOR_IDS environment variable is set, and set to the same uid
		   that we are running as, then behave as if the daemons are running
		   as user "condor" -- i.e. allow any user to submit jobs to these daemons,
		   not just the user running the daemons.
		*/
		if ( MyUid == envCondorUid ) {
			RealCondorUid = MyUid;
			RealCondorGid = MyGid;
		}
	}
	
	(void)endpwent();
	(void)SetSyscalls( scm );
	
	CondorIdsInited = TRUE;
}


static int
set_user_ids_implementation( uid_t uid, gid_t gid, const char *username, 
							 int is_quiet ) 
{
	if( uid == 0 || gid == 0 ) {
			// NOTE: we want this dprintf() even if we're in quiet
			// mode, since we should *never* be allowing this.
		dprintf( D_ALWAYS, "ERROR: Attempt to initialize user_priv " 
				 "with root privileges rejected\n" );
		return FALSE;
	}
		// So if we are not root, trying to use any user id is bogus
		// since the OS will disallow it.  So if we are not running as
		// root, may as well just set the user id to be the real id.
	// For setuid-root
	// -jaeyoung 05/22/07
	//if ( get_my_uid() != ROOT ) {
	if ( !can_switch_ids() ) {
		uid = get_my_uid();
		gid = get_my_gid();
	}

	if( UserIdsInited && UserUid != uid && !is_quiet ) {
		dprintf( D_ALWAYS, 
				 "warning: setting UserUid to %d, was %d previously\n",
				 uid, UserUid );
	}
	UserUid = uid;
	UserGid = gid;
	UserIdsInited = TRUE;

	// find the user login name for this uid.  note we should not
	// EXCEPT or log an error if we do not find it; it is OK for the
	// user not to be in the passwd file for a so-called SOFT_UID_DOMAIN.
	if( UserName ) {
		free( UserName );
	}

	if ( !username ) {

		if ( !(pcache()->get_user_name( UserUid, UserName )) ) {
			UserName = NULL;
		}
	} else {
		UserName = strdup( username );
	}
	return TRUE;
}


/*
  Initialize the correct uid/gid for user "nobody".  Most of the
  special-case logic for this code came from
  condor_starter.V5/starter_common.C: determine_user_ids()
*/
int
init_nobody_ids( int is_quiet )
{
	bool result;

	/* WARNING: We're initializing the nobody uid/gid's to 0!
	   That's a big no-no, so make sure that if we somehow don't
	   manage to find a valid nobody uid/gid, that we immediately
	   return FALSE and fail out. 

	   Unfortunately, there is no value you can set a uid_t/gid_t
	   to that indicates an uninitialized or invalid value. In the
	   case of this function however, we know that no matter what,
	   the nobody user should NEVER be 0, so it serves well for
	   this purpose.
	 */

	uid_t nobody_uid = 0;
	gid_t nobody_gid = 0;

	result = ( 	(pcache()->get_user_uid("nobody", nobody_uid)) &&
	   			(pcache()->get_user_gid("nobody", nobody_gid)) );

	if (! result ) {


#ifdef HPUX
		// the HPUX9 release does not have a default entry for nobody,
		// so we'll help condor admins out a bit here...
		nobody_uid = 59999;
		nobody_gid = 59999;
#else
		if( ! is_quiet ) {
			dprintf( D_ALWAYS, 
					 "Can't find UID for \"nobody\" in passwd file\n" );
		}
		return FALSE;
#endif
	} 

#ifdef HPUX
	// HPUX9 has a bug in that getpwnam("nobody") always returns
	// a gid of 60001, no matter what the group file (or NIS) says!
	// on top of that, legal UID/GIDs must be -1<x<60000, so unless we
	// patch here, we will generate an EXCEPT later when we try a
	// setgid().  -Todd Tannenbaum, 3/95
	if( (nobody_uid > 59999) || (nobody_uid <= 0) ) {
		nobody_uid = 59999;
	}
	if( (nobody_gid > 59999) || (nobody_gid <= 0) ) {
		nobody_gid = 59999;
	}
#endif

	/* WARNING: At the top of this function, we initialized 
	   nobody_uid and nobody_gid to 0, so if for some terrible 
	   reason we haven't set them to a valid nobody uid/gid
	   by this point, we need to fail immediately. */

	if ( nobody_uid == 0 || nobody_gid == 0 ) {
		return FALSE;
	}

		// Now we know what the uid/gid for nobody should *really* be,
		// so we can actually initialize this as the "user" priv.
	return set_user_ids_implementation( (uid_t)nobody_uid,
										(gid_t)nobody_gid, "nobody",
										is_quiet );
}


int
init_user_ids_implementation( const char username[], int is_quiet )
{
	int					scm;
	uid_t 				usr_uid;
	gid_t				usr_gid;

		// So if we are not root, trying to use any user id is bogus
		// since the OS will disallow it.  So if we are not running as
		// root, may as well just set the user id to be the real id.
	
	// For setuid-root
	// -jaeyoung 05/22/07
	//if ( get_my_uid() != ROOT ) {
	if ( !can_switch_ids() ) {
		return set_user_ids_implementation( get_my_uid(), get_my_gid(),
										NULL, is_quiet ); 
	}

	/*
	** N.B. if we are using the yellow pages, system calls which are
	** not supported by either remote system calls or file descriptor
	** mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
	*/
	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );

	if( ! strcasecmp(username, "nobody") ) {
			// There's so much special logic for user nobody that it's
			// all in a seperate function now.
		return init_nobody_ids( is_quiet );
	}

	if( !(pcache()->get_user_uid(username, usr_uid)) ||
	 	!(pcache()->get_user_gid(username, usr_gid)) ) {
		if( ! is_quiet ) {
			dprintf( D_ALWAYS, "%s not in passwd file\n", username );
		}
		(void)endpwent();
		(void)SetSyscalls( scm );
		return FALSE;
	}
	(void)endpwent();
	(void)SetSyscalls( scm );
	return set_user_ids_implementation( usr_uid, usr_gid, username, is_quiet ); 
}


int
init_user_ids( const char username[], const char /*domain*/[] ) {
	// we ignore the domain parameter on UNIX
	return init_user_ids_implementation( username, 0 );
}


int
init_user_ids_quiet( const char username[] ) {
	return init_user_ids_implementation( username, 1 );
}


int
set_user_ids(uid_t uid, gid_t gid)
{
	return set_user_ids_implementation( uid, gid, NULL, 0 );
}


int
set_user_ids_quiet(uid_t uid, gid_t gid)
{
	return set_user_ids_implementation( uid, gid, NULL, 1 );
}

void
set_user_tracking_gid(gid_t tracking_gid)
{
	TrackingGid = tracking_gid;
}

void
unset_user_tracking_gid()
{
	TrackingGid = 0;
}

void
uninit_user_ids()
{
	UserIdsInited = FALSE;
}


void
uninit_file_owner_ids() 
{
	OwnerIdsInited = FALSE;
}


int
set_file_owner_ids( uid_t uid, gid_t gid )
{
	if( OwnerIdsInited && OwnerUid != uid  ) {
		dprintf( D_ALWAYS, 
				 "warning: setting OwnerUid to %d, was %d previosly\n",
				 (int)uid, (int)OwnerUid );
	}
	OwnerUid = uid;
	OwnerGid = gid;
	OwnerIdsInited = TRUE;

	// find the user login name for this uid.  note we should not
	// EXCEPT or log an error if we do not find it; it is OK for the
	// user not to be in the passwd file...
	if( OwnerName ) {
		free( OwnerName );
	}
	if ( !(pcache()->get_user_name( OwnerUid, OwnerName )) ) {
		OwnerName = NULL;
	}
	return TRUE;
}


priv_state
_set_priv(priv_state s, const char *file, int line, int dologging)
{
	priv_state PrevPrivState = CurrentPrivState;
	if (s == CurrentPrivState) return s;
	if (CurrentPrivState == PRIV_USER_FINAL) {
		dprintf(D_ALWAYS,
				"warning: attempted switch out of PRIV_USER_FINAL\n");
		return PRIV_USER_FINAL;
	}
	if (CurrentPrivState == PRIV_CONDOR_FINAL) {
		dprintf(D_ALWAYS,
				"warning: attempted switch out of PRIV_CONDOR_FINAL\n");
		return PRIV_CONDOR_FINAL;
	}
	CurrentPrivState = s;

	if (can_switch_ids()) {
		switch (s) {
		case PRIV_ROOT:
			set_root_euid();
			set_root_egid();
			break;
		case PRIV_CONDOR:
			set_root_euid();	/* must be root to switch */
			set_condor_egid();
			set_condor_euid();
			break;
		case PRIV_USER:
			set_root_euid();	/* must be root to switch */
			set_user_egid();
			set_user_euid();
			break;
		case PRIV_FILE_OWNER:
			set_root_euid();	/* must be root to switch */
			set_owner_egid();
			set_owner_euid();
			break;
		case PRIV_USER_FINAL:
			set_root_euid();	/* must be root to switch */
			set_user_rgid();
			set_user_ruid();
			break;
		case PRIV_CONDOR_FINAL:
			set_root_euid();	/* must be root to switch */
			set_condor_rgid();
			set_condor_ruid();
		case PRIV_UNKNOWN:		/* silently ignore */
			break;
		default:
			dprintf( D_ALWAYS, "set_priv: Unknown priv state %d\n", (int)s);
		}
	}
	if( dologging == NO_PRIV_MEMORY_CHANGES ) {
			// This is a special case for the call to set_priv from
			// within a child process, just before exec(), where the
			// child may be sharing memory with the parent, and
			// therefore doesn't want to have set_priv() mess up the
			// parent's memory of what priv state the parent is in.
			// It is ok that we temporarily changed the
			// CurrentPrivState variable above in a non-thread-safe
			// way, because the parent is suspended, but before
			// returning, we must set it back to what it was. For the
			// rest of the life of the child, CurrentPrivState is a
			// lie.
		CurrentPrivState = PrevPrivState;
	}
	else if( dologging ) {
		log_priv(PrevPrivState, CurrentPrivState, file, line);
	}

	return PrevPrivState;
}	


uid_t
get_condor_uid()
{
	if( !CondorIdsInited ) {
		init_condor_ids();
	}

	return CondorUid;
}


gid_t
get_condor_gid()
{
	if( !CondorIdsInited ) {
		init_condor_ids();
	}

	return CondorGid;
}

bool
get_condor_uid_if_inited(uid_t &uid,gid_t &gid)
{
	if( !CondorIdsInited ) {
		uid = 0;
		gid = 0;
		return false;
	}

	uid = CondorUid;
	gid = CondorGid;
	return true;
}

/* This returns the string containing the username of whatever uid
   priv_state condor gives you. */
const char*
get_condor_username()
{
	if( !CondorIdsInited ) {
		init_condor_ids();
	}

	return CondorUserName;
}	 


uid_t
get_user_uid()
{
	if( !UserIdsInited ) {
		dprintf(D_ALWAYS, "get_user_uid() called when UserIds not inited!\n");
		return (uid_t)-1;
	}

	return UserUid;
}


gid_t
get_user_gid()
{
	if( !UserIdsInited ) {
		dprintf(D_ALWAYS, "get_user_gid() called when UserIds not inited!\n");
		return (gid_t)-1;
	}

	return UserGid;
}


uid_t
get_file_owner_uid()
{
	if( !OwnerIdsInited ) {
		dprintf( D_ALWAYS,
				 "get_file_owner_uid() called when OwnerIds not inited!\n" );
		return (uid_t)-1;
	}
	return OwnerUid;
}


gid_t
get_file_owner_gid()
{
	if( !OwnerIdsInited ) {
		dprintf( D_ALWAYS,
				 "get_file_owner_gid() called when OwnerIds not inited!\n" );
		return (gid_t)-1;
	}
	return OwnerGid;
}


uid_t
get_my_uid()
{
	return getuid();
}


gid_t
get_my_gid()
{
	return getgid();
}


uid_t
get_real_condor_uid()
{
	if( !CondorIdsInited ) {
		init_condor_ids();
	}

	return RealCondorUid;
}


gid_t
get_real_condor_gid()
{
	if( !CondorIdsInited ) {
		init_condor_ids();
	}

	return RealCondorGid;
}


int
set_condor_euid()
{
	if( !CondorIdsInited ) {
		init_condor_ids();
	}

	return SET_EFFECTIVE_UID(CondorUid);
}


int
set_condor_egid()
{
	if( !CondorIdsInited ) {
		init_condor_ids();
	}

	return SET_EFFECTIVE_GID(CondorGid);
}


int
set_user_euid()
{
	if( !UserIdsInited ) {
		dprintf(D_ALWAYS, "set_user_euid() called when UserIds not inited!\n");
		return -1;
	}

	return SET_EFFECTIVE_UID(UserUid);
}


int
set_user_egid()
{
	if( !UserIdsInited ) {
		dprintf(D_ALWAYS, "set_user_egid() called when UserIds not inited!\n");
		return -1;
	}
	
		// Now, call our caching version of initgroups with the 
		// right username so the user can access files belonging
		// to any group (s)he is a member of.  If we did not call
		// initgroups here, the user could only access files
		// belonging to his/her default group, and might be left
		// with access to the groups that root belongs to, which 
		// is a serious security problem.
		
	if( UserName ) {
		errno = 0;
		if(!(pcache()->init_groups(UserName)) ) {
			dprintf( D_ALWAYS, 
					 "set_user_egid - ERROR: initgroups(%s, %d) failed, "
					 "errno: %s\n", UserName, UserGid, strerror(errno) );
		}			
	}
	return SET_EFFECTIVE_GID(UserGid);
}


int
set_user_ruid()
{
	if( !UserIdsInited ) {
		dprintf(D_ALWAYS, "set_user_ruid() called when UserIds not inited!\n");
		return -1;
	}

	return SET_REAL_UID(UserUid);
}


int
set_user_rgid()
{
	if( !UserIdsInited ) {
		dprintf(D_ALWAYS, "set_user_rgid() called when UserIds not inited!\n");
		return -1;
	}

		// Now, call our caching version of initgroups with the 
		// right username so the user can access files belonging
		// to any group (s)he is a member of.  If we did not call
		// initgroups here, the user could only access files
		// belonging to his/her default group, and might be left
		// with access to the groups that root belongs to, which 
		// is a serious security problem.
		
	if( UserName ) {
		errno = 0;
		if( !(pcache()->init_groups(UserName, TrackingGid)) ) {
			dprintf( D_ALWAYS, 
					 "set_user_rgid - ERROR: initgroups(%s, %d) failed, "
					 "errno: %d\n", UserName, UserGid, errno );
		}			
	}
	return SET_REAL_GID(UserGid);
}


int
set_root_euid()
{
	return SET_EFFECTIVE_UID(ROOT);
}

int
set_root_egid()
{
	return SET_EFFECTIVE_GID(ROOT);
}


int
set_owner_euid()
{
	if( !OwnerIdsInited ) {
		dprintf( D_ALWAYS,
				 "set_owner_euid() called when OwnerIds not inited!\n" );
		return -1;
	}
	return SET_EFFECTIVE_UID(OwnerUid);
}


int
set_owner_egid()
{
	if( !OwnerIdsInited ) {
		dprintf( D_ALWAYS,
				 "set_owner_egid() called when OwnerIds not inited!\n" );
		return -1;
	}
	
		// Now, call our caching version of initgroups with the 
		// right username so the user can access files belonging
		// to any group (s)he is a member of.  If we did not call
		// initgroups here, the user could only access files
		// belonging to his/her default group, and might be left
		// with access to the groups that root belongs to, which 
		// is a serious security problem.
	if( OwnerName ) {
		errno = 0;
		if(!(pcache()->init_groups(OwnerName)) ) {
			dprintf( D_ALWAYS, 
					 "set_owner_egid - ERROR: initgroups(%s, %d) failed, "
					 "errno: %s\n", OwnerName, OwnerGid, strerror(errno) );
		}			
	}
	return SET_EFFECTIVE_GID(UserGid);
}


int
set_condor_ruid()
{
	if( !CondorIdsInited ) {
		init_condor_ids();
	}

	return SET_REAL_UID(CondorUid);
}


int
set_condor_rgid()
{
	if( !CondorIdsInited ) {
		init_condor_ids();
	}

	if( CondorUserName ) {
		errno = 0;
		if(!(pcache()->init_groups(CondorUserName)) ) {
			dprintf( D_ALWAYS, 
					 "set_condor_rgid - ERROR: initgroups(%s) failed, "
					 "errno: %s\n", CondorUserName, strerror(errno) );
		}                       
	}

	return SET_REAL_GID(CondorGid);
}

int
is_root( void ) 
{
	// If a program has setuid-root in Linux, 
	// the program will have non-zero uid and zero euid.
	// We need to make sure that other OSs have the same behavior.
	// -jaeyoung 05/21/07
	//return (! getuid() );
	return ( !getuid() || !geteuid() );
}


const char*
get_real_username( void )
{
	if( ! RealUserName ) {
		uid_t my_uid = getuid();
		if ( !(pcache()->get_user_name( my_uid, RealUserName)) ) {
			char buf[64];
			sprintf( buf, "uid %d", (int)my_uid );
			RealUserName = strdup( buf );
		}
	}
	return RealUserName;
}

/* return the login of whomever you get when you call set_user_priv() */
const char*
get_user_loginname() {
	return UserName;
}
#endif  /* #if defined(WIN32) */


const char*
priv_identifier( priv_state s )
{
	static char id[256];
	int id_sz = 256;	// for use w/ snprintf()

	switch( s ) {

	case PRIV_UNKNOWN:
		snprintf( id, id_sz, "unknown user" );
		break;

	case PRIV_FILE_OWNER:
		if( ! OwnerIdsInited ) {
			if( !can_switch_ids() ) {
				return priv_identifier( PRIV_CONDOR );
			}
			EXCEPT( "Programmer Error: priv_identifier() called for "
					"PRIV_FILE_OWNER, but owner ids are not initialized" );
		}
#ifdef WIN32
		EXCEPT( "Programmer Error: priv_identifier() called for "
				"PRIV_FILE_OWNER, on WIN32" );
#else
		snprintf( id, id_sz, "file owner '%s' (%d.%d)",
				  OwnerName ? OwnerName : "unknown", OwnerUid, OwnerGid );
#endif
		break;

	case PRIV_USER:
	case PRIV_USER_FINAL:
		if( ! UserIdsInited ) {
			if( !can_switch_ids() ) {
				return priv_identifier( PRIV_CONDOR );
			}
			EXCEPT( "Programmer Error: priv_identifier() called for "
					"%s, but user ids are not initialized",
					priv_to_string(s) );
		}
#ifdef WIN32
		snprintf( id, id_sz, "%s@%s", UserLoginName, UserDomainName );
#else
		snprintf( id, id_sz, "User '%s' (%d.%d)", 
				  UserName ? UserName : "unknown", UserUid, UserGid );
#endif
		break;

#ifdef WIN32
	case PRIV_ROOT:
	case PRIV_CONDOR:
		snprintf( id, id_sz, "SuperUser (system)" );
		break;
#else /* UNIX */
	case PRIV_ROOT:
		snprintf( id, id_sz, "SuperUser (root)" );
		break;

	case PRIV_CONDOR:
		snprintf( id, id_sz, "Condor daemon user '%s' (%d.%d)", 
				  CondorUserName ? CondorUserName : "unknown", 
						CondorUid, CondorGid );
		break;
#endif /* WIN32 vs. UNIX */

	default:
		EXCEPT( "Programmer error: unknown state (%d) in priv_identifier", 
				(int)s );

	} /* end of switch */

	return (const char*) id;
}

// compare 2 usernames that may or may not be fully qualified
// to see of they both refer to the same user.
//
bool is_same_user(
   const char user1[],  // "user@domain" or "user"
   const char user2[],  // "user@domain" or "user"
   CompareUsersOpt opt)
{
   // for now, treat COMPARE_DEFAULT as meaning IGNORE_DOMAINS
   // TODO: qualify domains and then compare them.
   if (COMPARE_DOMAIN_DEFAULT == opt) {
      opt = (CompareUsersOpt)(COMPARE_DOMAIN_PREFIX | ASSUME_UID_DOMAIN);
   }

   const char * pu1 = user1;
   const char * pu2 = user2;

   // first compare the username part
   while (pu1[0] && pu1[0] != '@') {

      if (pu1[0] != pu2[0])
         return false;

      ++pu1;
      ++pu2;
   }

   // if we hit the end of the user portion of username1
   // and not have NOT hit the end of user portion of username2
   // then they don't match.
   if (pu2[0] && pu2[0] != '@')
      return false;

   int cmp = opt & COMPARE_MASK;
   if (COMPARE_IGNORE_DOMAIN == cmp)
      return true;

   // compare domain part
   bool fmatch = true;

   if (pu1[0] == '@') ++pu1;
   if (pu2[0] == '@') ++pu2;

   char *domain = NULL; // in case we need to fetch UID_DOMAIN
   if (pu1[0] == '.' || (0 == pu1[0] && (opt & ASSUME_UID_DOMAIN))) {
      domain = param("UID_DOMAIN");
      pu1 = domain ? domain : "";
   }
   if (pu2[0] == '.' || (0 == pu2[0] && (opt & ASSUME_UID_DOMAIN))) {
      if ( ! domain) domain = param("UID_DOMAIN");
      pu2 = domain ? domain : "";
   }

   // compare domains, if one is a valid prefix of the other
   // then we consider that a match. we do this so that
   // tonic.cs.wisc.edu matches tonic
   //
   if (pu1 != pu2) {
      if (COMPARE_DOMAIN_FULL == cmp) {
         fmatch = ! strcasecmp(pu1, pu2);
      } else if (COMPARE_DOMAIN_PREFIX == cmp) {
         for (;;) {
            // if the first domain ends, and the second one 
            // has ended or is on a '.' then we call that a match
            if ( ! pu1[0]) {
               fmatch = ( ! pu2[0] || pu2[0] == '.');
               break;
            }
            // if the domains don't match at this point, then
            // this is a match only if the first domain is on a '.'
            // and the second domain has ended.
            // we know that pu1[0] cannot be 0 here because of the test above
            //
            if (toupper(pu1[0]) != toupper(pu2[0])) {
               fmatch = (pu1[0] == '.') && ! pu2[0];
               break;
            }

            ++pu1;
            ++pu2;
         }
      }
   }

   if (domain) free (domain);

   return fmatch;
}

