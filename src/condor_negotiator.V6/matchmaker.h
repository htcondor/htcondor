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

#include "compat_classad_list.h"
#include "condor_daemon_core.h"
#include "condor_accountant.h"
#include "condor_io.h"
#include "HashTable.h"
#include "dc_collector.h"
#include "condor_ver_info.h"
#include "matchmaker_negotiate.h"
#include "GroupEntry.h"

#include <vector>
#include <string>
#include <map>
#include <algorithm>

typedef struct MapEntry {
	char *remoteHost;
	int sequenceNum;
	ClassAd *oldAd;
} MapEntry;

/* Disable floating-point equality warnings */
GCC_DIAG_OFF(float-equal)

class Matchmaker : public Service
{
	public:
		// ctor/dtor
		Matchmaker();
		~Matchmaker();

		// initialization method (registers command handlers, etc)
		void initialize (const char *neg_name = NULL);

		// reinitialization method (reconfig)
		int reinitialize ();	

        typedef std::map<std::string, std::set<std::string> > ClaimIdHash;

		// command handlers
		int RESCHEDULE_commandHandler (int, Stream*);
		int RESET_ALL_USAGE_commandHandler(int, Stream*);
		int RESET_USAGE_commandHandler(int, Stream*);
		int DELETE_USER_commandHandler(int, Stream*);
		int SET_PRIORITYFACTOR_commandHandler(int, Stream*);
		int SET_PRIORITY_commandHandler(int, Stream*);
		int SET_CEILING_or_FLOOR_commandHandler(int, Stream*);
		int SET_FLOOR_commandHandler(int, Stream*);
		int SET_ACCUMUSAGE_commandHandler(int, Stream*);
		int SET_BEGINTIME_commandHandler(int, Stream*);
		int SET_LASTTIME_commandHandler(int, Stream*);
		int GET_PRIORITY_commandHandler(int, Stream*);
		int GET_PRIORITY_ROLLUP_commandHandler(int, Stream*);
		int GET_RESLIST_commandHandler(int, Stream*);
		int QUERY_ADS_commandHandler(int, Stream*);

		// timeout handler (for periodic negotiations)
		void negotiationTime( int timerID = -1 );

			// the order of values in this enumeration is important!
		enum PreemptState {PRIO_PREEMPTION,RANK_PREEMPTION,NO_PREEMPTION};

		/// Invalidate our negotiator ad at the collector(s).
		void invalidateNegotiatorAd( void );

		Accountant & getAccountant() { return accountant; }
		static double EvalNegotiatorMatchRank(char const *expr_name,ExprTree *expr,
		                              ClassAd &request,ClassAd *resource);

		bool getGroupInfoFromUserId(const char* user, std::string& groupName, double& groupQuota, double& groupUsage);

		void forwardAccountingData(std::set<std::string> &names);
		void forwardGroupAccounting(GroupEntry *ge);

		void calculateRanks(ClassAd &request, ClassAd *offer, PreemptState candidatePreemptState, double &candidateRankValue, double &candidatePreJobRankValue, double &candidatePostJobRankValue, double &candidatePreemptRankValue);

		void setDryRun(bool d) {m_dryrun = d;}
		bool getDryRun() const {return m_dryrun;}

    protected:
		char * NegotiatorName;
		bool NegotiatorNameInConfig;
		int update_interval;
		

	private:
		ClassAd * publicAd;
		void init_public_ad();

		int update_collector_tid;
		void updateCollector( int timerID = -1 );
		
		// auxillary functions
		bool obtainAdsFromCollector(ClassAdList &allAds, std::vector<ClassAd *> &startdAds, std::vector<ClassAd *> &submitterAds, std::set<std::string> &submitterNames, ClaimIdHash &claimIds );	
		char * compute_significant_attrs(std::vector<ClassAd *> & startdAds);
		bool consolidate_globaljobprio_submitter_ads(std::vector<ClassAd *>& submitterAds) const;

		void SetupMatchSecurity(std::vector<ClassAd *> &submitterAds);

		/**
		 * Start the network communication necessary for a negotiation cycle.
		 */
		typedef classad_shared_ptr<ResourceRequestList> RRLPtr;
		bool startNegotiateProtocol(const std::string &submitter, const ClassAd &submitterAd, ReliSock *&sock, RRLPtr &request_list);

