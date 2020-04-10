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

	void publish(ClassAd* ad);

	Starter* newStarter( ClassAd* job_ad, ClassAd* mach_ad,
						  bool &no_starter,
						  int starter_num = -1); 

	void printStarterInfo( int debug_level );

	bool haveStandardUni() const { return _haveStandardUni; }
private:

	Starter* registerStarter( const char* path );

	SimpleList<Starter*> starters;
	bool _haveStandardUni;

        // This makes this class un-copy-able:
    StarterMgr( const StarterMgr& );
    StarterMgr& operator = ( const StarterMgr& );

};


#endif /* _CONDOR_STARTER_MGR_H */
