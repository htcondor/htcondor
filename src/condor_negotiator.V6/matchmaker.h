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

			// the order of values in this enumeration is important!
		enum PreemptState {PRIO_PREEMPTION,RANK_PREEMPTION,NO_PREEMPTION};

		/// Invalidate our negotiator ad at the collector(s).
		void invalidateNegotiatorAd( void );

    protected:
		char * NegotiatorName;
		int update_interval;
		

	private:
		ClassAd * publicAd;
		void init_public_ad();

		int update_collector_tid;
		void updateCollector();

		// auxillary functions
		bool obtainAdsFromCollector (ClassAdList&, ClassAdList&, ClassAdList&, ClassAdList& );	
		
		/** Negotiate w/ one schedd for one user, for one 'pie spin'.
			@param scheddName Name attribute from the submitter ad.
			@param scheddAddr Sinful string of schedd for this submitter.
			@param priority Priority of this user from the accountant.
			@param share Priority w/o up-down (just relative prio factor).
			@param scheddLimit Give away this many matches max
			@param startdAds
			@param startdPvtAdss
			@param send_ad_to_schedd
			@param ignore_schedd_limit After hit scheddLimit, keep 
					negotiating but only consider startd rank.
			@return MM_RESUME if schedd hits its resource limit before
					negotiation finished,
					MM_DONE if schedd got all the resources it wanted,
					MM_ERROR if problem negotiating w/ this schedd.
		**/
		int negotiate( char *scheddName, char *scheddAddr, 
		   double priority, double share,
		   int scheddLimit,
		   ClassAdList &startdAds, ClassAdList &startdPvtAds, 
		   int send_ad_to_schedd, bool ignore_schedd_limit);

		int negotiateWithGroup ( ClassAdList& startdAds, 
			ClassAdList& startdPvtAds, ClassAdList& scheddAds, 
			int groupQuota=INT_MAX, const char* groupAccountingName=NULL);

		
		ClassAd *matchmakingAlgorithm(char*,char*,ClassAd&,ClassAdList&,
									  double=-1.0, double=1.0, bool=false);
		int matchmakingProtocol(ClassAd &request, ClassAd *offer, 
						ClassAdList &startdPvtAds, Sock *sock,
						char* scheddName, char* scheddAddr,
						int send_ad_to_schedd);
		void calculateNormalizationFactor (ClassAdList &, double &, double &,
										   double &, double &);
		char *getCapability (const char *, const char *, ClassAdList &);
		void addRemoteUserPrios( ClassAdList& );
		void reeval( ClassAd *ad );
		static int HashFunc(const MyString &Key, int TableSize);
		friend int comparisonFunction (AttrList *, AttrList *,
										void *);

		float EvalNegotiatorMatchRank(char const *expr_name,ExprTree *expr,
		                              ClassAd &request,ClassAd *resource);

			// If we are not considering preemption, this function will
			// trim out startd ads that are not in the Unclaimed state.
		int trimStartdAds(ClassAdList &startdAds);

		// configuration information
		char *AccountantHost;		// who (if at all) is the accountant?
		int  NegotiatorInterval;	// interval between negotiation cycles
		int  NegotiatorTimeout;		// timeouts for communication
		ExprTree *PreemptionReq;	// only preempt if true
		ExprTree *PreemptionRank; 	// rank preemption candidates
		ExprTree *NegotiatorPreJobRank;  // rank applied before job rank
		ExprTree *NegotiatorPostJobRank; // rank applied after job rank
		bool want_matchlist_caching;	// should we cache matches per autocluster?
		bool ConsiderPreemption; // if false, negotiation is faster (default=true)

		CollectorList* Collectors;

		typedef HashTable<MyString, MapEntry*> AdHash;
		AdHash *stashedAds;			
		
#ifdef WANT_NETMAN
		// allocate network capacity
		NetworkManager netman;
		bool allocNetworkShares;
#endif


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

		// Epoch time when we finished most rescent negotiation cycle
		time_t completedLastCycleTime;

		// diagnostics
		// did we reject the last match b/c of...
		int rejForNetwork; 		//   - limited network capacity?
		int rejForNetworkShare;	//   - limited network fair-share?
		int rejPreemptForPrio;	//   - insufficient prio to preempt?
		int rejPreemptForPolicy; //   - PREEMPTION_REQUIREMENTS == False?
		int rejPreemptForRank;	//   - startd RANKs new job lower?


		// Class used to store each individual entry in the
		// 'Match List' --- see class MatchListType below.
		class AdListEntry
		{
		public:
			AdListEntry() {
				RankValue = -(FLT_MAX);
				PreJobRankValue = -(FLT_MAX);
				PostJobRankValue = -(FLT_MAX);
				PreemptRankValue = -(FLT_MAX);
				PreemptStateValue = (Matchmaker::PreemptState)-1;
				ad = NULL;
			}			  
			double			RankValue;
			double			PreJobRankValue;
			double			PostJobRankValue;
			double			PreemptRankValue;
			PreemptState	PreemptStateValue;
			ClassAd *ad;
		};

		// List of matches.
		// This list is essentially a list of sorted matching
		// machine ads for a job ad of a given autocluster from
		// a given user and schedd.
		// When a job ad arrives, we store all machine ads that
		// match into this object --- a 'match list'.   We then
		// sort this list, and 'pop' off the top candidate.
		// Then if the next job the negotiator considers is the
		// same autocluster, we can just pop the next candidate
		// off of this list instead of traversing through all the
		// machine ads and resorting.
		class MatchListType
		{
		public:

			ClassAd* pop_candidate();
			void get_diagnostics(int & rejForNetwork,
					int & rejForNetworkShare,
					int & rejPreemptForPrio,
					int & rejPreemptForPolicy,
					int & rejPreemptForRank);
			void set_diagnostics(int rejForNetwork,
					int rejForNetworkShare,
					int rejPreemptForPrio,
					int rejPreemptForPolicy,
					int rejPreemptForRank);
			void add_candidate(ClassAd* candidate,
					double candidateRankValue,
					double candidatePreJobRankValue,
					double candidatePostJobRankValue,
					double candidatePreemptRankValue,
					PreemptState candidatePreemptState);
			void sort();
			int length() { return adListLen - adListHead; }

			MatchListType(int maxlen);
			~MatchListType();

		private:
			static int sort_compare(const void*, const void*);
			AdListEntry* AdListArray;			
			int adListLen;
			int adListHead;
			// rejection reasons
			int m_rejForNetwork; 		//   - limited network capacity?
			int m_rejForNetworkShare;	//   - limited network fair-share?
			int m_rejPreemptForPrio;	//   - insufficient prio to preempt?
			int m_rejPreemptForPolicy; //   - PREEMPTION_REQUIREMENTS == False?
			int m_rejPreemptForRank;	//   - startd RANKs new job lower?
			
		};
		MatchListType* MatchList;
		int cachedAutoCluster;
		char* cachedName;
		char* cachedAddr;

		//
		class SimpleGroupEntry
		{
		public:
			SimpleGroupEntry();
			~SimpleGroupEntry();
			char *groupName;
			float prio;
			int maxAllowed;
			int usage;
			ClassAdList submitterAds;			
		};
		static int groupSortCompare(const void*, const void*);
		
		
		
};



#endif//__MATCHMAKER_H__
