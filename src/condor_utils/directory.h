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

#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "condor_uid.h"
#include "stat_info.h"
#include "directory_util.h"

/** Class to iterate filenames in a subdirectory.  Given a subdirectory
	path, this class can iterate the names of the files in the directory,
	remove files, and/or
	report file access/modify/create times.  Also reports if
	the filename represents another subdirectory or not.  
	<p><b>Note:</b> This class does not recurse down into subdirectories
	except when calling the Remove_Current_File() or Remove_Entire_Directory()
	methods.
*/
class Directory
{
public:

	/** Constructor.  Upon instantiation, the user 
		should call Next() to fetch information on the first file.  
		If a priv_state is specified, this priv_state will be used 
		whenever accessing the filesystem.
		@param dirpath The full path to the subdirectory to operate upon
		@param priv The priv_state used when accessing the filesystem.
		If set to PRIV_UNKNOWN, then the class will use 
		whatever priv_state is currently in effect.  If set to
		PRIV_ROOT, note all operations will be tried as both PRIV_ROOT
		and as PRIV_CONDOR, just in case the files are being accessed
		over NFS (where root gets mapped to nobody).
		@see Next()
		@see priv_state
	*/
	Directory( const char *dirpath, priv_state priv = PRIV_UNKNOWN);

	/** Alternate Constructor.  Instead of passing in a pathname, 
		the caller can instantiate by using a StatInfo object (since
		in many cases, they've already got a copy of that object).
		This allows them to pass in more info, and to prevent us from
		re-doing the syscalls and other work to figure out things we'd
		like to know about the directory.  Otherwise, this constructor
		is just like the other, in terms of how to use the Directory
		object once it exists, the handling of priv states, etc.
		@param info Pointer to the StatInfo object to use for infor.
		@param priv The priv_state used when accessing the filesystem.
		@see Next()
		@see priv_state
		@see StatInfo
	*/
	Directory( StatInfo *info, priv_state priv = PRIV_UNKNOWN);

	/// Destructor<p>
	~Directory();

	/** Get full path to the directory as instantiated. */
	const char *GetDirectoryPath( void ) { return curr_dir; }

	/** Fetch information on the next file in the subdirectory and
		make it the 'current' file.
		@return The filename of the next file, or NULL if there are 
		no more files.  The filename returned is just the basename; call
		GetFullPath() to get the complete pathname.
		Do not free or delete this memory; the class handles all memory management. 
		<b>Warning:</b> This pointer is meaningless when the Directory object is deleted.
		@see GetFullPath()
	*/
	const char* Next();

	/** Restart the iteration.  After calling Rewind(), the next 
		call to Next() will return the first file in the directory again.
		@return true on successful rewind, otherwise false
		@see Next()
	*/
	bool Rewind();

	/** Find a entry with a given name.  Iterates through all entries in the
		directory until a match is found, or the end is reached.  If a match
		is found, the current entry matches it.  Note that this function
		rewind()s the directory object, and, by definition, changes the
		current entry pointer.
		@return true if a match is found, false if not.
		@see Next()
	*/

	bool Find_Named_Entry( const char *name );

	/** Get last access time of current file.  If there is no current
	    file, return 0.
		@return time in seconds since 00:00:00 UTC, January 1, 1970 */
	time_t GetAccessTime() { return curr ? curr->GetAccessTime() : 0; };

	/** Get last modification time of current file.  If there is no
	    current file, return 0.
		@return time in seconds since 00:00:00 UTC, January 1, 1970 */
	time_t GetModifyTime() { return curr ? curr->GetModifyTime() : 0; };

	/** Get creation time of current file.  If there is no current
	    file, return 0. 
		@return time in seconds since 00:00:00 UTC, January 1, 1970 */
	time_t GetCreateTime() { return curr ? curr->GetCreateTime() : 0; };

	/** Get size of current file.  If there is no current file, return 0.
		@return size of file in bytes */
	filesize_t GetFileSize() { return curr ? curr->GetFileSize() : 0; };

	/** Get mode of current file.  If there is no current file, return 0.
		@return permission mode of the current file
	*/
	mode_t GetMode() { return curr ? curr->GetMode() : 0; };

	/** Get the size of all the files and all the files in all subdirectories,
		starting with the directory specified by the constructor.
		@return the size of bytes (if we receive an error trying to determine
		the size of any file, we consider that file to have a size of zero). 
		we optionally return the number of files+dirs also
		*/
	filesize_t GetDirectorySize(size_t * number_of_entries=NULL);

	/** Get full path name to the current file.  If there is no current file,
		return NULL.
		@return file pathname of the file */
	const char* GetFullPath() { return curr ? curr->FullPath() : NULL; };

	/** Determine if the current file is the name of a subdirectory,
		or just a file.  If there is no current file, return false.
		(A Symbolic link that points to a directory will return true.)
		@return true if current file is a subdirectory name, false if not
	*/
	bool IsDirectory() { return curr ? curr->IsDirectory() : false; };

	/** Determine if the current file is a symbolic link.
		@return true if current file is a symbolic link, false if not
	*/
	bool IsSymlink() {return curr ? curr->IsSymlink() : false; }

	/** Remove the current file.  If the current file is a subdirectory,
	    then the subdirectory (and all files beneath it) are removed.
		@return true on successful removal, otherwise false
	*/
	bool Remove_Current_File( void );

	/** Remove the specified file.  If the given file is a subdirectory,
	    then the subdirectory (and all files beneath it) are removed. 
		@param path The full path to the file to remove
		@return true on successful removal, otherwise false
	*/
	bool Remove_Full_Path( const char *path );

