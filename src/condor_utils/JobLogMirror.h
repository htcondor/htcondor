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

#ifndef _JOBLOGMIRROR_H_
#define _JOBLOGMIRROR_H_

#include "condor_common.h"
#include "condor_daemon_core.h"

#include "ClassAdLogReader.h"

class JobLogMirror: public Service {
public:
	JobLogMirror(ClassAdLogConsumer *consumer,char const *_job_queue=NULL);
	~JobLogMirror();

	void init();
	void config();
	void stop();
	void poll();

private:
	ClassAdLogReader job_log_reader;
	std::string job_queue_file;

	int log_reader_polling_timer;
	int log_reader_polling_period;

	void TimerHandler_JobLogPolling( int timerID = -1 );
};

#endif
