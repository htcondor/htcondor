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
#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_debug.h"
#include "directory.h"
#include "condor_string.h"

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


StatInfo::StatInfo( const char *path )
{
	char *s, *last = NULL;
	fullpath = strnewp( path );
	dirpath = strnewp( path );

		// Since we've got our own copy of the full path now sitting
		// in dirpath, we can find the last directory delimiter, make
		// a copy of whatever is beyond it as the filename, and put a
		// NULL in the first character after the delim character so
		// that the dirpath always contains the directory delim.
	for( s = dirpath; s && *s != '\0'; s++ ) {
        if( *s == '\\' || *s == '/' ) {
			last = s;
        }
    }
	if( last[1] ) {
		filename = strnewp( &last[1] ); 
		last[1] = '\0';
	} else {
		filename = NULL;
	}
	do_stat( (const char*)fullpath );
}


StatInfo::StatInfo( const char *dirpath, const char *filename )
{
	this->filename = strnewp( filename );
	this->dirpath = make_dirpath( dirpath );
	fullpath = dircat( dirpath, filename );
	do_stat( (const char*)fullpath );
}


#ifdef WIN32
StatInfo::StatInfo( const char* dirpath, const char* filename, 
					time_t time_access, time_t time_create, 
					time_t time_modify, unsigned long fsize, bool is_dir )
{
	this->dirpath = strnewp( dirpath );
	this->filename = strnewp( filename );
	fullpath = dircat( dirpath, filename );
	si_error = SIGood;
	si_errno = 0;
	access_time = time_access;
	modify_time = time_modify;
	create_time = time_create;
	file_size = fsize;
	isdirectory = is_dir;
}
#endif /* WIN32 */


StatInfo::~StatInfo()
{
	if( filename ) {
		delete [] filename;
	}
	if( dirpath ) {
		delete [] dirpath;
	}
	if( fullpath ) {
		delete [] fullpath;
	}
}

void
StatInfo::do_stat( const char *path )
{
		// Initialize
	si_error = SIFailure;
	access_time = 0;
	modify_time = 0;
	create_time = 0;
	isdirectory = false;

	struct stat statbuf;	

#ifndef WIN32
	int stat_status = unix_do_stat( path, &statbuf );
#else
	int stat_status = nt_do_stat( path, &statbuf );
#endif /* WIN32 */

	switch( stat_status ) {
	case FALSE:
		si_error = SINoFile;
		break;
	case TRUE:
			// the do_stat succeeded
		si_error = SIGood;
		access_time = statbuf.st_atime;
		create_time = statbuf.st_ctime;
		modify_time = statbuf.st_mtime;
		file_size = statbuf.st_size;
#ifndef WIN32
		isdirectory = S_ISDIR(statbuf.st_mode);
#else
		isdirectory = ((_S_IFDIR & statbuf.st_mode) != 0);
#endif
		break;
	default:
			// Failure
		break;
	}
}


#ifndef WIN32
/*
  We want to stat the given file. We may be operating as root, but root
  == nobody across most NFS file systems, so we may have to do it as
  condor.  If we succeed, we return TRUE, and if the file has already
  been removed we return FALSE.  If we cannot do it as either root
  or condor, we return -1.
*/
int
StatInfo::unix_do_stat( const char *path, struct stat *buf )
{
	priv_state priv;

	errno = 0;
	if( stat( path, buf ) < 0 ) {
		si_errno = errno;
		switch( errno ) {
		case ENOENT:	// got rm'd while we were doing this
			return FALSE;
		case EACCES:	// permission denied, try as condor
			priv = set_condor_priv();
			if( stat( path, buf ) < 0 ) {
				si_errno = errno;
				set_priv( priv );
				if ( errno == ENOENT ) {
					return FALSE;	// got rm'd while we were doing this
				}
				dprintf( D_ALWAYS, 
						 "StatInfo::do_stat(%s,0x%x) failed, errno: %d\n",
						 path, &buf, errno );
				return -1;
			}
			set_priv( priv );
		}
	}
	return TRUE;
}
#else /* WIN32 */
/*
  We want to stat the given file.  Since we're NT here, there's no
  need to mess w/ priv_state stuff, (yet?).  If we succeed, we return
  TRUE, and if the file has already been removed we return FALSE.  If
  we failed for some other reason, we return -1.
*/
int
StatInfo::nt_do_stat( const char *path, struct stat *buf )
{
	if( stat( path, buf ) < 0 ) {
		si_errno = errno;
		switch( errno ) {
		case ENOENT:	// got rm'd while we were doing this
			return FALSE;
		default:
			dprintf( D_ALWAYS, 
					 "StatInfo::do_stat(%s,0x%x) failed, errno: %d\n",
					 path, &buf, errno );
			return -1;
		}
	}
	return TRUE;
}
#endif /* WIN32 */


char*
StatInfo::make_dirpath( const char* dir ) 
{
	char* rval;
	int dirlen = strlen(dir);
	if( dir[dirlen - 1] == DIR_DELIM_CHAR ) {
			// We've already got the delim, just return a copy of what
			// we were passed in:
		rval = new char[dirlen + 1];
		sprintf( rval, "%s", dir );
	} else {
			// We need to include the delim character.
		rval = new char[dirlen + 2];
		sprintf( rval, "%s%c", dir, DIR_DELIM_CHAR );
	}
	return rval;
}


