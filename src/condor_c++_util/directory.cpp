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
#include "condor_open.h"
#include "condor_constants.h"
#include "condor_debug.h"
#include "directory.h"
#include "condor_string.h"
#include "status_string.h"
#include "condor_config.h"
#include "stat_wrapper.h"
#include "perm.h"
#include "my_username.h"
#include "my_popen.h"


// Set DEBUG_DIRECTORY_CLASS to 1 to not actually remove
// files, but instead print out to the log file what would get
// removed.  Set DEBUG_DIRECTORY_CLASS to 0 to actually do
// the work.  Good luck!
#define DEBUG_DIRECTORY_CLASS 0

//  --- Macros to set and reset our priv state and/or return
#define Set_Access_Priv()	\
	priv_state saved_priv;	\
	if ( want_priv_change ) \
		saved_priv = _set_priv(desired_priv_state,__FILE__,__LINE__,1);
#define return_and_resetpriv(i) \
	if ( want_priv_change ) 		\
		_set_priv(saved_priv,__FILE__,__LINE__,1);		\
	return i;
// -----------------------------------------------


#ifndef WIN32
static bool GetIds( const char *path, uid_t *owner, gid_t *group );
#endif

#ifdef WIN32
static bool recursive_chown_win32(const char * path, perm *po);
#endif

Directory::Directory( const char *name, priv_state priv ) 
{
	initialize( priv );

	curr_dir = strnewp(name);
	ASSERT(curr_dir);

#ifndef WIN32
	owner_ids_inited = false;
	if( priv == PRIV_FILE_OWNER ) {
		EXCEPT( "Internal error: "
		           "Directory instantiated with PRIV_FILE_OWNER" );
	}
#endif
}


Directory::Directory( StatInfo* info, priv_state priv ) 
{
	initialize( priv );

	curr_dir = strnewp( info->FullPath() );
	ASSERT(curr_dir);

#ifndef WIN32
	owner_uid = info->GetOwner();
	owner_gid = info->GetGroup();
	owner_ids_inited = true;
	if( priv == PRIV_FILE_OWNER ) {
		EXCEPT( "Internal error: "
		           "Directory instantiated with PRIV_FILE_OWNER" );
	}
#endif
}


void
Directory::initialize( priv_state priv )
{
	curr = NULL;

#ifdef WIN32
	dirp = -1;
#else 
	dirp = NULL;
#endif

	if( can_switch_ids() ) {
		desired_priv_state = priv;
		if( priv == PRIV_UNKNOWN ) {
			want_priv_change = false;
		} else {
			want_priv_change = true;
		}
	} else {
			// we can't switch, so treat everything as PRIV_CONDOR
			// (which is what we're stuck in anyway) and don't try
		desired_priv_state = PRIV_CONDOR;
		want_priv_change = false;
	}
}


Directory::~Directory()
{
	delete [] curr_dir;
	if( curr ) {
		delete curr;
	}

#ifndef WIN32
	// Unix
	if( dirp ) {
		(void)closedir( dirp );
	}
#else
	// Win32
	if( dirp != -1 ) {
		(void)_findclose(dirp);
	}
#endif
}

filesize_t
Directory::GetDirectorySize()
{
	const char* thefile = NULL;
	filesize_t dir_size = 0;

	Set_Access_Priv();

	Rewind();

	while ( (thefile=Next()) ) {
		if ( IsDirectory() && !IsSymlink() ) {
			// recursively traverse down directory tree
			Directory subdir( GetFullPath(), desired_priv_state );
			dir_size += subdir.GetDirectorySize();
		} else {
			dir_size += GetFileSize();
		}
	}

	return_and_resetpriv(dir_size);
}

bool
Directory::Find_Named_Entry( const char *name )
{
	const char* entry = NULL;
	bool ret_value = false;

	Set_Access_Priv();

	Rewind();

	while ( (entry = Next()) ) {
		if ( ! strcmp( entry, name ) ) {
			ret_value = true;
			break;
		}
	}

	// Done
	return_and_resetpriv( ret_value );
}

bool
Directory::Remove_Entire_Directory( void )
{
	const char* thefile = NULL;
	bool ret_value = true;

	Set_Access_Priv();

	Rewind();

	while ( (thefile=Next()) ) {
		if( ! Remove_Current_File() ) {
			ret_value = false;
		}
	}

	return_and_resetpriv(ret_value);
}

