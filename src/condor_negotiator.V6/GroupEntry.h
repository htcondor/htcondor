#ifndef GROUP_ENTRY_H
#define GROUP_ENTRY_H

#include "condor_common.h"
#include "compat_classad_list.h"
#include "condor_accountant.h"

#include <vector>
#include <string>
#include <map>
#include <functional>

typedef HashTable<std::string, float> groupQuotasHashType;

void parse_group_name(const std::string& gname, std::vector<std::string>& gpath);

struct GroupEntry {
		typedef std::vector<int>::size_type size_type;

		GroupEntry();
		~GroupEntry();

        static GroupEntry *hgq_construct_tree(
       	 	std::vector<GroupEntry*> &hgq_groups,
			bool &global_autoregroup,
			bool &global_accept_surplus);

        void hgq_assign_quotas(double quota);

		double hgq_fairshare();
		double hgq_allocate_surplus(double surplus);
		double hgq_recover_remainders();
		double hgq_round_robin(double surplus);
		double strict_enforce_quota(Accountant &accountant, GroupEntry *hgq_root_group, double slots);

		static void hgq_prepare_for_matchmaking(
				double hgq_total_quota,
				GroupEntry *hgq_root_group,
				std::vector<GroupEntry *> &hgq_groups,
				Accountant &accountant,
				ClassAdListDoesNotDeleteAds &submitterAds);

		static void hgq_negotiate_with_all_groups(
						GroupEntry *hgq_root_group, 
						std::vector<GroupEntry *> &hqg_groups, 
						groupQuotasHashType *groupQuotasHash, 
						double hgq_total_quota, 
						Accountant &accountant,
						const std::function<void(GroupEntry *g, int limit)> &fn,
						bool global_accept_surplus);

		// This maps submitter names to their assigned accounting group.
		// When called with a defined group name, it maps that group name to itself.
		static GroupEntry *GetAssignedGroup(GroupEntry *hgq_root_group, const std::string& CustomerName);
		static GroupEntry *GetAssignedGroup(GroupEntry *hgq_root_group, const std::string& CustomerName, bool &IsGroup);
		static void Initialize(GroupEntry *hgq_root_group);

		struct ci_less {
			bool operator()(const std::string& a, const std::string& b) const {
				return strcasecmp(a.c_str(), b.c_str()) < 0;
			}
		};
  		static std::map<std::string, GroupEntry*, ci_less> hgq_submitter_group_map;

		void displayGroups(int dprintfLevel, bool onlyConfigInfo, bool firstLine = true) const;

		// these are set from configuration
		std::string name;
		double config_quota;
		bool static_quota;
		bool accept_surplus;
		bool autoregroup;

		// current usage information coming into this negotiation cycle
		double usage;
		ClassAdListDoesNotDeleteAds* submitterAds;

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
		std::map<std::string, size_type, GroupEntry::ci_less> chmap;

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
double calculate_subtree_usage(Accountant &accountant, GroupEntry *group);

struct ord_by_rr_time {
		std::vector<GroupEntry*>* data;
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
