/* 
** Copyright 1993 by Jim Pruyne, Miron Livny, and Mike Litzkow
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
** Author:  Jim Pruyne
**
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "types.h"
#include "proto.h"
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include "name_tab.h"

typedef unsigned short u_short;
typedef unsigned char u_char;
typedef unsigned long u_long;
#include <sys/socket.h>

extern "C" {
#include <netinet/in.h>
}
#include "fileno.h"
#include "condor_rsc.h"
#include "util_lib_proto.h"
#include "condor_fix_unistd.h"
#include "condor_fix_socket.h"
#include "list.h"
#include "starter.h"

#include "condor_sys.h"
#include "pvm_user_proc.h"
#include "pvm3.h"
#include "sdpro.h"

extern NameTable SigNames;
extern int Running_PVMD;
extern NameTable PvmMsgNames;

extern List<UserProc>	UProcList;		// List of user processes

int		shadow_tid;

extern void do_unlink( const char * );

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

void PVMdProc::execute()
{
	int		argc;
	char	*argv[ 2048 ];
	char	**argp;
	char	**envp;
	sigset_t	sigmask;
	long	arg_max;
	char	*tmp;
	char	a_out_name[ _POSIX_PATH_MAX ];
	int		user_syscall_fd;
	int		pipes[2];
	int		readlen;
	int		mytid;
	uid_t	old_uid;

		// We will use mkargv() which modifies its arguments in place
		// so we not use the original copy of the arguments
	if( (arg_max=sysconf(_SC_ARG_MAX)) == -1 ) {
		arg_max = _POSIX_ARG_MAX;
	}
	tmp = new char [arg_max];
	strncpy( tmp, args, arg_max );

	sprintf( a_out_name, "condor_exec.%d.%d", cluster, proc );

	argv[0] = a_out_name;
	mkargv( &argc, &argv[1], tmp );

	// set up environment vector
	envp = env_obj.get_vector();

		// print out arguments to execve
	dprintf( D_ALWAYS, "Calling execve( \"%s\"", a_out_name );
	for( argp = argv; *argp; argp++ ) {							// argv
		dprintf( D_ALWAYS | D_NOHEADER, ", \"%s\"", *argp );
	}
	dprintf( D_ALWAYS | D_NOHEADER, ", 0" );
	for( argp = envp; *argp; argp++ ) {							// envp
		dprintf( D_ALWAYS | D_NOHEADER, ", \"%s\"", *argp );
	}
	dprintf( D_ALWAYS | D_NOHEADER, ", 0 )\n" );

	pipe(pipes);
	dprintf( D_ALWAYS, "pipes: %d & %d\n", pipes[0], pipes[1] );

	if( (pid = fork()) < 0 ) {
		EXCEPT( "fork" );
	}

	if( pid == 0 ) {	// the child

			// Block only these 3 signals which have special meaning for
			// checkpoint/restart purposes.  Leave other signals ublocked
			// so that if we get an exception during the restart process,
			// we will get a core file to debug.
		sigemptyset( &sigmask );
		sigaddset( &sigmask, SIGUSR1 );
		sigaddset( &sigmask, SIGUSR2 );
		sigaddset( &sigmask, SIGTSTP );
		sigprocmask( SIG_SETMASK, &sigmask, 0 );

		if( chdir(local_dir) < 0 ) {
			EXCEPT( "chdir(%s)", local_dir );
		}

			// Posix says that a privileged process executing setuid()
			// will change real, effective, and saved uid's.  Thus the
			// child process will have only it's submitting uid, and cannot
			// switch back to root or some other uid.
		set_root_euid();
		setgid( gid );
		setuid( uid );

		dup2(pipes[1], 1);
		if (pipes[1] != 1) {
			close(pipes[1]);
		}
		close(pipes[0]);
/*		close_unused_file_descriptors();	// shouldn't need this */

			// Everything's ready, start it up...
		errno = 0;
		execve( a_out_name, argv, envp );

			// A successful call to execve() never returns, so it is an
			// error if we get here.  A number of errors are possible
			// but the most likely is that there is insufficient swap
			// space to start the new process.  We don't try to log
			// anything, since we have the UID/GID of the job's owner
			// and cannot write into the log files...
		dprintf( D_ALWAYS, "Exec failed - errno = %d\n", errno );
		exit( EXECFAILED );
	}

		// The parent
    close ( pipes[1] );
	dprintf( D_ALWAYS, "Started user job - PID = %d\n", pid );
	readlen = read(pipes[0], tmp, arg_max);
    close( pipes[0] );
	tmp[readlen] = '\0';
	dprintf( D_ALWAYS, "PVMd response: %s\n", tmp);
	old_uid = geteuid();
	set_root_euid();
	seteuid( uid );
	mytid = pvm_mytid();
	set_root_euid();
	seteuid( old_uid );
	dprintf( D_ALWAYS, "Starter tid = t%x\n", mytid );
	(void)REMOTE_syscall( PSEUDO_pvm_info, tmp, mytid );
	delete [] tmp;
	state = EXECUTING;
	dprintf( D_ALWAYS, "Shadow tid = t%x\n", shadow_tid);
	pvm_setopt(PvmRoute, PvmDontRoute);
	pvm_setopt(PvmResvTids, 1);
	Running_PVMD = TRUE;
}


