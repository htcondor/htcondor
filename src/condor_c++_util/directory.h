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
#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "condor_uid.h"

#if defined (ULTRIX42) || defined(ULTRIX43)
	/* _POSIX_SOURCE should have taken care of this,
		but for some reason the ULTRIX version of dirent.h
		is not POSIX conforming without it...
	*/
#	define __POSIX
#endif

#ifndef WIN32
#	include <dirent.h>
#endif

#if defined(SUNOS41) && defined(_POSIX_SOURCE)
	/* Note that function seekdir() is not required by POSIX, but the sun
	   implementation of rewinddir() (which is required by POSIX) is
	   a macro utilizing seekdir().  Thus we need the prototype.
	*/
	extern "C" void seekdir( DIR *dirp, long loc );
#endif

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

	/** Fetch information on the next file in the subdirectory and
		make it the 'current' file.
		@return The filename of the next file, or NULL if there are 
		no more files.  Do not free or delete
		this memory; the class handles all memory management. */
	const char* Next();

	/** Restart the iteration.  After calling Rewind(), the next 
		call to Next() will return the first file in the directory again.
		@return Always returns true (for now)
		@see Next()
	*/
	bool Rewind();

	/** Get last access time of current file.
		@return time in seconds since 00:00:00
			UTC, January 1, 1970 */
	time_t GetAccessTime() { return access_time; }

	/** Get last modification time of current file.
		@return time in seconds since 00:00:00
			UTC, January 1, 1970 */
	time_t GetModifyTime() { return modify_time; }

	/** Get creation time of current file.
		@return time in seconds since 00:00:00
			UTC, January 1, 1970 */
	time_t GetCreateTime() { return create_time; }

	/** Determine if the current file is the name of a subdirectory,
		or just a file.
		@return true if current file is a subdirectory name, false if not
	*/
	bool IsDirectory() { return curr_isdirectory; }

	/** Remove the current file.  If the current file is a subdirectory,
	   then the subdirectory (and all files beneath it) are removed.
		@return true on successful removal, otherwise false
	*/
	bool Remove_Current_File();

	/** Remove the all the files and subdirectories in the directory
		specified by the constructor.  Upon success, the subdirectory
		will still exist, but will be empty.
		@return true on successful removal, otherwise false
	*/
	bool Remove_Entire_Directory();

private:
	char *curr_dir;
	char *curr_filename;
	bool curr_isdirectory;
	time_t access_time;
	time_t modify_time;
	time_t create_time;
	bool want_priv_change;
	priv_state desired_priv_state;
	priv_state saved_priv;
	inline void Directory::Set_Access_Priv();
#ifdef WIN32
	long dirp;
	static struct _finddata_t filedata;
#else
	DIR *dirp;
	int do_stat( const char *path, struct stat *buf );
#endif
};

#endif
