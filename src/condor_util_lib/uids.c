/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Rick Rasumssen
** 	         University of Wisconsin, Computer Sciences Dept.
** Re-organized by Jim Basney
** 
*/ 

/*
** These routines used to switch back and forth from condor to root.
*/

#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/types.h>
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
#include "condor_sys.h"
#include "condor_uid.h"

char *_FileName_ = __FILE__;

#ifndef FALSE
#define FALSE 0
#endif FALSE

#ifndef TRUE
#define TRUE 1
#endif TRUE

#define ROOT 0

static uid_t CondorUid, UserUid;
static gid_t CondorGid, UserGid;
static int CondorIdsInited = FALSE;
static int UserIdsInited = FALSE;
static int SwitchIds = TRUE;
static priv_state CurrentPrivState = PRIV_UNKNOWN;

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
init_condor_ids()
{
	struct passwd *pwd;
	int scm;
	uid_t myuid;
	gid_t mygid;
	char *buf;

        /*
        ** N.B. if we are using the yellow pages, system calls which are
        ** not supported by either remote system calls or file descriptor
        ** mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
        */
	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );

	myuid = getuid();
	mygid = getgid();
	
	/* If we're root, set the Condor Uid and Gid to the value
	   specified in the "CONDOR_IDS" environment variable */
	if( myuid == ROOT ) {
		if( (buf = getenv( "CONDOR_IDS" )) ) {	
			if( sscanf(buf, "%d.%d", &CondorUid, &CondorGid) != 2 ) {
				fprintf(stderr, "ERROR: badly formed value in CONDOR_IDS ");
				fprintf(stderr, "environment variable.\n");
				fprintf(stderr, "Please set CONDOR_IDS to ");
				fprintf(stderr, "the '.' seperated uid, gid pair that\n");
				fprintf(stderr, "should be used by Condor.\n");
				exit(1);
			}
		} else {
			/* No CONDOR_IDS set, use condor.condor */
			pwd=getpwnam("condor");
			if( pwd ) {
				CondorUid = pwd->pw_uid;
				CondorGid = pwd->pw_gid;
			} else {
				fprintf(stderr,
						"Can't find \"condor\" in the password file and\n"
						"CONDOR_IDS environment variable not set\n"); 
				exit(1);
			}
		}
	} else {
		/* Non-root.  Set the CondorUid/Gid to our current uid/gid */
		CondorUid = myuid;
		CondorGid = mygid;
		/* no need to try to switch ids when running as non-root */
		SwitchIds = FALSE;
	}
	
	(void)endpwent();
	(void)SetSyscalls( scm );
	
	CondorIdsInited = TRUE;
}


void
init_user_ids(char username[])
{
    struct passwd       *pwd, *getpwnam();
	int					scm;

	/*
	** N.B. if we are using the yellow pages, system calls which are
	** not supported by either remote system calls or file descriptor
	** mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
	*/
	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );
    if( (pwd=getpwnam(username)) == NULL ) {
        dprintf(D_ALWAYS, "%s not in passwd file", username );
		return;
    }
	(void)endpwent();
	(void)SetSyscalls( scm );

	UserUid = pwd->pw_uid;
	UserGid = pwd->pw_gid;
	initgroups(username, UserGid);
	UserIdsInited = TRUE;
}


void
set_user_ids(uid_t uid, gid_t gid)
{
	UserUid = uid;
	UserGid = gid;
	UserIdsInited = TRUE;
}


priv_state
set_priv(priv_state s)
{
	priv_state PrevPrivState = CurrentPrivState;
	if (s == CurrentPrivState) return s;
	if (CurrentPrivState == PRIV_USER_FINAL) {
		dprintf(D_ALWAYS,
				"warning: attempted switch out of PRIV_USER_FINAL\n");
		return s;
	}
	CurrentPrivState = s;
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
			dprintf("unknown priv state %d\n", (int)s);
		}
		return PrevPrivState;
	}
}	


priv_state
set_condor_priv()
{
	return set_priv(PRIV_CONDOR);
}


priv_state
set_user_priv()
{
	return set_priv(PRIV_USER);
}


void
set_user_priv_final()
{
	set_priv(PRIV_USER_FINAL);
}


priv_state
set_root_priv()
{
	return set_priv(PRIV_ROOT);
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


uid_t
get_user_uid()
{
	if( !UserIdsInited ) {
		dprintf(D_ALWAYS, "get_user_uid() called when UserIds not inited!\n");
		return -1;
	}

	return UserUid;
}


gid_t
get_user_gid()
{
	return UserUid;
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
