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

  This file defines the StarterMgr class, the "starter manager".  This
  object is responsible for reading the STARTER_LIST config file
  attribute, getting the classads for all the starters listed there,
  and managing those ClassAds.  It knows how to publish information
  about the starter capabilities into a given classad, (like a
  resource ad for each vm), and how to do match making between a given
  job and the various starters.

  Written on 4/16/02 by Derek Wright <wright@cs.wisc.edu>
	
  ! ! ! BEWARE ! ! !

  The ShadowMgr object in the schedd is based pretty heavily on this
  object.  There was no easy way to share the code via common base
  classes, and the Starter and Shadow objects are very different from
  each other.  However, most of the code in
  condor_schedd.V6/shadow_mgr.[Ch] was lifted from here.  If you
  change anything in here, you should check in those two files, too,
  and make sure you don't need to make the same change on that side.

  ! ! ! BEWARE ! ! !
*/

#ifndef _CONDOR_STARTER_MGR_H
#define _CONDOR_STARTER_MGR_H

#include "Starter.h"
#include "simplelist.h"


class StarterMgr {
public:
	StarterMgr();
	~StarterMgr();

	void init( void );

	void publish( ClassAd* ad, amask_t mask );

	Starter* findStarter( ClassAd* job_ad, ClassAd* mach_ad, 
						  int starter_num = -1 ); 

	void printStarterInfo( int debug_level );

private:

	Starter* makeStarter( const char* path );

	SimpleList<Starter*> starters;

        // This makes this class un-copy-able:
    StarterMgr( const StarterMgr& );
    StarterMgr& operator = ( const StarterMgr& );

};


#endif /* _CONDOR_STARTER_MGR_H */
