/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
	dprintf( flags, "\tArgs: \"%s\"\n", s->args );
	dprintf( flags, "\tEnv: \"%s\"\n", s->env );
	dprintf( flags, "\tIwd: \"%s\"\n", s->iwd );
	dprintf( flags, "\tCkpt Wanted: %s\n", s->ckpt_wanted ? "TRUE" : "FALSE" );
	dprintf( flags, "\tIs Restart: %s\n", s->is_restart ? "TRUE" : "FALSE" );
	dprintf( flags, "\tCore Limit Valid: %s\n",
		s->coredump_limit_exists ? "TRUE" : "FALSE"
	);
	if( s->coredump_limit_exists ) {
		dprintf( flags, "\tCoredump Limit %ld\n", s->coredump_limit );
	}
}
