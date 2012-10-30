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

#include <vector>
#include <string>
#include <map>
#include <algorithm>

/* FILESQL include */
#include "file_sql.h"

typedef struct MapEntry {
	char *remoteHost;
	int sequenceNum;
	ClassAd *oldAd;
} MapEntry;
/* ODBC object extern */
//extern ODBC *DBObj;
/* FILESQL object extern */
extern FILESQL *FILEObj;

struct GroupEntry {
    typedef vector<int>::size_type size_type;

	GroupEntry();
	~GroupEntry();

    // these are set from configuration
	string name;
    double config_quota;
	bool static_quota;
	bool accept_surplus;
    bool autoregroup;

    // current usage information coming into this negotiation cycle
    double usage;
    ClassAdListDoesNotDeleteAds* submitterAds;
    double priority;

    // slot quota as computed by HGQ
    double quota;
    // slots requested: jobs submitted against this group
    double requested;
    // slots allocated to this group by HGQ
    double allocated;
    // sum of slot quotas in this subtree
    double subtree_quota;
    // all slots requested by this group and its subtree
    double subtree_requested;
    // true if this group got served by most recent round robin
    bool rr;
    // timestamp of most recent allocation from round robin
    double rr_time;
    double subtree_rr_time;

    // tree structure
    GroupEntry* parent;
    vector<GroupEntry*> children;
    map<string, size_type, Accountant::ci_less> chmap;

    // attributes for configurable sorting
    ClassAd* sort_ad;
    double sort_key;
};

/* Disable floating-point equality warnings */

