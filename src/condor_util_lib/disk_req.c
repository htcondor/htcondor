#define _POSIX_SOURCE

#if defined(SUNOS41)
typedef unsigned int u_int;
#endif

#if defined(ULTRIX42) || defined(ULTRIX43)
typedef char * caddr_t;
typedef unsigned int u_int;
#endif

#if defined(AIX32) || defined(OSF1)
typedef unsigned int u_int;
#endif

#if defined(AIX32)
#undef _NONSTD_TYPES
#endif
/* Solaris specific change ..dhaval 6/25 */
#if defined(Solaris)
#include "_condor_fix_types.h"
#endif 

#if defined(IRIX62)
typedef struct fd_set fd_set;
#endif 

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "condor_types.h"
#include "proc.h"
#include "debug.h"
#include "except.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

static int ckpt_wanted( PROC *proc );
static int ickpt_size( PROC *proc );

char *gen_ckpt_name( const char * dir, int cluster, int proc, int subproc );

extern char *Spool;

/*
  Return minimim disk to support this job in kilobytes.  If the job is
  to be checkpointed, we need sufficient disk to hold the original
  checkpoint file, the core, and a new checkpoint file; three times the
  "image_size".  If it is not to be checkpointed, we only need enough
  disk to hold the original a.out file.
*/
int
calc_disk_needed( proc )
PROC	*proc;
{
	int	answer;

	dprintf( D_FULLDEBUG, "Entering calc_disk_needed()\n" );
#if defined(V5)
	dprintf( D_FULLDEBUG, "Version 5 - using ickpt_size\n" );
	answer = ickpt_size( proc );
#else
	if( ckpt_wanted(proc) ) {
		dprintf( D_FULLDEBUG, "Checkpointing wanted - using triple rule\n" );
		answer =  3 * proc->image_size;
	} else {
		dprintf( D_FULLDEBUG, "Checkpointing not wanted - using ickpt_size\n" );
		answer = ickpt_size( proc );
	}
#endif
	dprintf( D_FULLDEBUG, "Returning %d\n", answer );
	return answer;
}

/*
  Return TRUE if the user wants this job to be checkpointed, and
  FALSE otherwise.
*/
static int
ckpt_wanted( proc )
PROC	*proc;
{
	char	*value, *GetEnvParam();

	value = GetEnvParam( "CHECKPOINT", proc->env );

	if( value ) {
		dprintf( D_FULLDEBUG, "CHECKPOINT = \"%s\"\n", value );
	} else {
		dprintf( D_FULLDEBUG, "CHECKPOINT envirnoment variable not set\n" );
	}

	if( value && stricmp("false",value) == 0 ) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/*
  Return the size in kilobytes of the initial checkpoint file (a.out).
*/
static int
ickpt_size( proc )
PROC	*proc;
{
	struct stat	buf;
	char	*file_name;
	static	last_cluster = -1;
	static	last_size;

	if( proc->id.cluster == last_cluster ) {
		return last_size;
	}

	file_name = gen_ckpt_name( Spool, proc->id.cluster, ICKPT, 0 );
	if( stat(file_name,&buf) < 0 ) {
		EXCEPT( "stat(%s,0x%x)", file_name, &buf );
	}

	last_cluster = proc->id.cluster;
	last_size = buf.st_size / 1024;
	return last_size;
}
