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

// Declare space for static data members
#ifdef WIN32
struct _finddata_t Directory::filedata;
#endif

//  --- A macro to reset our priv state and return
#define return_and_resetpriv(i) \
	if ( want_priv_change ) 		\
		set_priv(saved_priv);		\
	return i;
// -----------------------------------------------


Directory::Directory( const char *name, priv_state priv ) 
{
	curr_filename = NULL;
	dirp = -1;

#ifndef WIN32
	// Unix
	dirp = opendir( name );
	if( dirp == NULL ) {
		EXCEPT( "Can't open directory \"%s\"", name );
	}
#endif
	
	curr_dir = strnewp(name);
	ASSERT(curr_dir);

	if ( priv == PRIV_UNKNOWN ) {
		want_priv_change = false;
	} else {
		want_priv_change = true;
		desired_priv_state = priv;
	}
}

Directory::~Directory()
{
	delete [] curr_dir;

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

inline void
Directory::Set_Access_Priv()
{
	if ( want_priv_change ) {
		saved_priv = set_priv(desired_priv_state);
	}
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
Directory::Remove_Current_File()
{
	char buf[_POSIX_PATH_MAX];
	bool ret_val = true;
	priv_state priv;

	if ( curr_filename == NULL ) {
		// there is not current file; user probably did not call
		// Next() yet.
		return false;
	}

	Set_Access_Priv();

	if ( IsDirectory() ) {
		// the current file is a directory.
		// instead of messing with recursion, worrying about
		// stack overflows, ACLs, etc, we'll just call upon
		// the shell to do the dirty work.
		// TODO: we should really look return val from system()!
#ifdef WIN32
		sprintf(buf,"rmdir /s /q %s\\%s",curr_dir,curr_filename);
#else
		sprintf(buf,"/bin/rm -rf %s/%s",curr_dir,curr_filename);
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
		sprintf(buf,"%s%c%s",curr_dir,DIR_DELIM_CHAR,curr_filename);
#if DEBUG_DIRECTORY_CLASS
		dprintf(D_ALWAYS,"Directory: about to unlink %s\n",buf);
#else
		errno = 0;
		if ( unlink(buf) < 0 ) {
			ret_val = false;
			if( errno == EACCES ) {
				// Try again as Condor, in case we are going
				// across NFS since root access would get mapped
				// to nobody.  If on NT, remove
				// read-only access if exists.
#ifdef WIN32
				// Make file read/write on NT.
				_chmod(buf,_S_IWRITE);
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
	curr_filename = NULL;

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
	char buf[_POSIX_PATH_MAX];

	Set_Access_Priv();
	curr_filename = NULL;

#ifndef WIN32
	// Unix
	int stat_status;
	struct dirent *dirent;
	struct stat statbuf;	
	while( dirent = readdir(dirp) ) {
		if( strcmp(".",dirent->d_name) == MATCH ) {
			continue;
		}
		if( strcmp("..",dirent->d_name) == MATCH ) {
			continue;
		}
		curr_filename =  dirent->d_name;
		if ( curr_filename ) {
			sprintf(buf,"%s/%s",curr_dir,curr_filename);
			stat_status = do_stat(buf,&statbuf);
			if ( stat_status == FALSE ) {
				// do_stat thinks the file disappeared; it was 
				// probably deleted. just go on to the next file.
				continue;
			}
			if ( stat_status < 0 ) {
				// do_stat failed with an error!
				dprintf(D_ALWAYS,"Directory:: stat() failed for file %s",buf);
				access_time = modify_time = create_time = 0;
				curr_filename = NULL;
			} else {
				// do_stat succeeded
				access_time = statbuf.st_atime;
				create_time = statbuf.st_ctime;
				modify_time = statbuf.st_mtime;
				curr_isdirectory = S_ISDIR(statbuf.st_mode);
			}
		}
		break;
	}
#else 
	// Win32
	int result;
	sprintf(buf,"%s\\*.*",curr_dir);
	// do findfirst/findnext until we find a file which
	// is not "." or ".." or until there are no more files.
	do {
		if ( dirp == -1 ) {
			dirp = _findfirst(buf,&filedata);
			result = dirp;
		} else {
			result = _findnext(dirp,&filedata);
		}
	} while ( (result != -1) && 
		( strcmp(filedata.name,".") == MATCH ||
		  strcmp(filedata.name,"..") == MATCH ) );
	if ( result != -1 ) {
		// findfirst/findnext succeeded
		curr_filename = filedata.name;
		access_time = filedata.time_access;
		create_time = filedata.time_create;
		modify_time = filedata.time_write;
		curr_isdirectory = (filedata.attrib & _A_SUBDIR) != 0;
	} else {
		access_time = modify_time = create_time = 0;
		curr_filename = NULL;
	}
#endif

	return_and_resetpriv( curr_filename );		
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
do_stat( const char *path, struct stat *buf )
{
	priv_state priv;

	if( stat(path,buf) < 0 ) {
		switch( errno ) {
		  case ENOENT:	// got rm'd while we were doing this
			return FALSE;
		  case EACCES:	// permission denied, try as condor
			priv = set_condor_priv();
			if( stat(path,buf) < 0 ) {
				set_priv(priv);
				if ( errno == ENOENT ) {
					return FALSE;	// got rm'd while we were doing this
				}
				dprintf(D_ALWAYS,"Directory::do_stat(%s,0x%x) failed",
					path, &buf );
				return -1;
			}
			set_priv(priv);
		}
	}
	return TRUE;
}
#endif
