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


class Shadow {
public:
	Shadow( const char* path, ClassAd* ad );
	Shadow( const Shadow& s );
	~Shadow();

	bool	provides( const char* ability );

	char*	path( void ) { return s_path; };
	bool	isDC( void ) { return s_is_dc; };

	void	printInfo( int debug_level );

private:
	ClassAd* s_ad;
	char* s_path;
	bool s_is_dc;
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
