/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** Converted to use Stream library by Jim Basney.
** 
*/ 

#include "condor_common.h"
#include <sys/param.h>
#include <sys/time.h>


#include "trace.h"
#include "except.h"
#include "condor_io.h"

#ifndef NEW_PROC
#define NEW_PROC
#endif
#include "proc.h"

#include "debug.h"

#ifndef LINT
static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */
#endif

#if defined(NEW_PROC)
int stream_pipe_desc( Stream *s, P_DESC *pipe )
{
	if( !s->code(pipe->writer) )
		return FALSE;
	if( !s->code(pipe->reader) )
		return FALSE;
	if( !s->code(pipe->name) )
		return FALSE;
	return TRUE;
}

/*
** transfer routine for a version 3 PROC struct.
*/
int stream_proc_vers3( Stream *s, PROC *proc )
{
	int		i;

	if( !s->code(proc->id) )
		return FALSE;
	if( !s->code(proc->universe) )
		return FALSE;
	if( !s->code(proc->checkpoint) )
		return FALSE;
	if( !s->code(proc->remote_syscalls) )
		return FALSE;
	if( !s->code(proc->owner) )
		return FALSE;
	if( !s->code(proc->q_date) )
		return FALSE;
	if( !s->code(proc->completion_date) )
		return FALSE;
	if( !s->code(proc->status) )
		return FALSE;
	if( !s->code(proc->prio) )
		return FALSE;
	if( !s->code(proc->notification) )
		return FALSE;
	if( !s->code(proc->image_size) )
		return FALSE;
	if( !s->code(proc->env) )
		return FALSE;

	if( !s->code(proc->n_cmds) ) {
		return FALSE;
	}

	if( s->is_decode() ) {
		proc->cmd = (char **)calloc( proc->n_cmds, sizeof(char *) );
		proc->args = (char **)calloc( proc->n_cmds, sizeof(char *) );
		proc->in = (char **)calloc( proc->n_cmds, sizeof(char *) );
		proc->out = (char **)calloc( proc->n_cmds, sizeof(char *) );
		proc->err = (char **)calloc( proc->n_cmds, sizeof(char *) );
		proc->remote_usage = (struct rusage *)
						calloc( proc->n_cmds, sizeof(struct rusage) );
		proc->exit_status = (int *)
						calloc( proc->n_cmds, sizeof(int) );
	}

	for( i=0; i<proc->n_cmds; i++ ) {
		if( !s->code(proc->cmd[i]) )
			return FALSE;
		if( !s->code(proc->args[i]) )
			return FALSE;
		if( !s->code(proc->in[i]) )
			return FALSE;
		if( !s->code(proc->out[i]) )
			return FALSE;
		if( !s->code(proc->err[i]) )
			return FALSE;
		if( !s->code(proc->remote_usage[i]) )
			return FALSE;
		if( !s->code(proc->exit_status[i]) )
			return FALSE;
	}

	if( !s->code(proc->n_pipes) )
		return FALSE;

	if( proc->n_pipes > 0 && s->is_decode() ) {
		proc->pipe = (P_DESC *)calloc( proc->n_pipes, sizeof(P_DESC) );
	}

	for( i=0; i<proc->n_pipes; i++ ) {
		if( !stream_pipe_desc(s,&proc->pipe[i]) )
			return FALSE;
	}

	if ( !s->code(proc->min_needed) ) {
		return FALSE;
	}

	if ( !s->code(proc->max_needed) ) {
		return FALSE;
	}

	if( !s->code(proc->rootdir) )
		return FALSE;
	if( !s->code(proc->iwd) )
		return FALSE;
	if( !s->code(proc->requirements) )
		return FALSE;
	if( !s->code(proc->preferences) )
		return FALSE;
	if( !s->code(proc->local_usage) )
		return FALSE;
	return TRUE;
}
#endif

/*
** transfer routine for a version 2 PROC struct.
*/
int stream_proc_vers2( Stream *s, V2_PROC *proc )
{

	if( !s->code(proc->id) )
		return FALSE;
	if( !s->code(proc->owner) )
		return FALSE;
	if( !s->code(proc->q_date) )
		return FALSE;
	if( !s->code(proc->completion_date) )
		return FALSE;
	if( !s->code(proc->status) )
		return FALSE;
	if( !s->code(proc->prio) )
		return FALSE;
	if( !s->code(proc->notification) )
		return FALSE;
	if( !s->code(proc->image_size) )
		return FALSE;
	if( !s->code(proc->cmd) )
		return FALSE;
	if( !s->code(proc->args) )
		return FALSE;
	if( !s->code(proc->env) )
		return FALSE;
	if( !s->code(proc->in) )
		return FALSE;
	if( !s->code(proc->out) )
		return FALSE;
	if( !s->code(proc->err) )
		return FALSE;
	if( !s->code(proc->rootdir) )
		return FALSE;
	if( !s->code(proc->iwd) )
		return FALSE;
	if( !s->code(proc->requirements) )
		return FALSE;
	if( !s->code(proc->preferences) )
		return FALSE;
	if( !s->code(proc->local_usage) )
		return FALSE;
	if( !s->code(proc->remote_usage) )
		return FALSE;
	return TRUE;
}

/*
** stream routine for a PROC.
*/
int stream_old_proc( Stream *s, OLD_PROC *proc )
{
	if( !s->code(proc->id) )
		return FALSE;
	if( !s->code(proc->owner) )
		return FALSE;
	if( !s->code(proc->q_date) )
		return FALSE;
	if( !s->code(proc->status) )
		return FALSE;
	if( !s->code(proc->prio) )
		return FALSE;
	if( !s->code(proc->notification) )
		return FALSE;
	if( !s->code(proc->cmd) )
		return FALSE;
	if( !s->code(proc->args) )
		return FALSE;
	if( !s->code(proc->env) )
		return FALSE;
	if( !s->code(proc->in) )
		return FALSE;
	if( !s->code(proc->out) )
		return FALSE;
	if( !s->code(proc->err) )
		return FALSE;
	if( !s->code(proc->rootdir) )
		return FALSE;
	if( !s->code(proc->iwd) )
		return FALSE;
	if( !s->code(proc->requirements) )
		return FALSE;
	if( !s->code(proc->preferences) )
		return FALSE;
	if( !s->code(proc->local_usage) )
		return FALSE;
	if( !s->code(proc->remote_usage) )
		return FALSE;
	return TRUE;
}
