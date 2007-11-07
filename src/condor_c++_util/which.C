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
#include "string_list.h"
#include "condor_debug.h"
#include "which.h"
#include "directory.h"
#include "condor_environ.h"

#ifdef WIN32
#include <direct.h>
#endif


// A version that doesn't require having MyStrings passed to it, in
// case we find this easier to use...
MyString
which( const char* strFilename, const char* strAdditionalSearchDir )
{
	MyString file( strFilename );
	if( strAdditionalSearchDir ) {
		MyString additionalSearch( strAdditionalSearchDir );
		return which( file, additionalSearch );
	} 
	return which( file );
}


MyString
which(const MyString &strFilename, const MyString &strAdditionalSearchDir)
{
	MyString strPath = getenv( EnvGetName( ENV_PATH ) );
	dprintf( D_FULLDEBUG, "Path: %s\n", strPath.Value());

	char path_delim[3];
	sprintf( path_delim, "%c", PATH_DELIM_CHAR );
	StringList listDirectoriesInPath( strPath.Value(), path_delim );

#ifdef WIN32
	int iLength = strFilename.Length();
	if (!stricmp(strFilename.Substr(iLength - 4, iLength - 1).Value(), ".dll"))
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

		listDirectoriesInPath.rewind();
		listDirectoriesInPath.next();

		// #5
		char psNewDir[MAX_PATH];
		GetWindowsDirectory(psNewDir, MAX_PATH);
		listDirectoriesInPath.insert(psNewDir);

		listDirectoriesInPath.rewind();
		listDirectoriesInPath.next();

		// #4 
		strcat(psNewDir, "\\System");
		listDirectoriesInPath.insert(psNewDir);

		listDirectoriesInPath.rewind();
		listDirectoriesInPath.next();

		// #3
		GetSystemDirectory(psNewDir, MAX_PATH);
		listDirectoriesInPath.insert(psNewDir);

		listDirectoriesInPath.rewind();
		listDirectoriesInPath.next();

		// #2
		_getcwd(psNewDir, MAX_PATH);
		listDirectoriesInPath.insert(psNewDir);

		// #1  had better be covered by the user passing in strAdditionalSearchDir
	}
#endif

	listDirectoriesInPath.rewind();
	listDirectoriesInPath.next();

	if( strAdditionalSearchDir != "" ) {
		listDirectoriesInPath.insert(strAdditionalSearchDir.Value());
	}
	
	listDirectoriesInPath.rewind();

	const char *psDir;
	while( (psDir = listDirectoriesInPath.next()) )
	{
		dprintf( D_FULLDEBUG, "Checking dir: %s\n", psDir );

		char *psFullDir = dircat(psDir, strFilename.Value());
		MyString strFullDir = psFullDir;
		delete [] psFullDir;

		StatInfo info(strFullDir.Value());
		if( info.Error() == SIGood ) {
			return strFullDir;
		}
	}
	return "";
}