#if ! defined(WIN32)
bool
Directory::Recursive_Chown(uid_t src_uid, uid_t dst_uid, gid_t dst_gid,
		bool non_root_okay /*= true*/)
{
	return recursive_chown(GetDirectoryPath(),
		src_uid, dst_uid, dst_gid, non_root_okay);
}
#endif /* ! defined(WIN32) */


bool 
Directory::Remove_Entry( const char* name )
{
	MyString path;
	path = curr_dir;
	path += DIR_DELIM_CHAR;
	path += name;
	return do_remove( path.Value(), false );
}

bool
Directory::Remove_Full_Path( const char *path )
{
	return do_remove( path, false );
}

bool 
Directory::Remove_Current_File( void )
{
	if ( curr == NULL ) {
		// there is no current file; user probably did not call
		// Next() yet.
		return false;
	}
	return do_remove( curr->FullPath(), true );
} 


bool 
Directory::do_remove( const char* path, bool is_curr )
{
	bool is_dir = false;

		// NOTE: we do *NOT* call Set_Access_Priv() here, since we're
		// dealing with priv stuff in the two helper methods below.

	if( is_curr ) {
		is_dir = IsDirectory() && !IsSymlink();
	} else {
		StatInfo si( path );
		is_dir = si.IsDirectory() && ! si.IsSymlink();
	}

	if( is_dir ) {
		return do_remove_dir( path );
	}
	return do_remove_file( path );
}


bool 
Directory::do_remove_dir( const char* path )
{
		// the given path is a directory.  instead of messing with
		// recursion, worrying about stack overflows, ACLs, etc, we'll
		// just call upon the shell to do the dirty work.

		// First, try it as whatever priv state we've been requested
		// to use...
	rmdirAttempt( path, desired_priv_state );
	StatInfo si1( path );
	if( si1.Error() == SINoFile ) {
			// Great, the thing we tried to remove is now totally
			// gone.  we can safely return success now.  we don't have
			// to worry about priv states, since we haven't changed
			// ours at all yet.
		return true;
	}

#ifdef WIN32
		// on windows, we'll try to chown the directory so that
		// the current user will have Full access. This should 
		// enable us to delete everything.

	char *usr, *dom;

	usr = my_username();
	dom = my_domainname();

	Recursive_Chown(usr, dom);
	free(usr);
	free(dom);

	rmdirAttempt( path, desired_priv_state );

	StatInfo si2( path );

	if( si2.Error() == SINoFile ) {
		return true;
	} else {
	
		dprintf( D_ALWAYS, "ERROR: %s still exists after trying "
				 "to add Full control to ACLs for %s\n", path, 
				 priv_to_string(desired_priv_state) );
		return false;
	}

#else /* UNIX */

		// if we made it here, there's still hope. ;) if we can switch
		// uids, and we tried once as whatever the caller wanted and
		// that failed, it's very likely that we just tried as root
		// but we're on an NFS root-squashing directory, so we can try
		// again as the file owner...
		// NOTE: we'd like to have this StatInfo object on the stack,
		// but as soon as you instantiate it, it tries to stat(), and
		// we need to use it outside the scope of the if() clauses
		// where we're ready to stat from. :(
	StatInfo* si2 = NULL;
	if( want_priv_change ) {
		dprintf( D_FULLDEBUG, "Removing %s as %s failed, "
				 "trying again as file owner\n", path, 
				 priv_to_string(get_priv()) );
		rmdirAttempt( path, PRIV_FILE_OWNER );
		si2 = new StatInfo( path );
		if( si2->Error() == SINoFile ) {
				// Woo hoo, that was good enough, we're done.
			delete si2;
			return true;
		} else {
			dprintf( D_FULLDEBUG, "WARNING: %s still exists after "
					 "trying to remove it as the owner\n", path );
		}
	} else {
			// if we're not switching, we're going to need this
			// StatInfo object below, so create it here.
		si2 = new StatInfo( path );
	}

		// There's still a problem. :( It's possible if we tried as
		// root it failed b/c of NFS, and when we tried as the owner,
		// it failed b/c of weird permissions in the sandbox
		// (directories without owner write perms).  In this case, we
		// can recursively chmod() it and try again.

	Directory subdir( si2, desired_priv_state );
		// delete this now that we're done with it so we don't leak.
	delete si2;
	si2 = NULL;

	dprintf( D_FULLDEBUG, "Attempting to chmod(0700) %s and all subdirs\n",
			 path );
	if( ! subdir.chmodDirectories(0700) ) {
		dprintf( D_ALWAYS, "Failed to chmod(0700) %s and all subdirs\n",
				 path );
			// at this point, we're totally screwed, bail now.
		dprintf( D_ALWAYS, "Can't remove \"%s\" as %s, giving up!\n",
				 path, want_priv_change ? "directory owner" 
				 : priv_identifier(get_priv()) );
		return false;
	}
		// Cool, the chmod worked, try once more as the owner. 
	rmdirAttempt( path, PRIV_FILE_OWNER );
	StatInfo si3( path );
	if( si3.Error() == SINoFile ) {
			// Woo hoo, we're finally done!
		return true;
	}
	dprintf( D_ALWAYS, "After chmod(), still can't remove \"%s\" "
			 "as %s, giving up!\n", path, want_priv_change ? 
			 "directory owner" : priv_identifier(get_priv()) );
	return false;

#endif /* UNIX vs. WIN32 */

	EXCEPT( "Programmer error: Directory::do_remove_dir() didn't return" );
	return false;
}


