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

#ifndef _ROOSTER_H
#define _ROOSTER_H

#include "condor_common.h"
#include "condor_daemon_core.h"

// Rooster is a daemon that monitors offline machines and wakes
// them up according to a configurable policy.

class Rooster: public Service {
 public:
	Rooster();
	~Rooster();

	void init();
	void config();
	void stop();

	void poll(); // do the periodic checks of offline startds
	bool wakeUp(ClassAd *startd_ad);

 private:
	int m_polling_interval; // delay between checks of offline startds
	int m_polling_timer;
	std::string m_unhibernate_constraint;
	std::string m_wakeup_cmd;
	ArgList m_wakeup_args;
	ClassAd m_rank_ad;
	int m_max_unhibernate;
};


#endif
