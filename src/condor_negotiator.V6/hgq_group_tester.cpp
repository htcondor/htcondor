#include "GroupEntry.h"

#include "compat_classad_list.h"
#include "condor_common.h"
#include "condor_config.h"

#include <algorithm>
// Test accountant
// Just enough of the accountant to test

std::map<std::string, float> usage;

float
Accountant::GetWeightedResourcesUsed(const std::string &name) {
	float result = usage[name];
	printf("GetWeightedResourcesUsed called for %s returning %g\n", name.c_str(), result);
	return result;
}

bool
Accountant::UsingWeightedSlots() const {return true;}

Accountant::Accountant() : concurrencyLimits(hashFunction) {}
Accountant::~Accountant() {}

int main(int /*argc*/, char ** /*argv*/) {

	//const char * root_config = "ONLY_ENV";
	//int config_options = CONFIG_OPT_WANT_META | CONFIG_OPT_USE_THIS_ROOT_CONFIG | CONFIG_OPT_NO_EXIT;
	//config_host(NULL, config_options, root_config);
	config();

	dprintf_set_tool_debug("TOOL", 0);

	bool autoregroup = false;
	bool accept_surplus = false;

	std::vector<GroupEntry*> hgq_groups;
	groupQuotasHashType *groupQuotasHash = new groupQuotasHashType(hashFunction);

	int total_available_cores = 100;
	classad::ClassAdParser	parser;
	ClassAdListDoesNotDeleteAds submitterAds;

	std::string adtext;
	ClassAd *ad = nullptr;

	/*
	adtext = "[ Name = \"group_a.user1@chevre.cs.wisc.edu\"; WeightedIdleJobs = 100; WeightedRunningJobs = 7;] ";
	ad = parser.ParseClassAd(adtext);
	usage["group_a"] = 7;
	submitterAds.Insert(ad);

	adtext = "[ Name = \"group_a.user2@chevre.cs.wisc.edu\"; WeightedIdleJobs = 2000; WeightedRunningJobs = 7;] ";
	ad = parser.ParseClassAd(adtext);
	usage["group_a"] += 7;
	submitterAds.Insert(ad);
	*/

	adtext = "[ Name = \"group_b.user3@chevre.cs.wisc.edu\"; WeightedIdleJobs = 100; WeightedRunningJobs = 7;] ";
	ad = parser.ParseClassAd(adtext);
	usage["group_b"] = 7;
	submitterAds.Insert(ad);

	adtext = "[ Name = \"group_a.user4@chevre.cs.wisc.edu\"; WeightedIdleJobs = 100; WeightedRunningJobs = 1;] ";
	ad = parser.ParseClassAd(adtext);
	usage["group_a"] = 1;
	submitterAds.Insert(ad);

	auto callback = [](GroupEntry *g, int quota) {
		float currentUsage = usage[g->name];
		int new_usage = std::min(int(currentUsage + g->currently_requested), quota);
		usage[g->name] = new_usage;
		printf("Callback fired for group %s with %d submitters with limit %d requested is %g new_usage is %d\n", g->name.c_str(),g->submitterAds->Length(), quota, g->currently_requested, new_usage);
	};

	Accountant accountant;
	GroupEntry *root_group = GroupEntry::hgq_construct_tree(hgq_groups, autoregroup, accept_surplus);

	GroupEntry::hgq_prepare_for_matchmaking(total_available_cores, root_group, hgq_groups, accountant, submitterAds);

	GroupEntry::hgq_negotiate_with_all_groups(
			root_group, 
			hgq_groups,
			groupQuotasHash,
			total_available_cores,
			accountant,
			callback,
			accept_surplus);

	dprintf(D_ALWAYS, "After negotiate with all, groups look like\n");
	root_group->displayGroups(D_ALWAYS, true);

	return 0;
}
