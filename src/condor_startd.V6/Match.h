/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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
/*  
	This file defines the following classes:

	Match, Capability, and Client

	A Match object contains all of the information a startd needs
    about a given match, such as the capability, the client of this
    match, etc.  The capability in the match is just a pointer to a
    Capability object.  The client is also just a pointer to a Client
    object.  The startd maintains two Match objects in the "rip", the
    per-resource information structure.  One for the current match,
    and one for the possibly preempting match that is pending.
 
	A Capability object contains the capability string, and some
	functions to manipulate and compare against this string.  The
	constructor generates a new capability with the following form:
	<ip:port>#random_integer

	A Client object contains all the info about a given client of a
	startd.  In particular, the client name (a.k.a. "user"), the
	address ("<www.xxx.yyy.zzz:pppp>" formed ip/port), and the
	hostname.

	Written 9/29/97 by Derek Wright <wright@cs.wisc.edu> 
*/

#ifndef _MATCH_H
#define _MATCH_H


class Capability
{
public:
	Capability();
	~Capability();

	char*	capab() {return c_capab;};
	int		matches(char* capab);	// Return 1 if given capab matches
									// current, 0 if not.
private:
	char*	c_capab;	// capability string
};


class Client
{
public:
	Client();
	~Client();

	char*	name()	{return c_name;};
	char*	user()	{return c_name;};	// For compatibility only
	char*	host()	{return c_host;};
	char*	addr() 	{return c_addr;};

	void	setname(char* name);
	void	setaddr(char* addr);
	void	sethost(char* host);

		// send a message to the client and accountant that the match
		// is a being vacated
	void	vacate(char* cap);
private:
	char	*c_name;	// name of the client, a.k.a. "user"
	char	*c_host;	// hostname of the clientmachine
	char	*c_addr;	// <ip:port> of the client
};


class Match : public Service
{
public:
	Match( Resource* );
	~Match();

		// Operations you can perform on a Match
	void vacate();	// Send a vacate command to the client of this match
	void alive();	// Process a keep alive for this match
		// Send the given cmd to the accountant, followed by the
		// capability of this match. 
	int	send_accountant( int );	

	void publish( ClassAd*, amask_t );

	void dprintf( int, char* ... );

	void refuse_agent();

		// Timer functions
	void start_match_timer();
	void cancel_match_timer();
	int  match_timed_out();		// We were matched, but not claimed in time
	void start_claim_timer();
	void cancel_claim_timer();
	int  claim_timed_out(); 	// We were claimed, but didn't get a
								// keep alive in time from the schedd

		// Functions that return data
	float		rank()			{return m_rank;};
	float		oldrank()		{return m_oldrank;};
	char*		capab() 		{return m_cap->capab();};
	Client* 	client() 		{return m_client;};
	Capability* cap()			{return m_cap;};
	ClassAd*	ad() 			{return m_ad;};
	int			universe()		{return m_universe;};
	Stream*		agentstream()	{return m_agentstream;};
	int			cluster()		{return m_cluster;};
	int			proc()			{return m_proc;};
	int			job_start() 	{return m_job_start;};
	int			last_pckpt() 	{return m_last_pckpt;};

		// Functions that set the values of data
	void setrank(float rank)	{m_rank=rank;};
	void setoldrank(float rank) {m_oldrank=rank;};
	void setad(ClassAd *ad);		// Set our ad to the given pointer
	void deletead(void);
	void setuniverse(int universe)	{m_universe=universe;};
	void setagentstream(Stream* stream);	
	void setaliveint(int alive)		{m_aliveint=alive;};
	void setproc(int proc) 			{m_proc=proc;};
	void setcluster(int cluster)	{m_cluster=cluster;};
	void setlastpckpt(int lastckpt) {m_last_pckpt=lastckpt;};
	void setjobstart(int jobstart) 	{m_job_start=jobstart;};

private:
	Resource	*rip;
	Client 		*m_client;
	Capability 	*m_cap;
	ClassAd*	m_ad;
	float		m_rank;
	float		m_oldrank;
	int			m_universe;
	int			m_proc;
	int			m_cluster;
	int			m_job_start;
	int			m_last_pckpt;
	Stream*		m_agentstream;	// cedar sock that the schedd agent is
								// waiting for a response on

	int			m_match_tid;	// DaemonCore timer id for this
								// match.  If we're matched but not
								// claimed within 5 minutes, throw out
								// the match.
	int			m_claim_tid;	// DeamonCore timer id for this
								// claim.  If we don't get a keep
								// alive in 3 alive intervals, the
								// schedd has died and we need to
								// release the claim.
	int			m_aliveint;		// Alive interval for this match
};


#endif _MATCH_H

