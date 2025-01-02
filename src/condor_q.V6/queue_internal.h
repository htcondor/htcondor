#ifndef _QUEUE_INTERNAL_H
#define _QUEUE_INTERNAL_H

#include <map>
#include <vector>
#include <deque>
#include "expr_analyze.h"
#include "adcluster.h"

#define CONDOR_Q_HANDLE_CLUSTER_AD 1
#define TWENTYFOUR_ANAL_IMPROVEMENTS 24

typedef std::unique_ptr<ClassAd> UniqueClassAdPtr;

// structure to track sets of related classads
struct RelatedClassads {

	class AnnotatedClassAd {
	public:
		UniqueClassAdPtr ad;
		int index{-1};     // set to the index of the Ad within the deque
		int id{-1};        // slot_id or cluster_id
		int sub_id{-1};    // optional dslot_id or proc_id
		int group_id{-1};  // group id, used for autocluster_id or for slot matching subgroup
		
		ClassAd * get() { return ad.get(); } // to avoid code churn
	};

	std::deque< AnnotatedClassAd > allAds;   // the deque owns the ads
	std::map< long long, int >      adsById;  // map of ads by id
	std::map< std::string, int >    adsByKey; // map of ads sorted by key

	// these are used to group ads, for slot ads we group by machine, for job ads by (autocluster?)
	// the map is used to map the grouping string to a group_id, the vector is indexed by group_id
	std::map< std::string, int >    groupMap;
	class GroupCounters {
	public:
		unsigned short tot{0};              // total slots in group

		unsigned char  major:4{0};          // total p-slots in group

		unsigned char  considered:1{0};     // considered at least one slot
		unsigned char  within_limits:1{0};  // job fits within resource limits of a p-slot without needing an overlay
		unsigned char  forward_match:1{0};  // job_matches at least 1 slot
		unsigned char  reverse_match:1{0};  // at leaset 1 slot matches job

		unsigned char  both_match:1{0};     // at least  1 slot has forward and reverse match
		unsigned char  you_use:1{0};        // you are using this machine
		unsigned char  other_use:1{0};      // machine is in use by others
		unsigned char  offline:1{0};        // machine is offline

		unsigned char  backfill:1{0};       // machine has backfill
		unsigned char  drain_sim:1{0};      // used an overlay to simulate draining
	};
	std::vector<GroupCounters>      groupIdCount;
	int numGroups{0};
	int numStatic{0};
	int numDynamic{0};
	int numOverlayCandidates{0};
	int numOverlayAds{0};

	size_t size() { return allAds.size(); }
	void clear() { 
		groupMap.clear();
		groupIdCount.clear();
		adsById.clear();
		adsByKey.clear();
		allAds.clear();
	}
};

// jobid to ad map
typedef std::map< long long, UniqueClassAdPtr > IdToClassaAdMap;


struct 	PrioEntry { std::string name; float prio; };
int read_userprio_file(const char *filename, std::vector<PrioEntry> & prios);

/* Warn about schedd-wide limits that may confuse analysis code */
bool warnScheddGlobalLimits(DaemonAllowLocateFull *schedd,std::string &result_buf);

// these are in queue.cpp
bool iter_ads_from_file(const char *filename,
	bool (*pfnProcess)(void* pv, ClassAd* ad), void* pvProcess,
	CondorClassAdFileParseHelper & parse_helper, ExprTree * constraint);
void profile_print(size_t & cbBefore, double & tmBefore, int cAds, bool fCacheStats=true);
int getDisplayWidth();


/* counters for job matchmaking analysis */
typedef struct AnalysisCounters {
	int fReqConstraint;   // # of slots that don't match job Requirements
	int fOffConstraint;   // # of slots that match Job requirements, but refuse the job because of their own Requirements
	int fPreemptPrioCond; // match but are serving users with a better priority in the pool
	int fRankCond;        // match but reject the job for unknown reasons
	int fPreemptReqTest;  // match but will not currently preempt their existing job
	int fOffline;         // match but are currently offline
	int available_now;    // match now
	int available_if_drained; // match if drain overlay is applied
	int job_matches_slot; // # of slots that match the job's requirements (used to detect no matches)
	int both_match;       // # of slots that match both ways (used to detect no matches)

	int totalSlots;       // total slots considered by the basic match check (may be less than slots fetched)
	int slotsRunningJobs;     // number of slots running jobs (including yours)
	int slotsRunningYourJobs; // number of slots running your jobs

	int totalUniqueMachines;
	int totalDSlotsSkipped; // dslots that we skipped doing match counts on

	void clear() { memset((void*)this, 0, sizeof(*this)); }
} anaCounters;

