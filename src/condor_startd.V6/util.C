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
#if defined(WIN32)
	char buf[2048];
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

	// Instantiate a directory object pointing at the execute directory
	Directory execute_dir( exec_path, PRIV_ROOT );

	if( pid ) {
		char	pid_dir[_POSIX_PATH_MAX];

			// We're trying to delete a specific subdirectory, either
			// b/c a starter just exited and we might need to clean up
			// after it, or because we're in a recursive call.
		sprintf( pid_dir, "dir_%d", pid );

			// Look for it
		if ( execute_dir.Find_Named_Entry( pid_dir ) ) {

				// Remove the execute directory
			execute_dir.Remove_Current_File();
		}

		return;

	} else {
			// We were called in cleanup-everything mode and we're not
			// trying to get rid of a specific directory.  So, we've
			// got to iterate through the entire execute directory,
			// and try to delete each subdirectory individually.
			// recursion to the rescue...
		sleep( 1 );
		execute_dir.Remove_Entire_Directory();

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
	ExprTree* tree;
	bool rval = true;

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
	if( prefix ) {
			// since there's no way to rename the attribute name in an
			// existing ExprTree, we have to print it out to a string
			// to append the prefix...
		char* str = NULL;
		tree->PrintToNewStr( &str );
		MyString modified_str = prefix;
		modified_str += str;
		free( str );
		str = NULL;
		if( ! target->Insert(modified_str.Value()) ) {
			rval = false;
		}
	} else {
			// there's no prefix, we can just Insert a copy directly
			// (like we always do on the V6_6-branch version, which
			// doesn't support a prefix).
		ExprTree* new_tree = tree->DeepCopy();
		if( ! target->Insert(new_tree) ) {
			rval = false;
		}			
	}

	if( ! rval ) {
		dprintf( D_ALWAYS, "caInsert: Can't insert %s into target classad.\n",
				 attr );
	}		
	return rval;
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


