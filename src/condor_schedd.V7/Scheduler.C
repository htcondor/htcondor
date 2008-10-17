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


#include "Scheduler.h"
#include "condor_config.h"
#include "get_daemon_name.h"

Scheduler::Scheduler() {
	log_reader_polling_timer = -1;
	log_reader_polling_period = 10;
	m_public_ad_update_timer = -1;
	m_public_ad_update_interval = -1;
}

Scheduler::~Scheduler() {
}

void
Scheduler::init() {
	config();
}

void
Scheduler::stop() {
	InvalidatePublicAd();
}

void
Scheduler::InitPublicAd() {
	m_public_ad = ClassAd();

	//TODO: this is the wrong ADTYPE.
	//Use the new generic ad type when it becomes available.
	//Until then, actual writes to the collector are disabled,
	//because it conflicts with the schedd on the same host..

	m_public_ad.SetMyTypeName( SCHEDD_ADTYPE );
	m_public_ad.SetTargetTypeName( "" );

	m_public_ad.Assign(ATTR_NAME,m_name.c_str());

        // Publish all DaemonCore-specific attributes, which also handles
        // SCHEDD_ATTRS for us.
    daemonCore->publish(&m_public_ad);
}

void
Scheduler::config() {
	char *name = param("JOB_ROUTER_NAME");
	if(name) {
		char *valid_name = build_valid_daemon_name(name);
		m_name = valid_name;
		free(name);
		delete [] valid_name;
	}
	else {
		char *default_name = default_daemon_name();
		if(default_name) {
			m_name = default_name;
			delete [] default_name;
		}
	}

	char *spool = param("SPOOL");
	if(!spool) {
		EXCEPT("No SPOOL defined in config file.\n");
	}
	else {
		std::string job_log_fname(spool);
		job_log_fname += "/job_queue.log";
		job_log_reader.SetJobLogFileName(job_log_fname.c_str());
		free(spool);
	}	

		// read the polling period and if one is not specified use 
		// default value of 10 seconds
	char *polling_period_str = param("POLLING_PERIOD");
	log_reader_polling_period = 10;
	if(polling_period_str) {
		log_reader_polling_period = atoi(polling_period_str);
		free(polling_period_str);
	}

		// clear previous timers
	if (log_reader_polling_timer >= 0) {
		daemonCore->Cancel_Timer(log_reader_polling_timer);
		log_reader_polling_timer = -1;
	}
		// register timer handlers
	log_reader_polling_timer = daemonCore->Register_Timer(
		0, 
		log_reader_polling_period,
		(Eventcpp)&Scheduler::TimerHandler_JobLogPolling, 
		"Scheduler::TimerHandler_JobLogPolling", this);

	InitPublicAd();

	int update_interval = 60;
	char *update_interval_str = param("UPDATE_INTERVAL");
	if(update_interval_str) {
		update_interval = atoi(update_interval_str);
		free(update_interval_str);
	}
	if(m_public_ad_update_interval != update_interval) {
		m_public_ad_update_interval = update_interval;

		if(m_public_ad_update_timer >= 0) {
			daemonCore->Cancel_Timer(m_public_ad_update_timer);
			m_public_ad_update_timer = -1;
		}
		dprintf(D_FULLDEBUG, "Setting update interval to %d\n",
				m_public_ad_update_interval);
		m_public_ad_update_timer = daemonCore->Register_Timer(
			0,
			m_public_ad_update_interval,
			(Eventcpp)&Scheduler::TimerHandler_UpdateCollector,
			"Scheduler::TimerHandler_UpdateCollector",
			this);
	}
}

void
Scheduler::TimerHandler_JobLogPolling() {
	dprintf(D_FULLDEBUG, "TimerHandler_JobLogPolling() called\n");
	job_log_reader.poll(&ad_collection);
	//DebugDisplayClassAdCollection(&ad_collection);
}

void
Scheduler::TimerHandler_UpdateCollector() {
	//daemonCore->sendUpdates(UPDATE_SCHEDD_AD, &m_public_ad);
}

void
Scheduler::InvalidatePublicAd() {
	//daemonCore->sendUpdates(INVALIDATE_SCHEDD_ADS, &m_public_ad);
}