bool 
Directory::do_remove_file( const char* path )
{
    bool ret_val = true;    // we'll set this to false if we fail
	Set_Access_Priv();

#if DEBUG_DIRECTORY_CLASS
	dprintf( D_ALWAYS, "Directory: about to unlink %s\n", path );
#else
	errno = 0;
	if ( unlink( path ) < 0 ) {
		ret_val = false;
		if( errno == EACCES ) {
				// Try again as the owner, in case we are going
				// across NFS since root access would get mapped
				// to nobody.  
				// If on NT, remove read-only access if exists.
#ifdef WIN32
				// Make file read/write on NT.
			_chmod( path, _S_IWRITE );
#else /* UNIX */
			if( want_priv_change && (desired_priv_state == PRIV_ROOT) ) {
				priv_state priv = setOwnerPriv( path );
				if( priv == PRIV_UNKNOWN ) {
					dprintf( D_ALWAYS, "Directory::do_remove_file(): "
							 "Failed to unlink(%s) as %s and can't find "
							 "file owner, giving up\n", path, 
							 priv_to_string(get_priv()) );
					return false;
				}
			}
#endif /* WIN32 vs. UNIX */
			if( unlink(path) < 0 ) {
				ret_val = false;
			} else {
				ret_val = true;
			}
				// we don't need to set our priv back to the desired
				// priv state right here, since we're about to return
				// and reset it to what it was originally...
		}	// end of if errno = EACCESS
	}	// end of if unlink() < 0
#endif  /* of if !DEBUG_DIRECTORY_CLASS */

	// if the file no longer exits, return success since the file is
	// gone anyway.
	if( ret_val == false &&  errno == ENOENT ) {
		ret_val = true;
	} 
	return_and_resetpriv(ret_val);
}


