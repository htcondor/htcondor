/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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
