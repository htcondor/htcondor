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

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "subsystem_info.h"

#include "ClassAdLogReader.h"
#include "Scheduler.h"
#include "JobRouter.h"
#include "JobLogMirror.h"
#include "NewClassAdJobLogConsumer.h"


JobRouter *job_router;

//-------------------------------------------------------------
Scheduler::Scheduler(int id)
	: m_consumer(nullptr)
	, m_mirror(nullptr)
	, m_id(id)
{
	// lookup knobs corresponding to our id.  id==0 is special case where
	// we look at both JOB_QUEUE_SCHEDD1 knobs, and also the default JOB_QUEUE_LOG knob
	std::string knob, job_queue;
	formatstr(knob, "JOB_ROUTER_SCHEDD%d_JOB_QUEUE_LOG", id ? id : 1);
	if ( ! param(job_queue, knob.c_str())) {
		// no knob for the log, look for the (deprecated) spool knob
		formatstr(knob, "$(JOB_ROUTER_SCHEDD%d_SPOOL:,)/job_queue.log", id ? id : 1);
		expand_param(knob.c_str(), job_queue);
		if (job_queue.empty() || job_queue[0] == ',') { // JOB_ROUTER_SCHEDD<id>_SPOOL knob not defined
			job_queue.clear();
			if (0 == id) {
				// only if we are id 0, do we look for the default job_queue.log file.
				param(job_queue, "JOB_QUEUE_LOG");
			}
		}
	}
	// if we found a log to follow, then create consumer and mirror classes
	if ( ! job_queue.empty()) {
		m_follow_log = job_queue;
		m_consumer = new NewClassAdJobLogConsumer();
		m_mirror = new JobLogMirror(m_consumer, m_follow_log.c_str());
	}
}

Scheduler::~Scheduler()
{
	delete m_mirror; // this has the side effect of deleting m_consumer.
	m_mirror = NULL;
	m_consumer = NULL;
}

classad::ClassAdCollection *Scheduler::GetClassAds() const
{
	return m_consumer->GetClassAds();
}

void Scheduler::init() { m_mirror->init(); }
void Scheduler::config() { m_mirror->config(); }
void Scheduler::stop()  { m_mirror->stop(); }
void Scheduler::poll()  { m_mirror->poll(); }


//-------------------------------------------------------------

void main_init(int   /*argc*/, char ** /*argv*/)
{
	dprintf(D_ALWAYS, "main_init() called\n");

	job_router = new JobRouter();
	job_router->init();
}

//-------------------------------------------------------------

void 
main_config()
{
	dprintf(D_ALWAYS, "main_config() called\n");

	job_router->config();
}

static void Stop()
{
	// JobRouter creates an instance lock, so delete it now to clean up.
	delete job_router;
	job_router = NULL;
	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	Stop();
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	Stop();
}

//-------------------------------------------------------------

int
main( int argc, char **argv )
{
	set_mySubSystem("JOB_ROUTER", true, SUBSYSTEM_TYPE_SCHEDD );	// used by Daemon Core

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
