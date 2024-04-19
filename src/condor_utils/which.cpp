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
#include "stl_string_utils.h"
#include "condor_debug.h"
#include "which.h"
#include "directory.h"
#include "condor_environ.h"

#ifdef WIN32
#include <direct.h>
#endif

std::string
which(const std::string &strFilename, const std::string &strAdditionalSearchDirs)
{
	const char *strPath = getenv(ENV_PATH);
	if (strPath == NULL) {
		strPath = "";
	}
	dprintf( D_FULLDEBUG, "Path: %s\n", strPath);

	char path_delim[3];
	snprintf( path_delim, sizeof(path_delim), "%c", PATH_DELIM_CHAR );
	std::vector<std::string> listDirectoriesInPath = split(strPath, path_delim);

#ifdef WIN32
	int iLength = strFilename.length();
	if (!strcasecmp(strFilename.substr(iLength - 4, 4).c_str(), ".dll"))
	{	// if the filename ends in ".dll"
		
		// in order to mimic the behavior of LoadLibrary
		// we need to artificially insert some other stuff in the search path

		/* from MSDN LoadLibrary
			1.) The directory from which the application loaded. 
			2.) The current directory. 
				Windows XP: If HKLM\System\CurrentControlSet\Control\SessionManager\SafeDllSearchMode is 1, the current directory is the last directory searched. The default value is 0. 

			3.) The Windows system directory. Use the GetSystemDirectory function to get the path of this directory. 
				Windows NT/2000/XP: The wname of this directory is System32. 

			4.) Windows NT/2000/XP: The 16-bit Windows system directory. There is no function that obtains the path of this directory, but it is searched. The name of this directory is System. 
			5.) The Windows directory. Use the GetWindowsDirectory function to get the path of this directory. 
			6.) The directories that are listed in the PATH environment variable. 
		*/

		// #5
		char psNewDir[MAX_PATH];
		if (GetWindowsDirectory(psNewDir, MAX_PATH) > 0)
			listDirectoriesInPath.insert(listDirectoriesInPath.begin(), psNewDir);
		else
			dprintf( D_FULLDEBUG, "GetWindowsDirectory() failed, err=%d\n", GetLastError());

		// #4 
		strcat(psNewDir, "\\System");
		listDirectoriesInPath.insert(listDirectoriesInPath.begin(), psNewDir);

		// #3
		if (GetSystemDirectory(psNewDir, MAX_PATH) > 0)
			listDirectoriesInPath.insert(listDirectoriesInPath.begin(), psNewDir);
		else
			dprintf( D_FULLDEBUG, "GetSystemDirectory() failed, err=%d\n", GetLastError());

		// #2
		if (_getcwd(psNewDir, MAX_PATH))
			listDirectoriesInPath.insert(listDirectoriesInPath.begin(), psNewDir);
		else
			dprintf( D_FULLDEBUG, "_getcwd() failed, err=%d\n", errno);

		// #1  had better be covered by the user passing in strAdditionalSearchDirs
	}
#endif

	// add additional dirs if specified
	// path_delim was set above
	for (auto& dir: StringTokenIterator(strAdditionalSearchDirs, path_delim)) {
		if (!contains(listDirectoriesInPath, dir)) {
			listDirectoriesInPath.emplace_back(dir);
		}
	}

	for (auto& psDir: listDirectoriesInPath) {
		dprintf( D_FULLDEBUG, "Checking dir: %s\n", psDir.c_str() );

		std::string strFullDir;
		dircat(psDir.c_str(), strFilename.c_str(), strFullDir);

		StatInfo info(strFullDir.c_str());
		if( info.Error() == SIGood ) {
			return strFullDir;
		}
	}
	return "";
}
