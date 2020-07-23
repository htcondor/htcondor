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


/*
  This file defines the ShadowMgr class, the "shadow manager".
  This object is responsible for reading the SHADOW_LIST config
  file attribute, getting the classads for all the shadows listed
  there, and managing those ClassAds.  It knows how to do match
  making between a given job and the various shadows.
  
  Written on 4/16/02 by Derek Wright <wright@cs.wisc.edu>

  ! ! ! BEWARE ! ! !

  These two objects are based pretty heavily on the Starter and
  StarterMgr objects in the startd.  There was no easy way to share
  the code via common base classes, and the Starter and Shadow objects
  are very different from each other.  However, most of the code in
  here was lifted from condor_startd.V6/Starter.h and
  condor_startd.V6/starter_mgr.h.  If you change anything in here, you
  should check in those two files, too, and make sure you don't need
  to make the same change on that side.  

  ! ! ! BEWARE ! ! !
*/


#ifndef _CONDOR_SHADOW_MGR_H
#define _CONDOR_SHADOW_MGR_H

#include "condor_classad.h"
#include "simplelist.h"
#include "condor_ver_info.h"


class Shadow {
public:
	Shadow( const char* path, ClassAd* ad );
	Shadow( const Shadow& s );
	~Shadow();

	bool	provides( const char* ability );

	char*	path( void ) { return s_path; };
	bool	isDC( void ) const { return s_is_dc; };

	void	printInfo( int debug_level );

	bool builtSinceVersion(int major, int minor, int sub_minor);

private:
	ClassAd* s_ad;
	char* s_path;
	bool s_is_dc;
	CondorVersionInfo* m_version_info;
};


class ShadowMgr {
public:
	ShadowMgr();
	~ShadowMgr();

	void init( void );

	Shadow* findShadow( const char* ability );

	void printShadowInfo( int debug_level );

private:

	Shadow* makeShadow( const char* path );

	SimpleList<Shadow*> shadows;

        // This makes this class un-copy-able:
    ShadowMgr( const ShadowMgr& );
    ShadowMgr& operator = ( const ShadowMgr& );
};


#endif /* _CONDOR_SHADOW_MGR_H */
