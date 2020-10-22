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

#ifndef STAT_INFO_H
#define STAT_INFO_H

#include "condor_common.h"
#include "condor_sys_types.h"
#include "condor_uid.h"

#ifndef WIN32
#	include <dirent.h>
#endif

// DON'T include "stat_wrapper.h"; just pre-declare the StatWrapper
// class below.  This way, we avoid the possible problems of including
// all of the system includes into places where we don't want them.
class StatWrapper;	// See comment above

enum si_error_t { SIGood = 0, SINoFile, SIFailure };

/** Class to store information when you stat a file on either Unix or
    NT.  This class is used by the Directory class defined below.
	@see Directory
*/
class StatInfo
{
public:

	/** Constructor.  This performs the stat() (or equivalent) and
	    sets the _error field to the appropriate value, which can be
	    viewed eith the Error() method.
		@param path The full path to the file to get info about.
		@see Error()
	*/
	StatInfo( const char *path );

	/** Alternate Constructor.  This does the same thing as the other
  	    constructor, except the file is specified with a directory
		path and a filename, instead of one big full path.  We do this
		because we really want these seperate anyway, and if we get it
		that way in the first place, it saves us some trouble.
		@param dirpath The directory path to the file.
		@param filename The filename (without the directory).
		@see Error()
	*/
	StatInfo( const char *dirpath,
			  const char *filename );

	/** Alternate Constructor.  This does the same thing as the other
  	    constructor, except that a file descriptor is passed instead
		of a file path.
		@param fd The file descriptor
		@see Error()
	*/
	StatInfo( int fd );

#ifdef WIN32
	/** NT's other constructor.  This just stores the given values
	    into the object, so they can be retrieved later.  We do this
		because in the Directory object, the Next() function uses
		findfirst() and findnext() to get the next file in a
		directory, and when we do, we have all the info we need about
		the file already, so to be efficient, we just store it
		directly with this form of the constructor.
		@param dirpath The directory path to the file.
		@param filename The filename (without the directory).
		@param time_access The access time of the file.
		@param time_create The creation time of the file.
		@param time_modify The modification time of the file.
		@param is_dir Is this file a directory?
	*/
	StatInfo( const char* dirpath, const char* filename, time_t
			  time_access,  time_t time_create, time_t time_modify,
			  filesize_t fsize, bool is_dir, bool is_symlink );  
#endif

	/// Destructor<p>
	~StatInfo( void );

	/** Shows the possible error condition of this StatInfo object.
	    If the appropriate stat() call failed when creating this
		object, its return value is returned here.  Note, the value of
		this enum is 0 if everything was ok, so you can treat a call
		to Error() as a bool, with success as 0 and failure as
		non-zero. 
		@return An enum describing the error condition of this StatInfo object.
	*/
	si_error_t Error() { return si_error; };

	/** This function returns the errno as set from the attempt to get
	    information about this file.  If there was no error, this will
		return 0. Note that this value is only valid when Error() returns
		SINoFile or SIFailure.
		@return The errno from getting info for this StatInfo object.
	*/
	int Errno() const { return si_errno; };

	/** Get the full path to the file.
		@return A string containing the full path of this file.
		Do not free or delete this pointer.
		<b>Warning:</b> This pointer is meaningless when this StatInfo object is deleted.
	*/
	const char* FullPath() { return (const char*)fullpath; };

	/** Get just the 'basename' for the file: its name without the
	    directory path.
		@return A string containing the 'basename' of this file.
		Do not free or delete this pointer.
		<b>Warning:</b> This pointer is meaningless when this StatInfo object is deleted.
	*/
	const char* BaseName() { return (const char*)filename; };

	/** Get just the directory path for the file.
		@return A string containing the directory path of this file.
		Do not free or delete this pointer.
		<b>Warning:</b> This pointer is meaningless when this StatInfo object is deleted.
	*/
	const char* DirPath() { return (const char*)dirpath; };

	/** Get last access time.
		@return time in seconds since 00:00:00 UTC, January 1, 1970
    */
	time_t GetAccessTime() const { return access_time; }

	/** Get last modification time.
		@return time in seconds since 00:00:00 UTC, January 1, 1970
    */
	time_t GetModifyTime() const { return modify_time; }

	/** Get creation time.
		@return time in seconds since 00:00:00 UTC, January 1, 1970
    */
	time_t GetCreateTime() const { return create_time; }

	/** Get file size.
		@return size of the file in bytes
	*/
	filesize_t GetFileSize() const { return file_size; }

		/// Return the file's permission mode
	mode_t GetMode();

	/** Determine if the file is the name of a subdirectory,
		or just a file.  This also returns true for symlinks
		that point to directories.
		@return true if the file is a subdirectory name, false if not
	*/
	bool IsDirectory() const { return m_isDirectory; }

	/** Determine if the file is has the execute bit set at all.
		@return true if the file has the user, group or other execute bit
		set; false if the execute bit is not set.
	*/
	bool IsExecutable() const { return m_isExecutable; }

	/** Determinen if the file is a symbolic link
		@return true if the file is a symbolic link, false if not
	*/
	bool IsSymlink() const { return m_isSymlink; }

	/** Determine if the file is a domain socket
		@return true if the file is a domain socket, false if not
	*/
	bool IsDomainSocket() const { return m_isDomainSocket; }

#ifndef WIN32
	/** Get the owner of the entry.
		@return the uid of the entry's owner
	*/
	uid_t GetOwner() const;

	/** Get the group owner of the entry.
		@return the gid of the entry's group id
	*/
	gid_t GetGroup() const;
#endif

private:
	si_error_t si_error;
	int si_errno;
	bool m_isDirectory;
	bool m_isExecutable;
	bool m_isSymlink; //m_isDirectory may also be set if this points to a dir
	bool m_isDomainSocket;
	time_t access_time;
	time_t modify_time;
	time_t create_time;
#ifndef WIN32
	uid_t owner;
	gid_t group;
#endif
	bool valid;
	mode_t file_mode;
	filesize_t file_size;
	char* dirpath;
	char* filename;
	char* fullpath;
	void stat_file( const char *path );
	void stat_file( int fd );
	void init( StatWrapper *buf = NULL );

	/** Checks for and adds the directory delimiter to the string if needed.
		Returns NULL if passed NULL (See ticket #1619).
	*/
	char* make_dirpath( const char* dir );
};

#endif
