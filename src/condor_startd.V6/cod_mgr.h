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


#ifndef _CONDOR_COD_MGR_H
#define _CONDOR_COD_MGR_H

#include "simplelist.h"

class Resource;
class Claim;


class CODMgr {
public:
	CODMgr( Resource* my_rip );
	~CODMgr();

	void init( void );

	void publish( ClassAd* ad, amask_t mask );
	
	Claim* addClaim();
	bool removeClaim( Claim* c );

	void starterExited( Claim* c );

	Claim* findClaimById( const char* id );
	Claim* findClaimByPid( pid_t pid );

	int numClaims( void );
	bool hasClaims( void );
	bool isRunning( void );

	void shutdownAllClaims( bool graceful );

		// functions for the classad-only claim management protocol
	int release( Stream* s, ClassAd* req, Claim* claim );
	int activate( Stream* s, ClassAd* req, Claim* claim );
	int deactivate( Stream* s, ClassAd* req, Claim* claim );
	int suspend( Stream* s, ClassAd* req, Claim* claim );
	int resume( Stream* s, ClassAd* req, Claim* claim );

private:

		/*
		  helper functions which deal with the interaction of the COD
		  claims and the opportunistic claims on the resource.  since
		  there are multiple ways that a COD job could start running
		  (activate or resume) or could stop (suspend or starter
		  exits), we put the code for these actions in one place so we
		  don't duplicate code.
		*/
	void interactionLogicCODRunning( void );
	void interactionLogicCODStopped( void );

	Resource* rip;
	SimpleList<Claim*> claims;


};


#endif /* _CONDOR_COD_MGR_H */