bool
Directory::rmdirAttempt( const char* path, priv_state priv )
{

	MyString rm_buf;
	const char* log_msg;
	priv_state saved_priv;
	int rval;

		/*
		  In this case, our job is a little more complicated than the
		  usual Set_Access_Priv() we use in the rest of this file.  If
		  the caller asked for PRIV_FILE_OWNER, we need to make sure
		  our owner priv is initialized properly for this particular
		  rmdir, which requires a call to setOwnerPriv(), not just the
		  usual set_priv().
		*/

	if( want_priv_change ) {
		switch( priv ) {

		case PRIV_UNKNOWN:
			log_msg = priv_identifier( get_priv() );
			break;

		case PRIV_FILE_OWNER:
#ifdef WIN32
			EXCEPT( "Programmer error: Directory::rmdirAttempt() called "
					"with PRIV_FILE_OWNER on WIN32!" );
#else
			saved_priv = setOwnerPriv( path );
			log_msg = priv_identifier( priv );
#endif
			break;

		case PRIV_USER:
		case PRIV_ROOT:
		case PRIV_CONDOR:
			saved_priv = set_priv( priv );
			log_msg = priv_identifier( priv );
			break;

		default:
			EXCEPT( "Programmer error: Directory::rmdirAttempt() called "
					"with unexpected priv_state (%d: %s)", priv,
					priv_to_string(priv) );
			break;
		}
	} else {
			// We not switching our priv, but we can grab the
			// identifier for whatever state we're in now.  if we
			// can't even switch privs, this will be PRIV_CONDOR, but
			// that's ok, because in that case, we initialize the
			// CondorUserName (and on UNIX, uid/gid) to be the current
			// user, so this will be accurate in all cases.
		log_msg = priv_identifier( get_priv() );
	}

		// Log what we're about to do
	dprintf( D_FULLDEBUG, "Attempting to remove %s as %s\n",
			 path, log_msg );

#ifdef WIN32
		rm_buf = "cmd.exe /s /c \"rmdir /s /q \"";
		rm_buf += path;
		rm_buf += "\"\"";
#else
		rm_buf = "/bin/rm -rf ";
		rm_buf += path;
#endif

		// Finally, do the work
#if DEBUG_DIRECTORY_CLASS
	dprintf( D_ALWAYS, "Directory: about to call \"%s\" as %s\n",
			 rm_buf.Value(), log_msg );
#elif defined( WIN32 )
		// we use system here instead of my_system since rmdir is a shell command
		rval = my_system(rm_buf.Value());
#else
		rval = my_spawnl( "/bin/rm", "/bin/rm", "-rf", path, NULL );
#endif

		// When all of that is done, switch back to our normal user
	if( want_priv_change ) {
		set_priv( saved_priv );
	}

	if( rval != 0 ) { 
		MyString errmsg;
		if( rval < 0 ) {
			errmsg = "my_spawnl returned ";
			errmsg += rval;
		} else {
			errmsg = "/bin/rm ";
			statusString( rval, errmsg );
		}
		dprintf( D_FULLDEBUG, "Removing \"%s\" as %s failed: %s\n", path, 
				 log_msg, errmsg.Value() );
		return false;
	}
	return true;
}


#ifndef WIN32

priv_state
Directory::setOwnerPriv( const char* path )
{
	uid_t	uid;
	gid_t	gid;
	bool is_root_dir = false;

	if( ! strcmp(path, curr_dir) ) {
		is_root_dir = true;
	}
	
	if( is_root_dir && owner_ids_inited ) {
		uid = owner_uid;
		gid = owner_gid;
	} else {
			// If we don't already know, figure out what user owns our
			// parent directory...
		if( ! GetIds( path, &uid, &gid ) ) {
			dprintf( D_ALWAYS, "Directory::setOwnerPriv() -- failed to "
					 "find owner of %s\n", path );
			return PRIV_UNKNOWN;
		}
		if( is_root_dir ) {
			owner_uid = uid;
			owner_gid = gid;
			owner_ids_inited = true;
		}
	}

		// !! Refuse to do anything as root !!
	if ( (0 == uid) || (0 == gid) ) {
		dprintf( D_ALWAYS, "Directory::setOwnerPriv(): NOT changing "
				 "priv state to owner of \"%s\" (%d.%d), that's root!\n",
				 path, (int)uid, (int)gid );
		return PRIV_UNKNOWN;
	}

		// Become the user who owns the directory
	uninit_file_owner_ids();
	set_file_owner_ids( uid, gid );
	return set_file_owner_priv();
}