		/**
		 * Get a resource request list for purposes of negotiation
		 */
		RRLPtr startNegotiate(const std::string &submitter, const ClassAd &submitterAd, ReliSock *&sock);
		void endNegotiate(const std::string &scheddAddr);

		/**
		 * Try starting negotiations with all schedds in parallel.
		 */
		void prefetchResourceRequestLists(std::vector<ClassAd *> &submitterAds);
		typedef std::map<std::string, classad_shared_ptr<ResourceRequestList> > RRLHash;
		RRLHash m_cachedRRLs;

		struct JobRanks {
               double PreJobRankValue;
               double PostJobRankValue;
               double PreemptRankValue;
		};

		typedef std::map<ClassAd *, JobRanks> RanksMapType;
		RanksMapType ranksMap;

		/** Negotiate w/ one schedd for one user, for one 'pie spin'.
            @param groupName name of group negotiating under (or NULL)
			@param submitterName Name attribute from the submitter ad.
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
		int negotiate(char const* groupName, char const *submitterName, const ClassAd *submitterAd,
		   double priority,
           double submitterLimit, double submitterLimitUnclaimed, int submitterCeiling,
		   std::vector<ClassAd *> &startdAds, ClaimIdHash &claimIds, 
		   bool ignore_schedd_limit, time_t deadline,
           int& numMatched, double &pieLeft);

		int negotiateWithGroup ( bool isFloorRound,
								 int untrimmed_num_startds,
								 double untrimmedSlotWeightTotal,
								 double minSlotWeight,
								 std::vector<ClassAd *>& startdAds, 
								 ClaimIdHash& claimIds,
								 std::vector<ClassAd *>& submitterAds, 
								 double groupQuota=INT_MAX, const char* groupName=NULL);

		
		ClassAd *matchmakingAlgorithm(const char* submitterName, const char* scheddAddr, const char* scheddName, ClassAd& request, std::vector<ClassAd *>& startdAds,
									  double preemptPrio, 
                                      double limitUsed, double limitUsedUnclaimed,
                                      double submitterLimit, double submitterLimitUnclaimed, 
                                      double pieLeft, bool only_for_startdrank);
		int matchmakingProtocol(ClassAd &request, ClassAd *offer, 
						ClaimIdHash &claimIds, Sock *sock,
						const char* submitterName, const char* scheddAddr);
		void calculateNormalizationFactor (std::vector<ClassAd *> &submitterAds, double &max, double &normalFactor,
										   double &maxAbs, double &normalAbsFactor);

			// Take a submitter ad from the collector and apply any changes necessary.
			// For example, we can change the submitter name based on the authenticated
			// identity in the collector.
		bool TransformSubmitterAd(classad::ClassAd &ad);

		// Check to see if any concurrency limit is violated with the given set of limits.
		// *Note* limits will be changed to lower-case.
		bool rejectForConcurrencyLimits(std::string &limits);


		/** Calculate a submitter's share of the pie.
			@param quiet Do not emitt debug information about the calculation
			@param submitterName Name attribute from the submitter ad.
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
		void calculateSubmitterLimit(const std::string &submitterName,
		                          char const *groupAccountingName,
		                          float groupQuota,
					  float groupusage,
		                          double maxPrioValue,
		                          double maxAbsPrioValue,
		                          double normalFactor,
		                          double normalAbsFactor,
								  double slotWeightTotal,
								  bool isFloorRound,
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
			@param submitterAds List of submitters
			@param groupAccountingName Group name for all of these submitters
			@param groupQuota Usage limit for this group.
			@param maxPrioValue Largest prio value of any submitter.
			@param maxAbsPrioValue Largest prio factor of any submitter
			@param normalFactor Normalization for prio values
			@param normalAbsFactor Normalization for prio factors
			@param pieLeft Sum of submitterLimits
		**/
		void calculatePieLeft( std::vector<ClassAd *> &submitterAds,
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

		void findBelowFloorSubmitters(std::vector<ClassAd *> &submitterAds, std::vector<ClassAd*> &belowFloorSubmitters);
			// rewrite the requirements expression to make matchmaking faster
		void OptimizeMachineAdForMatchmaking(ClassAd *ad);

			// rewrite the requirements expression to make matchmaking faster
		void OptimizeJobAdForMatchmaking(ClassAd *ad);

		void MakeClaimIdHash(ClassAdList &startdPvtAdList, ClaimIdHash &claimIds);
		void addRemoteUserPrios( ClassAd* ad );
		void addRemoteUserPrios( std::vector<ClassAd *> &cal );
		void insertNegotiatorMatchExprs(ClassAd *ad);
		void insertNegotiatorMatchExprs(std::vector<ClassAd *> &cal );
		void reeval( ClassAd *ad );
		void updateNegCycleEndTime(time_t startTime, ClassAd *submitter);
		friend int comparisonFunction (ClassAd *, ClassAd *,
										void *);

		friend struct submitterLessThan;

		std::vector<std::pair<ClassAd*,ClassAd*> > unmutatedSlotAds;
		std::map<std::string, ClassAd *> m_slotNameToAdMap;

		bool pslotMultiMatch(ClassAd *job, ClassAd *machine, const char* submitterName,
			bool only_startd_rank, std::string &dslot_claims, PreemptState &candidatePreemptState);

		/** trimStartdAds will throw out startd ads have no business being 
			visible to the matchmaking engine, but were fetched from the 
			collector because perhaps the accountant needs to see them.  
			This method is called after accounting completes, but before
			matchmaking begins.  trimStartdAds() calls out to subroutines
			like trimStartdAds_PreemptionLogic as needed.
			@param startdAds List of startd ads to trim
			@return the number of ads removed from the startdAds list 
		**/
		int trimStartdAds(std::vector<ClassAd *> &startdAds);
		// Note: these are called by trimStartdAds as required
		int trimStartdAds_PreemptionLogic(std::vector<ClassAd *> &startdAds) const;
		int trimStartdAds_ShutdownLogic(std::vector<ClassAd *> &startdAds);
		int trimStartdAds_ClaimedPslotLogic(std::vector<ClassAd *> &startdAds);

		bool SubmitterLimitPermits(ClassAd* request, ClassAd* candidate, double used, double allowed, double pieLeft);
		double sumSlotWeights(std::vector<ClassAd *> &startdAds,double *minSlotWeight, ExprTree* constraint);

			// Returns a pslot to the match list (after consumption policies have been applied).
			// Recalculates ranks and re-sorts match list.
			// ASSUMES NO_PREEMPTION for pslots.
		bool returnPslotToMatchList(ClassAd &request, ClassAd *offer);


		void RegisterAttemptedOfflineMatch( ClassAd *job_ad, ClassAd *startd_ad, const char *schedd_addr );

		// configuration information
		char *AccountantHost;		// who (if at all) is the accountant?
		int  NegotiatorInterval;	// max interval between negotiation cycles
		int  NegotiatorMinInterval;	// min interval between negotiation cycles
		int  NegotiatorTimeout;		// timeouts for communication
		int  MaxTimePerCycle;		// how long for total negotiation cycle
		int  MaxTimePerSubmitter;   // how long to talk to any one submitter
		int  MaxTimePerSpin;        // How long per pie spin
		int  MaxTimePerSchedd;		// How long to talk to any one schedd
		ExprTree *PreemptionReq;	// only preempt if true
		ExprTree *PreemptionRank; 	// rank preemption candidates
		bool preemption_req_unstable;
		bool preemption_rank_unstable;
		ExprTree *NegotiatorPreJobRank;  // rank applied before job rank
		ExprTree *NegotiatorPostJobRank; // rank applied after job rank
		bool want_globaljobprio;	// cached value of config knob USE_GLOBAL_JOB_PRIOS
		bool want_matchlist_caching;	// should we cache matches per autocluster?
		bool PublishCrossSlotPrios; // value of knob NEGOTIATOR_CROSS_SLOT_PRIOS, default of false
		bool ConsiderPreemption; // if false, negotiation is faster (default=true)
		bool ConsiderEarlyPreemption; // if false, do not preempt slots that still have retirement time
		bool MatchWorkingCmSlots;
		/// Should the negotiator inform startds of matches?
		bool want_inform_startd;	
		/// Should the negotiator use non-blocking connect to contact startds?
		bool want_nonblocking_startd_contact;
        char * strSlotConstraint; // query collector for machineAds with this constraint
        ExprTree *SlotPoolsizeConstraint;   // Filter machineAds by this
                                         // constraint before calculating quotas
                                         // formerly DynQuotaMachConstraint Added for CDF.

		std::string m_SubmitterConstraintStr;
		std::string m_JobConstraintStr;

		bool m_staticRanks;

		std::map<std::string, std::string> NegotiatorMatchExprs;

		std::map<std::string, int> ScheddsTimeInCycle;

		CollectorList* Collectors;

		typedef HashTable<std::string, MapEntry*> AdHash;
		AdHash *stashedAds;			

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

		// Epoch time when we started/finished most rescent negotiation cycle
		time_t startedLastCycleTime;
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
		int rejForSubmitterCeiling;   //   - not enough submitter ceiling ?
	std::set<std::string> rejectedConcurrencyLimits;
	std::string lastRejectedConcurrencyString;
		bool m_dryrun;


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
			std::string			DslotClaims;
			ClassAd *ad;
		};

