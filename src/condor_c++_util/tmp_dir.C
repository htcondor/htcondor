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
		MyString	errMsg;
		if ( !Cd2MainDir(errMsg) ) {
			dprintf( D_ALWAYS,
					"ERROR: Cd2Main fails in TmpDir::~TmpDir(): %s\n",
					errMsg.Value() );
		}
	}
}

//-------------------------------------------------------------------------
bool
TmpDir::Cd2TmpDir(const char *directory, MyString &errMsg)
{
	dprintf( D_FULLDEBUG, "TmpDir(%d)::Cd2TmpDir(%s)\n",
				m_objectNum, directory );

	bool	result = true;

	errMsg = "";

	if ( !hasMainDir ) {
		if ( condor_getcwd( mainDir ) ) {
		 	hasMainDir = true;
		} else {
			errMsg += MyString( "Unable to get cwd: " ) +
				strerror( errno ) + " (errno " + errno + ")";
			dprintf( D_ALWAYS, "ERROR: %s\n", errMsg.Value() );
			result = false;
		}
	}

	if ( result && strcmp( directory, "" ) ) {
		if ( chdir( directory ) != 0 ) {
			errMsg += MyString( "Unable to chdir to " ) +
					directory + ": " + strerror( errno );
			dprintf( D_FULLDEBUG, "ERROR: %s\n", errMsg.Value() );
			result = false;
		} else {
			m_inMainDir = false;
		}
	}

	return result;
}

//-------------------------------------------------------------------------
bool
TmpDir::Cd2TmpDirFile(const char *filePath, MyString &errMsg)
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
TmpDir::Cd2MainDir(MyString &errMsg)
{
	dprintf( D_FULLDEBUG, "TmpDir(%d)::Cd2MainDir()\n", m_objectNum );

	bool	result = true;
	errMsg = "";

	if ( hasMainDir ) {
		if ( chdir( mainDir.Value() ) != 0 ) {
			errMsg += MyString( "Unable to chdir to " ) + mainDir +
					": " + strerror( errno );
			dprintf( D_FULLDEBUG, "ERROR: %s\n", errMsg.Value() );
			result = false;
		}
	}

	if ( result ) m_inMainDir = true;

	return result;
}
