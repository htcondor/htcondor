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
#ifndef __MATCHMAKER_H__
#define __MATCHMAKER_H__

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_accountant.h"
#include "condor_io.h"
#ifdef WANT_NETMAN
#include "../condor_netman/netman.h"
#endif

class Matchmaker : public Service
{
	public:
		// ctor/dtor
		Matchmaker();
		~Matchmaker();

		// initialization method (registers command handlers, etc)
		void initialize ();

		// reinitialization method (reconfig)
		int reinitialize ();	

		// command handlers
		int RESCHEDULE_commandHandler (int, Stream*);
		int RESET_ALL_USAGE_commandHandler(int, Stream*);
		int RESET_USAGE_commandHandler(int, Stream*);
		int SET_PRIORITYFACTOR_commandHandler(int, Stream*);
		int SET_PRIORITY_commandHandler(int, Stream*);
		int GET_PRIORITY_commandHandler(int, Stream*);
		int GET_RESLIST_commandHandler(int, Stream*);
#ifdef WANT_NETMAN
		int REQUEST_NETWORK_commandHandler(int, Stream*);
#endif

		// timeout handler (for periodic negotiations)
		int negotiationTime ();

	private:
		// auxillary functions
		bool obtainAdsFromCollector (ClassAdList&, ClassAdList&, ClassAdList&, ClassAdList&);	
		int  negotiate(char *, char *, double, double, int,
			ClassAdList &, ClassAdList &, int, bool );
		ClassAd *matchmakingAlgorithm(char*,char*,ClassAd&,ClassAdList&,
									  double=-1.0, double=1.0, bool=false);
		int matchmakingProtocol(ClassAd &request, ClassAd *offer, 
						ClassAdList &startdPvtAds, Sock *sock,
						char* scheddName, char* scheddAddr,
						int send_ad_to_schedd);
		void calculateNormalizationFactor (ClassAdList &, double &, double &,
										   double &, double &);
		char *getCapability (char *, char *, ClassAdList &);
		void addRemoteUserPrios( ClassAdList& );

		friend int comparisonFunction (ClassAd *, ClassAd *, void *);

		// configuration information
		char *AccountantHost;		// who (if at all) is the accountant?
		int  NegotiatorInterval;	// interval between negotiation cycles
		int  NegotiatorTimeout;		// timeouts for communication
		ExprTree *PreemptionReq;	// only preempt if true
		ExprTree *PreemptionRank; 	// rank preemption candidates

#ifdef WANT_NETMAN
		// allocate network capacity
		NetworkManager netman;
		bool allocNetworkShares;
#endif

		// diagnostics
									// did we reject the last match b/c of
		bool rejForNetwork; 		//   - limited network capacity?
		bool rejForNetworkShare;	//   - limited network fair-share?
		bool rejPreemptForPrio;		//   - insufficient prio to preempt?
		bool rejPreemptForPolicy; 	//   - PREEMPTION_REQUIREMENTS == False?
		bool rejPreemptForRank;		//   - startd RANKs new job lower?

		// rank condition on matches
		ExprTree *rankCondStd;// no preemption or machine rank-preemption 
							  // i.e., (Rank > CurrentRank)
		ExprTree *rankCondPrioPreempt;// prio preemption (Rank >= CurrentRank)

		// the accounting subsystem
		Accountant accountant;

		// Cache of socket connections to schedds
		SocketCache	*sockCache;

		// DaemonCore Timer ID for periodic negotiations
		int negotiation_timerID;
		bool GotRescheduleCmd;
};


#endif//__MATCHMAKER_H__
