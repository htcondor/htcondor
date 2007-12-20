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
#include "startd.h"
#include "directory.h"
#include "dynuser.h"	// used in cleanup_execute_dir() for WinNT
#include "daemon.h"
#include "../condor_privsep/condor_privsep.h"

static void
check_execute_dir_perms( char const *exec_path )
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
		EXCEPT( "stat exec path (%s), errno: %d (%s)", exec_path, errno,
				strerror( errno ) ); 
	}

	// in PrivSep mode, the EXECUTE directory must be trusted by
	// the PrivSep kernel. we can't determine this ourselves in general
	// (since the PrivSep Switchboard can be recompiled to trust
	// non-root users), so we'll have to be satisfied for now that we
	// could stat its path
	//
	if (privsep_enabled()) {
		return;
	}

	if ((st.st_mode & desired_mode) != desired_mode) {
		dprintf(D_FULLDEBUG, "Changing permission on %s\n", exec_path);
		mode = st.st_mode | desired_mode;
		if (chmod(exec_path, mode) < 0) {
			EXCEPT( "chmod exec path (%s), errno: %d (%s)", exec_path,
					errno, strerror( errno ) );
		}
	}
}

void
check_execute_dir_perms( StringList &list )
{
	char const *exec_path;

	list.rewind();

	while( (exec_path = list.next()) ) {
		check_execute_dir_perms( exec_path );
	}
}

void
cleanup_execute_dirs( StringList &list )
{
	char const *exec_path;

	list.rewind();

	while( (exec_path = list.next()) ) {
#if defined(WIN32)
		dynuser nobody_login;
		// remove all users matching this prefix
		nobody_login.cleanup_condor_users("condor-run-");

		// get rid of everything in the execute directory
		Directory execute_dir(exec_path);
		execute_dir.Remove_Entire_Directory();
#else
		// if we're using PrivSep, the Switchboard will only allow
		// us to remove subdirectories of EXECUTE - so we need to
		// list them and ask the Switchboard to delete each one
		//
		Directory execute_dir( exec_path, PRIV_ROOT );
		if (privsep_enabled()) {
			execute_dir.Rewind();
			while (execute_dir.Next()) {
				privsep_remove_dir(execute_dir.GetFullPath());
			}
		}
		else {
			execute_dir.Remove_Entire_Directory();
		}
#endif
	}
}

void
cleanup_execute_dir(int pid, char const *exec_path)
{
	ASSERT( pid );

#if defined(WIN32)
	MyString buf;
	dynuser nobody_login;

	if ( nobody_login.reuse_accounts() == false ) {
	// before removing subdir, remove any nobody-user account associtated
	// with this starter pid.  this account might have been left around
	// if the starter did not clean up completely.
	//sprintf(buf,"condor-run-dir_%d",pid);
		buf.sprintf("condor-run-%d",pid);
		if ( nobody_login.deleteuser(buf.Value()) ) {
			dprintf(D_FULLDEBUG,"Removed account %s left by starter\n",buf.Value());
		}
	}

	// now remove the subdirectory.  NOTE: we only remove the 
	// subdirectory _after_ removing the nobody account, because the
	// existence of the subdirectory persistantly tells us that the
	// account may still exist [in case the startd blows up as well].

	buf.sprintf( "%s\\dir_%d", exec_path, pid );
 
	Directory dir( buf.Value() );
	dir.Remove_Full_Path(buf.Value());



#else /* UNIX */

	MyString	pid_dir;

		// We're trying to delete a specific subdirectory, either
		// b/c a starter just exited and we might need to clean up
		// after it, or because we're in a recursive call.
	pid_dir.sprintf( "dir_%d", pid );

		// if we're using PrivSep, we won't have the permissions
		// needed to clean up - ask the Switchboard to do it; but
		// before we do that, use stat to see if there's anything
		// to clean up and save the Switchboard invocation if not
	if (privsep_enabled()) {
		MyString pid_dir_path;
		pid_dir_path.sprintf("%s/%s", exec_path, pid_dir.Value());
		struct stat stat_buf;
		if (stat(pid_dir_path.Value(), &stat_buf) == -1) {
			return;
		}
		if (!privsep_remove_dir(pid_dir_path.Value())) {
			dprintf(D_ALWAYS,
			        "privsep_remove_dir failed to remove %s\n",
			        pid_dir_path.Value());
		}
		return;
	}

	// Instantiate a directory object pointing at the execute directory
	Directory execute_dir( exec_path, PRIV_ROOT );

		// Look for it
	if ( execute_dir.Find_Named_Entry( pid_dir.Value() ) ) {

			// Remove the execute directory
		execute_dir.Remove_Current_File();
	}
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
	/* FALSE means this is an incoming connection */
	rsock->bind( FALSE );
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
		ClaimIdParser idp( id );
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with ClaimId (%s) -- perhaps this claim was already removed?\n", idp.publicClaimId() );
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


