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
	This file defines the following classes:

	Claim, ClaimId, and Client

	A Claim object contains all of the information a startd needs
    about a given claim, such as the ClaimId, the client of this
    claim, etc.  The ClaimId in the Claim is just a pointer to a
    ClaimId object.  The client is also just a pointer to a Client
    object.  The startd maintains two Claim objects in the "rip", the
    per-resource information structure.  One for the current claim,
    and one for the possibly preempting claim that is pending.
 
	A ClaimId object contains the ClaimId string, and some
	functions to manipulate and compare against this string.  The
	constructor generates a new ClaimId with the following form:
	<ip:port>#startd-birthdate#sequence-number

	A Client object contains all the info about a given client of a
	startd.  In particular, the client name (a.k.a. "user"), the
	address ("<www.xxx.yyy.zzz:pppp>" formed ip/port), and the
	hostname.

	Originally written 9/29/97 by Derek Wright <wright@cs.wisc.edu> 

	Decided the Match object should really be called "Claim" (the
	files were renamed in cvs from Match.[Ch] to claim.[Ch], and
	renamed everything on 1/10/03 - Derek Wright
*/

#ifndef _CLAIM_H
#define _CLAIM_H

#include "enum_utils.h"
#include "Starter.h"
#include "condor_claimid_parser.h"
#include "authentication.h"
class CODMgr;


class ClaimId
{
public:
	ClaimId( ClaimType claim_type, char const *slotname );
	~ClaimId();

	char*	id() {return c_id;};
	char*	codId() {return c_cod_id;};
	bool	matches( const char* id );
	void	dropFile( int slot_id );
	char const *publicClaimId() { return claimid_parser.publicClaimId(); }
	char const *secSessionId() { return claimid_parser.secSessionId(); }

private:
    char*   c_id;       // ClaimId string
    char*   c_cod_id;   // COD Id for this Claim (NULL if not COD)
	ClaimIdParser claimid_parser;
};


class Client
{
public:
	Client();
	~Client();

	char*	name()	{return c_user;};	// For compatibility only
	char*	user()	{return c_user;};
	char*	owner()	{return c_owner;};
	char*	accountingGroup() {return c_acctgrp;};
	char*	host()	{return c_host;};
	char*	addr() 	{return c_addr;};
	char*   getConcurrencyLimits() {return c_concurrencyLimits; };
    char*   rmtgrp() {return c_rmtgrp;}
    char*   neggrp() {return c_neggrp;}
    bool    autorg() const {return c_autorg;}
	int     numPids() const {return c_numPids;};

	void	setuser(const char* user);
	void	setowner(const char* owner);
	void	setAccountingGroup(const char* grp);
	void	setaddr(const char* addr);
	void	sethost(const char* host);
	void    setConcurrencyLimits(const char* limits);
    void    setrmtgrp(const char* rmtgrp);
    void    setneggrp(const char* neggrp);
    void    setautorg(const bool autorg);
	void    setNumPids(int numJobPids);

		// send a message to the client and accountant that the claim
		// is a being vacated
	void	vacate( char* claim_id );
private:
	char	*c_owner;	// name of the owner
	char	*c_user;	// name of the user
	char	*c_acctgrp; // name of the accounting group, if any
	char	*c_host;	// hostname of the clientmachine
	char	*c_addr;	// <ip:port> of the client
	char	*c_concurrencyLimits; // limits, if any
    char*   c_rmtgrp;   // the submitter's accounting group
    char*   c_neggrp;   // the negotiating accounting group
    bool    c_autorg;   // true if negotiated via autoregroup policy
	int     c_numPids;

};


class Claim : public Service
{
public:
	Claim( Resource*, ClaimType claim_type = CLAIM_OPPORTUNISTIC,
		   int lease_duration = -1 );
	~Claim();

		// Operations you can perform on a Claim
	void vacate();	// Send a vacate command to the client of this claim
	void alive( bool alive_from_schedd = false );	// Process a keep alive for this claim

	void publish( ClassAd* );
	void publishPreemptingClaim( ClassAd* ad );
	void publishCOD( ClassAd* );
	void publishStateTimes( ClassAd* );

	void dprintf( int, const char* ... ) CHECK_PRINTF_FORMAT(3,4);

	void refuseClaimRequest();

		/** We finally accepted a claim, so change our state, and if
			we're opportunistic, start a timer.
		*/
	void beginClaim( void );	

		/** Load info used by the accountant into this object from the
			current classad.
		 */
	void loadAccountingInfo();

		/**
		 * Load information from the request (c_ad) into the client
		 * (c_client).
		 */
	void loadRequestInfo();
	
		/**
		 * Load statistics from the request (c_ad) into the client
		 * (c_client).
		 */
	void loadStatistics();

		/** 
			We're servicing a request to activate a claim and we want
			to save the request classad into our claim object for
			future use.  This method also gets some info out of the
			ClassAd we'll need to spawn the job, like the job ID.

			@param request_ad The ClassAd for the current activation.
			  If NULL, we reuse the ClassAd already in the claim object.
		*/
	void cacheJobInfo(ClassAd* job);

		// Timer functions
	void start_match_timer();
	void cancel_match_timer();
	void  match_timed_out();		// We were matched, but not claimed in time
	void startLeaseTimer();
	void cancelLeaseTimer();
	void  leaseExpired();		// We were claimed, but didn't get a
								// keep alive in time from the schedd
	void sendAlive();
	int sendAliveConnectHandler(Stream *sock);
	int sendAliveResponseHandler( Stream *sock );

	void scheddClosedClaim();

		// Functions that return data
	float		rank() const			{return c_rank;};
	float		oldrank() const		{return c_oldrank;};
	ClaimType	type()			{return c_type;};
	char*		codId()			{return c_id->codId();};
    char*       id();
	char const *secSessionId();
	char const *publicClaimId();
    bool        idMatches( const char* id );
	Client* 	client() 		{return c_client;};
	Resource* 	rip()			{return c_rip;};
	ClassAd*	ad() 			{return c_jobad;};
	ClassAd*	execution_overlay()	{return &c_job_execution_overlay_ad;};
	int			universe() const		{return c_universe;};
	int			cluster() const		{return c_cluster;};
	int			proc() const			{return c_proc;};
	Stream*		requestStream()	{return c_request_stream;};
	int			getaliveint() const	{return c_aliveint;};
	ClaimState	state()			{return c_state;};
	void		updateUsage(double & percentCpuUsage, long long & imageSize);
	CODMgr*		getCODMgr( void );
	bool		hasPendingCmd() const {return c_pending_cmd != -1;};
	bool		hasJobAd();
	int  		pendingCmd() const	{return c_pending_cmd;};
	bool		wantsRemove() const	{return c_wants_remove;};
	time_t      getJobTotalRunTime();
	time_t      getClaimAge() const;
	bool        mayUnretire() const   {return c_may_unretire;}
	bool        getRetirePeacefully() const {return c_retire_peacefully;}
	bool        preemptWasTrue() const {return c_preempt_was_true;}
	int         getPledgedMachineMaxVacateTime() const {return c_pledged_machine_max_vacate_time;}

		// Functions that set the values of data
	void setrank(float therank)	{c_rank=therank;};
	void setoldrank(float therank) {c_oldrank=therank;};
	// take ownership of the given job ad pointer.
	void setjobad(ClassAd * ad);
	void setRequestStream(Stream* stream);	
	void setaliveint(int alive);
	void disallowUnretire()     {c_may_unretire=false;}
	void setRetirePeacefully(bool value) {c_retire_peacefully=value;}
	void preemptIsTrue() {c_preempt_was_true=true;}
	void setBadputCausedByDraining() {c_badput_caused_by_draining=true;}
	bool getBadputCausedByDraining() const {return c_badput_caused_by_draining;}
	void setBadputCausedByPreemption() {c_badput_caused_by_preemption=true;}
	bool getBadputCausedByPreemption() const {return c_badput_caused_by_preemption;}
	int activationCount() const {return c_activation_count;}

		// starter-related functions
	int	 spawnStarter( Starter* starter, ClassAd * job, Stream* = NULL );
	void starterExited( Starter* starter, int status );
	bool starterPidMatches( pid_t starter_pid ) const;
	bool isDeactivating( void );
	bool isActive( void );
	bool isRunning( void );	
	bool deactivateClaim( bool graceful );
	bool suspendClaim( void );
	bool resumeClaim( void );
	bool starterSignal( int sig ) const;
	bool starterKillFamily();
	bool starterKillSoft( bool state_change = false );
	bool starterKillHard( void );
	void starterHoldJob( char const *hold_reason,int hold_code,int hold_subcode,bool soft );
	void makeStarterArgs( ArgList &args );
	bool verifyCODAttrs( ClassAd* req );
	bool publishStarterAd( ClassAd* ad ) const;

	const char * executeDir() const {
		Starter * s = findStarterByPid( c_starter_pid );
		if( s ) { return s->executeDir(); }
		else { return NULL; }
	}
	pid_t starterPID() const { return c_starter_pid; }

	bool periodicCheckpoint( void );

	bool ownerMatches( const char* owner );
	bool globalJobIdMatches( const char* id );

		/**
		   Remove this claim
		   @param graceful Gracefully release or quickly kill?
		   @return true if the claim is gone, 
		           false if we're waiting for the starter to exit
		*/
	bool removeClaim( bool graceful );

	bool setPendingCmd( int cmd );
	int	 finishPendingCmd( void );
	void changeState( ClaimState s );


		/** Write out the request's ClassAd to the given file
			descriptor.  This is used in the COD world to write the
			job ad to the starter's STDIN pipe, but might be useful
			for other things, too.
		*/
	bool writeJobAd( int fd );

		/** Write out the machine ClassAd to the provided stream
 		*/
	bool writeMachAdAndOverlay( Stream *stream );

	void receiveJobClassAdUpdate( ClassAd &update_ad, bool final_update );

		// registered callback for premature closure of connection from
		// schedd requesting this claim
	int requestClaimSockClosed(Stream *s);

	void setResource( Resource* _rip ) { c_rip = _rip; };

	bool waitingForActivation() const;
	void invalidateID();

private:
	Resource	*c_rip;
	Client 		*c_client;
	ClaimId 	*c_id;
	ClaimType	c_type;
	ClassAd*	c_jobad;
	ClassAd		c_job_execution_overlay_ad;
	pid_t		c_starter_pid;
	float		c_rank;
	float		c_oldrank;
	int			c_universe;
	int			c_cluster;
	int			c_proc;
	char*		c_global_job_id;
	double		c_job_start; // unix timestamp, or -1 if there is no time.
	int			c_last_pckpt;
	time_t      c_claim_started;
	time_t		c_entered_state;
	time_t		c_job_total_run_time;
	time_t		c_job_total_suspend_time;
	time_t		c_claim_total_run_time;
	time_t		c_claim_total_suspend_time;
	int			c_activation_count;
	Stream*		c_request_stream; // cedar sock that a remote request
                                  // is waiting for a response on

	int			c_match_tid;	// DaemonCore timer id for this
								// match.  If we're matched but not
								// claimed within 5 minutes, throw out
								// the match.

		/*  DeamonCore timer id for this claim/job lease.  If we don't
			get a keepalive before the job lease expires, schedd has
			died and we need to release the claim.
		*/
	int			c_lease_tid;
	int			c_sendalive_tid;
	Sock*		c_alive_inprogress_sock;	// NULL if no alive in progress
	int			c_lease_duration; // Duration of our claim/job lease
	int			c_aliveint;		// Alive interval for this claim
	bool		c_starter_handles_alives;  // if true, don't send alives when starter running
	bool		c_startd_sends_alives; // set by param with override by schedd via an attribute in the job.

	char*		c_cod_keyword;	// COD keyword for this claim, if any
	bool		c_has_job_ad;	// Do we have a job ad for the COD claim?

	ClaimState	c_state;		// the state of this claim
	ClaimState	c_last_state;	// the state when a release was requested
	int			c_pending_cmd;	// the pending command, or -1 if none
	bool		c_wants_remove;	// are we trying to remove this claim?
	bool        c_may_unretire;
	bool        c_retire_peacefully;
	bool        c_preempt_was_true; //was PREEMPT ever true for this claim?
	bool        c_badput_caused_by_draining; // was job preempted due to draining?
	bool        c_badput_caused_by_preemption; // was job preempted due policy, PREEMPT, RANK, user prio
	bool        c_schedd_closed_claim;
	int         c_pledged_machine_max_vacate_time; // evaluated at activation time

	// these are updated periodically when Resource::compute_condor_usage() calls updateUsage
	double c_cpus_usage;    // CpusUsage from last call to updateUsage
	long long c_image_size;	// ImageSize from last call to updateUsage

		// Helper methods
	int  finishReleaseCmd( void );
	int  finishDeactivateCmd( void );
	int  finishKillClaim( void );

		/** Once the starter exits and the claim is no longer active,
			reset it by clearing out all the activation-specific data
		*/
	void resetClaim( void );

		/**
		   We accepted a request to activate the claim and spawn a
		   starter.  Pull all the information we need out of the
		   request and store it in this Claim object, along with any
		   attributes that care about the time the job was spawned. 
		*/
	void beginActivation( double now );

	void makeCODStarterArgs( ArgList &args );
#if HAVE_JOB_HOOKS
	void makeFetchStarterArgs( ArgList &args );
#endif /* HAVE_JOB_HOOKS */

};


#endif /* _CLAIM_H */

