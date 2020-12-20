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

        static GroupEntry *hgq_construct_tree(
       	 	std::map<std::string, GroupEntry*> &group_entry_map,
       	 	std::vector<GroupEntry*> &hgq_groups,
			bool &global_autoregroup,
			bool &global_accept_surplus);

        void hgq_assign_quotas(double quota);

		double hgq_fairshare();
		double hgq_allocate_surplus(double surplus);
		double hgq_recover_remainders();
		double hgq_round_robin(double surplus);

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

struct group_order {
		bool autoregroup;
		GroupEntry* root_group;

		group_order(bool arg, GroupEntry* rg): autoregroup(arg), root_group(rg) {}

		bool operator()(const GroupEntry* a, const GroupEntry* b) const {
				if (autoregroup) {
						// root is never before anybody:
						if (a == root_group) return false;
						// a != root, and b = root, so a has to be before b:
						if (b == root_group) return true;
				}
				return a->sort_key < b->sort_key;
		}

		private:
		// I don't want anybody defaulting this obj by accident
		group_order() : autoregroup(false), root_group(0) {}
};

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

extern void hgq_allocate_surplus_loop(bool by_quota,
				std::vector<GroupEntry*>& groups, std::vector<double>& allocated, std::vector<double>& subtree_requested,
				double& surplus, double& requested);
#endif
