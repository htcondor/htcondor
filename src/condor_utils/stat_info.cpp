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
#include "condor_debug.h"
#include "stat_info.h"
#include "status_string.h"
#include "condor_config.h"
#include "perm.h"
#include "my_username.h"
#include "my_popen.h"
#include "directory.h"

StatInfo::StatInfo( const char *path )
{
	char *s, *last = NULL, *trail_slash = NULL, chslash;
	fullpath = path ? strdup( path ) : NULL;
	dirpath = path ? strdup( path ) : NULL;

		// Since we've got our own copy of the full path now sitting
		// in dirpath, we can find the last directory delimiter, make
		// a copy of whatever is beyond it as the filename, and put a
		// NULL in the first character after the delim character so
		// that the dirpath always contains the directory delim.
	for (s = dirpath; s && *s != '\0'; s++ ) {
		if (
#ifdef WIN32
			*s == '\\' ||
#endif
			*s == '/' ) {
				last = s;
		}
	}
	if( last != NULL && last[1] ) {
		filename = strdup( &last[1] );
		last[1] = '\0';
	} else {
		filename = NULL;
		if (last != NULL) {
			// we only get here if the input path ended with a dir separator
			// we can't stat that on windows, and *nix does't care, so we remove it.
			trail_slash = &fullpath[last - dirpath];
		}
	}
	// remove trailing slash before we stat, and then put it back after. this fixes #4747
	// why do we put it back?  because things crash if we don't and this is a stable series fix.
	if (trail_slash) { chslash = *trail_slash; *trail_slash = 0; }
	stat_file( fullpath );
	if (trail_slash) { *trail_slash = chslash; }
}

StatInfo::StatInfo( const char *param_dirpath,
					const char *param_filename )
{
	this->filename = strdup( param_filename );
	this->dirpath = make_dirpath( param_dirpath );
	std::string buf;
	dircat( param_dirpath, param_filename, buf );
	fullpath = strdup( buf.c_str() );
	stat_file( fullpath );
}

StatInfo::StatInfo( int fd )
{
	filename = NULL;
	fullpath = NULL;
	dirpath = NULL;

	stat_file( fd );
}

#ifdef WIN32
StatInfo::StatInfo( const char* dirpath, const char* filename,
					time_t time_access, time_t time_create,
					time_t time_modify, filesize_t fsize,
					bool is_dir, bool is_symlink )
{
	this->dirpath = strdup( dirpath );
	this->filename = strdup( filename );
	std::string buf;
	dircat( dirpath, filename, buf);
	fullpath = strdup( buf.c_str() );
	si_error = SIGood;
	si_errno = 0;
	access_time = time_access;
	modify_time = time_modify;
	create_time = time_create;
	valid = false;
	file_size = fsize;
	m_isDirectory = is_dir;
	m_isSymlink = is_symlink;
}
#endif /* WIN32 */


StatInfo::~StatInfo( void )
{
	if ( filename ) free( filename );
	if ( dirpath )  free( dirpath );
	if ( fullpath ) free( fullpath );
}


