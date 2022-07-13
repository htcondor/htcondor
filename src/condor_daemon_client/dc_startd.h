/***************************************************************
 *
 * Copyright (C) 1990-2012, Condor Team, Computer Sciences Department,
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
#include "condor_claimid_parser.h"


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
			  const char* const addr, const char* const id,
			  const char* const ids = NULL);

	DCStartd( const ClassAd *ad, const char *pool = NULL );

		/// Destructor.
	~DCStartd();

		/** Set the ClaimId to use when talking to this startd. 
			@param id The ClaimID string
			@return true on success, false on invalid input (NULL)
		*/
	bool setClaimId( const char* id );
	bool setClaimId( const std::string &id ) {return setClaimId(id.c_str());}

		/** @return the ClaimId string for this startd, NULL if we
			don't have a value yet.
		*/
	const char* getClaimId( void ) { return claim_id; };

		/** This is the old-style way of requesting a claim, not the
			"generic ClassAd" way, which currently only supports COD
			claims.  In the terminology of the "generic ClassAd" way,
			all non-COD claims are "opportunistic" claims, so that is
			why this function is named the way it is.

			You MUST call setClaimId() before calling this function.

		    @param req_ad The initial job ad for this claim
		    @param description for informative logging
		    @param scheduler_addr Address to tell startd for contacting schedd
		    @param alive_interval Seconds
		    @param timeout Socket timeout.
		    @param cb - callback object

			To see the result of the request, check claimed_startd_success()
			in the DCStartdMsg object, which may be obtained from the callback
			object at callback time.
		*/
	void asyncRequestOpportunisticClaim( ClassAd const *req_ad, char const *description, char const *scheduler_addr, int alive_interval, int timeout, int deadline_timeout, classy_counted_ptr<DCMsgCallback> cb );

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

		/** Swap an active claim in an execute slot with the
			inactive claim in a transfer slot.
			The source of the swap is identified by claim id, the
			destination is identified by slot name. Slots can only
			swap claims if there is a pre-established relationship
			between them in the startd as indicated by each slot
			having the name of the other slot in their SlotPairName
			attribute.
			If the claim_id and dest_slot_name both identify the
			same slot, this is considered trivial success, not a failure.

			@param claim_id Claim id of the source slot of the swap
			@param src_descrip Description of the source slot for logging
			@param dest_slot_name Slot name of the destination slot of the swap
			@param timeout Socket timeout
			@param cb Callback object

			To see the result of the request, check swap_claims_success()
			in the DCStartdMsg object, which may be obtained from the callback
			object at callback time.
		*/
	void asyncSwapClaims(const char * claim_id, char const *src_descrip,
						const char * dest_slot_name,
						int timeout, classy_counted_ptr<DCMsgCallback> cb);

		/** Before activating a claim, attempt to delegate the user proxy
			(if there is one). We used do this from the shadow if
			GLEXEC_STARTER was set, since if the startd requires that
			glexec be used to spawn the starter, then it needs to get
			the proxy early (since glexec uses it to determine what UID
			to run the starter under). The normal way of delegating the
			proxy via file transfer happens when the starter is already
			running, and thus too late.
			@param proxy Location of user proxy to delegate
			@expiration_time 0 if none; o.w. time of delegated proxy expiration
			@result_expiration_time if non-NULL, gets set to delegated proxy expiration, which may be shorter than requested if proxy expires sooner
			@return OK if proxy is delegated, NOT_OK if startd doesn't
			need it, CONDOR_ERROR on error
		*/
	int delegateX509Proxy( const char* proxy, time_t expiration_time, time_t *result_expiration_time );

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

	/**
	 * Non-COD based suspendClaim
	 */
	bool _suspendClaim ();
	
	/**
	 * Non-COD based continueClaim
	 */
	bool _continueClaim();
	
	bool vacateClaim( const char* name );

	bool checkpointJob( const char* name );

	bool getAds( ClassAdList &adsList );

		// request_id: set to the request id (can be used to cancel request)
		// returns: true/false on success/failure
		// call error() to get a descriptive error message
		// how_fast: DRAIN_GRACEFUL, DRAIN_QUICK, DRAIN_FAST
		// check_expr: optional expression that must be true for all slots to be drained or request fails
		// start_expr: optional, specifies START expression to use while draining
	bool drainJobs(int how_fast,const char * reason,int on_completion,char const *check_expr,char const *start_expr,std::string &request_id);

		// request_id: the draining request id
		// returns: true/false on success/failure
		// call error() to get a descriptive error message
	bool cancelDrainJobs(char const *request_id);

	bool updateMachineAd( const ClassAd * update, ClassAd * reply, int timeout = -1 );

 private:
	char* claim_id;
	char* extra_ids;

		// Helper methods
	bool checkClaimId( void );
	bool checkVacateType( VacateType t );

		// I can't be copied (yet)
	DCStartd( const DCStartd& );
	DCStartd& operator = ( const DCStartd& );

};

