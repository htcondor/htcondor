/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#ifndef USER_PROC_H
#define USER_PROC_H

#include "environ.h"

#define EXECFAILED 255 // exit status if call to execv() fails

	// Reason user process exited (or didn't execute)
typedef enum {
	ABNORMAL_EXIT,	// process exited abnormally due to signal
	BAD_LINK,		// checkpoint file not linked with condor library
	BAD_MAGIC,		// bad magic number in checkpoint file
	CANT_FETCH,		// can't get executable file from submitting machine
	CHECKPOINTING,	// process terminated for checkpointing
	EXECUTING,		// currently executing
	NEW,			// haven't tried to execute it yet
	NON_RUNNABLE,	// process kicked off due to non-condor work
	NORMAL_EXIT,	// process exited normally with some status value
	RUNNABLE,		// ready to go, but not running
	SUSPENDED		// process is stopped
} PROC_STATE;

	// N.B. This name table *must* match the above enumerator.
#if defined(INSERT_TABLES)
static NAME_VALUE	ProcStateNames[] = {
	{ ABNORMAL_EXIT,	"ABNORMAL_EXIT",	},
	{ BAD_LINK,			"BAD_LINK",			},
	{ BAD_MAGIC,		"BAD_MAGIC",		},
	{ CHECKPOINTING,	"CHECKPOINTING",	},
	{ EXECUTING,		"EXECUTING"			},
	{ NEW,				"NEW"				},
	{ NON_RUNNABLE,		"NON_RUNNABLE",		},
	{ NORMAL_EXIT,		"NORMAL_EXIT",		},
	{ RUNNABLE,			"RUNNABLE",			},
	{ SUSPENDED,		"SUSPENDED",		},
	{ -1,				"(UNKNOWN)"			}
};
NameTable ProcStates( ProcStateNames );
#endif

/*
  The following are already defined in "proc.h".
	#define STANDARD 1	// Original - single process jobs, 1 per machine
	#define PIPE	 2	// Pipes - all processes on single machine
	#define LINDA	 3	// Parallel applications via Linda
	#define PVM		 4	// Parallel applications via Parallel Virtual Machine
*/
typedef int JOB_CLASS;

	// N.B. This name table *must* match the above enumerator.
#if defined(INSERT_TABLES)
static NAME_VALUE	JobClassNames[] = {
	{ STANDARD,		"STANDARD"	},
	{ PIPE,			"PIPE"		},
	{ LINDA,		"LINDA",	},
	{ PVM,			"PVM",		},
	{ VANILLA,		"VANILLA",	},
    { PVMD,			"PVMD",		},
	{ -1,			"(UNKNOWN)"	}
};
NameTable JobClasses( JobClassNames );
#endif


class UserProc;

UserProc *get_job_info();
inline void display_bool( int debug_flags, char *name, int value );
void *bsd_rusage( clock_t user_time, clock_t sys_time );
void *bsd_status( int, PROC_STATE, int, int );

class UserProc {
public:
		// Constructors, etc.
	UserProc( V2_PROC &, char *, char *, uid_t, uid_t, int );
	UserProc( V3_PROC &, char *, char *, uid_t, uid_t, int, int );
	virtual ~UserProc();
	void display();

		// Handle files associated with a process
	int  fetch_ckpt();		// get checkpoint from submitting machine
	void store_ckpt();		// send checkpoint to submitting machine
	void store_core();		// send abnormal core to submitting machine
	int  update_ckpt();		// create "ckpt.tmp" from "ckpt" and "core"
	void commit_ckpt();		// mv "ckpt.tmp" to "ckpt"
	virtual void delete_files();	// clean up files on executing machine

		// Process control
	virtual void execute();
	virtual void suspend();
	virtual void resume();
	virtual void request_ckpt();
	virtual void request_exit();
	virtual void kill_forcibly();
	virtual void handle_termination( int exit_stat );

		// Access functions
	PROC_STATE	get_state() { return state; }
	pid_t	get_pid() { return pid; }
	int		get_id() { return v_pid; }
    virtual int get_tid() { return 0; }
	int		get_cluster() { return cluster; }
	int		get_proc() { return proc; }
	JOB_CLASS	get_class() { return job_class; }
	int		is_runnable() { return state == RUNNABLE; }
	int		is_running() { return state == EXECUTING; }
	int		is_suspended() { return state == SUSPENDED; }
	int		is_checkpointing() { return state == CHECKPOINTING; }
	int		exited_normally() { return state == NORMAL_EXIT; }
	int		exited_abnormally() { return state == ABNORMAL_EXIT; }
	int		non_runnable() { return state == NON_RUNNABLE; }
	int     ckpt_enabled() { return ckpt_wanted; }
	char    *get_env_param( char * );
	int	    get_image_size() { return image_size; }
	void	*bsd_exit_status();		// BSD style exit status for shadow
	void	*accumulated_rusage();	// accumulated so far
	void	*guaranteed_rusage();	// guaranteed by xfer of ckpt or termination
	void set_times(int u_time, int s_time) { user_time = u_time; sys_time = s_time; }


protected:
	
	int		cluster;
	int		proc;

	char	*in;
	char	*out;
	char	*err;
	char	*rootdir;
	char	*cmd;
	char	*args;
	char	*env;
	Environ	env_obj;

	char	*orig_ckpt;		// on submitting machine
	char	*target_ckpt;	// on submitting machine

	char	*local_dir;		// on executing machine
	char	*cur_ckpt;		// on executing machine
	char	*tmp_ckpt;		// on executing machine
	char	*core_name;		// on executing machine

	uid_t	uid;
	uid_t	gid;

	pid_t	v_pid;			// shadow's pid for the process
	pid_t	pid;			// local pid of the process

	JOB_CLASS	job_class;		// Normal Condor, PVM, Pipe, etc.
	PROC_STATE	state;

		// CPU accumulated by completed executions
	clock_t	user_time;
	clock_t sys_time;

private:
	virtual void send_sig( int );
	int  estimate_image_size();
	int  is_restart();
	void accumulate_cpu_time();
	void commit_cpu_time();
	char *expand_exec_name( int &on_this_host );
	int link_to_executable( char *src, int & error_code );
	int transfer_executable( char *src, int & error_code );
	int  linked_for_condor();

	static int proc_index;

	int		exit_status_valid;	// TRUE only if process has run and exited
	int		exit_status;	// POSIX exit status

	int		ckpt_wanted;	// TRUE if user wants periodic checkpointing
	int		coredump_limit_exists;
	int		coredump_limit;

	int		soft_kill_sig;	// Signal to request process to exit

	int		new_ckpt_created;
	int		ckpt_transferred;
	int		core_created;
	int		core_transferred;
	int		image_size;		// in 1024 bytes blocks

		// CPU guaranteed by transfer of ckpt or termination of job
	clock_t	guaranteed_user_time;
	clock_t	guaranteed_sys_time;
};


#endif
