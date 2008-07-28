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


#ifndef _CONDOR_DC_STARTD_H
#define _CONDOR_DC_STARTD_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_query.h"
#include "condor_io.h"
#include "enum_utils.h"


/** The subclass of the Daemon object for talking to a startd
*/
class DCStartd : public Daemon {
public:

		/** Constructor.  Same as a Daemon object.
		  @param name The name (or sinful string) of the daemon, NULL
		              if you want local  
		  @param pool The name of the pool, NULL if you want local
		*/
	DCStartd( const char* const name = NULL, const char* pool = NULL );

		/** Alternate constructor that takes more info if have it
			@param name The name of the daemon, NULL for local  
			@param pool The name of the pool, NULL for local
			@param addr The address (sinful string), NULL if unknown
			@param id The ClaimId, NULL if unknown
		*/
	DCStartd( const char* const name, const char* const pool,
			  const char* const addr, const char* const id );

		/// Destructor.
	~DCStartd();

		/** Set the ClaimId to use when talking to this startd. 
			@param id The ClaimID string
			@return true on success, false on invalid input (NULL)
		*/
	bool setClaimId( const char* id );

		/** @return the ClaimId string for this startd, NULL if we
			don't have a value yet.
		*/
	char* getClaimId( void ) { return claim_id; };

		/** Send the command to this startd to deactivate the claim 
			@param graceful Should we be graceful or forcful?
			@param claim_is_closing startd indicates if not accepting more jobs
			@return true on success, false on failure
		 */
	bool deactivateClaim( bool graceful = true, bool *claim_is_closing=NULL );

		/** Try to activate the claim on this started with the given
			job ClassAd and version of the starter we want to use. 
			@param job_ad Job ClassAd to activate with
			@param starter_version Which starter should we use?
			@param claim_sock_ptr If non-NULL, and if the activation
			       worked, your pointer will be set to the ReliSock
				   used to activate the claim.  If you pass in a valid
				   pointer (to a pointer), and anything goes
				   wrong, your pointer will be set to NULL.
			@return OK on success, NOT_OK on failure, CONDOR_TRY_AGAIN
		        if the startd is busy and wants us to try back later.
		*/
	int activateClaim( ClassAd* job_ad, int starter_version, 
					   ReliSock** claim_sock_ptr );

		/** Before activating a claim, attempt to delegate the user proxy
			(if there is one). We do this from the shadow if
			GLEXEC_STARTER is set, since if the startd requires that
			glexec be used to spawn the starter, then it needs to get
			the proxy early (since glexec uses it to determine what UID
			to run the starter under). The normal way of delegating the
			proxy via file transfer happens when the starter is already
			running, and thus too late.
			@param proxy Location of user proxy to delegate
			@return OK if proxy is delegated, NOT_OK if startd doesn't
			need it, CONDOR_ERROR on error
		*/
	int delegateX509Proxy( const char* proxy );

		// Generic ClassAd-only protocol for managing claims

	bool requestClaim( ClaimType type, const ClassAd* req_ad,
					   ClassAd* reply, int timeout = -1 );

	bool activateClaim( const ClassAd* job_ad, ClassAd* reply,
						int timeout = -1 );

	bool suspendClaim( ClassAd* reply, int timeout = -1 );

	bool resumeClaim( ClassAd* reply, int timeout = -1 );

	bool deactivateClaim( VacateType type, ClassAd* reply,
						  int timeout = -1 );

	bool releaseClaim( VacateType type, ClassAd* reply,
					   int timeout = -1 );

	bool renewLeaseForClaim( ClassAd* reply, int timeout );

	bool locateStarter( const char* global_job_id, 
						const char* claim_id,
						const char* schedd_public_addr,
						ClassAd* reply, 
						int timeout = -1 );

	bool vacateClaim( const char* name );

	bool checkpointJob( const char* name );

	bool getAds( ClassAdList &adsList );

 private:
	char* claim_id;

		// Helper methods
	bool checkClaimId( void );
	bool checkVacateType( VacateType t );

		// I can't be copied (yet)
	DCStartd( const DCStartd& );
	DCStartd& operator = ( const DCStartd& );

};

#endif /* _CONDOR_DC_STARTD_H */
