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
#include "dynuser.h"	// used in cleanup_execute_dir() for WinNT

void
check_perms()
{
	struct stat st;
	mode_t mode;
#ifdef WIN32
	mode_t desired_mode = _S_IREAD | _S_IWRITE;
#else
	mode_t desired_mode = (0777 | S_ISVTX);
#endif
		// We want execute to be world-writable w/ the sticky bit set.  

	if (stat(exec_path, &st) < 0) {
		EXCEPT( "stat exec path (%s), errno: %d", exec_path, errno ); 
	}

	if ((st.st_mode & desired_mode) != desired_mode) {
		dprintf(D_FULLDEBUG, "Changing permission on %s\n", exec_path);
		mode = st.st_mode | desired_mode;
		if (chmod(exec_path, mode) < 0) {
			EXCEPT( "chmod exec path (%s), errno: %d", exec_path,
					errno );  
		}
	}
}


void
cleanup_execute_dir(int pid)
{
	char buf[2048];

#if defined(WIN32)

	dynuser nobody_login;

	if( pid ) {
		// before removing subdir, remove any nobody-user account associtated
		// with this starter pid.  this account might have been left around
		// if the starter did not clean up completely.
		sprintf(buf,"condor-run-dir_%d",pid);
		if ( nobody_login.deleteuser(buf) ) {
			dprintf(D_FULLDEBUG,"Removed account %s left by starter\n",buf);
		}

		// now remove the subdirectory.  NOTE: we only remove the 
		// subdirectory _after_ removing the nobody account, because the
		// existence of the subdirectory persistantly tells us that the
		// account may still exist [in case the startd blows up as well].
		sprintf( buf, "rmdir /s /q \"%.256s\\dir_%d\"",
				 exec_path, pid );
		system( buf );
	} else {
		// get rid of everything in the execute directory
		Directory dir(exec_path);

		// remove all users matching this prefix
		nobody_login.cleanup_condor_users("condor-run-");

		// now that we took care of any old nobody accounts, blow away
		// everything in the execute directory.
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
	case VACATE_CLAIM_FAST:
		return "vacate_claim_fast";
	case VACATE_ALL_CLAIMS:
		return "vacate_all_claims";
	case VACATE_ALL_FAST:
		return "vacate_all_fast";
	default:
		return "unknown";
	}
	return "error";
}


bool
reply( Stream* s, int cmd )
{
	s->encode();
	if( !s->code( cmd ) || !s->end_of_message() ) {
		return false;
	} else {
		return true;
	}
}


bool
refuse( Stream* s )
{
	s->end_of_message();
	s->encode();
	if( !s->put(NOT_OK) ) {
		return false;
	} 
	if( !s->end_of_message() ) {
		return false;
	}
	return true;
}


bool
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
		return false;
	}
	tree->PrintToStr( buf );

	if( ! target->Insert( buf ) ) {
		dprintf( D_ALWAYS, "caInsert: Can't insert %s into target classad.\n",
				 attr );
		return false;
	}		
	return true;
}


/*
  This method takes a pointer to a classad, the name of an attribute
  in the config file, and a flag that says if that attribute isn't
  there, if it should be a fatal error or not.  If the attribute is
  defined, we insert "attribute = value" into the given classad and
  return true.  If the attribute wasn't defined, we return false, and
  if the is_fatal flag is set, we EXCEPT
  -Derek Wright <wright@cs.wisc.edu> 4/12/00
*/
bool
configInsert( ClassAd* ad, const char* attr, bool is_fatal )
{
	return configInsert( ad, attr, attr, is_fatal );
}


/*
  This version just allows for the name of the thing you're looking
  for in the config file to be different than the classad attribute
  name you want to insert it as.
*/
bool
configInsert( ClassAd* ad, const char* param_name, 
			  const char* attr, bool is_fatal ) 
{
	char* val = param( param_name );
	char* tmp;
	if( ! val ) {
		if( is_fatal ) {
			EXCEPT( "Required attribute \"%s\" is not defined", attr );
		}
		return false;
	}

	int size = 0;
	size += strlen( val );
	size += strlen( attr );
	size += 4;
	
	tmp = (char*) malloc( size * sizeof(char) );
	if( ! tmp ) {
		EXCEPT( "Out of memory!" );
	}
	sprintf( tmp, "%s = %s", attr, val );

	ad->Insert( tmp );

	free( tmp );
	free( val );
	return true;
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
   return the pointer to it, otherwise, we return NULL.  */
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
