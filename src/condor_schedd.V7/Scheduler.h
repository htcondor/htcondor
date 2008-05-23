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

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#define WANT_CLASSAD_NAMESPACE
#undef open
#include "classad/classad_distribution.h"

#include "JobLogReader.h"

class Scheduler: public Service {
public:
	Scheduler();
	virtual ~Scheduler();
	void init();
	void config();
	void stop();

	classad::ClassAdCollection *GetClassAds() {return &ad_collection;}
	char const *Name() const {return m_name.c_str();}

private:
	std::string m_name;

	ClassAd m_public_ad;
	int m_public_ad_update_interval;
	int m_public_ad_update_timer;

	classad::ClassAdCollection ad_collection;
	JobLogReader job_log_reader;

	int log_reader_polling_timer;
	int log_reader_polling_period;

	void InitPublicAd();
	void TimerHandler_UpdateCollector();
	void InvalidatePublicAd();

	void TimerHandler_JobLogPolling();
};

#endif
