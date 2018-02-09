#ifndef _QUEUE_INTERNAL_H
#define _QUEUE_INTERNAL_H

#include <map>
#include <vector>
#include "expr_analyze.h"
#include "adcluster.h"

#define CONDOR_Q_HANDLE_CLUSTER_AD 1

#if 1 // cant use unique_ptr<> in a std::map on Rhel6, so for now, use our own class

// this class is a bit like unique_ptr<ClassAd> but works on Rhel6 because it doesn't poison the copy constructor
// The copy constructor in this class asserts if the pointer being copied is not NULL.
// also, assignment is has SWAP behavior. This works for our limited use case.
// This class safe to use in a std::map or std::set when the code to populate the map/set uses the pattern
//   auto pp = map->insert(std::pair<KEYTYPE, UniqueClassAdPtr>(key,UniqueClassAdPtr())
//   if ( ! pp.second) {
//      // insert failed, map[key] already has a value
//   } else {
//      // write the ClassAd* pointer into the map entry
//      pp.first->second.reset(ad);
//   }
// This works because the copy constructor is only invoked on instances of the class that hold a null pointer
class UniqueClassAdPtr {
	ClassAd * ptr;
public:
	UniqueClassAdPtr(ClassAd* p=NULL) : ptr(p) {}
	~UniqueClassAdPtr() { delete ptr; ptr = NULL; }

	ClassAd* get() const { return ptr; }
	ClassAd* operator->() const { return ptr; }
	ClassAd& operator*() const { return *ptr; }
	operator bool() const { return ptr != NULL; }

	ClassAd* detach() { ClassAd* p = ptr; ptr = NULL; return p; }
	void reset(ClassAd* p) { if (p != ptr) { delete ptr; } ptr = p; }

	// copy constructor works only when the pointer being copied is NULL
	UniqueClassAdPtr(const UniqueClassAdPtr& that) : ptr(NULL) { ASSERT(!that.ptr); }

	// move constructor steals the pointer from the input
	//UniqueClassAdPtr(UniqueClassAdPtr&& that) : ptr(that.detach()) {} // move ptr from that to this

	// assignment operator swaps pointers with the input
	//friend void swap(UniqueClassAdPtr& first, UniqueClassAdPtr& second) { ClassAd* t=first.ptr; first.ptr = second.ptr; second.ptr = t; }
	//UniqueClassAdPtr & operator=(UniqueClassAdPtr that) { swap(*this, that); return *this; }
};

#else

// For fully c++ 11 compilers/runtimes we can just use unique_ptr
typedef std::unique_ptr<ClassAd> UniqueClassAdPtr;

#endif

typedef std::map< long long, UniqueClassAdPtr > IdToClassaAdMap;
typedef std::map< std::string, UniqueClassAdPtr > KeyToClassaAdMap;

struct 	PrioEntry { MyString name; float prio; };
int read_userprio_file(const char *filename, ExtArray<PrioEntry> & prios);

/* Warn about schedd-wide limits that may confuse analysis code */
bool warnScheddGlobalLimits(DaemonAllowLocateFull *schedd,MyString &result_buf);

// these are in queue.cpp
bool iter_ads_from_file(const char *filename,
	bool (*pfnProcess)(void* pv, ClassAd* ad), void* pvProcess,
	CondorClassAdFileParseHelper & parse_helper, ExprTree * constraint);
void profile_print(size_t & cbBefore, double & tmBefore, int cAds, bool fCacheStats=true);
int getDisplayWidth();


/* counters for job matchmaking analysis */
typedef struct {
	int fReqConstraint;   // # of slots that don't match job Requirements
	int fOffConstraint;   // # of slots that match Job requirements, but refuse the job because of their own Requirements
	int fPreemptPrioCond; // match but are serving users with a better priority in the pool
	int fRankCond;        // match but reject the job for unknown reasons
	int fPreemptReqTest;  // match but will not currently preempt their existing job
	int fOffline;         // match but are currently offline
	int available;        // are available to run your job
	int exhausted_partionable; // partitionable slots that match requirements, but do not fit WithinResourceLimits
	int job_matches_slot; // # of slots that match the job's requirements (used to detect no matches)
	int both_match;       // # of slots that match both ways (used to detect no matches)
	int totalMachines;
	int machinesRunningJobs; // number of machines serving other users
	int machinesRunningUsersJobs; 

	void clear() { memset((void*)this, 0, sizeof(*this)); }
} anaCounters;

/* lists of machines for matchmaking analysis */
class anaMachines {
public:
	/*
	std::string ReqConstraintStr;
	std::string OffConstraintStr;
	std::string ExhaustedStr;
	std::string OfflineStr;
	std::string PreemptPrioStr;
	std::string PreemptReqTestStr;
	std::string RankCondStr;
	*/
	anaMachines() : limit(100) {} // maximum length of each list
	enum entry { ReqConstraint, OffConstraint, Exhausted, Offline, PreemptPrio, PreemptReqTest, RankCond };
	void append_to_fail_list(entry ety, const char * machine);
	int print_fail_list(std::string & out, entry ety);
	size_t limit;
	std::string alist[RankCond+1];
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

int			findSubmittor(const char *);
int			fetchSubmittorPriosFromNegotiator(DCCollector* pool, ExtArray<PrioEntry> & prios);
void		printJobRunAnalysis(ClassAd*, DaemonAllowLocateFull*, int details, bool withUserprio, int PslotMode=2);
enum {
	anaMatchModeNo=0,
	anaMatchModeAlways=1,
	anaMatchModePslot=2,
	anaMatchModePandDslot=3
};
bool doJobRunAnalysis(ClassAd *request, DaemonAllowLocateFull* schedd, std::string &status, int mode, anaCounters & ac, anaPrio * prio, anaMachines * pmat);
const char * appendJobRunAnalysisToBuffer(std::string & out, ClassAd *job, std::string &job_status);
const char * appendJobRunAnalysisToBuffer(std::string & out, ClassAd *job, anaCounters & ac, anaPrio * prio, anaMachines * pmat);
const char	*doJobMatchAnalysisToBuffer(std::string & return_buf, ClassAd *request, int details);

// map of Autocluster id's to jobs
typedef std::map<int, std::vector<ClassAd *> > JobClusterMap;
void		doSlotRunAnalysis(ClassAd*, JobClusterMap & clusters, int console_width, int analyze_detail_level, bool tabular);
const char	*doSlotRunAnalysisToBuffer(ClassAd*, JobClusterMap & clusters, int console_width, int analyze_detail_level, bool tabular);
void		buildJobClusterMap(IdToClassaAdMap & jobs, const char * attr, JobClusterMap & clusters);

int setupAnalysis(
	CollectorList * Collectors,
	const char *machineads_file, ClassAdFileParseType::ParseType machineads_file_format,
	const char *user_slot_constraint);
void setupUserpriosForAnalysis(DCCollector* pool, const char *userprios_file);
void cleanupAnalysis();

// this is in queue.cpp
bool print_jobs_analysis(
	IdToClassaAdMap & jobs,
	const char * source_label,
	DaemonAllowLocateFull * pschedd_daemon);

// shared global state (controls output)
extern bool widescreen;
extern bool verbose;
extern bool dash_profile;
extern KeyToClassaAdMap startdAds;
extern ExtArray<PrioEntry> prioTable;
extern int longest_slot_machine_name;
extern int longest_slot_name;
extern bool single_machine;
extern const char* single_machine_label;

#endif /* _CONDOR_SETFLAGS_H */
