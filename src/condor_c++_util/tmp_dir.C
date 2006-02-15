/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"

#ifndef WIN32
	#include <unistd.h> // for getcwd()
#endif

#include "basename.h"
#include "condor_debug.h"
#include "tmp_dir.h"

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
		if ( getcwd( mainDir, sizeof(mainDir) ) ) {
		 	hasMainDir = true;
		} else {
			errMsg += MyString( "Unable to get cwd: " ) +
				strerror( errno );
			dprintf( D_FULLDEBUG, "ERROR: %s\n", errMsg.Value() );
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
		if ( chdir( mainDir ) != 0 ) {
			errMsg += MyString( "Unable to chdir to " ) + mainDir +
					": " + strerror( errno );
			dprintf( D_FULLDEBUG, "ERROR: %s\n", errMsg.Value() );
			result = false;
		}
	}

	if ( result ) m_inMainDir = true;

	return result;
}
