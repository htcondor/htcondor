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
Scheduler::Scheduler(char const *_alt_spool_param /*=NULL*/, int id /*=0*/)
	: m_id(id)
{
	m_consumer = new NewClassAdJobLogConsumer();
	m_mirror = new JobLogMirror(m_consumer, _alt_spool_param);
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
int  Scheduler::id() const { return m_id; }


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
	set_mySubSystem("JOB_ROUTER", SUBSYSTEM_TYPE_SCHEDD );	// used by Daemon Core

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
