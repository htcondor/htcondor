#define _POSIX_SOURCE

#include <sys/types.h>
#include "condor_common.h"
#include "condor_jobqueue.h"
#include "condor_constants.h"
#include "condor_debug.h"
#include "startup.h"
#include "proc.h"
static char *_FileName_ = __FILE__;

void
display_startup_info( const STARTUP_INFO *s, int flags )
{
	dprintf( flags, "Startup Info:\n" );

	dprintf( flags, "\tVersion Number: %d\n", s->version_num );
	dprintf( flags, "\tId: %d.%d\n", s->cluster, s->proc );
	switch( s->job_class ) {
		case STANDARD:
			dprintf( flags, "\tJobClass: Standard\n" );
			break;
		case PIPE:
			dprintf( flags, "\tJobClass: Pipe\n" );
			break;
		case LINDA:
			dprintf( flags, "\tJobClass: Linda\n" );
			break;
		case PVM:
			dprintf( flags, "\tJobClass: PVM\n" );
			break;
		default:
			dprintf( flags, "\tJobClass: UNKNOWN (%d)\n", s->job_class );
	}
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
