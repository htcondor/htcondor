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


#ifndef __WIN_CREDD__
#define __WIN_CREDD__

#include "condor_daemon_core.h"

class CredDaemon : public Service {

public:
	CredDaemon();
	~CredDaemon();

	void reconfig();
	void cleanup();

private:

	void get_passwd_handler(int, Stream*);
	void nop_handler(int, Stream*);
	void initialize_classad();
	void update_collector();
	void invalidate_ad();

	char* m_name;

	int m_update_collector_tid;
	int m_update_collector_interval;

	ClassAd m_classad;
};

#endif