	/** Remove the all the files and subdirectories in the directory
		specified by the constructor.  Upon success, the subdirectory
		will still exist, but will be empty.
		@return true on successful removal, otherwise false
	*/
	bool Remove_Entire_Directory( void );


#ifndef WIN32

	/** Recursively change ownership of this directory and kids

	Implemented in terms of recursive_chown, see that for most
	up to date details.

	@see recursive_chown

	Changes ownership of path to the UID dst_uid, and GID dst_gid
	from the UID src_uid.  The directory and all files and
	directories within it are changed.  If a file is encountered
	that is not owned by src_uid or dst_uid, it is considered an
	error.

	On an error, a message is reported to dprintf(D_ALWAYS) and false
	is returned.  Otherwise true is returned.

	@param src_uid UID who is allowed to already own the file

	@param dst_uid UID to change the file's ownership to.  (Will
	silently skip a file already downed by this user)

	@param dst_gid GID to change the file's ownership to.

	@param non_root_okay Should we silently skip chown attempt and
	report success if this process isn't root?  chown only works if
	this process has root permissions.  If this process isn't root
	and non_root_okay is true (the default), this function will
	silently return true.  If this process isn't root and
	non_root_okay is false, this function will fail.
	*/
	bool Recursive_Chown(uid_t src_uid, uid_t dst_uid, gid_t dst_gid,
		bool non_root_okay = true);


		/** Recursively walk through the directory tree and chmod()
			any real directories (ignoring symlinks) to the given
			mode.
			NOTE: This will call Rewind(), so it is NOT safe to use
			this during another iteration over the directory.
			@param mode The file mode you want to set directories to
		*/
	bool chmodDirectories( mode_t mode );

#endif /* ! WIN32 */

	/** Recursively walk through the directory tree and change 
	    the owner of all files and directories to the given 
	    username.	
		@param username The username to that will owner.
		@param domain The domain of the user that will be the owner (on Win32).
	 */
	bool Recursive_Chown(const char *username, const char *domain);


private:
	char *curr_dir;
	StatInfo* curr;
	bool want_priv_change;
	priv_state desired_priv_state;
	bool do_remove( const char *path, bool is_curr );
	bool do_remove_dir( const char *path );
	bool do_remove_file( const char *path );
	void initialize( priv_state priv );
	bool rmdirAttempt( const char* path, priv_state priv );

#ifdef WIN32
	LONG_PTR dirp;
#ifdef _M_X64
	struct _finddatai64_t filedata;
#else
	struct _finddata_t filedata;
#endif
#else
	condor_DIR *dirp;
	priv_state setOwnerPriv( const char* path, si_error_t &err );
	uid_t owner_uid;
	gid_t owner_gid;
	bool owner_ids_inited;
#endif
};

// A handy utility class that deletes the underlying file
// when the class instance is deleted
class DeleteFileLater {
 public:
	DeleteFileLater (const char * _name);
	~DeleteFileLater ();
 protected:
	char * filename;
};


/** Determine if the given file is the name of a subdirectory,
  or just a file.
  @param path The full path to the file to test
  @return true if given file is a subdirectory name (or symlink to one), false if not
*/
bool IsDirectory( const char* path );

/** Determine if the given file is the name of a symlink.
  @param path The full path to the file to test
  @return true if given file is a symlink, false if not
*/
bool IsSymlink( const char* path );



#if ! defined(WIN32)
/** Recursively change ownership of a file or directory tree

Changes ownership of path to the UID dst_uid, and GID dst_gid
from the UID src_uid.  If path is a directory, the directory and
all files and directories within it are also changed.  If a file
is encountered that is not owned by src_uid or dst_uid, it is
considered an error.

On an error, a message is reported to dprintf(D_ALWAYS) and false
is returned.  Otherwise true is returned.

@param path File or directory to chown

@param src_uid UID who is allowed to already own the file

@param dst_uid UID to change the file's ownership to.  (Will
silently skip a file already downed by this user)

@param dst_gid GID to change the file's ownership to.

@param non_root_okay Should we silently skip chown attempt and
report success if this process isn't root?  chown only works if
this process has root permissions.  If this process isn't root
and non_root_okay is true (the default), this function will
silently return true.  If this process isn't root and
non_root_okay is false, this function will fail.
*/
bool recursive_chown(const char * path,
	uid_t src_uid, uid_t dst_uid, gid_t dst_gid, bool
	non_root_okay = true);

#endif /* ! defined(WIN32) */


char * create_temp_file(bool create_as_subdirectory = false);

/** Actual implementation of the recursive chown function that is called
    by the Directory::Recursive_Chown() member function. This function 
	can be called without firing up a whole directory object, if desired.
	@param path Path to be traversed recursively and chown'd
	@param username Username to chown to.
	@param domain Domain of the user to chown to.
*/
bool recursive_chown( const char *path, 
					  const char *username, const char *domain );

/** Create a directory any any missing parent directories with the specified
    mode and priv state.  If the directory already exists, it is left as is
    (no attempt to make the mode or ownership match).
    Returns true on success; false on error and sets errno
    Encountering a pre-existing directory is counted as success, but errno
	will be set to EEXIST in that case.
 */
bool mkdir_and_parents_if_needed( const char *path, mode_t mode, priv_state priv = PRIV_UNKNOWN );
bool mkdir_and_parents_if_needed( const char *path, mode_t mode, mode_t parent_mode, priv_state priv = PRIV_UNKNOWN );

/** Create parent directories of a path if they do not exist.
    If the parent directory already exists, it is left as is
    (no attempt to make the mode or ownership match).
    Returns true on success; false on error and sets errno
 */
bool make_parents_if_needed( const char *path, mode_t mode, priv_state priv = PRIV_UNKNOWN );

#endif
