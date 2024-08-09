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



#ifndef _CONDOR_COD_MGR_H
#define _CONDOR_COD_MGR_H


class Resource;
class Claim;


class CODMgr {
public:
	CODMgr( Resource* my_rip );
	~CODMgr();

	void init( void );

	void publish( ClassAd* ad );
	
	Claim* addClaim(int lease_duration);
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
	int renew( Stream* s, ClassAd* req, Claim* claim );

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
	std::vector<Claim*> claims;


};


#endif /* _CONDOR_COD_MGR_H */
