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

#if 1

class JobLogMirror;
class NewClassAdJobLogConsumer;
class Scheduler {
public:
	Scheduler(char const *_alt_spool_param=NULL, int id=0);
	~Scheduler();
	classad::ClassAdCollection *GetClassAds() const;
	void init();
	void config();
	void stop();
	void poll();
	int id() const;

private:

	NewClassAdJobLogConsumer * m_consumer;
	JobLogMirror * m_mirror;
	int m_id; // id of this instance
};
#else
#include "JobLogMirror.h"
#include "NewClassAdJobLogConsumer.h"

class Scheduler: virtual public JobLogMirror {
public:
	Scheduler(NewClassAdJobLogConsumer *_consumer,char const *_alt_spool_param=NULL):
		JobLogMirror(_consumer,_alt_spool_param),
		m_consumer(_consumer)
	{ }

	classad::ClassAdCollection *GetClassAds()
	{
		return m_consumer->GetClassAds();
	}

private:
	NewClassAdJobLogConsumer *m_consumer;
};
#endif

#endif
