/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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

#if defined(WIN32)

	dynuser nobody_login;

	if( pid ) {
		if ( nobody_login.reuse_accounts() == false ) {
		// before removing subdir, remove any nobody-user account associtated
		// with this starter pid.  this account might have been left around
		// if the starter did not clean up completely.
		//sprintf(buf,"condor-run-dir_%d",pid);
			sprintf(buf,"condor-run-%d",pid);
			if ( nobody_login.deleteuser(buf) ) {
				dprintf(D_FULLDEBUG,"Removed account %s left by starter\n",buf);
			}
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

	char pathbuf[2048];
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
caInsert( ClassAd* target, ClassAd* source, const char* attr,
		  const char* prefix )
{
    char  buf[4096];
	char* str = NULL;
	const char* good_str = NULL; 
	string modified_str;
	ExprTree* tree;
	ClassAdUnParser unp;
	string bufString;

	if( !attr ) {
		EXCEPT( "caInsert called with NULL attribute" );
	}
	if( !target || !source ) {
		EXCEPT( "caInsert called with NULL classad" );
	}

	tree = source->Lookup( attr );
	if( !tree ) {
		return false;
	}

 //	tree->PrintToNewStr( &str );
	unp.Unparse( bufString, tree );
	
	if( prefix ) {
        sprintf(buf, "%s=%s%s", attr, prefix, bufString);
	} else {
        sprintf(buf, "%s=%s", attr, bufString);
	}

	if( ! target->Insert( buf ) ) {
		dprintf( D_ALWAYS, "caInsert: Can't insert %s into target classad.\n",
				 attr );
		free( str );
		return false;
	}		
	free( str );
	return true;
}


/*
  This method takes a pointer to a classad, the name of an attribute
  in the config file, and a flag that says if that attribute isn't
  there, if it should be a fatal error or not.  If the attribute is
  defined, we insert "attribute = value" into the given classad and
  return true.  If the attribute wasn't defined, we return false, and
  if the is_fatal flag is set, we EXCEPT().  If we found the attribute
  but failed to insert it into the ClassAd, there's a syntax error and
  we should EXCEPT(), even if is_fatal is false.  If it's it's in the
  config file, the admin is trying to use it, so if there's a syntax
  error, we should let them know right away. 
  -Derek Wright <wright@cs.wisc.edu> 4/12/00
  -Syntax error checking by Derek on 6/25/03
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

	if( ! ad->Insert(tmp) ) {
		EXCEPT( "Syntax error in %s expression: '%s'", attr, val );
	}

	free( tmp );
	free( val );
	return true;
}


/* 
   This function reads of a ClaimId string and an eom from the
   given stream.  It looks up that ClaimId in the resmgr to find
   the corresponding Resource*.  If such a Resource is found, we
   return the pointer to it, otherwise, we return NULL.  */
Resource*
stream_to_rip( Stream* stream )
{
	char* id = NULL;
	Resource* rip;

	stream->decode();
	if( ! stream->code(id) ) {
		dprintf( D_ALWAYS, "Can't read ClaimId\n" );
		free( id );
		return NULL;
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't read end_of_message\n" );
		free( id );
		return NULL;
	}
	rip = resmgr->get_by_cur_id( id );
	if( !rip ) {
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with ClaimId (%s)\n", id );
		free( id );
		return NULL;
	}
	free( id );
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


