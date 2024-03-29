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

class JobLogMirror;
class NewClassAdJobLogConsumer;
class Scheduler {
public:
	Scheduler(int id);
	~Scheduler();
	classad::ClassAdCollection *GetClassAds() const;
	void init();
	void config();
	void stop();
	void poll();
	int id() const { return m_id; };
	const char * following() const { return m_follow_log.empty() ? NULL : m_follow_log.c_str(); };

private:

	NewClassAdJobLogConsumer * m_consumer;
	JobLogMirror * m_mirror;
	std::string m_follow_log;
	int m_id; // id of this instance
};

#endif
