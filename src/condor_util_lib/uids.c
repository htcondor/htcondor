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
** 
*/ 

/*
** These routines used to switch back and forth from condor to root.
*/

#include <stdio.h>
#include <pwd.h>
#include <sys/types.h>

#if defined(AIX31) || defined(AIX32)
#include <sys/types.h>
#include <sys/id.h>
#define SET_EFFECTIVE_ID(id) setuidx(ID_REAL|ID_EFFECTIVE,id)
#define SET_REAL_ID(id) setuidx(ID_SAVED|ID_REAL|ID_EFFECTIVE,id)
#else
#define SET_EFFECTIVE_ID(id) seteuid(id)
#define SET_REAL_ID(id) setuid(id)
#endif

#include "except.h"
#include "debug.h"
#include "condor_sys.h"

char *_FileName_ = __FILE__;

#ifndef FALSE
#define FALSE 0
#endif FALSE

#ifndef TRUE
#define TRUE 1
#endif TRUE

#define ROOT 0

static uid_t CondorUid;
static int CondorUidInited = FALSE;

static int fatal_uid_sets = TRUE;

int
set_fatal_uid_sets(flag)
int     flag;
{
    int old_val;
    old_val = fatal_uid_sets;
    fatal_uid_sets = flag;
    return old_val;
}


init_condor_uid()
{
    struct passwd       *pwd, *getpwnam();
	int					scm;

	/*
	** N.B. if we are using the yellow pages, system calls which are
	** not supported by either remote system calls or file descriptor
	** mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
	*/
	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );
    if( (pwd=getpwnam("condor")) == NULL ) {
        EXCEPT( "condor not in passwd file" );
    }
	(void)endpwent();
	(void)SetSyscalls( scm );

    CondorUid = pwd->pw_uid;
	CondorUidInited = TRUE;
}


int
get_condor_uid()
{
	if( !CondorUidInited ) {
		init_condor_uid();
	}

	return CondorUid;
}


set_condor_euid()
{
	if( !CondorUidInited ) {
		init_condor_uid();
	}

	if( SET_EFFECTIVE_ID(CondorUid) < 0 ) {
        EXCEPT( "SET_EFFECTIVE_ID(%d)", CondorUid);
    }
}

set_condor_ruid()
{
	if( !CondorUidInited ) {
		init_condor_uid();
	}

	if( SET_REAL_ID(CondorUid) < 0 ) {
		EXCEPT( "SET_REAL_ID(%d)", CondorUid );
	}
}

set_root_euid()
{
	if( SET_EFFECTIVE_ID(ROOT) < 0 ) {
		EXCEPT( "SET_EFFECTIVE_ID(%d)", ROOT );
	}
}


set_effective_uid(uid_t new_uid)
{
	if( SET_EFFECTIVE_ID(new_uid) < 0 ) {
		EXCEPT( "SET_EFFECTIVE_ID(%d)", new_uid );
	}
}
