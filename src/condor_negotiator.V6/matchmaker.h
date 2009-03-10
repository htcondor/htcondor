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

#ifndef __MATCHMAKER_H__
#define __MATCHMAKER_H__

#include "condor_daemon_core.h"
#include "condor_accountant.h"
#include "condor_io.h"
#include "HashTable.h"
#include "string_list.h"
#include "dc_collector.h"
#include "condor_ver_info.h"

/* FILESQL include */
#include "file_sql.h"
#ifdef WANT_NETMAN
#include "../condor_netman/netman.h"
#endif

typedef struct MapEntry {
	char *remoteHost;
	int sequenceNum;
	ClassAd *oldAd;
} MapEntry;
/* ODBC object extern */
//extern ODBC *DBObj;
/* FILESQL object extern */
extern FILESQL *FILEObj;
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

		typedef HashTable<MyString, MyString> ClaimIdHash;

		// command handlers
		int RESCHEDULE_commandHandler (int, Stream*);
		int RESET_ALL_USAGE_commandHandler(int, Stream*);
		int RESET_USAGE_commandHandler(int, Stream*);
		int DELETE_USER_commandHandler(int, Stream*);
		int SET_PRIORITYFACTOR_commandHandler(int, Stream*);
		int SET_PRIORITY_commandHandler(int, Stream*);
		int SET_ACCUMUSAGE_commandHandler(int, Stream*);
		int SET_BEGINTIME_commandHandler(int, Stream*);
		int SET_LASTTIME_commandHandler(int, Stream*);
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

		Accountant & getAccountant() { return accountant; }
		static float EvalNegotiatorMatchRank(char const *expr_name,ExprTree *expr,
		                              ClassAd &request,ClassAd *resource);

    protected:
		char * NegotiatorName;
		int update_interval;

	private:
		ClassAd * publicAd;
		void init_public_ad();

		int update_collector_tid;
		void updateCollector();

		// auxillary functions
		bool obtainAdsFromCollector (ClassAdList&, ClassAdList&, ClassAdList&, ClaimIdHash& );	
		char * compute_significant_attrs(ClassAdList & startdAds);
		
		/** Negotiate w/ one schedd for one user, for one 'pie spin'.
			@param scheddName Name attribute from the submitter ad.
			@param scheddAddr Sinful string of schedd for this submitter.
			@param priority Priority of this user from the accountant.
			@param share Priority w/o up-down (just relative prio factor).
			@param scheddLimit Give away this many matches max
			@param startdAds
			@param claimIds
			@param scheddVersion
			@param ignore_schedd_limit After hit scheddLimit, keep 
					negotiating but only consider startd rank.
			@param numMatched on return this is set to number of machines
			        matched to this submitter.
			@return MM_RESUME if schedd hits its resource limit before
					negotiation finished,
					MM_DONE if schedd got all the resources it wanted,
					MM_ERROR if problem negotiating w/ this schedd.
		**/
		int negotiate( char const *scheddName, const ClassAd *scheddAd, 
		   double priority, double share,
		   int scheddLimit,
		   ClassAdList &startdAds, ClaimIdHash &claimIds, 
		   const CondorVersionInfo & scheddVersion,
		   bool ignore_schedd_limit, time_t startTime, int &numMatched);

		int negotiateWithGroup ( int untrimmed_num_startds,
			ClassAdList& startdAds, 
			ClaimIdHash& claimIds, ClassAdList& scheddAds, 
			int groupQuota=INT_MAX, const char* groupAccountingName=NULL);

		
		ClassAd *matchmakingAlgorithm(const char*,const char*,ClassAd&,ClassAdList&,
									  double=-1.0, double=1.0, bool=false);
		int matchmakingProtocol(ClassAd &request, ClassAd *offer, 
						ClaimIdHash &claimIds, Sock *sock,
						const char* scheddName, const char* scheddAddr);
		void calculateNormalizationFactor (ClassAdList &, double &, double &,
										   double &, double &);

		/** Calculate a submitter's share of the pie.
			@param quiet Do not emitt debug information about the calculation
			@param scheddName Name attribute from the submitter ad.
			@param groupAccountingName Group name from the submitter ad.
			@param groupQuota Usage limit for this group.
			@param numStartdAds Size of the pie in this spin.
			@param maxPrioValue Largest prio value of any submitter.
			@param maxAbsPrioValue Largest prio factor of any submitter
			@param normalFactor Normalization for prio values
			@param normalAbsFactor Normalization for prio factors

			@param scheddLimit Resulting submitter share of this pie
			@param scheddUsage Set to number of slots claimed by this submitter
			@param scheddShare Resulting fractional share by prio and factor
			@param scheddAbsShare Resulting fractional share by prio factor
			@param scheddPrio User priority
			@param scheddPrioFactor Result is this submitter's prio factor
			@param scheddLimitRoundoff Difference between ideal share of pie
			                           and rounded integer share.
		**/
		void calculateScheddLimit(char const *scheddName,
		                          char const *groupAccountingName,
		                          int groupQuota,
		                          int numStartdAds,
		                          double maxPrioValue,
		                          double maxAbsPrioValue,
		                          double normalFactor,
		                          double normalAbsFactor,
		                            /* result parameters: */
		                          int &scheddLimit,
		                          int &scheddUsage,
		                          double scheddShare,
		                          double &scheddAbsShare,
		                          double &scheddPrio,
		                          double &scheddPrioFactor,
		                          double &scheddLimitRoundoff );

		/** Calculate a submitter's share of the pie.
			@param quiet Do not emitt debug information about the calculation
			@param scheddAds List of submitters
			@param groupAccountingName Group name for all of these submitters
			@param groupQuota Usage limit for this group.
			@param numStartdAds Size of the pie in this spin.
			@param maxPrioValue Largest prio value of any submitter.
			@param maxAbsPrioValue Largest prio factor of any submitter
			@param normalFactor Normalization for prio values
			@param normalAbsFactor Normalization for prio factors
			@param userprioCrumbs Resulting number of batch slots in this
			                      pie which are left over after handing out
			                      rounded integer shares.
		**/
		void calculateUserPrioCrumbs( ClassAdList &scheddAds,
		                              char const *groupAccountingName,
		                              int groupQuota,
		                              int numStartdAds,
		                              double maxPrioValue,
		                              double maxAbsPrioValue,
		                              double normalFactor,
		                              double normalAbsFactor,
		                                   /* result parameters: */
		                              int &userprioCrumbs );

		void MakeClaimIdHash(ClassAdList &startdPvtAdList, ClaimIdHash &claimIds);
		char const *getClaimId (const char *, const char *, ClaimIdHash &, MyString &);
		void addRemoteUserPrios( ClassAd* ad );
		void insertNegotiatorMatchExprs(ClassAd *ad);
		void insertNegotiatorMatchExprs( ClassAdList &cal );
		void reeval( ClassAd *ad );
		void updateNegCycleEndTime(time_t startTime, ClassAd *submitter);
		static unsigned int HashFunc(const MyString &Key);
		friend int comparisonFunction (AttrList *, AttrList *,
										void *);

			// If we are not considering preemption, this function will
			// trim out startd ads that are not in the Unclaimed state.
		int trimStartdAds(ClassAdList &startdAds);

		/* ODBC insert functions */
		void insert_into_rejects(char const *userName, ClassAd& job);
		void insert_into_matches(char const *userName, ClassAd& request, ClassAd& offer);
		
		// configuration information
		char *AccountantHost;		// who (if at all) is the accountant?
		int  NegotiatorInterval;	// interval between negotiation cycles
		int  NegotiatorTimeout;		// timeouts for communication
		int  MaxTimePerSubmitter;   // how long to talk to any one submitter
		int  MaxTimePerSpin;        // How long per pie spin
		ExprTree *PreemptionReq;	// only preempt if true
		ExprTree *PreemptionRank; 	// rank preemption candidates
		bool preemption_req_unstable;
		bool preemption_rank_unstable;
		ExprTree *NegotiatorPreJobRank;  // rank applied before job rank
		ExprTree *NegotiatorPostJobRank; // rank applied after job rank
		bool want_matchlist_caching;	// should we cache matches per autocluster?
		bool ConsiderPreemption; // if false, negotiation is faster (default=true)
		/// Should the negotiator inform startds of matches?
		bool want_inform_startd;	
		/// Should the negotiator use non-blocking connect to contact startds?
		bool want_nonblocking_startd_contact;
        ExprTree *DynQuotaMachConstraint;   // Filter machineAds by this
                                            // constraint before calculating
                                            // dynamic quotas.  Added for CDF.

		StringList NegotiatorMatchExprNames;
		StringList NegotiatorMatchExprValues;

		CollectorList* Collectors;

		typedef HashTable<MyString, MapEntry*> AdHash;
		AdHash *stashedAds;			

		typedef HashTable<MyString, int> groupQuotasHashType;
		groupQuotasHashType *groupQuotasHash;

		bool getGroupInfoFromUserId( const char *user, int & groupQuota, 
			 int & groupUsage );
		
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

		// external references in startd ads ... used for autoclustering
		char * job_attr_references;

		// Epoch time when we finished most rescent negotiation cycle
		time_t completedLastCycleTime;

		// diagnostics
		// did we reject the last match b/c of...
		int rejForNetwork; 		//   - limited network capacity?
		int rejForNetworkShare;	//   - limited network fair-share?
		int rejForConcurrencyLimit;	//   - limited concurrency?
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
			bool cache_still_valid(ClassAd &request,ExprTree *preemption_req,
				ExprTree *preemption_rank,bool preemption_req_unstable, bool preemption_rank_unstable);
			void get_diagnostics(int & rejForNetwork,
					int & rejForNetworkShare,
					int & rejForConcurrencyLimit,
					int & rejPreemptForPrio,
					int & rejPreemptForPolicy,
					int & rejPreemptForRank);
			void set_diagnostics(int rejForNetwork,
					int rejForNetworkShare,
					int rejForConcurrencyLimit,
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

			// AdListEntry* peek_candidate();
			static int sort_compare(const void*, const void*);
			AdListEntry* AdListArray;			
			int adListMaxLen;	// max length of AdListArray
			int adListLen;		// current length of AdListArray
			int adListHead;
			bool already_sorted;
			// rejection reasons
			int m_rejForNetwork; 		//   - limited network capacity?
			int m_rejForNetworkShare;	//   - limited network fair-share?
			int m_rejForConcurrencyLimit;	//   - limited concurrency?
			int m_rejPreemptForPrio;	//   - insufficient prio to preempt?
			int m_rejPreemptForPolicy; //   - PREEMPTION_REQUIREMENTS == False?
			int m_rejPreemptForRank;	//   - startd RANKs new job lower?
			
		};
		MatchListType* MatchList;
		int cachedAutoCluster;
		char* cachedName;
		char* cachedAddr;
		double cachedPrio;
		bool cachedOnlyForStartdRank;

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
		
		char RejectsTable[40];
		char MatchesTable[40];
		int prevLHF;
};



#endif//__MATCHMAKER_H__