class ClaimStartdMsg: public DCMsg {
public:
	ClaimStartdMsg( char const *claim_id, char const *extra_ids, ClassAd const *job_ad, char const *description, char const *scheduler_addr, int alive_interval );

		// Functions that override DCMsg
	bool writeMsg( DCMessenger *messenger, Sock *sock );
	bool readMsg( DCMessenger *messenger, Sock *sock );
	MessageClosureEnum messageSent(DCMessenger *messenger, Sock *sock );
	void cancelMessage(char const *reason=NULL);

	char const *description() {return m_description.c_str();}
	char const *claim_id() {return m_claim_id.c_str();}

		// Message results:
	bool claimed_startd_success() const { return m_reply == OK; }
	char const *startd_ip_addr() {return m_startd_ip_addr.c_str();}
	char const *startd_fqu() {return m_startd_fqu.c_str();}
	bool have_leftovers() const { return m_have_leftovers; }
	char const *leftover_claim_id() { return m_leftover_claim_id.c_str(); }
	ClassAd * leftover_startd_ad() 
		{ return m_have_leftovers ? &m_leftover_startd_ad : NULL; }

	bool have_paired_slot() const { return m_have_paired_slot; }
	char const *paired_claim_id() { return m_paired_claim_id.c_str(); }
	ClassAd *paired_startd_ad()
		{ return m_have_paired_slot ? &m_paired_startd_ad : NULL; }

	const ClassAd *getJobAd() { return &m_job_ad;}
	bool putExtraClaims(Sock *sock);
private:
	std::string m_claim_id;
	std::string m_extra_claims;
	ClassAd m_job_ad;
	std::string m_description;
	std::string m_scheduler_addr;
	int m_alive_interval;

		// the startd's reply:
	int m_reply;

		// if claiming a repartitionable slot, the startd
		// may send over the newly created repatitionable slot with
		// the leftover unclaimed resources.
	bool m_have_leftovers;
	std::string m_leftover_claim_id;
	ClassAd m_leftover_startd_ad;

		// If claiming a paired static slot, the startd will send over
		// the other slot's ad and claim id.
	bool m_have_paired_slot;
	std::string m_paired_claim_id;
	ClassAd m_paired_startd_ad;

	std::string m_startd_ip_addr;
	std::string m_startd_fqu;
};

class SwapClaimsMsg: public DCMsg {
public:
	SwapClaimsMsg( char const *claim_id, const char *src_descrip, const char * dest_slot_name);

		// Functions that override DCMsg
	bool writeMsg( DCMessenger *messenger, Sock *sock );
	bool readMsg( DCMessenger *messenger, Sock *sock );
	MessageClosureEnum messageSent(DCMessenger *messenger, Sock *sock );
	void cancelMessage(char const *reason=NULL);

	char const *description() {return m_description.c_str();}
	char const *claim_id() {return m_claim_id.c_str();}
	char const *dest_slot_name() {return m_dest_slot_name.c_str();}

