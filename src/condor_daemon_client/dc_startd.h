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

#ifndef _CONDOR_DC_STARTD_H
#define _CONDOR_DC_STARTD_H

#include "condor_common.h"
#include "condor_classad.h"
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
			@return true on success, false on failure
		 */
	bool deactivateClaim( bool graceful = true );

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

	bool locateStarter( const char* global_job_id, 
						const char* claim_id,
						ClassAd* reply, 
						int timeout = -1 );

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
