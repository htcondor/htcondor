/*
 * Copyright 2008 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "subsystem_info.h"

#include "Triggerd.h"

com::redhat::grid::Triggerd* agent;

//-------------------------------------------------------------

void main_init(int argc, char* argv[])
{
   dprintf(D_ALWAYS, "main_init() called\n");

   agent = new com::redhat::grid::Triggerd();
   agent->init();
}

//-------------------------------------------------------------

void
main_config( /*bool is_full*/ )
{
   dprintf(D_ALWAYS, "main_config() called\n");

   agent->config();
}

static void Stop()
{
   delete agent;
   agent = NULL;
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
	set_mySubSystem("TRIGGERD", SUBSYSTEM_TYPE_DAEMON);

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
