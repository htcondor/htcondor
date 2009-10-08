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
#include "condor_open.h"
#include "subsystem_info.h"
#include "hadoop.h"

//-------------------------------------------------------------

// about self
DECL_SUBSYSTEM("HDFS", SUBSYSTEM_TYPE_DAEMON );	// used by Daemon Core

//-------------------------------------------------------------

static Hadoop hadoop;

int main_init(int /* argc */, char * /* argv */ [])
{     
        hadoop.initialize();
        return TRUE;
}

//-------------------------------------------------------------

int 
main_config( bool /* is_full */ )
{
        hadoop.initialize();
        return TRUE;
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
        hadoop.stop(true);
        DC_Exit(0);
        return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
        hadoop.stop();
        return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

void
main_pre_dc_init( int /* argc */, char* /* argv */ [] )
{
		// dprintf isn't safe yet...
}


void
main_pre_command_sock_init( )
{
}