/*
  UNIX note:
  We want to stat the given file. We may be operating as root, but
  root == nobody across most NFS file systems, so we may have to do it
  as condor.  If we succeed, we proceed, if the file has already been
  removed we handle it.  If we cannot do it as either root or condor,
  we report an error.
*/
void
StatInfo::stat_file( const char *path )
{
	bool do_stat = true;
	bool saw_symlink = false;

		// Initialize
	init( );

		// Ok, run stat
	struct stat statbuf;
	int status = 0;

# if (! defined WIN32)
	// Start with an lstat() on unix
	status = lstat( path, &statbuf );
	if ( status == 0 ) {
		saw_symlink = S_ISLNK( statbuf.st_mode );
	}
	// If lstat() resulted in a symlink, then we need to
	// follow up with a stat().
	do_stat = saw_symlink;
# endif

	if ( do_stat ) {
		status = stat( path, &statbuf );
	}

		// How'd it go?
	if ( status ) {
		si_errno = errno;

# if (! defined WIN32 )
		if ( EACCES == si_errno ) {
				// permission denied, try as condor
			priv_state priv = set_condor_priv();

			do_stat = true;
			// If our previous lstat() revealed a symlink, then we
			// can skip straight to a stat() with our new priv-state.
			if ( !saw_symlink ) {
				status = lstat( path, &statbuf );
				if ( status == 0 ) {
					saw_symlink = S_ISLNK( statbuf.st_mode );
				}
				do_stat = saw_symlink;
			}
			if ( do_stat ) {
				status = stat( path, &statbuf );
			}

			if( status < 0 ) {
				si_errno = errno;
			}

			set_priv( priv );
		}
# endif
	}

		// If we've failed, just bail out
	if ( status ) {
		if (( ENOENT == si_errno ) || (EBADF == si_errno) ) {
			si_error = SINoFile;
		} else {
			dprintf( D_FULLDEBUG,
					 "StatInfo::stat(%s) failed, errno: %d = %s\n",
					 path,si_errno,strerror(si_errno) );
		}
		return;
	}

	init( &statbuf );

	m_isSymlink = saw_symlink;
}
void
StatInfo::stat_file( int fd )
{
		// Initialize
	init( );

		// Ok, run stat
	struct stat statbuf;
	int status = fstat( fd, &statbuf );


		// How'd it go?
	if ( status ) {
		si_errno = errno;

# if (! defined WIN32 )
		if ( EACCES == si_errno ) {
				// permission denied, try as condor
			priv_state priv = set_condor_priv();
			status = fstat(fd, &statbuf);
			if( status < 0 ) {
				si_errno = errno;
			}
			set_priv( priv );
		}
# endif
	}

		// If we've failed, just bail out
	if ( status ) {
		if (( ENOENT == si_errno ) || (EBADF == si_errno) ) {
			si_error = SINoFile;
		} else {
			dprintf( D_FULLDEBUG,
					 "StatInfo::stat(fd=%d) failed, errno: %d = %s\n",
					 fd, si_errno, strerror(si_errno) );
		}
		return;
	}

	init( &statbuf );
}

void
StatInfo::init( struct stat *sb )
{
		// Initialize
	if ( NULL == sb )
	{
		si_error = SIFailure;
		access_time = 0;
		modify_time = 0;
		create_time = 0;
		file_size = 0;
		m_isDirectory = false;
		m_isExecutable = false;
		m_isSymlink = false;
		m_isDomainSocket = false;
		valid = false;
	}
	else
	{
		// the do_stat succeeded
		si_error = SIGood;
		access_time = sb->st_atime;
		create_time = sb->st_ctime;
		modify_time = sb->st_mtime;
		file_size = sb->st_size;
		file_mode = sb->st_mode;
		valid = true;
# if (! defined WIN32)
		m_isDirectory = S_ISDIR(sb->st_mode);
		// On Unix, if any execute bit is set (user, group, other), we
		// consider it to be executable.
		m_isExecutable = ((sb->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) != 0 );
		m_isSymlink = S_ISLNK(sb->st_mode);
		m_isDomainSocket = (S_ISSOCK( sb->st_mode ) == 1);
		owner = sb->st_uid;
		group = sb->st_gid;
# else
		m_isDirectory = ((_S_IFDIR & sb->st_mode) != 0);
		m_isExecutable = ((_S_IEXEC & sb->st_mode) != 0);
# endif
	}
}


char*
StatInfo::make_dirpath( const char* dir )
{
	ASSERT(dir);

	char* rval;
	size_t rvalsz;
	int dirlen = (int)strlen(dir);
	if( dir[dirlen - 1] == DIR_DELIM_CHAR ) {
			// We've already got the delim, just return a copy of what
			// we were passed in:
		rvalsz = dirlen + 1;
		rval = (char *)malloc(rvalsz);
		snprintf( rval, rvalsz, "%s", dir );
	} else {
			// We need to include the delim character.
		rvalsz = dirlen + 2;
		rval = (char *)malloc(rvalsz);
		snprintf( rval, rvalsz, "%s%c", dir, DIR_DELIM_CHAR );
	}
	return rval;
}


mode_t
StatInfo::GetMode( void )
{
	if(!valid) {
		stat_file( fullpath );
	}
	if(!valid) {
		EXCEPT("Avoiding a use of an undefined mode");
	}

	return file_mode;	
}

#ifndef WIN32
uid_t
StatInfo::GetOwner( void ) const
{
	// This is defensive programming, but it's better than returning an
	// undefined value.
	if(!valid) {
		EXCEPT("Avoiding a use of an undefined uid");
	}

	return owner;
}

gid_t
StatInfo::GetGroup( void ) const
{
	// This is defensive programming, but it's better than returning an
	// undefined value.
	if(!valid) {
		EXCEPT("Avoiding a use of an undefined gid");
	}

	return group;
}
#endif