		// Message results:
	bool swap_claims_success() const { return m_reply == OK || m_reply == SWAP_CLAIM_ALREADY_SWAPPED; }
private:
	std::string m_claim_id;
	std::string m_description;
	std::string m_dest_slot_name;
	ClassAd m_opts;

		// the startd's reply:
	int m_reply;
};

/*
 * DCClaimIdMsg is used in cases where we are sending a single claim id
 */
class DCClaimIdMsg: public DCMsg {
public:
	DCClaimIdMsg( int cmd, char const *claim_id );

	bool writeMsg( DCMessenger *messenger, Sock *sock );
	bool readMsg( DCMessenger *messenger, Sock *sock );

private:
	std::string m_claim_id;
};

class NotifyStartdOfMatchHandler {
public:
	std::string m_startdName;
	std::string  m_startdAddr;
	std::string  m_claim_id;
	int m_timeout;
	DCStartd m_startd;
	bool m_nonblocking;

	NotifyStartdOfMatchHandler(char const *startdName,char const *startdAddr,int timeout,char const *claim_id,bool nonblocking):
		
		m_startdName(startdName),
		m_startdAddr(startdAddr),
		m_claim_id(claim_id),
		m_timeout(timeout),
		m_startd(startdAddr),
		m_nonblocking(nonblocking) {}

	static void startCommandCallback(bool success,Sock *sock,CondorError * /*errstack*/,
		const std::string & /*trust_domain*/, bool /*should_try_token_request*/, void *misc_data)
	{
		NotifyStartdOfMatchHandler *self = (NotifyStartdOfMatchHandler *)misc_data;
		ASSERT(misc_data);

		if(!success) {
			dprintf (D_ALWAYS,"      Failed to initiate socket to send MATCH_INFO to %s\n",
					 self->m_startdName.c_str());
		}
		else {
			self->WriteMatchInfo(sock);
		}
		if(sock) {
			delete sock;
		}
		delete self;
	}

	bool WriteMatchInfo(Sock *sock) const
	{
		ClaimIdParser idp( m_claim_id.c_str() );
		ASSERT(sock);

		// pass the startd MATCH_INFO and claim id string
		dprintf (D_FULLDEBUG, "      Sending MATCH_INFO/claim id to %s\n",
		         m_startdName.c_str());
		dprintf (D_FULLDEBUG, "      (Claim ID is \"%s\" )\n",
		         idp.publicClaimId() );

		if ( !sock->put_secret (m_claim_id.c_str()) ||
			 !sock->end_of_message())
		{
			dprintf (D_ALWAYS,
			        "      Could not send MATCH_INFO/claim id to %s\n",
			        m_startdName.c_str() );
			dprintf (D_FULLDEBUG,
			        "      (Claim ID is \"%s\")\n",
			        idp.publicClaimId() );
			return false;
		}
		return true;
	}

	bool startCommand()
	{
		dprintf (D_FULLDEBUG, "      Connecting to startd %s at %s\n",
					m_startdName.c_str(), m_startdAddr.c_str());

		if(!m_nonblocking) {
			Stream::stream_type st = m_startd.hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
			Sock *sock =  m_startd.startCommand(MATCH_INFO,st,m_timeout);
			bool result = false;
			if(!sock) {
				dprintf (D_ALWAYS,"      Failed to initiate socket (blocking mode) to send MATCH_INFO to %s\n",
						 m_startdName.c_str());
			}
			else {
				result = WriteMatchInfo(sock);
			}
			if(sock) {
				delete sock;
			}
			delete this;
			return result;
		}

		Stream::stream_type st = m_startd.hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
		m_startd.startCommand_nonblocking (
			MATCH_INFO,
			st,
			m_timeout,
			NULL,
			NotifyStartdOfMatchHandler::startCommandCallback,
			this);

			// Since this is nonblocking, we cannot give any immediate
			// feedback on whether the message to the startd succeeds.
		return true;
	}
};

#endif /* _CONDOR_DC_STARTD_H */
