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

#ifndef WIN32
	#include <unistd.h> // for getcwd()
#endif

#include "basename.h"
#include "condor_debug.h"
#include "tmp_dir.h"
#include "condor_getcwd.h"

int TmpDir::nextObjectNum = 0;

//-------------------------------------------------------------------------
TmpDir::TmpDir() :
		hasMainDir		(false),
		m_inMainDir		(true)
{
	m_objectNum = nextObjectNum++;

	dprintf( D_FULLDEBUG, "TmpDir(%d)::TmpDir()\n", m_objectNum );
}

//-------------------------------------------------------------------------
TmpDir::~TmpDir()
{
	dprintf( D_FULLDEBUG, "TmpDir(%d)::~TmpDir()\n", m_objectNum );

	if ( !m_inMainDir ) {
		std::string errMsg;
		if ( !Cd2MainDir(errMsg) ) {
			dprintf( D_ALWAYS,
					"ERROR: Cd2Main fails in TmpDir::~TmpDir(): %s\n",
					errMsg.c_str() );
		}
	}
}

//-------------------------------------------------------------------------
bool
TmpDir::Cd2TmpDir(const char *directory, std::string &errMsg)
{
	dprintf( D_FULLDEBUG, "TmpDir(%d)::Cd2TmpDir(%s)\n",
				m_objectNum, directory );

	errMsg = "";

		// Don't do anything if the directory path is "" or "."
		// (in DAGMan we get a directory of "" if no directory is
		// specified for a node, and -usedagdir is not set).
	if ( directory != NULL && strcmp( directory, "" ) != MATCH &&
			strcmp( directory, "." ) != MATCH ) {
		if ( !hasMainDir ) {
			if ( !condor_getcwd( mainDir ) ) {
				formatstr( errMsg, "Unable to get cwd: %s (errno %d)",
				           strerror( errno ), errno );
				dprintf( D_ALWAYS, "ERROR: %s\n", errMsg.c_str() );
				EXCEPT( "Unable to get current directory!" );
				return false; // never get here
			}

		 	hasMainDir = true;
		}

		if ( chdir( directory ) != 0 ) {
			formatstr( errMsg, "Unable to chdir to %s: %s",
			           directory, strerror( errno ) );
			dprintf( D_FULLDEBUG, "ERROR: %s\n", errMsg.c_str() );
			return false;
		}

		m_inMainDir = false;
	}

	return true;
}

//-------------------------------------------------------------------------
bool
TmpDir::Cd2TmpDirFile(const char *filePath, std::string &errMsg)
{
	dprintf( D_FULLDEBUG, "TmpDir(%d)::Cd2TmpDirFile(%s)\n",
				m_objectNum, filePath );

	bool	result = true;

	char *	dir = condor_dirname( filePath );
	result = Cd2TmpDir( dir, errMsg );
	free( dir );

	return result;
}

//-------------------------------------------------------------------------
bool
TmpDir::Cd2MainDir(std::string &errMsg)
{
	dprintf( D_FULLDEBUG, "TmpDir(%d)::Cd2MainDir()\n", m_objectNum );

	errMsg = "";

		// If we didn't really change directories, we don't have to
		// do anything.
	if ( ! m_inMainDir ) {
		if ( !hasMainDir ) {
			EXCEPT( "Illegal condition -- m_inMainDir and hasMainDir "
					"both false!" );
			return false; // never get here
		}

		if ( chdir( mainDir.c_str() ) != 0 ) {
			formatstr( errMsg, "Unable to chdir to %s: %s", mainDir.c_str(),
			           strerror( errno ) );
			dprintf( D_FULLDEBUG, "ERROR: %s\n", errMsg.c_str() );
			EXCEPT( "Unable to chdir() to original directory!" );
			return false; // never get here
		}

		m_inMainDir = true;
	}

	return true;
}
