/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_debug.h"
#include "startup.h"
#include "proc.h"

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