bool
Directory::chmodDirectories( mode_t mode )
{
	const char* thefile = NULL;	
	int chmod_rval;
	bool rval = true;
	priv_state saved_priv;

		// NOTE: we don't want to just call Set_Access_Priv() here
		// since a) we *always* want to do the chmod() as the file
		// owner, regardless of our desired priv state, and b) since
		// if we do want to become the file owner, we have to
		// initialize the ids with the setOwnerPriv() call.  However,
		// since we're using the same variable name (saved_priv), we
		// can still use return_and_resetpriv() in here...
	if( want_priv_change ) {
		saved_priv = setOwnerPriv( GetDirectoryPath() );
		if( saved_priv == PRIV_UNKNOWN ) {
			dprintf( D_ALWAYS, "Directory::chmodDirectories(): "
					 "failed to find owner of \"%s\"\n",
					 GetDirectoryPath() );
				// if setOwnerPriv() returns PRIV_UNKNOWN, it didn't
				// touch our priv state so we can safely just return
				// directly, instead of using return_and_resetpriv()
			return false;
		}
	}

		// Log what we're about to do
	dprintf( D_FULLDEBUG, "Attempting to chmod %s as %s\n", 
			 GetDirectoryPath(), priv_identifier(get_priv()) );

		// First, change the mode on the parent directory itself.
	chmod_rval = chmod( GetDirectoryPath(), mode );

	if( chmod_rval < 0 ) {
		dprintf( D_ALWAYS, "chmod(%s) failed: %s (errno %d)\n",
				 GetDirectoryPath(), strerror(errno), errno );
		return_and_resetpriv( false );
	}

		// Now, iterate over everything in this directory.  If we find
		// any subdirectories that aren't symlinks, we'll instantiate
		// another Directory object and recursively call
		// chmodDirectories() on it. 
	Rewind();
	while( (thefile = Next()) ) {
		if( IsDirectory() && !IsSymlink() ) {
			Directory subdir( curr, desired_priv_state );
			if( ! subdir.chmodDirectories(mode) ) {
				rval = false;
			}
		}
	}
	return_and_resetpriv( rval );
}

#endif /* ! WIN32 */


bool
Directory::Rewind()
{
	if( curr ) {
		delete curr;
		curr = NULL;
	}

	Set_Access_Priv();

#ifndef WIN32
	// Unix
	if( dirp == NULL ) {
		errno = 0;
		dirp = opendir( curr_dir );
		if( dirp == NULL ) {
				// This could only mean that a) we've been given a bad
				// priv_state to try to go in or b) we're trying to do
				// things as root on a root-squashing NFS directory.
				// In either case, we can try again as the owner of
				// this directory.  If that works, we should save this
				// as our desired priv_state for future actions...
			if( ! want_priv_change ) {
				dprintf( D_ALWAYS, "Can't open directory \"%s\" as %s, "
						 "errno: %d (%s)\n", curr_dir, 
						 priv_to_string(get_priv()), errno, 
						 strerror(errno) );
				return_and_resetpriv(false);
			}

				// If we *DO* want to try priv-switching, try the owner
			priv_state old_priv = setOwnerPriv( curr_dir );
			if( old_priv == PRIV_UNKNOWN ) {
					dprintf( D_ALWAYS, "Directory::Rewind(): "
							 "failed to find owner of \"%s\"\n", curr_dir );
						// we can just set our priv back to what it was
						// before we called Set_Access_Priv()...
				return_and_resetpriv(false);
			}
				// if we made it here, setOwnerPriv() worked so we 
				// can try the opendir() again. 
			errno = 0;
			dirp = opendir( curr_dir );
			if( dirp == NULL ) {
				dprintf( D_ALWAYS, "Can't open directory \"%s\" as owner, "
						 "errno: %d (%s)", curr_dir, errno, strerror(errno) );
					// we can just set our priv back to what it was
					// before we called Set_Access_Priv()...
				return_and_resetpriv(false);
			}
		}
	}
	rewinddir( dirp );
#else
	// Win32
	if ( dirp != -1 ) {
		(void)_findclose(dirp);
	}
	dirp = -1;
#endif

	return_and_resetpriv(true);
}

