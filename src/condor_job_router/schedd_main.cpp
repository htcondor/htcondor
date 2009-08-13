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

#include "JobLogReader.h"
#include "Scheduler.h"
#include "JobRouter.h"

// about self
DECL_SUBSYSTEM("JOB_ROUTER", SUBSYSTEM_TYPE_SCHEDD );	// used by Daemon Core


Scheduler *schedd;
JobRouter *job_router;
NewClassAdJobLogConsumer *log_consumer;

//-------------------------------------------------------------

int main_init(int argc, char *argv[])
{
	dprintf(D_ALWAYS, "main_init() called\n");

	log_consumer = new NewClassAdJobLogConsumer();

	schedd = new Scheduler(log_consumer);

	schedd->init();

	job_router = new JobRouter(schedd);
	job_router->init();

	return TRUE;
}

//-------------------------------------------------------------

int 
main_config( bool is_full )
{
	dprintf(D_ALWAYS, "main_config() called\n");

	schedd->config();
	job_router->config();

	return TRUE;
}

static void Stop()
{
	// JobRouter creates an instance lock, so delete it now to clean up.
	schedd->stop();
	delete schedd;
	schedd = NULL;
	delete job_router;
	job_router = NULL;
	DC_Exit(0);
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	Stop();
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	Stop();
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

void
main_pre_dc_init( int argc, char* argv[] )
{
		// dprintf isn't safe yet...
}


void
main_pre_command_sock_init( )
{
}