		/** This class is just like ClassAdList, expept that it will
		    also invoke Matchmaker::DeleteMatchList in the destructor.
			We want this because DeleteMatchList will dereference pointers
			to ads in this ClassAdList, so this must hapen before the
			ads are deleted.
		*/
		class ClassAdList_DeleteAdsAndMatchList: public ClassAdList
		{
		public:
			ClassAdList_DeleteAdsAndMatchList(Matchmaker * const p) : 
				pMatchmaker(p) {};
			virtual ~ClassAdList_DeleteAdsAndMatchList() {
				pMatchmaker->DeleteMatchList();
			};
		private:
			Matchmaker * const pMatchmaker;
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

			ClassAd* pop_candidate(std::string &dslot_claims);
				// Return the previously-pop'd candidate back into the list.
				// Note that this assumes there is empty space in the front of the list
				// Also assume list was already sorted.
				// Returns false if there was no space (i.e., this wasn't previously pop'd).
			bool insert_candidate(ClassAd * candidate,
					double candidateRankValue,
					double candidatePreJobRankValue,
					double candidatePostJobRankValue,
					double candidatePreemptRankValue,
					PreemptState candidatePreemptState);
			bool cache_still_valid(ClassAd &request,ExprTree *preemption_req,
				ExprTree *preemption_rank,bool preemption_req_unstable, bool preemption_rank_unstable);
			void get_diagnostics(int & rejForNetwork,
					int & rejForNetworkShare,
					int & rejForConcurrencyLimit,
					int & rejPreemptForPrio,
					int & rejPreemptForPolicy,
					int & rejPreemptForRank,
					int & rejForSubmitterLimit,
					int & rejForSubmitterCeiling) const;
			void set_diagnostics(int rejForNetwork,
					int rejForNetworkShare,
					int rejForConcurrencyLimit,
					int rejPreemptForPrio,
					int rejPreemptForPolicy,
					int rejPreemptForRank,
					int rejForSubmitterLimit,
					int rejForSubmitterCeiling);
			void add_candidate(ClassAd* candidate,
					double candidateRankValue,
					double candidatePreJobRankValue,
					double candidatePostJobRankValue,
					double candidatePreemptRankValue,
					PreemptState candidatePreemptState,
					const std::string &candidateDslotClaims);
			void sort();
			int length() const { return adListLen - adListHead; }

			MatchListType(int maxlen);
			~MatchListType();

			void increment_rejForSubmitterLimit() { m_rejForSubmitterLimit++; }


		private:
			
			// AdListEntry* peek_candidate();
			static bool sort_compare(const AdListEntry &Elem1, const AdListEntry &Elem2);
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
			int m_rejForSubmitterCeiling;     //  - not enough submitter ceiling?
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
		std::vector<GroupEntry*> hgq_groups;
        bool accept_surplus;
        bool autoregroup;

        // true if resource ads with consumption policies are present
        // for the current negotiation cycle
        bool cp_resources;

		int prevLHF;

		char *slotWeightStr;
		
		#define MAX_NEGOTIATION_CYCLE_STATS 100
		class NegotiationCycleStats *negotiation_cycle_stats[MAX_NEGOTIATION_CYCLE_STATS];
		int num_negotiation_cycle_stats;

		void StartNewNegotiationCycleStat();
		void publishNegotiationCycleStats( ClassAd *ad );
};
GCC_DIAG_ON(float-equal)


#endif//__MATCHMAKER_H__
