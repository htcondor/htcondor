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
#include "startd.h"
#include "directory.h"

void
check_perms()
{
	struct stat st;
	mode_t mode;

	if (stat(exec_path, &st) < 0) {
		EXCEPT("stat exec path");
	}

	if ((st.st_mode & 0777) != 0777) {
		dprintf(D_FULLDEBUG, "Changing permission on %s\n", exec_path);
		mode = st.st_mode | 0777;
		if (chmod(exec_path, mode) < 0) {
			EXCEPT("chmod exec path");
		}
	}
}


void
cleanup_execute_dir(int pid)
{
	char buf[2048];

#if defined(WIN32)
	if( pid ) {
		sprintf( buf, "rmdir /s /q %.256s\\dir_%d",
				 exec_path, pid );
		system( buf );
	} else {
		// get rid of everything in the execute directory
		Directory dir(exec_path);
		dir.Remove_Entire_Directory();
	}
#else
	if( pid ) {
		sprintf( buf, "/bin/rm -rf %.256s/dir_%d",
				exec_path, pid );
	} else {
		sprintf( buf, "/bin/rm -rf %.256s/condor_exec* %.256s/dir_*",
				 exec_path, exec_path );
	}
	system( buf );
#endif
}


float
compute_rank( ClassAd* mach_classad, ClassAd* req_classad ) 
{
	float rank;

	if( mach_classad->EvalFloat( ATTR_RANK, req_classad, rank ) == 0 ) {
		dprintf( D_ALWAYS, "Error evaluating rank.\n" );
		rank = 0;
	}
	return rank;
}


int
create_port( ReliSock* rsock )
{
	rsock->bind();
	rsock->listen();
	return rsock->get_file_desc();
}


char*
command_to_string( int cmd )
{
	switch( cmd ) {
	case CONTINUE_CLAIM:
		return "continue_claim";
	case SUSPEND_CLAIM:
		return "suspend_claim";
	case DEACTIVATE_CLAIM:
		return "deactivate_claim";
	case DEACTIVATE_CLAIM_FORCIBLY:
		return "deactivate_claim_forcibly";
	case MATCH_INFO:
		return "match_info";
	case ALIVE:
		return "alive";
	case REQUEST_CLAIM:
		return "request_claim";
	case RELEASE_CLAIM:
		return "release_claim";
	case ACTIVATE_CLAIM:
		return "activate_claim";
	case GIVE_STATE:
		return "give_state";
	case PCKPT_JOB:
		return "pckpt_job";
	case PCKPT_ALL_JOBS:
		return "pckpt_all_jobs";
	case VACATE_CLAIM:
		return "vacate_claim";
	case VACATE_ALL_CLAIMS:
		return "vacate_all_claims";

	default:
		return "unknown";
	}
	return "error";
}


int
reply( Stream* s, int cmd )
{
	s->encode();
	if( !s->code( cmd ) || !s->end_of_message() ) {
		return FALSE;
	} else {
		return TRUE;
	}
}


int
caInsert( ClassAd* target, ClassAd* source, const char* attr, int verbose )
{
	char buf[4096];
	buf[0] = '\0';
	ExprTree* tree;
	if( !attr ) {
		EXCEPT( "caInsert called with NULL attribute" );
	}
	if( !target || !source ) {
		EXCEPT( "caInsert called with NULL classad" );
	}

	tree = source->Lookup( attr );
	if( !tree ) {
		if( verbose ) {
			dprintf( D_ALWAYS, 
					 "caInsert: Can't find %s in source classad.\n", 
					 attr );
		}
		return FALSE;
	}
	tree->PrintToStr( buf );

	if( ! target->Insert( buf ) ) {
		dprintf( D_ALWAYS, "caInsert: Can't insert %s into target classad.\n",
				 attr );
		return FALSE;
	}		
	return TRUE;
}


int
send_classad_to_sock( int cmd, Sock* sock, ClassAd* pubCA, ClassAd*
					  privCA )  
{
	sock->encode();
	if( ! sock->put( cmd ) ) {
		dprintf( D_ALWAYS, "Can't send command\n");
		sock->end_of_message();
		return FALSE;
	}
	if( pubCA ) {
		if( ! pubCA->put( *sock ) ) {
			dprintf( D_ALWAYS, "Can't send public classad\n");
			sock->end_of_message();
			return FALSE;
		}
	}
	if( privCA ) {
		if( ! privCA->put( *sock ) ) {
			dprintf( D_ALWAYS, "Can't send private classad\n");
			sock->end_of_message();
			return FALSE;
		}
	}
	if( ! sock->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't send end_of_message\n");
		return FALSE;
	}
	return TRUE;
}


/* 
   This function reads of a capability string and an eom from the
   given stream.  It looks up that capability in the resmgr to find
   the corresponding Resource*.  If such a Resource is found, we
   return the pointer to it, otherwise, we return NULL.
*/
Resource*
stream_to_rip( Stream* stream )
{
	char* cap = NULL;
	Resource* rip;

	stream->decode();
	if( ! stream->code(cap) ) {
		dprintf( D_ALWAYS, "Can't read capability\n" );
		free( cap );
		return NULL;
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't read end_of_message\n" );
		free( cap );
		return NULL;
	}
	rip = resmgr->get_by_cur_cap( cap );
	if( !rip ) {
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with capability (%s)\n",
				 cap );
		free( cap );
		return NULL;
	}
	free( cap );
	return rip;
}