GCC_DIAG_OFF(float-equal)

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
		int GET_PRIORITY_ROLLUP_commandHandler(int, Stream*);
		int GET_RESLIST_commandHandler(int, Stream*);

		// timeout handler (for periodic negotiations)
		void negotiationTime ();

			// the order of values in this enumeration is important!
		enum PreemptState {PRIO_PREEMPTION,RANK_PREEMPTION,NO_PREEMPTION};

		/// Invalidate our negotiator ad at the collector(s).
		void invalidateNegotiatorAd( void );

		Accountant & getAccountant() { return accountant; }
		static float EvalNegotiatorMatchRank(char const *expr_name,ExprTree *expr,
		                              ClassAd &request,ClassAd *resource);

		bool getGroupInfoFromUserId(const char* user, string& groupName, float& groupQuota, float& groupUsage);

    protected:
		char * NegotiatorName;
		int update_interval;
		

	private:
		ClassAd * publicAd;
		void init_public_ad();

		int update_collector_tid;
		void updateCollector();

		// auxillary functions
		bool obtainAdsFromCollector (ClassAdList&, ClassAdListDoesNotDeleteAds&, ClassAdListDoesNotDeleteAds&, ClaimIdHash& );	
		char * compute_significant_attrs(ClassAdListDoesNotDeleteAds & startdAds);
		bool consolidate_globaljobprio_submitter_ads(ClassAdListDoesNotDeleteAds & scheddAds);
		
		/** Negotiate w/ one schedd for one user, for one 'pie spin'.
            @param groupName name of group negotiating under (or NULL)
			@param scheddName Name attribute from the submitter ad.
			@param scheddAddr Sinful string of schedd for this submitter.
			@param priority Priority of this user from the accountant.
			@param share Priority w/o up-down (just relative prio factor).
			@param submitterLimit Give away this many matches max
			@param startdAds
			@param claimIds
			@param scheddVersion
			@param ignore_schedd_limit After hit submitterLimit, keep 
					negotiating but only consider startd rank.
			@param numMatched on return this is set to number of machines
			        matched to this submitter.
			@return MM_RESUME if schedd hits its resource limit before
					negotiation finished,
					MM_DONE if schedd got all the resources it wanted,
					MM_ERROR if problem negotiating w/ this schedd.
		**/
		int negotiate(char const* groupName, char const *scheddName, const ClassAd *scheddAd, 
		   double priority,
           double submitterLimit, double submitterLimitUnclaimed,
		   ClassAdListDoesNotDeleteAds &startdAds, ClaimIdHash &claimIds, 
		   bool ignore_schedd_limit, time_t startTime, 
           int& numMatched, double &pieLeft);

		int negotiateWithGroup ( int untrimmed_num_startds,
								 double untrimmedSlotWeightTotal,
								 double minSlotWeight,
			ClassAdListDoesNotDeleteAds& startdAds, 
			ClaimIdHash& claimIds, ClassAdListDoesNotDeleteAds& scheddAds, 
			float groupQuota=INT_MAX, const char* groupName=NULL);

		
		ClassAd *matchmakingAlgorithm(const char* scheddName, const char* scheddAddr, ClassAd& request, ClassAdListDoesNotDeleteAds& startdAds,
									  double preemptPrio, 
                                      double limitUsed, double limitUsedUnclaimed,
                                      double submitterLimit, double submitterLimitUnclaimed, 
                                      double pieLeft, bool only_for_startdrank);
		int matchmakingProtocol(ClassAd &request, ClassAd *offer, 
						ClaimIdHash &claimIds, Sock *sock,
						const char* scheddName, const char* scheddAddr);
		void calculateNormalizationFactor (ClassAdListDoesNotDeleteAds &, double &, double &,
										   double &, double &);

		/** Calculate a submitter's share of the pie.
			@param quiet Do not emitt debug information about the calculation
			@param scheddName Name attribute from the submitter ad.
			@param groupAccountingName Group name from the submitter ad.
			@param groupQuota Usage limit for this group.
			@param maxPrioValue Largest prio value of any submitter.
			@param maxAbsPrioValue Largest prio factor of any submitter
			@param normalFactor Normalization for prio values
			@param normalAbsFactor Normalization for prio factors
			@param slotWeightTotal Size of the pie in this spin.

			@param submitterLimit Resulting submitter share of this pie
			@param submitterUsage Set to number of slots claimed by this submitter
			@param submitterShare Resulting fractional share by prio and factor
			@param submitterAbsShare Resulting fractional share by prio factor
			@param submitterPrio User priority
			@param submitterPrioFactor Result is this submitter's prio factor
		**/
		void calculateSubmitterLimit(char const *scheddName,
		                          char const *groupAccountingName,
		                          float groupQuota,
					  float groupusage,
		                          double maxPrioValue,
		                          double maxAbsPrioValue,
		                          double normalFactor,
		                          double normalAbsFactor,
								  double slotWeightTotal,
		                            /* result parameters: */
								  double &submitterLimit,
                                  double& submitterLimitUnclaimed,
								  double &submitterUsage,
		                          double &submitterShare,
		                          double &submitterAbsShare,
		                          double &submitterPrio,
		                          double &submitterPrioFactor);

		/** Calculate how much pie might be dished out in this round.
			@param quiet Do not emitt debug information about the calculation
			@param scheddAds List of submitters
			@param groupAccountingName Group name for all of these submitters
			@param groupQuota Usage limit for this group.
			@param maxPrioValue Largest prio value of any submitter.
			@param maxAbsPrioValue Largest prio factor of any submitter
			@param normalFactor Normalization for prio values
			@param normalAbsFactor Normalization for prio factors
			@param pieLeft Sum of submitterLimits
		**/
		void calculatePieLeft( ClassAdListDoesNotDeleteAds &scheddAds,
		                       char const *groupAccountingName,
		                       float groupQuota,
				       float groupusage,
		                       double maxPrioValue,
		                       double maxAbsPrioValue,
		                       double normalFactor,
		                       double normalAbsFactor,
		                       double slotWeightTotal,
		                            /* result parameters: */
		                       double &pieLeft);

			// rewrite the requirements expression to make matchmaking faster
		void OptimizeMachineAdForMatchmaking(ClassAd *ad);

			// rewrite the requirements expression to make matchmaking faster
		void OptimizeJobAdForMatchmaking(ClassAd *ad);

		void MakeClaimIdHash(ClassAdList &startdPvtAdList, ClaimIdHash &claimIds);
		char const *getClaimId (const char *, const char *, ClaimIdHash &, MyString &);
		void addRemoteUserPrios( ClassAd* ad );
		void addRemoteUserPrios( ClassAdListDoesNotDeleteAds &cal );
		void insertNegotiatorMatchExprs(ClassAd *ad);
		void insertNegotiatorMatchExprs( ClassAdListDoesNotDeleteAds &cal );
		void reeval( ClassAd *ad );
		void updateNegCycleEndTime(time_t startTime, ClassAd *submitter);
		static unsigned int HashFunc(const MyString &Key);
		friend int comparisonFunction (AttrList *, AttrList *,
										void *);

			// If we are not considering preemption, this function will
			// trim out startd ads that are not in the Unclaimed state.
		int trimStartdAds(ClassAdListDoesNotDeleteAds &startdAds);

		bool SubmitterLimitPermits(ClassAd *candidate, double used, double allowed, double pieLeft);
		double sumSlotWeights(ClassAdListDoesNotDeleteAds &startdAds,double *minSlotWeight, ExprTree* constraint);

		/* ODBC insert functions */
		void insert_into_rejects(char const *userName, ClassAd& job);
		void insert_into_matches(char const *userName, ClassAd& request, ClassAd& offer);
		

		void RegisterAttemptedOfflineMatch( ClassAd *job_ad, ClassAd *startd_ad );

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
		bool want_globaljobprio;	// cached value of config knob USE_GLOBAL_JOB_PRIOS
		bool want_matchlist_caching;	// should we cache matches per autocluster?
		bool ConsiderPreemption; // if false, negotiation is faster (default=true)
		/// Should the negotiator inform startds of matches?
		bool want_inform_startd;	
		/// Should the negotiator use non-blocking connect to contact startds?
		bool want_nonblocking_startd_contact;
        char * strSlotConstraint; // query collector for machineAds with this constraint
        ExprTree *SlotPoolsizeConstraint;   // Filter machineAds by this
                                         // constraint before calculating quotas
                                         // formerly DynQuotaMachConstraint Added for CDF.

		StringList NegotiatorMatchExprNames;
		StringList NegotiatorMatchExprValues;

		CollectorList* Collectors;

		typedef HashTable<MyString, MapEntry*> AdHash;
		AdHash *stashedAds;			

		typedef HashTable<MyString, float> groupQuotasHashType;
		groupQuotasHashType *groupQuotasHash;

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
		int rejForSubmitterLimit;   //   - not enough group quota?
        string rejectedConcurrencyLimit; // the name of concurrency limit rejected


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

		void DeleteMatchList();

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
					int & rejPreemptForRank,
					int & rejForSubmitterLimit);
			void set_diagnostics(int rejForNetwork,
					int rejForNetworkShare,
					int rejForConcurrencyLimit,
					int rejPreemptForPrio,
					int rejPreemptForPolicy,
					int rejPreemptForRank,
					int rejForSubmitterLimit);
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

			void increment_rejForSubmitterLimit() { m_rejForSubmitterLimit++; }

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
			int m_rejPreemptForRank;    //   - startd RANKs new job lower?
			int m_rejForSubmitterLimit;     //  - not enough group quota?
			float m_submitterLimit;
			
			
		};
		MatchListType* MatchList;
		int cachedAutoCluster;
		char* cachedName;
		char* cachedAddr;
		double cachedPrio;
		bool cachedOnlyForStartdRank;

        // set at startup/restart/reinit
        GroupEntry* hgq_root_group;
        string hgq_root_name;
        vector<GroupEntry*> hgq_groups;
        map<string, GroupEntry*> group_entry_map;
        bool accept_surplus;
        bool autoregroup;
        bool allow_quota_oversub;

        void hgq_construct_tree();
        void hgq_assign_quotas(GroupEntry* group, double quota);
        double hgq_fairshare(GroupEntry* group);
        double hgq_allocate_surplus(GroupEntry* group, double surplus);
        double hgq_recover_remainders(GroupEntry* group);
        double hgq_round_robin(GroupEntry* group, double surplus);

        struct ord_by_rr_time {
            vector<GroupEntry*>* data;
            bool operator()(unsigned long const& ja, unsigned long const& jb) const {
                GroupEntry* a = (*data)[ja];
                GroupEntry* b = (*data)[jb];
                if (a->subtree_rr_time != b->subtree_rr_time) return a->subtree_rr_time < b->subtree_rr_time;
                if (a->subtree_quota != b->subtree_quota) return a->subtree_quota > b->subtree_quota;
                return a->subtree_requested > b->subtree_requested;
            }
        };

		int prevLHF;

		#define MAX_NEGOTIATION_CYCLE_STATS 100
		class NegotiationCycleStats *negotiation_cycle_stats[MAX_NEGOTIATION_CYCLE_STATS];
		int num_negotiation_cycle_stats;

		void StartNewNegotiationCycleStat();
		void publishNegotiationCycleStats( ClassAd *ad );
};
GCC_DIAG_ON(float-equal)


#endif//__MATCHMAKER_H__