const char *
Directory::Next()
{
	MyString path;
	bool done = false;
	Set_Access_Priv();
	if( curr ) {
		delete curr;
		curr = NULL;
	}

#ifndef WIN32
	if( dirp == NULL ) {
		Rewind();
	}
	// Unix
	struct dirent *dirent;
	while( ! done && dirp && (dirent = readdir(dirp)) ) {
		if( strcmp(".",dirent->d_name) == MATCH ) {
			continue;
		}
		if( strcmp("..",dirent->d_name) == MATCH ) {
			continue;
		}
		if ( dirent->d_name ) {
			path = curr_dir;
			path += DIR_DELIM_CHAR;
			path += dirent->d_name;
			curr = new StatInfo( path.Value() );
			switch( curr->Error() ) {
			case SINoFile:
					// This file was deleted, continue with the next file. 
				delete curr; 
				curr = NULL;
				break;
			case SIFailure:
					// do_stat failed with an error!
				dprintf( D_FULLDEBUG,
					 "Directory::stat() failed for \"%s\", errno: %d (%s)\n",
					 path.Value(), curr->Errno(), strerror(curr->Errno()) );
				delete curr;
				curr = NULL;
				break;
			default:
					// Everything's cool, we're done.
				done = true;
				break;
			}
		}
	}
#else 
	// Win32
	int result;
	path = curr_dir;
	path += "\\*.*";

	// do findfirst/findnext until we find a file which
	// is not "." or ".." or until there are no more files.
	do {
		if ( dirp == -1 ) {
			dirp = _findfirst(path.Value(),&filedata);
			result = dirp;
		} else {
			result = _findnext(dirp,&filedata);
		}
	} while ( (result != -1) && 
		( strcmp(filedata.name,".") == MATCH ||
		  strcmp(filedata.name,"..") == MATCH ) );
	if ( result != -1 ) {
		// findfirst/findnext succeeded
		curr = new StatInfo( curr_dir, filedata.name, 
		                     filedata.time_access,
		                     filedata.time_create,
		                     filedata.time_write, 
		                     filedata.size,
		                     ((filedata.attrib & _A_SUBDIR) != 0),
		                     false); 
	} else {
		curr = NULL;
	}
#endif /* WIN32 */

	if( curr ) {
		return_and_resetpriv( curr->BaseName() );		
	} else {
		return_and_resetpriv( NULL );
	}
}


bool
Directory::Recursive_Chown(const char *username, const char *domain) {
	return recursive_chown(GetDirectoryPath(), username, domain);
}


// The rest of the functions in this file are global functions and can
// be used with or without a Directory or StatInfo object.

#ifdef WIN32
bool 
recursive_chown(const char *path, const char *username, const char *domain) {
	// WINDOWS
	
	// On Windows, we really just want an ACL that gives us Full control.
	// In the event that we can't set such an ACL because the current ACL
	// denies us access, we will then change the owner of the file, which
	// gives us the ability to reset the ACLs on the file as well.

	perm po;

	if ( ! po.init(username, domain) ) {

		dprintf(D_ALWAYS, "recursive_chown() failed because the perm object "
				"failed to initialize.\n");
		return false;
	}
	
	return recursive_chown_win32(path, &po);

}

#else

bool 
recursive_chown(const char *, const char *, const char *) {
	// UNIX - needs implementation.

	return false;
}
#endif

#ifdef WIN32

// actual implementation that passes a pointer to the perm object
// instead of creating a new one with every recursive call.
bool 
recursive_chown_win32(const char * path, perm *po) {

	bool result = true;


	// perm::set_acls() adds the Full Control ACL

	// we chown the current path first, since we could potentially
	// be locked out of traversing deeper into the tree if we don't
	// have access.

	if ( ! po->set_acls(path) ) {

		// if we can't set the ACLs, try changing the owner.
		if ( ! po->set_owner(path) ) {
			dprintf(D_FULLDEBUG, "perm obj failed to set ACLs and change owner"
				   " on %s\n", path);
			result = false;
		} else {
			// ok, we reset the owner, so we should be able to
			// change the ACLs without any trouble now.
			if ( ! po->set_acls(path) ) {
				dprintf(D_FULLDEBUG, "perm obj failed to set ACLs on %s\n",
					   	path);
				result = false;
			}
		}
	}


	// even if setting the ACLs failed, let's just try to traverse
	// anyways...
	if( IsDirectory(path) ) {
		Directory dir(path);
		const char * filename = 0;
		while( (filename = dir.Next()) ) {
			filename = dir.GetFullPath();
			if( ! recursive_chown_win32(filename, po) ) {
				dprintf(D_FULLDEBUG, "recursive_chown() failed for %s; "
						"iteration stopping.\n", path);
				return false;
			}
		}

	}

	return result;
}

#endif

bool 
IsDirectory( const char *path )
{
	StatInfo si( path );
	switch( si.Error() ) {
	case SIGood:
		return si.IsDirectory();
		break;
	case SINoFile:
			// Silently return false
		return false;
		break;
	case SIFailure:
		dprintf( D_ALWAYS, "IsDirectory: Error in stat(%s), errno: %d\n", 
				 path, si.Errno() );
		return false;
		break;
	}

	EXCEPT("IsDirectory() unexpected error code"); // does not return
	return false;
}

