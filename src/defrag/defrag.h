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

#ifndef _DEFRAG_H
#define _DEFRAG_H

#include <set>
#include "condor_common.h"
#include "condor_daemon_core.h"
#include "defrag_stats.h"

// Defrag is a daemon that schedules the draining of machines
// according to a configurable policy.  The intention is to
// defragment partitionable slots so that jobs requiring
// whole machines do not starve.

class Defrag: public Service {
 public:
	Defrag();
	~Defrag();

	void init();
	void config();
	void stop();

	void poll(); // do the periodic policy evaluation

	typedef std::set< std::string > MachineSet;

 private:

	int m_polling_interval; // delay between evaluations of the policy
	int m_polling_timer;
	double m_draining_per_hour;
	int m_draining_per_poll;
	int m_draining_per_poll_hour;
	int m_draining_per_poll_day;
	int m_max_draining;
	int m_max_whole_machines;
	std::string m_whole_machine_expr;
	std::string m_defrag_requirements;
	std::string m_draining_start_expr;
	ClassAd m_rank_ad;
	classad::References m_drain_attrs;  // attributes needed to evaluate draining and DEFRAG_RANK expression
	int m_draining_schedule;
	std::string m_draining_schedule_str;
	std::string m_cancel_requirements; // Requirements to stop a drain.

	time_t m_last_poll;

	std::string m_state_file;

	std::string m_defrag_name;
	std::string m_daemon_name;
	int m_public_ad_update_interval;
	int m_public_ad_update_timer;
	ClassAd m_public_ad;
	DefragStats m_stats;

	MachineSet m_prev_whole_machines;
	MachineSet m_prev_draining_machines;

	int m_whole_machines_arrived;
	time_t m_last_whole_machine_arrival;
	double m_whole_machine_arrival_sum;
	double m_whole_machine_arrival_mean_squared;

	void poll_cancel(MachineSet &); // Cancel any machines that match DEFRAG_CANCEL_REQUIREMENTS

	bool drain_this(const ClassAd &startd_ad);
	bool cancel_this_drain(const ClassAd &startd_ad);

	void validateExpr(char const *constraint,char const *constraint_source);
	bool queryMachines(char const *constraint,char const *constraint_source,ClassAdList &startdAds,classad::References * projection);

		// returns number of machines matching constraint
		// (does not double-count ads with matching machine attributes)
		// returns -1 on error
	int countMachines(char const *constraint,char const *constraint_source,MachineSet *machines=NULL);

	void loadState();
	void saveState();
	void slotNameToDaemonName(std::string const &name,std::string &machine);

	void publish(ClassAd *ad);
	void updateCollector();
	void invalidatePublicAd();
	void queryDrainingCost();

	void dprintf_set(const char *, MachineSet *) const;
};

#endif