void PVMdProc::delete_files()
{
	char	buf[80];
	uid_t	old_uid;

	sprintf(buf, "/tmp/pvml.%d", uid );
	old_uid = geteuid();
	set_root_euid();
	seteuid( uid );
	do_unlink( buf );
	seteuid( old_uid );
	/* delete_files is virtual, I also want to do the super-class actions.
	   This seems right, but looks ugly, is there a better way? JCP */
	((UserProc *) this)->delete_files();
}


void PVMUserProc::execute()
{
	int		argc;
	char	*argv[ 2048 ];
	char	**argp;
	char	**envp;
	sigset_t	sigmask;
	long	arg_max;
	char	*tmp;
	char	a_out_name[ _POSIX_PATH_MAX ];
	int		user_syscall_fd;
	int		pipes[2];
	int		readlen;
	int		mytid;
	uid_t	old_uid;
	int		trc_tid;
	int		trc_cod;
	int		out_tid;
	int		out_cod;
	int		pvm_parent;
	int		pvm_flags;
	int		arg_count;
	int		env_count;
	int		one = 1;

		// We will use mkargv() which modifies its arguments in place
		// so we not use the original copy of the arguments
	if( (arg_max=sysconf(_SC_ARG_MAX)) == -1 ) {
		arg_max = _POSIX_ARG_MAX;
	}
	tmp = new char [arg_max];
	strncpy( tmp, args, arg_max );

	sprintf( a_out_name, "../%s/condor_exec.%d.%d", local_dir, cluster, proc );

	argv[0] = a_out_name;
	mkargv( &argc, &argv[1], tmp );

	// set up environment vector
	envp = env_obj.get_vector();

	pvm_parent = atoi(argv[1]);
	pvm_flags = atoi(argv[2]);
	out_tid = atoi(argv[3]);
	out_cod = atoi(argv[4]);
	trc_tid = atoi(argv[5]);
	trc_cod = atoi(argv[6]);
		// print out arguments to execve
	dprintf( D_ALWAYS, "Calling execve( \"%s\"", a_out_name );
	arg_count = 0;
	for( argp = &argv[7]; *argp; argp++ ) {							// argv
		dprintf( D_ALWAYS | D_NOHEADER, ", \"%s\"", *argp );
		arg_count++;
	}
	dprintf( D_ALWAYS | D_NOHEADER, ", 0" );
	env_count = 0;
	for( argp = envp; *argp; argp++ ) {							// envp
		dprintf( D_ALWAYS | D_NOHEADER, ", \"%s\"", *argp );
		env_count++;
	}
	dprintf( D_ALWAYS | D_NOHEADER, ", 0 )\n" );

	pvm_initsend(PvmDataFoo);
	pvm_pkint(&pvm_parent, 1, 1);
	pvm_pkstr(a_out_name);
	pvm_pkint(&pvm_flags, 1, 1);
	pvm_pkint(&one, 1, 1);		// How many to spawn
	pvm_pkint(&arg_count, 1, 1);
	for ( argp = &argv[7]; *argp; argp++) {
		pvm_pkstr(*argp);
	}
	pvm_pkint(&out_tid, 1, 1);
	pvm_pkint(&out_cod, 1, 1);
	pvm_pkint(&trc_tid, 1, 1);
	pvm_pkint(&trc_cod, 1, 1);
	pvm_pkint(&env_count, 1, 1);
	for( argp = envp; *argp; argp++ ) {							// envp
	    pvm_pkstr(*argp);
	}
	pvm_send( TIDPVMD, SM_EXEC );			/* Go To it! */

	pvm_recv( TIDPVMD, SM_EXECACK );
	dprintf( D_ALWAYS, "Got EXECACK!\n");
	state = EXECUTING;
	pvm_upkint( &one, 1, 1 );
	pvm_upkint( &pvm_tid, 1, 1);
	dprintf( D_ALWAYS, "New Tid = 0x%x\n", pvm_tid );

	REMOTE_syscall( PSEUDO_pvm_task_info, v_pid, pvm_tid );
	delete [] tmp;

}


