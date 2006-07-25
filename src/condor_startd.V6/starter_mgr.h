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
