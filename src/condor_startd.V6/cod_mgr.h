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