void PVMUserProc::send_sig( int sig )
{

	dprintf( D_ALWAYS, "Sending Signal %s to PVM task t%x\n", 
			SigNames.get_name(sig), pvm_tid );
	pvm_sendsig( pvm_tid, sig );
}


/*
  Handle message from PVMd about death of a child process.
*/
void
pvm_reaper()
{
	int			st;
	pid_t		pid;
	UserProc	*proc;
	int			bufid;
	int			tid;
	int			user_secs;
	int			user_usecs;
	int			system_secs;
	int			system_usecs;

	pvm_upkint(&tid, 1, 1);
	pvm_upkint(&st, 1, 1);
	pvm_upkint(&user_secs, 1, 1);
	pvm_upkint(&user_usecs, 1, 1);
	pvm_upkint(&system_secs, 1, 1);
	pvm_upkint(&system_usecs, 1, 1);

	dprintf( D_ALWAYS, "TASKX on %x, status = %x, u_time = %d\n", 
			tid, st, user_secs );

		// Find the corresponding UserProc object
	dprintf( D_FULLDEBUG,
		"Task t%x exited, searching process list...\n", tid
	);

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if (proc->get_class() == PVM) {
			if( proc->get_tid() == tid ) {
				break;
			}
		}
	}

		// If we didn't find the process's object, bail out now
	if( proc == NULL ) {
		EXCEPT(
			"received tid t%x, but no such process marked as EXECUTING",
			tid
		);
	}

		// Ok, update the object
	dprintf( D_FULLDEBUG, "Found object for pvm task %x\n", tid );
	proc->handle_termination( st );
	proc->set_times( user_secs, system_secs );
}


