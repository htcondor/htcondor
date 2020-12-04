#ifndef GROUP_ENTRY_H
#define GROUP_ENTRY_H

#include "condor_common.h"
#include "compat_classad_list.h"
#include "condor_accountant.h"

#include <vector>
#include <string>
#include <map>

struct GroupEntry {
    typedef std::vector<int>::size_type size_type;

	GroupEntry();
	~GroupEntry();

    // these are set from configuration
	std::string name;
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
    // slots requested, not mutated
    double currently_requested;
    // slots allocated to this group by HGQ
    double allocated;
    // sum of slot quotas in this subtree
    double subtree_quota;
    // all slots requested by this group and its subtree
    double subtree_requested;

	// sum of usage of this node and all children
	double subtree_usage;
    // true if this group got served by most recent round robin
    bool rr;
    // timestamp of most recent allocation from round robin
    double rr_time;
    double subtree_rr_time;

    // tree structure
    GroupEntry* parent;
	std::vector<GroupEntry*> children;
	std::map<std::string, size_type, Accountant::ci_less> chmap;

    // attributes for configurable sorting
    ClassAd* sort_ad;
    double sort_key;
};
#endif
