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
#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "condor_uid.h"

#ifndef WIN32
#	include <dirent.h>
#endif

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
	StatInfo( const char *dirpath, const char *filename );

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
			  unsigned long fsize, bool is_dir, bool is_symlink );  
#endif

	/// Destructor<p>
	~StatInfo();

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
		return 0.
		@return The errno from getting info for this StatInfo object.
	*/
	int Errno() { return si_errno; };

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
	time_t GetAccessTime() { return access_time; }

	/** Get last modification time.
		@return time in seconds since 00:00:00 UTC, January 1, 1970
    */
	time_t GetModifyTime() { return modify_time; }

	/** Get creation time.
		@return time in seconds since 00:00:00 UTC, January 1, 1970
    */
	time_t GetCreateTime() { return create_time; }

	/** Get file size.
		@return size of the file in bytes
	*/
	unsigned long GetFileSize() { return file_size; }

	/** Determine if the file is the name of a subdirectory,
		or just a file.  This also returns true for symlinks
		that point to directories.
		@return true if the file is a subdirectory name, false if not
	*/
	bool IsDirectory() { return isdirectory; }

	/** Determine if the file is has the execute bit set at all.
		@return true if the file has the user, group or other execute bit
		set; false if the execute bit is not set.
	*/
	bool IsExecutable() { return isexecutable; }

	/** Determinen if the file is a symbolic link
		@return true if the file is a symbolic link, false if not
	*/
	bool IsSymlink() { return issymlink; }

#ifndef WIN32
	/** Get the owner of the entry.
		@return the uid of the entry's owner
	*/
	uid_t GetOwner( void ) { return owner; };

	/** Get the group owner of the entry.
		@return the gid of the entry's group id
	*/
	gid_t GetGroup( void ) { return group; };
#endif

private:
	si_error_t si_error;
	int si_errno;
	bool isdirectory;
	bool isexecutable;
	bool issymlink; //isdirectory may also be set if this points to a directory
	time_t access_time;
	time_t modify_time;
	time_t create_time;
#ifndef WIN32
	uid_t owner;
	gid_t group;
#endif
	unsigned long file_size;
	char* dirpath;
	char* filename;
	char* fullpath;
	void do_stat( const char *path );
	char* make_dirpath( const char* dir );
#ifndef WIN32
	int unix_do_stat( const char *path, struct stat *buf );
#else
	int nt_do_stat( const char *path, struct stat *buf );
#endif
};


enum dir_rempriv_t { DIR_REMPRIV_DEFAULT,
					 DIR_REMPRIV_OWNER };

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
		@return Always returns true (for now)
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
	unsigned long GetFileSize() { return curr ? curr->GetFileSize() : 0; };

	/** Get the size of all the files and all the files in all subdirectories,
		starting with the directory specified by the constructor.
		@return the size of bytes (if we receive an error trying to determine
		the size of any file, we consider that file to have a size of zero). */
	unsigned long Directory::GetDirectorySize();

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
	bool Remove_Current_File( dir_rempriv_t RemPriv = DIR_REMPRIV_DEFAULT );

	/** Remove the specified file.  If the given file is a subdirectory,
	    then the subdirectory (and all files beneath it) are removed. 
		@param path The full path to the file to remove
		@return true on successful removal, otherwise false
	*/
	bool Remove_Full_Path( const char *path,
						   dir_rempriv_t RemPriv = DIR_REMPRIV_DEFAULT );

	/** Remove the specified directory entry.  If the given file is a
		subdirectory, then the subdirectory (and all files beneath it)
		are removed. @param path The full path to the file to remove
		@return true on successful removal, otherwise false */
	bool Remove_Entry( const char* name,
					   dir_rempriv_t RemPriv = DIR_REMPRIV_DEFAULT );

	/** Remove the all the files and subdirectories in the directory
		specified by the constructor.  Upon success, the subdirectory
		will still exist, but will be empty.
		@return true on successful removal, otherwise false
	*/
	bool Remove_Entire_Directory( dir_rempriv_t RemPriv =
								  DIR_REMPRIV_DEFAULT );
	

private:
	char *curr_dir;
	StatInfo* curr;
	bool want_priv_change;
	priv_state desired_priv_state;
	bool do_remove( const char *path, bool is_curr, dir_rempriv_t RemPriv );
#ifdef WIN32
	long dirp;
	struct _finddata_t filedata;
#else
	DIR *dirp;
#endif
};


/** Determine if the given file is the name of a subdirectory,
  or just a file.
  @param path The full path to the file to test
  @return true if given file is a subdirectory name, false if not
*/
bool IsDirectory( const char* path );

/** Determine if the given file is the name of a symlink.
  @param path The full path to the file to test
  @return true if given file is a symlink, false if not
*/
bool IsSymlink( const char* path );

/** Take two strings, a directory path, and a filename, and
  concatenate them together.  If the directory path doesn't end with
  the appropriate directory deliminator for this platform, this
  function will ensure that the directory delimiter is included in the
  resulting full path.
  @param dirpath The directory path.
  @param filename The filename.
  @return A string created with new() that contains the full pathname.
*/
char* dircat( const char* dirpath, const char* filename );

/** Returns a path to subdirectory to use for temporary files.
  @return The pointer returned must be de-allocated by the caller w/ free()
*/
char* temp_dir_path();


char * create_temp_file();
#endif