/*
  Received some PVM msg, decode it and see where to go next.
*/
int
read_pvm_msg()
{
	int		bufid, tag, src, buflen;
	int		i;
	static int req_id = 1;
	int		msg_req_id;
	char	*hold_buffer;
	static struct {
		int		starter_req_id;
		int		task_req_id;
		int		task_tid;
	} request_map[50];
	static int request_map_pos = 0;
	static struct {
		int		msg_id;
		int		task_msg_id;
		int		task_tid;
	} notifications[50];
	static int notifications_pos = 0;
	int	sent = 0;

	bufid = pvm_getrbuf();
	if (bufid <= 0) {
		dprintf( D_ALWAYS, "read_pvm_msg: No PVM message ready\n");
		return DEFAULT;
	}
	pvm_bufinfo(bufid, &buflen, &tag, &src);
	dprintf( D_ALWAYS, "read_pvm_msg: Received msg %s from t%x\n",
			PvmMsgNames.get_name(tag), src
		   );
	switch( tag ) {
	    case SM_TASKX:
		    return PVM_CHILD_EXIT;
			break;

		case SM_NOTIFY:
			{
				int		kind;
				int		msg_id;
				int		count;
				int		tid;

				pvm_initsend(PvmDataDefault);
				pvm_upkint(&kind, 1, 0);
				pvm_pkint(&kind, 1, 0);
				pvm_upkint(&msg_id, 1, 0);
				pvm_pkint(&req_id, 1, 0);
				pvm_upkint(&count, 1, 0);
				pvm_pkint(&count, 1, 0);
				if ( kind != PvmHostAdd ) {
					for (;count > 0; count--) {
						pvm_upkint(&tid, 1, 0);
						pvm_pkint(&tid, 1, 0);
					}
				}
				pvm_pkint(&req_id, 1, 0);
				pvm_send(shadow_tid, SM_NOTIFY);
				notifications[notifications_pos].msg_id = req_id++;
				notifications[notifications_pos].task_msg_id = msg_id++;
				notifications[notifications_pos].task_tid = src;
			}
			return DEFAULT;
			break;

		case SM_SPAWN:
		case SM_TASK:
		case SM_CONFIG:
		case SM_ADDHOST:
		case SM_DELHOST:
		case SM_SCHED:
			hold_buffer = (char *) malloc(buflen - sizeof(int));
			pvm_initsend(PvmDataFoo);
			if (src != shadow_tid) { /* From local, route to the shadow */
				pvm_upkbyte(hold_buffer, buflen - sizeof(int), 1);
				pvm_upkint(&msg_req_id, 1, 1);
				pvm_pkbyte(hold_buffer, buflen - sizeof(int), 1);
				pvm_pkint(&req_id, 1, 1);
				pvm_send(shadow_tid, tag);
				request_map[request_map_pos].starter_req_id = req_id;
				request_map[request_map_pos].task_req_id = msg_req_id;
				request_map[request_map_pos].task_tid = src;
				req_id++;
				request_map_pos++;
			} else { /* From the shadow, route to local task */
				pvm_upkint(&msg_req_id, 1, 1);
				pvm_upkbyte(hold_buffer, buflen - sizeof(int), 1);
				for (i = 0; i < request_map_pos; i++) {
					if (msg_req_id == request_map[i].starter_req_id) {
						dprintf( D_ALWAYS, 
							"Mapping from req_id %d to task t%x, req_id %d\n",
								msg_req_id, request_map[i].task_tid,
								request_map[i].task_req_id);
						pvm_pkint(&(request_map[i].task_req_id), 1, 1);
						pvm_pkbyte(hold_buffer, buflen - sizeof(int), 1);
						pvm_send(request_map[i].task_tid, tag);
						sent = 1;
						request_map[i] = request_map[request_map_pos];
						request_map_pos--;
						break;
					}
				}
				if (sent == 0) {
					dprintf( D_ALWAYS, "Couldn't find starter req_id %d\n",
							msg_req_id);
				}
			}
			free(hold_buffer);
			return DEFAULT;
			break;
			
		default:	/* Could be a notification to forward to a user */
			for (i = 0; i < notifications_pos; i++) {
				if (tag == notifications[i].msg_id) {
					pvm_setsbuf(bufid);
					pvm_send(notifications[i].task_msg_id, 
							 notifications[i].task_tid);
					dprintf( D_ALWAYS, 
							"Sending notification to t%x w/ tag %d\n",
							notifications[i].task_tid,
							notifications[i].task_msg_id );
					sent = 1;
					notifications[i] = notifications[notifications_pos];
					notifications_pos--;
					break;
				}
			}
			if (sent == 0) {
				dprintf( D_ALWAYS, "Unexpected message tag %d\n", tag);
			}
			return DEFAULT;
			break;
	}
}

