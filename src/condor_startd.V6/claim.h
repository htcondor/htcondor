/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
class CODMgr;


class ClaimId
{
public:
	ClaimId( bool is_cod = false );
	~ClaimId();

	char*	id() {return c_id;};
	char*	codId() {return c_cod_id;};
	bool	matches( const char* id );

private:
    char*   c_id;       // ClaimId string
    char*   c_cod_id;   // COD Id for this Claim (NULL if not COD)
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

	void	setuser(const char* user);
	void	setowner(const char* owner);
	void	setAccountingGroup(const char* grp);
	void	setaddr(const char* addr);
	void	sethost(const char* host);

		// send a message to the client and accountant that the claim
		// is a being vacated
	void	vacate( char* claim_id );
private:
	char	*c_owner;	// name of the owner
	char	*c_user;	// name of the user
	char	*c_acctgrp; // name of the accounting group, if any
	char	*c_host;	// hostname of the clientmachine
	char	*c_addr;	// <ip:port> of the client
};


class Claim : public Service
{
public:
	Claim( Resource*, bool is_cod = false );
	~Claim();

		// Operations you can perform on a Claim
	void vacate();	// Send a vacate command to the client of this claim
	void alive();	// Process a keep alive for this claim

	void publish( ClassAd*, amask_t );
	void publishCOD( ClassAd* );
	void publishStateTimes( ClassAd* );

	void dprintf( int, char* ... );

	void refuseClaimRequest();

		/** We finally accepted a claim, so change our state, and if
			we're opportunistic, start a timer.
		*/
	void beginClaim( void );	

		/** We accepted a request to activate the claim and spawn a
			starter.  Pull all the information we need out of the
			request and store it in this Claim object, along with any
			attributes that care about the time the job was spawned. 
		*/
	void beginActivation( time_t now ); 

		/** We're servicing a request to activate a claim and we want
			to save the request classad into our claim object for
			future use.  This method also gets some info out of the
			ClassAd we'll need to spawn the job, like the job ID.
		*/
	void saveJobInfo( ClassAd* request_ad );

		// Timer functions
	void start_match_timer();
	void cancel_match_timer();
	int  match_timed_out();		// We were matched, but not claimed in time
	void startLeaseTimer();
	void cancelLeaseTimer();
	int  leaseExpired();		// We were claimed, but didn't get a
								// keep alive in time from the schedd

		// Functions that return data
	float		rank()			{return c_rank;};
	float		oldrank()		{return c_oldrank;};
	bool		isCOD()			{return c_is_cod;};
	char*		codId()			{return c_id->codId();};
    char*       id();
    bool        idMatches( const char* id );
	Client* 	client() 		{return c_client;};
	Resource* 	rip()			{return c_rip;};
	ClassAd*	ad() 			{return c_ad;};
	int			universe()		{return c_universe;};
	int			cluster()		{return c_cluster;};
	int			proc()			{return c_proc;};
	Stream*		requestStream()	{return c_request_stream;};
	int			getaliveint()	{return c_aliveint;};
	ClaimState	state()			{return c_state;};
	float		percentCpuUsage( void );
	unsigned long	imageSize( void );
	CODMgr*		getCODMgr( void );
	bool		hasPendingCmd() {return c_pending_cmd != -1;};
	bool		hasJobAd()		{return c_has_job_ad != 0;};
	int  		pendingCmd()	{return c_pending_cmd;};
	bool		wantsRemove()	{return c_wants_remove;};

		// Functions that set the values of data
	void setrank(float rank)	{c_rank=rank;};
	void setoldrank(float rank) {c_oldrank=rank;};
	void setad(ClassAd *ad);		// Set our ad to the given pointer
	void setRequestStream(Stream* stream);	
	void setaliveint(int alive);

		// starter-related functions
	int	 spawnStarter( time_t, Stream* = NULL );
	void setStarter( Starter* s );
	void starterExited( void );
	bool starterPidMatches( pid_t starter_pid );
	bool isDeactivating( void );
	bool isActive( void );
	bool isRunning( void );	
	bool deactivateClaim( bool graceful );
	bool suspendClaim( void );
	bool resumeClaim( void );
	bool starterKill( int sig );
	bool starterKillPg( int sig );
	bool starterKillSoft( void );
	bool starterKillHard( void );
	char* makeCODStarterArgs( void );
	bool verifyCODAttrs( ClassAd* req );
	bool publishStarterAd( ClassAd* ad );

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

private:
	Resource	*c_rip;
	Client 		*c_client;
	ClaimId 	*c_id;
	ClassAd*	c_ad;
	Starter*	c_starter;
	float		c_rank;
	float		c_oldrank;
	int			c_universe;
	int			c_proc;
	int			c_cluster;
	char*		c_global_job_id;
	int			c_job_start;
	int			c_last_pckpt;
	time_t		c_entered_state;
	time_t		c_job_total_run_time;
	time_t		c_job_total_suspend_time;
	time_t		c_claim_total_run_time;
	time_t		c_claim_total_suspend_time;
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
	int			c_lease_duration; // Duration of our claim/job lease
	int			c_aliveint;		// Alive interval for this claim

	bool		c_is_cod;       // are we a COD claim or not?
	char*		c_cod_keyword;	// COD keyword for this claim, if any
	int			c_has_job_ad;	// Do we have a job ad for the COD claim?

	ClaimState	c_state;		// the state of this claim
	ClaimState	c_last_state;	// the state when a release was requested
	int			c_pending_cmd;	// the pending command, or -1 if none
	bool		c_wants_remove;	// are we trying to remove this claim?


		// Helper methods
	int  finishReleaseCmd( void );
	int  finishDeactivateCmd( void );

		/** Once the starter exits and the claim is no longer active,
			reset it by clearing out all the activation-specific data
		*/
	void resetClaim( void );
};


#endif /* _CLAIM_H */