bool 
IsSymlink( const char *path )
{
	StatInfo si( path );
	switch( si.Error() ) {
	case SIGood:
		return si.IsSymlink();
		break;
	case SINoFile:
			// Silently return false
		return false;
		break;
	case SIFailure:
		dprintf( D_ALWAYS, "IsSymlink: Error in stat(%s), errno: %d\n", 
				 path, si.Errno() );
		return false;
		break;
	}

	EXCEPT("IsSymlink() unexpected error code"); // does not return
	return false;
}


#ifndef WIN32
static bool 
GetIds( const char *path, uid_t *owner, gid_t *group )
{
	StatInfo si( path );
	switch( si.Error() ) {
	case SIGood:
		*owner = si.GetOwner( );
		*group = si.GetGroup( );
		return true;
		break;
	case SINoFile:
			// Silently return false
		return false;
		break;
	case SIFailure:
		dprintf( D_ALWAYS, "GetIds: Error in stat(%s), errno: %d (%s)\n", 
				 path, si.Errno(), strerror(si.Errno()) );
		return false;
		break;
	}

	EXCEPT("GetIds() unexpected error code"); // does not return
	return false;
}
#endif


/*
  Concatenates a given directory path and filename into a single
  string, stored in space allocated with new[].  This function makes
  sure that if the given directory path doesn't end with the
  appropriate directory delimiter for this platform, that the new
  string includes that.  Delete return string with delete[].
*/
char*
dircat( const char *dirpath, const char *filename )
{
	bool needs_delim = true;
	int extra = 2, dirlen = strlen(dirpath);
	char* rval;
	if( dirpath[dirlen - 1] == DIR_DELIM_CHAR ) {
		needs_delim = false;
		extra = 1;
	}
	rval = new char[ extra + dirlen + strlen(filename)];
	if( needs_delim ) {
		sprintf( rval, "%s%c%s", dirpath, DIR_DELIM_CHAR, filename );
	} else {
		sprintf( rval, "%s%s", dirpath, filename );
	}
	return rval;
}

/*
  Returns a path to subdirectory to use for temporary files.
  The pointer returned must be de-allocated by the caller w/ free().
*/
char*
temp_dir_path()
{
	char *prefix = param("TMP_DIR");
	if  (!prefix) {
		prefix = param("TEMP_DIR");
	}
	if (!prefix) {
#ifndef WIN32
		prefix = strdup("/tmp");
#else
			// try to get the temp dir, then try SPOOL,
			// then use the root directory
		char buf[MAX_PATH];
		int len;
		if ((len = GetTempPath(sizeof(buf), buf)) <= sizeof(buf)) {
			buf[len - 1] = '\0';
			prefix = strdup(buf);
		} else {
			dprintf(D_ALWAYS, "GetTempPath: buffer of size %d too small\n", sizeof(buf));

			prefix = param("SPOOL");
			if (!prefix) {
				prefix = strdup("\\");
			}
		}
#endif
	}
	return prefix;
}

/*
  Atomically creates a unique file or subdirectory in the temporary directory 
  (as returned by temp_dir_path())
  Returns the name of the new file or subdirectory.
  The pointer returned must be de-allocated by the caller w/ free().
*/

char * 
create_temp_file(bool create_as_subdirectory) {
	char * temp_dir = temp_dir_path();
	char * filename = (char*)(malloc (500));
	int mypid;
	static unsigned int counter = 0;

#ifdef WIN32
	mypid = GetCurrentProcessId();
#else
	mypid = getpid();
#endif

		//int pid = daemonCore->getPid();
	int timestamp = (int)time(NULL);
	int fd=-1;

	int retry_count = 10;

	do {
		snprintf (filename, 500, "%s/tmp.%d.%d.%d", temp_dir, mypid, timestamp++, counter++);
	} while ((--retry_count > 0) && 
			 ( (!create_as_subdirectory && (fd=safe_open_wrapper(filename, O_EXCL | O_CREAT, S_IREAD | S_IWRITE)) == -1) ||
			   (create_as_subdirectory && (fd=mkdir(filename, 0700)) == -1) )
			 );

	if (fd == -1) {
		free(temp_dir);
		free(filename);
		return NULL;
	}

	if ( !create_as_subdirectory ) {
		close (fd);
	}

	free (temp_dir);

	return filename;
}



#if ! defined(WIN32)

static bool recursive_chown_impl(const char * path, 
	uid_t src_uid, uid_t dst_uid, gid_t dst_gid);

