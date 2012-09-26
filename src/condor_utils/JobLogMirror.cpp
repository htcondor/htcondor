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


#include "JobLogMirror.h"
#include "condor_config.h"

JobLogMirror::JobLogMirror(ClassAdLogConsumer *consumer,char const *_alt_spool_param):
	job_log_reader(consumer),
	alt_spool_param(_alt_spool_param ? _alt_spool_param : "")
{
	log_reader_polling_timer = -1;
	log_reader_polling_period = 10;
}

JobLogMirror::~JobLogMirror() {
	stop();
}

void
JobLogMirror::init() {
	config();
}

void
JobLogMirror::stop() {
	if( log_reader_polling_timer != -1 ) {
		daemonCore->Cancel_Timer(log_reader_polling_timer);
		log_reader_polling_timer = -1;
	}
}

void
JobLogMirror::config() {
	char *spool = NULL;
	if( !alt_spool_param.empty() ) {
		spool = param(alt_spool_param.c_str());
	}
	if( !spool ) {
		spool = param("SPOOL");
	}
	if(!spool) {
		EXCEPT("No SPOOL defined in config file.\n");
	}
	else {
		std::string job_log_fname(spool);
		job_log_fname += "/job_queue.log";
		job_log_reader.SetClassAdLogFileName(job_log_fname.c_str());
		free(spool);
	}	

		// read the polling period and if one is not specified use 
		// default value of 10 seconds
	log_reader_polling_period = param_integer("POLLING_PERIOD", 10);

		// clear previous timers
	if (log_reader_polling_timer >= 0) {
		daemonCore->Cancel_Timer(log_reader_polling_timer);
		log_reader_polling_timer = -1;
	}
		// register timer handlers
	log_reader_polling_timer = daemonCore->Register_Timer(
		0, 
		log_reader_polling_period,
		(TimerHandlercpp)&JobLogMirror::TimerHandler_JobLogPolling, 
		"JobLogMirror::TimerHandler_JobLogPolling", this);
}

void
JobLogMirror::TimerHandler_JobLogPolling() {
	dprintf(D_FULLDEBUG, "TimerHandler_JobLogPolling() called\n");
	assert(job_log_reader.Poll() != POLL_ERROR);
}
