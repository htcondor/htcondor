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
#include "daemon.h"

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
	char pathbuf[2048];

#if defined(WIN32)

	dynuser nobody_login;

	if( pid ) {
		// before removing subdir, remove any nobody-user account associtated
		// with this starter pid.  this account might have been left around
		// if the starter did not clean up completely.
		//sprintf(buf,"condor-run-dir_%d",pid);
		sprintf(buf,"condor-run-%d",pid);
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

#else /* UNIX */

	struct stat st;
	priv_state priv;

	if( pid ) {
			// We're trying to delete a specific subdirectory, either
			// b/c a starter just exited and we might need to clean up
			// after it, or because we're in a recursive call.

		sprintf( pathbuf, "%.512s/dir_%d", exec_path, pid );

			// stat() the directory so we know what user owns it, if
			// it's a symlink, etc.
		errno = 0;
		if( lstat(pathbuf, &st) < 0 ) {
				// woah, stat() failed.  let's hope errno is the right
				// thing:
			if( errno != ENOENT ) {
				dprintf( D_ALWAYS, "Error in cleanup_execute_dir(): "
						 "stat(%s) failed with errno: %d\n", pathbuf,
						 errno );
					// what else can we do here?
			} else {
					// stat() said the directory is already gone.
					// this probably means we're dealing with an old
					// starter that cleans up after itself...  let the
					// log know, at least.
				dprintf( D_FULLDEBUG, "cleanup_execute_dir(): "
						 "%s does not exist\n", pathbuf );
			}
				// either way, if the directory is gone, we're done. 
			return;
		}

			// If we're here, stat() worked, so the directory is here.
			// See what user owns the directory, and try to become
			// that user to do the remove.  This avoids executing
			// stuff as root which is good both for security reasons,
			// and in case the directory lives on NFS and root doesn't
			// have permission to do the remove, anyway.

			// First, make sure the thing we're looking at isn't a
			// symlink.  If it is, someone's probably trying to mess
			// with us, so don't let them get away with it.
		if( S_ISLNK(st.st_mode) ) {
			dprintf( D_ALWAYS, "Error in cleanup_execute_dir(): "
					 "%s is a symlink, ignoring!\n", pathbuf );
			return;
		}

			// Make sure it's not owned by root, since that would also
			// be Bad(tm).
		if( st.st_uid == 0 ) {
				// Nothing should be owned by root in here...  we
				// certainly don't want to go deleting it, since
				// someone might be trying to exploit a security hole.
			dprintf( D_ALWAYS, "Error in cleanup_execute_dir(): "
					 "%s is owned by root, ignoring!\n", pathbuf );
			return;
		}

			// ok, things are actually safe to do the remove:
		uninit_user_ids();	// clear out whatever was there to avoid
			                // dprintf() warnings...
		set_user_ids( st.st_uid, st.st_gid );
		sprintf( buf, "/bin/rm -rf %s", pathbuf );
		priv = set_user_priv();
		system( buf );
		set_priv( priv );
		return;

	} else {
			// We were called in cleanup-everything mode and we're not
			// trying to get rid of a specific directory.  So, we've
			// got to iterate through the entire execute directory,
			// and try to delete each subdirectory individually.
			// recursion to the rescue...

		char* path_to_remove;
		char* tmp;
		double tmp_pid;
		Directory exec_dir( exec_path );
		exec_dir.Rewind();
		while( (path_to_remove = (char*)exec_dir.Next()) ) { 
				// if we're here, there's something left in the
				// directory.  
			if( (tmp = strstr(path_to_remove, "dir_")) ) {
					// the thing we're looking at is a starter's
					// temporary directory, so pull out the pid and
					// call ourselves recursively to handle that
					// specific subdir
				if( sscanf(tmp, "dir_%lf", &tmp_pid) == 1 ) {
						// we found the right pid, call ourselves to
						// clean this directory up.
					if ( tmp_pid > INT_MAX ) {
							// Something's whacked here.  Someone's
							// probably trying to mess with us, so
							// just print a warning and ignore it.
						dprintf( D_ALWAYS, 
								 "Warning in cleanup_execute_dir(): "
								 "invalid path \"%s\": pid is too large to "
								 "be legitimate, ignoring\n",
								 path_to_remove ); 
						continue;
					}
					cleanup_execute_dir( (int)tmp_pid );
				} else { 
						// couldn't parse this path, move on.
					dprintf( D_ALWAYS, "Error in cleanup_execute_dir(): "
							 "failed to parse path \"%s\", ignoring\n",
							 path_to_remove ); 
				}
			} else {
					// we're not trying to delete a "dir_<pid>"
					// directory.  nothing else should be in here, so
					// print a log message about it and let the admin
					// check it out.
				dprintf( D_ALWAYS, "Warning in cleanup_execute_dir(): "
						 "%s contains unknown entry \"%s\", ignoring\n", 
						 exec_path, path_to_remove );
			}
		}
			// nothing left in the directory, we're done.
		return;
	} // end of else clause for if(pid)
#endif  /* UNIX */
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
send_classad_to_sock( int cmd, Daemon * d, ClassAd* pubCA, ClassAd*
					  privCA )  
{
    Sock * sock = d->safeSock();
    if (! d->startCommand(cmd, sock)) {
		dprintf( D_ALWAYS, "Can't send command\n");
		sock->end_of_message();
		delete sock;
		return FALSE;
	}

	if( pubCA ) {
		if( ! pubCA->put( *sock ) ) {
			dprintf( D_ALWAYS, "Can't send public classad\n");
			sock->end_of_message();
			delete sock;
			return FALSE;
		}
	}
	if( privCA ) {
		if( ! privCA->put( *sock ) ) {
			dprintf( D_ALWAYS, "Can't send private classad\n");
			sock->end_of_message();
			delete sock;
			return FALSE;
		}
	}
	if( ! sock->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't send end_of_message\n");
		delete sock;
		return FALSE;
	}

	delete sock;
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


VacateType
getVacateType( ClassAd* ad )
{
	VacateType vac_t;
	char* vac_t_str = NULL;
	if( ! ad->LookupString(ATTR_VACATE_TYPE, &vac_t_str) ) { 
		return VACATE_GRACEFUL;
	}
	vac_t = getVacateTypeNum( vac_t_str );
	free( vac_t_str );
	return vac_t;
}


int
sendCAReply( Stream* s, char* cmd_str, ClassAd* reply )
{
	reply->SetMyTypeName( REPLY_ADTYPE );
	reply->SetTargetTypeName( COMMAND_ADTYPE );

	MyString line;
	line = ATTR_VERSION;
	line += " = \"";
	line += CondorVersion();
	line += '"';
	reply->Insert( line.Value() );

	line = ATTR_PLATFORM;
	line += " = \"";
	line += CondorPlatform();
	line += '"';
	reply->Insert( line.Value() );

	s->encode();
	if( ! reply->put(*s) ) {
		dprintf( D_ALWAYS,
				 "ERROR: Can't send reply classad for %s, aborting\n",
				 cmd_str );
		return FALSE;
	}
	if( ! s->eom() ) {
		dprintf( D_ALWAYS, "ERROR: Can't send eom for %s, aborting\n", 
				 cmd_str );
		return FALSE;
	}
	return TRUE;
}



int
sendErrorReply( Stream* s, char* cmd_str, CAResult result, 
				const char* err_str ) 
{
	dprintf( D_ALWAYS, "Aborting %s\n", cmd_str );
	dprintf( D_ALWAYS, "%s\n", err_str );

	ClassAd reply;
	MyString line;

	line = ATTR_RESULT;
	line += " = \"";
	line += getCAResultString( result );
	line += '"';
	reply.Insert( line.Value() );

	line = ATTR_ERROR_STRING;
	line += " = \"";
	line += err_str;
	line += '"';
	reply.Insert( line.Value() );
	
	return sendCAReply( s, cmd_str, &reply );
}

