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

	/** Change to the given directory (note: if directory is "" or NULL
		, we don't try to change to it and just return true as in '.').
		@param The directory to cd to.
		@param A MyString to hold any error message.
		@return true if successful, false otherwise
	*/
	bool Cd2TmpDir(const char *directory, std::string &errMsg);

	/** Change to the directory containing the given file. (note: if
		directory is "" or NULL, we don't try to change to it and 
		just return true)
		@param The path to a file.
		@param A MyString to hold any error message.
		@return true if successful, false otherwise
	*/
	bool Cd2TmpDirFile(const char *filePath, std::string &errMsg);

	/** Change back to the original directory.
		@return true if successful, false otherwise
	*/
	bool Cd2MainDir(std::string &errMsg);

protected:
	bool	hasMainDir;
	std::string mainDir;
	int		m_objectNum;
	bool	m_inMainDir;

	static int	nextObjectNum;
};

#endif /* #ifndef TMP_DIR_H */
