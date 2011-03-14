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

// about self
DECL_SUBSYSTEM("TRIGGERD", SUBSYSTEM_TYPE_DAEMON);

com::redhat::grid::Triggerd* agent;

//-------------------------------------------------------------

int main_init(int argc, char* argv[])
{
   dprintf(D_ALWAYS, "main_init() called\n");

   agent = new com::redhat::grid::Triggerd();
   agent->init();

   return TRUE;
}

//-------------------------------------------------------------

int
main_config( /*bool is_full*/ )
{
   dprintf(D_ALWAYS, "main_config() called\n");

   agent->config();

   return TRUE;
}

static void Stop()
{
   delete agent;
   agent = NULL;
   DC_Exit(0);
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
        dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
        Stop();
        return TRUE;    // to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
        dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
        Stop();
        return TRUE;    // to satisfy c++
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
