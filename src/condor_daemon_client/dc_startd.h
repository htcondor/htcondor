/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department,
 *University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.
 *No use of the CONDOR Software Program Source Code is authorized
 *without the express consent of the CONDOR Team.  For more
 *information contact: CONDOR Team, Attention: Professor Miron Livny,
 *7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685,
 *(608) 262-0856 or miron@cs.wisc.edu.
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

#ifndef _CONDOR_DC_STARTD_H
#define _CONDOR_DC_STARTD_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "enum_utils.h"


class DCStartd : public Daemon {
public:

		/** Constructor.  Same as a Daemon object.
		  @param name The name (or sinful string) of the daemon, NULL
		              if you want local  
		  @param pool The name of the pool, NULL if you want local
		*/
	DCStartd( const char* const name = NULL, const char* pool = NULL );

		/// Destructor.
	~DCStartd();

		/** Set the capability for use when talking to this startd.
			This method is deprecated.  You should use setClaimId(),
			instead.  
			@param cap_str The capability string
			@return true on success, false on invalid input (NULL)
		*/
	bool setCapability( const char* cap_str );

		/** Set the ClaimId to use when talking to this startd. 
			@param id The ClaimID string
			@return true on success, false on invalid input (NULL)
		*/
	bool setClaimId( const char* id );


		/** @return the capability string for this startd, NULL if we
			don't have a value yet.  This method is deprecated.  You
			should use getClaimId(), instead.  

		*/
	char* getCapability( void ) { return claim_id; };
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
