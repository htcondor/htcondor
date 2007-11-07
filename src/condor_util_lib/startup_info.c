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
#include "condor_constants.h"
#include "condor_debug.h"
#include "startup.h"
#include "proc.h"

void display_startup_info( const STARTUP_INFO *s, int flags );

void
display_startup_info( const STARTUP_INFO *s, int flags )
{
	dprintf( flags, "Startup Info:\n" );

	dprintf( flags, "\tVersion Number: %d\n", s->version_num );
	dprintf( flags, "\tId: %d.%d\n", s->cluster, s->proc );
	dprintf( flags, "\tJobClass: %s\n", CondorUniverseName(s->job_class) );
	dprintf( flags, "\tUid: %d\n", s->uid );
	dprintf( flags, "\tGid: %d\n", s->gid );
	dprintf( flags, "\tVirtPid: %d\n", s->virt_pid );
	dprintf( flags, "\tSoftKillSignal: %d\n", s->soft_kill_sig );
	dprintf( flags, "\tCmd: \"%s\"\n", s->cmd );
	dprintf( flags, "\tArgs: \"%s\"\n", s->args_v1or2 );
	dprintf( flags, "\tEnv: \"%s\"\n", s->env_v1or2 );
	dprintf( flags, "\tIwd: \"%s\"\n", s->iwd );
	dprintf( flags, "\tCkpt Wanted: %s\n", s->ckpt_wanted ? "TRUE" : "FALSE" );
	dprintf( flags, "\tIs Restart: %s\n", s->is_restart ? "TRUE" : "FALSE" );
	dprintf( flags, "\tCore Limit Valid: %s\n",
		s->coredump_limit_exists ? "TRUE" : "FALSE"
	);
	if( s->coredump_limit_exists ) {
		dprintf( flags, "\tCoredump Limit %d\n", s->coredump_limit );
	}
}