Directory::Directory( const char *name, priv_state priv ) 
{
	curr = NULL;

#ifndef WIN32
	// Unix
	errno = 0;
	dirp = opendir( name );
	if( dirp == NULL ) {
		EXCEPT( "Can't open directory \"%s\", errno: %d", name, errno );
	}
	rewinddir( dirp );
#else
	dirp = -1;
#endif
	
	curr_dir = strnewp(name);
	ASSERT(curr_dir);

	desired_priv_state = priv;
	if ( priv == PRIV_UNKNOWN ) {
		want_priv_change = false;
	} else {
		want_priv_change = true;
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
	(void)closedir( dirp );
#else
	// Win32
	if ( dirp ) {
		(void)_findclose(dirp);
	}
#endif
}

unsigned long
Directory::GetDirectorySize()
{
	const char* thefile = NULL;
	unsigned long dir_size = 0;

	Set_Access_Priv();

	Rewind();

	while ( (thefile=Next()) ) {
		if ( IsDirectory() ) {
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
Directory::Remove_Entire_Directory()
{
	const char* thefile = NULL;
	bool ret_value = true;

	Set_Access_Priv();

	Rewind();

	while ( (thefile=Next()) ) {
		if ( Remove_Current_File() == false ) {
			ret_value = false;
		}
	}

	return_and_resetpriv(ret_value);
}


bool 
Directory::Remove_File( const char* path )
{
	return do_remove( path, false );
}


bool 
Directory::Remove_Current_File()
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
	char buf[_POSIX_PATH_MAX];
	bool ret_val = true;
	bool is_dir = false;
	priv_state priv;

	Set_Access_Priv();

	if( is_curr ) {
		is_dir = IsDirectory();
	} else {
		is_dir = ::IsDirectory( path );
	}

	if( is_dir ) {
		// the current file is a directory.
		// instead of messing with recursion, worrying about
		// stack overflows, ACLs, etc, we'll just call upon
		// the shell to do the dirty work.
		// TODO: we should really look return val from system()!
#ifdef WIN32
		sprintf( buf,"rmdir /s /q \"%s\"", path );
#else
		sprintf( buf,"/bin/rm -rf %s", path );
#endif

#if DEBUG_DIRECTORY_CLASS
		dprintf(D_ALWAYS,"Directory: about to call %s\n",buf);
#else
		system( buf );
#endif

		// for good measure, repeat the above operation a second
		// time as user Condor.  We do this because if we currently
		// have root priv, and we are accessing files across NFS, we
		// will get re-mapped to user "nobody" by any NFS daemon worth
		// its salt.  
		if ( want_priv_change && (desired_priv_state == PRIV_ROOT) ) {
			priv = set_condor_priv(); 

#if DEBUG_DIRECTORY_CLASS
			dprintf(D_ALWAYS,
				"Directory: with condor priv about to call %s\n",buf);
#else
			system( buf );
#endif

			set_priv(priv);
		}
	} else {
		// the current file is not a directory, just a file	

#if DEBUG_DIRECTORY_CLASS
		dprintf(D_ALWAYS,"Directory: about to unlink %s\n",buf);
#else
		errno = 0;
		if ( unlink( path ) < 0 ) {
			ret_val = false;
			if( errno == EACCES ) {
				// Try again as Condor, in case we are going
				// across NFS since root access would get mapped
				// to nobody.  If on NT, remove
				// read-only access if exists.
#ifdef WIN32
				// Make file read/write on NT.
				_chmod( path ,_S_IWRITE );
#endif
				if ( want_priv_change && (desired_priv_state == PRIV_ROOT) ) {
					priv = set_condor_priv(); 
				}
				
				if ( unlink( path ) < 0 ) {
					ret_val = false;
				} else {
					ret_val = true;
				}

				if ( want_priv_change && (desired_priv_state == PRIV_ROOT) ) {
					set_priv(priv);
				}
			}	// end of if errno = EACCESS
		}	// end of if unlink() < 0

#endif  // of if !DEBUG_DIRECTORY_CLASS
	}		// end of if file is a regular file, not a directory

	// if the file simple no longer exits, return success
	// since the file is gone anyway.
	if( ret_val == false &&  errno == ENOENT ) {
		ret_val = true;
	} 

	return_and_resetpriv(ret_val);
}

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
	char path[_POSIX_PATH_MAX];
	bool done = false;
	Set_Access_Priv();
	if( curr ) {
		delete curr;
		curr = NULL;
	}

#ifndef WIN32
	// Unix
	struct dirent *dirent;
	while( ! done && (dirent = readdir(dirp)) ) {
		if( strcmp(".",dirent->d_name) == MATCH ) {
			continue;
		}
		if( strcmp("..",dirent->d_name) == MATCH ) {
			continue;
		}
		if ( dirent->d_name ) {
			sprintf( path, "%s/%s", curr_dir, dirent->d_name );
			curr = new StatInfo( path );
			switch( curr->Error() ) {
			case SINoFile:
					// This file was deleted, continue with the next file. 
				delete curr; 
				curr = NULL;
				break;
			case SIFailure:
					// do_stat failed with an error!
				dprintf( D_ALWAYS,
						 "Directory:: stat() failed for file %s, errno: %d\n",
						 path, curr->Errno() );
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
	sprintf(path,"%s\\*.*",curr_dir);
	// do findfirst/findnext until we find a file which
	// is not "." or ".." or until there are no more files.
	do {
		if ( dirp == -1 ) {
			dirp = _findfirst(path,&filedata);
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
							 ((filedata.attrib & _A_SUBDIR) != 0) ); 
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


// The rest of the functions in this file are global functions and can
// be used with or without a Directory or StatInfo object.

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


/*
  Concatenates a given directory path and filename into a single
  string, stored in space allocated with new().  This function makes
  sure that if the given directory path doesn't end with the
  appropriate directory delimiter for this platform, that the new
  string includes that.
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