// collections of machine indexes by match category (for verbose)
class anaMachines {
public:
	enum entry { ReqConstraint, OffConstraint, Exhausted, Offline, PreemptPrio, PreemptReqTest, RankCond, Willing, WillingIfDrained };
	void append_to_list(entry ety, RelatedClassads::AnnotatedClassAd & machine);
	int print_list(std::string & out, entry ety, const char * label, RelatedClassads & adlist, int limit=10);
	bool empty_list(entry ety) { return lists.size() < ety || lists[ety].empty(); }
	int print_fail_list(std::string & out, entry ety);
	std::vector< std::vector<int> > lists;
};

class anaPrio {
public:
	int ixSubmittor;
	int niceUser;
	anaPrio() : ixSubmittor(-1), niceUser(0) {}
};

class CollectorList;
class DCCollector;

// for run failure analysis

enum {
	anaModeNone = 0,
	anaModeAnal,	// -anal
	anaModeBetter,	// -better-anal
	anaModeSummary,	// -better-sum
	anaModeUnmatchable, // -unmatchable
};

int			findSubmittor(const std::string &);
int			fetchSubmittorPriosFromNegotiator(DCCollector* pool, FILE* capture_fp, std::vector<PrioEntry> & prios);

enum {
	anaMatchModeNo=0,
	anaMatchModeAlways=1,
	anaMatchModePslot=2,
	anaMatchModePandDslot=3
};
void		printJobRunAnalysis(ClassAd*, DaemonAllowLocateFull*, int details, bool withUserprio, int PslotMode=anaMatchModePslot);

bool doJobRunAnalysis(ClassAd *request, DaemonAllowLocateFull* schedd, std::string &status, int mode, anaCounters & ac, anaPrio * prio, anaMachines * pmat);
// analysis report based on job status
const char * appendBasicJobRunAnalysisToBuffer(std::string & out, ClassAd *job, std::string &job_status);
// analysis report based on counts of machines
const char * appendJobMatchTotalsToBuffer(std::string & out, ClassAd *job, anaCounters & ac, anaPrio * prio, anaMachines * pmat);
// detailed analysis of Requirements expression clauses
const char	*doJobMatchAnalysisToBuffer(std::string & return_buf, ClassAd *request, int details, int mode);

// map of Autocluster id's to jobs
typedef std::map<int, std::vector<ClassAd *> > JobClusterMap;
void		doSlotRunAnalysis(ClassAd*, JobClusterMap & clusters, int console_width, int analyze_detail_level, bool tabular);
const char	*doSlotRunAnalysisToBuffer(std::string & return_buf, ClassAd*, JobClusterMap & clusters, int console_width, int analyze_detail_level, bool tabular);
void		buildJobClusterMap(IdToClassaAdMap & jobs, const char * attr, JobClusterMap & clusters);

int setupAnalysis(
	CollectorList * Collectors,
	const char *machineads_file, ClassAdFileParseType::ParseType machineads_file_format,
	const char *user_slot_constraint,
	const char *user_slot_machine);
void setupUserpriosForAnalysis(DCCollector* pool, FILE* capture_fp, const char *userprios_file);
void cleanupAnalysis();

// thease are in queue.cpp
bool print_jobs_analysis(
	IdToClassaAdMap & jobs,
	const char * source_label,
	DaemonAllowLocateFull * pschedd_daemon);
bool dump_long_to_fp(void * pv, ClassAd *job); // used for capture_raw

// shared global state (controls output)
extern bool widescreen;
extern bool verbose;
extern bool dash_profile;
extern struct RelatedClassads startdAds;
extern FILE* capture_raw_fp;
extern int longest_slot_machine_name;
extern int longest_slot_name;
extern bool single_machine;
extern const char* single_machine_label;

#endif /* _CONDOR_SETFLAGS_H */