/** Helper function. See recursive_chown.

This implements "fast" chowning of a file or directory.
That is, it assumes that it has permission and just uses "chown()"
*/
static bool recursive_chown_impl_fast(const char * path,
	uid_t src_uid, uid_t dst_uid, gid_t dst_gid)
{
	StatInfo si(path);
	if(si.Error() != SIGood) {
		if(si.Error() == SINoFile) {
			dprintf(D_FULLDEBUG, "Attempting to chown '%s', "
				"but it doesn't appear to exist.\n", path); 
		} else {
			dprintf(D_ALWAYS, "Attempting to chown '%s', "
				"but encountered an error inspecting it (errno %d)\n",
				path, si.Errno()); 
		}
		return false;
	}

	uid_t real_uid = si.GetOwner();
	if(real_uid != src_uid && real_uid != dst_uid) {
		dprintf(D_ALWAYS, "Attempting to chown '%s' from %d to %d.%d,"
			" but the path was unexpectedly owned by %d\n", 
			path, (int)src_uid, (int)dst_uid, (int)dst_gid, 
			(int)real_uid);
		return false;
	}

	if( IsDirectory(path) ) {
		Directory dir(path);
		const char * filename = 0;
		while( (filename = dir.Next()) ) {
				// Directory skips . and .. for us.  Keen.
			//if(strcmp(filename, ".") == 0) { continue; }
			//if(strcmp(filename, "..") == 0) { continue; }
			filename = dir.GetFullPath();
			if( ! recursive_chown_impl(filename, src_uid,
					dst_uid, dst_gid) ) {
				return false;
			}
		}
	}

	// We specifically do the path itself last.  This way
	// we can take the optional risky optimization: if the
	// root level is already correctly owned, skip it.  In this
	// case we'll do the right thing if we redo the chown
	// after crashing mid-way through.
	if( chown(path, dst_uid, dst_gid) != 0 ) {
		return false;
	}
	return true;
}


/// Helper function. See recursive_chown.
static bool recursive_chown_impl(const char * path, 
	uid_t src_uid, uid_t dst_uid, gid_t dst_gid)
{
	ASSERT(get_priv() == PRIV_ROOT);
	
	if( recursive_chown_impl_fast(path, src_uid, dst_uid, dst_gid) ) {
		return true;
	}
	// No luck with the fast (chown()-based) implementation.  Try
	// the slow (copy-based) implementation
	/* // TODO
		// Disabled.  The slow implemention opened all sorts of
		// hairy issues, especially regarding recovery
	if( recursive_chown_impl_slow(const char * path, uid_t uid, gid_t gid) ) {
		return true;
	}
	*/

	// Still no luck.
	dprintf(D_FULLDEBUG, "Error: Unable to chown '%s' from %d to %d.%d\n",
		path, (int)src_uid, (int)dst_uid, (int)dst_gid);
	return false;
}

/* TODO?: Optional speed hack: If the caller turns this option one,
pass it to recursive_chown_impl.  recursive_chown_impl will check
the existing ownership of the file.  If it's already correct, it will
immediately return.
*/
bool recursive_chown(const char * path,
	uid_t src_uid, uid_t dst_uid, gid_t dst_gid, 
	bool non_root_okay /* = true */)
{
	// Quick check if the chown is even possible
	if( ! can_switch_ids()) {

		// We're not root, we're doomed.
		if( non_root_okay) {
			// But it's okay!
			dprintf(D_FULLDEBUG, "Unable to chown %s from %d to %d.%d.  Process lacks the ability to change UIDs (probably isn't root).  This is probably harmless.  Skipping chown attempt.\n", path, src_uid, dst_uid, dst_gid);
			return true;

		} else {
			// Oops, we expected to be root.
			dprintf(D_ALWAYS, "Error: Unable to chown %s to from %d %d.%d; we're not root.\n", path, src_uid, dst_uid, dst_gid);
			return false;

		}
	}

	// Lower levels of the implementation assume we're
	// root, which is reasonable since we have no hope of doing
	// a chown if we're not root.
	priv_state previous = set_priv(PRIV_ROOT);
	bool ret = recursive_chown_impl(path,src_uid,dst_uid,dst_gid);
	set_priv(previous);
	return ret;
}

#endif /* ! defined(WIN32) */
