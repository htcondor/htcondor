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
#ifndef __MATCHMAKER_H__
#define __MATCHMAKER_H__

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_accountant.h"
#include "condor_io.h"
#include "HashTable.h"
#include "dc_collector.h"

#ifdef WANT_NETMAN
#include "../condor_netman/netman.h"
#endif

typedef struct MapEntry {
	char *remoteHost;
	int sequenceNum;
	ClassAd *oldAd;
} MapEntry;

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
		int DELETE_USER_commandHandler(int, Stream*);
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
		bool obtainAdsFromCollector (ClassAdList&, ClassAdList&, ClassAdList&, ClassAdList& );	
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
		void reeval( ClassAd *ad );
		static int HashFunc(const MyString &Key, int TableSize);
		friend int comparisonFunction (AttrList *, AttrList *,
										void *);

		float EvalNegotiatorMatchRank(char const *expr_name,ExprTree *expr,
		                              ClassAd &request,ClassAd *resource);


		// configuration information
		char *AccountantHost;		// who (if at all) is the accountant?
		int  NegotiatorInterval;	// interval between negotiation cycles
		int  NegotiatorTimeout;		// timeouts for communication
		ExprTree *PreemptionReq;	// only preempt if true
		ExprTree *PreemptionRank; 	// rank preemption candidates
		ExprTree *NegotiatorPreJobRank;  // rank applied before job rank
		ExprTree *NegotiatorPostJobRank; // rank applied after job rank

		DaemonList* Collectors;

		typedef HashTable<MyString, MapEntry*> AdHash;
		AdHash *stashedAds;	
		
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
