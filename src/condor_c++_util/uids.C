/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "condor_uid.h"
#include "condor_config.h"
#include "condor_environ.h"
#include "condor_distribution.h"
#include "my_username.h"


/* See condor_uid.h for description. */
static char* CondorUserName = NULL;
static const char* RealUserName = NULL;
THREAD_LOCAL_STORAGE static priv_state CurrentPrivState = PRIV_UNKNOWN;
static int SwitchIds = TRUE;

/* must be listed in the same order as enum priv_state in condor_uid.h */
static char *priv_state_name[] = {
	"PRIV_UNKNOWN",
	"PRIV_ROOT",
	"PRIV_CONDOR",
	"PRIV_USER",
	"PRIV_USER_FINAL"
};


/* Start Common Bits */
#define HISTORY_LENGTH 32

static struct {
	time_t		timestamp;
	priv_state	priv;
	char 		*file;
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
log_priv(priv_state prev, priv_state new_priv, char file[], int line)
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
	if (SwitchIds) {
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

int
init_user_ids(const char username[], const char domain[]) 
{
	int retval = 0;

	if (!username || !domain) {
		dprintf(D_ALWAYS, "WARNING: init_user_ids() called with"
			   " NULL arguments!");
	   	return 0;
   	}

	
	// see if we already have a user handle for the requested user.
	// if so, just return 1. 
	// TODO: cache multiple user handles to save time.

	dprintf(D_FULLDEBUG, "init_user_ids: want user '%s@%s', "
			"current is '%s@%s'\n",
		username, domain, UserLoginName, UserDomainName);
	
	if ( CurrUserHandle = cached_tokens.getToken(username, domain)) {
		dprintf(D_FULLDEBUG, "init_user_ids: Already have handle for %s@%s,"
			" so returning.\n", username, domain);
		return 1;
	} else {
		char* myusr = my_username();
		char* mydom = my_domainname();

		// see if our calling thread matches the user and domain
		// we want a token for. This happens if we're submit for example.
		if ( strcmp( myusr, username ) == 0 &&
			 stricmp( mydom, domain ) == 0 ) { // domain is case insensitive

			dprintf(D_FULLDEBUG, "init_user_ids: Calling thread has token "
					"we want, so returning.\n");
			CurrUserHandle = my_usertoken();
			if (! CurrUserHandle ) {
				dprintf(D_FULLDEBUG, "init_user_ids: handle is null!\n");
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

		// these should probably be snprintfs
		swprintf(w_fullname, L"%S@%S", username, domain);
		sprintf(user, "%s", username);
		sprintf(dom, "%s", domain);
		
		// make sure we're SYSTEM when we do this
		w_pw = lsaMan.query(w_fullname);


		if ( ! w_pw ) {
			dprintf(D_ALWAYS, "ERROR: Could not locate credential for user "
				"'%s@%s'\n", username, domain);
			return 0;
		} else {
			sprintf(pw, "%S", w_pw);

			// we don't need the wide char pw anymore, so clean it up
			ZeroMemory(w_pw, wcslen(w_pw)*sizeof(wchar_t));
			delete[](w_pw);

			dprintf(D_FULLDEBUG, "Found credential for user '%s'\n", username);
			retval = LogonUser(
				user,						// user name
				dom,						// domain or server - local for now
				pw,							// password
				LOGON32_LOGON_INTERACTIVE,	// type of logon operation. 
											// LOGON_BATCH doesn't seem to work right here.
				LOGON32_PROVIDER_DEFAULT,	// logon provider
				&CurrUserHandle				// receive tokens handle
			);

			// clear pw from memory
			ZeroMemory(pw, 255);

			dprintf(D_FULLDEBUG, "LogonUser completed.\n");

			UserLoginName = strdup(username);
			UserDomainName = strdup(domain);

			if ( !retval ) {
				dprintf(D_ALWAYS, "init_user_ids: LogonUser failed with NT Status %ld\n", 
					GetLastError());
				return 0;
			} else {
				// stash the new token in our cache
				cached_tokens.storeToken(UserLoginName, UserDomainName,
					   	CurrUserHandle);
				return 1;
			}
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
			// Oh shit.  
			EXCEPT("Failed to create a user nobody");
		}
	
		UserLoginName = strdup( myDynuser->get_accountname() );
		UserDomainName = strdup( "." );

		// we created a new user, now just stash the token
		CurrUserHandle = myDynuser->get_token();

		// drop the handle in our cache too
		cached_tokens.storeToken(UserLoginName, UserDomainName,
			   		CurrUserHandle);
		return 1;
	}
}

priv_state
_set_priv(priv_state s, char file[], int line, int dologging)
{
	priv_state PrevPrivState = CurrentPrivState;

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
		dprintf(D_ALWAYS,
				"warning: attempted switch out of PRIV_USER_FINAL\n");
		return PRIV_USER_FINAL;
	}

	CurrentPrivState = s;
	if (SwitchIds) {
		switch (s) {
		case PRIV_ROOT:
		case PRIV_CONDOR:
			RevertToSelf();
			break;
		case PRIV_USER:
		case PRIV_USER_FINAL:
			if ( CurrUserHandle ) {
				if ( PrevPrivState == PRIV_UNKNOWN ) {
					// make certain we're back to 'condor' before impersonating
					RevertToSelf();
				}
				ImpersonateLoggedOnUser(CurrUserHandle);
			} else {
				// Tried to set_user_priv() but it failed, so bail out!

				EXCEPT("set_user_priv() failed!");
			}
			break;
		case PRIV_UNKNOWN:		/* silently ignore */
			break;
		default:
			dprintf( D_ALWAYS, "set_priv: Unknown priv state %d\n", (int)s);
		}
	}

logandreturn:
	if (dologging) log_priv(PrevPrivState, CurrentPrivState, file, line);
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
	if ( CondorUserName ) {
		sprintf(CondorUserName, "%s/%s",szDomainName,szAccountName);
	}

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
	char* user = my_username( 0 );
	if( ! user ) {
		dprintf( D_ALWAYS, 
				 "ERROR in is_root(): my_username() returned NULL\n" );
		return 0;
	}
	if( !stricmp(user, "SYSTEM") ) {
		root = 1;
	}
	free( user );
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

#include "debug.h"


#ifndef FALSE
#define FALSE 0
#endif /* FALSE */

#ifndef TRUE
#define TRUE 1
#endif /* TRUE */

#define ROOT 0

static uid_t CondorUid, UserUid, MyUid, RealCondorUid;
static gid_t CondorGid, UserGid, MyGid, RealCondorGid;
static int CondorIdsInited = FALSE;
static int UserIdsInited = FALSE;
static char* UserName = NULL;

static int set_condor_euid();
static int set_condor_egid();
static int set_user_euid();
static int set_user_egid();
static int set_user_ruid();
static int set_user_rgid();
static int set_root_euid();
static int set_condor_ruid();
static int set_condor_rgid();

/* We don't use EXCEPT here because this file is used in
   condor_syscall_lib.a.  -Jim B. */

void
_condor_disable_uid_switching()
{
	CondorIdsInited = TRUE;
	SwitchIds = FALSE;
}

void
init_condor_ids()
{
	struct passwd *pwd;
	int scm;
	char *buf;

        /*
        ** N.B. if we are using the yellow pages, system calls which are
        ** not supported by either remote system calls or file descriptor
        ** mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
        */
	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );

	MyUid = getuid();
	MyGid = getgid();
	
	if( CondorUserName ) {
		free( CondorUserName );
	}

	pwd=getpwnam( myDistro->Get() );
	if( pwd ) {
		RealCondorUid = pwd->pw_uid;
		RealCondorGid = pwd->pw_gid;
	} else {
		RealCondorUid = (uid_t)-1;
		RealCondorGid = (gid_t)-1;
	}

	/* If we're root, set the Condor Uid and Gid to the value
	   specified in the "CONDOR_IDS" environment variable */
	if( MyUid == ROOT ) {
		const char	*envName = EnvGetName( ENV_UG_IDS ); 
		if( (buf = getenv( envName )) ) {	
			if( sscanf(buf, "%d.%d", &CondorUid, &CondorGid) != 2 ) {
				fprintf(stderr, "ERROR: badly formed value in %s ", envName );
				fprintf(stderr, "environment variable.\n");
				fprintf(stderr, "Please set %s to ", envName);
				fprintf(stderr, "the '.' seperated uid, gid pair that\n");
				fprintf(stderr, "should be used by %s.\n", myDistro->Get() );
				exit(1);
			}
			pwd = getpwuid( CondorUid );
			if( pwd ) {
				CondorUserName = strdup( pwd->pw_name );
			} else {
				fprintf( stderr, "ERROR: the uid specified in %s ", envName );
				fprintf( stderr, "environment variable (%d)\n", CondorUid );
				fprintf(stderr, "does not exist in your password information.\n" );
				fprintf(stderr, "Please set %s to ", envName);
				fprintf(stderr, "the '.' seperated uid, gid pair that\n");
				fprintf(stderr, "should be used by %s.\n", myDistro->Get() );
				exit(1);
			}
		} else {
			/* No CONDOR_IDS set, use condor.condor */
			if( RealCondorUid > 0 ) {
				CondorUid = RealCondorUid;
				CondorGid = RealCondorGid;
				CondorUserName = strdup( myDistro->Get() );
			} else {
				fprintf(stderr,
						"Can't find \"condor\" in the password file and\n"
						"%s environment variable not set\n", envName); 
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
		pwd = getpwuid( CondorUid );
		if( pwd ) {
			CondorUserName = strdup( pwd->pw_name );
		} else {
			/* Cannot find an entry in the passwd file for this uid */
			CondorUserName = strdup("Unknown");
		}

		/* no need to try to switch ids when running as non-root */
		/* Can't dprintf() here, see above comment. -Derek 12/21/98 */
		/* dprintf(D_PRIV, "running as non-root; no privilege switching\n"); */
		SwitchIds = FALSE;
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

	if( UserIdsInited && UserUid != uid && !is_quiet ) {
		dprintf( D_ALWAYS, 
				 "warning: setting UserUid to %d, was %d previosly\n",
				 uid, UserUid );
	}
	UserUid = uid;
	UserGid = gid;
	UserIdsInited = TRUE;

	// find the user login name for this uid.  note we should not
	// EXCEPT or log an error if we do not find it; it is OK for the
	// user not to be in the passwd file for a so-called SOFT_UID_DOMAIN.
	if ( !username ) {
		struct passwd *	pwd = getpwuid( UserUid );
		if( pwd ) {
			username = pwd->pw_name;
		}
	}

		// Finally, save the username so we can call initgroups later.
	if( UserName ) {
		free( UserName );
	}
	if( username ) {
		UserName = strdup( username );
	} else {
		UserName = NULL;
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
    struct passwd *pwd_entry = NULL;
	int nobody_uid = -1;
	int nobody_gid = -1;

	if( (pwd_entry = getpwnam("nobody")) == NULL ) {
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

	nobody_uid = pwd_entry->pw_uid;
	nobody_gid = pwd_entry->pw_gid;

#ifdef HPUX
	// HPUX9 has a bug in that getpwnam("nobody") always returns
	// a gid of 60001, no matter what the group file (or NIS) says!
	// on top of that, legal UID/GIDs must be -1<x<60000, so unless we
	// patch here, we will generate an EXCEPT later when we try a
	// setgid().  -Todd Tannenbaum, 3/95
	if( (nobody_uid > 59999) || (nobody_uid < 0) ) {
		nobody_uid = 59999;
	}
	if( (nobody_gid > 59999) || (nobody_gid < 0) ) {
		nobody_gid = 59999;
	}
#endif

#ifdef IRIX
		// Same weirdness on IRIX.  60001 is the default uid for
		// nobody, lets hope that works.
	if( (nobody_uid >= UID_MAX ) || (nobody_uid < 0) ) {
		nobody_uid = 60001;
	}
	if( (nobody_gid >= UID_MAX) || (nobody_gid < 0) ) {
		nobody_gid = 60001;
	}
#endif

		// Now we know what the uid/gid for nobody should *really* be,
		// so we can actually initialize this as the "user" priv.
	return set_user_ids_implementation( (uid_t)nobody_uid,
										(gid_t)nobody_gid, "nobody",
										is_quiet );
}


int
init_user_ids_implementation( const char username[], int is_quiet )
{
    struct passwd       *pwd;
	int					scm;

	/*
	** N.B. if we are using the yellow pages, system calls which are
	** not supported by either remote system calls or file descriptor
	** mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
	*/
	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );

	if( ! stricmp(username, "nobody") ) {
			// There's so much special logic for user nobody that it's
			// all in a seperate function now.
		return init_nobody_ids( is_quiet );
	}

	if( (pwd=getpwnam(username)) == NULL ) {
		if( ! is_quiet ) {
			dprintf( D_ALWAYS, "%s not in passwd file\n", username );
		}
		return FALSE;
	}
	(void)endpwent();
	(void)SetSyscalls( scm );
	return set_user_ids_implementation( pwd->pw_uid, pwd->pw_gid,
										username, is_quiet ); 
}


int
init_user_ids( const char username[], const char domain[] ) {
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
uninit_user_ids()
{
	UserIdsInited = FALSE;
}


priv_state
_set_priv(priv_state s, char file[], int line, int dologging)
{
	priv_state PrevPrivState = CurrentPrivState;
	if (s == CurrentPrivState) return s;
	if (CurrentPrivState == PRIV_USER_FINAL) {
		dprintf(D_ALWAYS,
				"warning: attempted switch out of PRIV_USER_FINAL\n");
		return PRIV_USER_FINAL;
	}
	CurrentPrivState = s;

		// If we haven't already done so, we want to try to init the
		// condor ids, since that's where we figure out if we're root
		// or not, and therefore, initialize the SwitchIds variable
		// to the right thing (there's no need to try switching unless
		// we were started as root).
	if( !CondorIdsInited ) {
		init_condor_ids();
	}

	if (SwitchIds) {
		switch (s) {
		case PRIV_ROOT:
			set_root_euid();
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
		case PRIV_USER_FINAL:
			set_root_euid();	/* must be root to switch */
			set_user_rgid();
			set_user_ruid();
			break;
		case PRIV_UNKNOWN:		/* silently ignore */
			break;
		default:
			dprintf( D_ALWAYS, "set_priv: Unknown priv state %d\n", (int)s);
		}
	}
	if (dologging) log_priv(PrevPrivState, CurrentPrivState, file, line);
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
get_my_uid()
{
	if( !CondorIdsInited ) {
		init_condor_ids();
	}

	return MyUid;
}


gid_t
get_my_gid()
{
	if( !CondorIdsInited ) {
		init_condor_ids();
	}

	return MyGid;
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
	
		// Now, call initgroups with the right username so the user
		// can access files belonging to any group (s)he is a member
		// of.  If we did not call initgroups here, the user could
		// only access files belonging to his/her default group, and
		// might be left with access to the groups that root belongs
		// to, which is a serious security problem.
	if( UserName ) {
		errno = 0;
		if( (initgroups(UserName, UserGid) < 0) ) {
			dprintf( D_ALWAYS, 
					 "set_user_egid - ERROR: initgroups(%s, %d) failed, "
					 "errno: %d\n", UserName, UserGid, errno );
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

		// Now, call initgroups with the right username so the user
		// can access files belonging to any group (s)he is a member
		// of.  If we did not call initgroups here, the user could
		// only access files belonging to his/her default group, and
		// might be left with access to the groups that root belongs
		// to, which is a serious security problem.
	if( UserName ) {
		errno = 0;
		if( (initgroups(UserName, UserGid) < 0) ) {
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

	return SET_REAL_GID(CondorGid);
}


int
is_root( void ) 
{
	return (! getuid() );
}


const char*
get_real_username( void )
{
	if( ! RealUserName ) {
		uid_t my_uid = getuid();
		struct passwd *pwd;
		pwd = getpwuid( my_uid );
		if( pwd ) {
			RealUserName = strdup( pwd->pw_name );
		} else {
			char buf[64];
			sprintf( buf, "uid %d", (int)my_uid );
			RealUserName = strdup( buf );
		}
	}
	return RealUserName;
}

#endif  /* #if defined(WIN32) */
