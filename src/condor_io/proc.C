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
#include "condor_debug.h"
#include "condor_io.h"

#ifndef NEW_PROC
#define NEW_PROC
#endif
#include "proc.h"


#ifndef LINT
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
	if( !s->code(proc->env_v1or2) )
		return FALSE;

	if( !s->code(proc->n_cmds) ) {
		return FALSE;
	}

	if( s->is_decode() ) {
		proc->cmd = (char **)calloc( proc->n_cmds, sizeof(char *) );
		proc->args_v1or2 = (char **)calloc( proc->n_cmds, sizeof(char *) );
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
		if( !s->code(proc->args_v1or2[i]) )
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
	if( !s->code(proc->args_v1or2) )
		return FALSE;
	if( !s->code(proc->env_v1or2) )
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
	if( !s->code(proc->args_v1or2) )
		return FALSE;
	if( !s->code(proc->env_v1or2) )
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
