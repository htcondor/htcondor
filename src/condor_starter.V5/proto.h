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

#include <signal.h>

extern "C" {
#if 0
#include "util_lib_proto.h"
#endif
	int SetSyscalls( int );
	void InitStaticFile( int, int );
	void set_debug_flags( char * );
	XDR * RSC_Init( int rscsock, int errsock );
	int xdr_proc( XDR *xdrs, PROC *proc );
	int get_file( char *remote, char *local, int mode );
	int send_file( char *local_name, char *remote_name, int mode );
}

BOOLEAN EarlyAbort( int *reason );
void GenerateEarlyAbort( int event );
void set_posix_environment();
void delay( int );

int NewConnection( int );
int Start_Synch();
int GetJob_Synch();
int GetJob_Asynch();
int Spawn_Synch();
int Supervise_Synch();
int Supervise_Asynch();
int Terminate_Synch();
int DoCkpt_Synch();
int DoCkpt_Asynch();
int SendCkpt_Synch();
int SendCkpt_Asynch();
int SendCore_Synch();
int SendCore_Asynch();
int CkptDone_Synch();
int DoVacate_Synch();
int DoVacate_Asynch();
int End_Synch();

void GetJob_Handler( int );
void Supervise_Handler( int );
void DoCkpt_Handler( int );
void SendCkpt_Handler( int );
void SendCore_Handler( int );
void DoVacate_Handler( int );

void set_alarm( unsigned int );
void cancel_alarm();
void suspend_alarm();
void resume_alarm();
void init_shadow_connections();
void init_logging();
void read_config_files( char *my_name );
void move_to_execute_directory();
void set_resource_limits();
void close_unused_file_descriptors();
void init_signal_masks();
void init_shadow_connections();
void init_params();
void do_rename();
void copy_file( char *old_name, char *new_name, mode_t mode );
void signal_user_job( pid_t pid, int sig );
char *generate_ckpt_name( int cluster, int proc, char *ext );
void delay( int sec );
void waste_a_second();
int calc_free_disk_blocks();
void get_ckpt_file();
int verify_ckpt_file();
char *get_user_env_param( char * );
void initial_bookeeping( int argc, char *argv[] );
void wait_for_debugger( int do_wait );
void limit( int resource, rlim_t new_limit );
void send_ckpt_file();
void send_core_file();
void update_cpu_usage();
void final_shadow_update();
void cleanup();
int create_ckpt();
void start_job();
void request_ckpt();
void request_exit();
void suspend_user_job();
void restart_user_job();
void kill_user_job();
void usage( char *my_name );
void init_sig_mask();
int	get_job();
int magic_check( char *a_out );
int symbol_main_check( char *name );
off_t physical_file_size( char *name );
off_t logical_file_size( char *name );
int calc_hdr_blocks();
int calc_text_blocks( char * );
int core_is_valid( char * );
int have_running_process();
int exception_cleanup();
extern "C" {
	void display_sigmask( char *name, sigset_t *mask );
	void display_cur_sigmask();
	void install_handler( void (*func)(int), sigset_t mask );
	void AllowAsynchEvents();
	void BlockAsynchEvents();
	char *sig_name( int );
	int  REMOTE_syscall( int, ... );
	XDR	 *init_syscall_connection( int, int );
}
