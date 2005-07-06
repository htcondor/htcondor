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

// A class to use when you need to change to a temporary directory
// (or directories), but need to make sure you get back to the original
// directory when you're done.  The "trick" here is that the destructor
// cds back to the original directory, so if you use an automatic
// TmpDir object you're guaranteed to get back to the original directory
// at the end of your function.

#ifndef TMP_DIR_H
#define TMP_DIR_H

#include "MyString.h"

class TmpDir {
public:
	/** Constructor.
	*/
	TmpDir();

	/** Destructor -- cds back to the original directory.
	*/
	~TmpDir();

	/** Change to the given directory (note: if directory is "", we
	    don't try to change to it).
		@param The directory to cd to.
		@param A MyString to hold any error message.
		@return true if successful, false otherwise
	*/
	bool Cd2TmpDir(const char *directory, MyString &errMsg);

	/** Change to the directory containing the given file.
		@param The path to a file.
		@param A MyString to hold any error message.
		@return true if successful, false otherwise
	*/
	bool Cd2TmpDirFile(const char *filePath, MyString &errMsg);

	/** Change back to the original directory.
		@return true if successful, false otherwise
	*/
	bool Cd2MainDir(MyString &errMsg);

protected:
	bool	hasMainDir;
	char    mainDir[_POSIX_PATH_MAX];
};

#endif /* #ifndef TMP_DIR_H */
