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


 

#include <signal.h>

#include "condor_io.h"

#include "limit.h"

extern "C" {
	int SetSyscalls( int );
	void InitStaticFile( int, int );
	void set_debug_flags( const char * );
	ReliSock *RSC_Init( int rscsock, int errsock );
	int get_file( char *remote, char *local, int mode );
	int send_a_file( const char *local_name, const char *remote_name, int mode );
}

BOOLEAN EarlyAbort( int *reason );
void GenerateEarlyAbort( int event );
void set_posix_environment();

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
void move_to_execute_directory();
void close_unused_file_descriptors();
void init_signal_masks();
void init_shadow_connections();
void init_params();
void do_rename();
void copy_file( char *old_name, char *new_name, mode_t mode );
void signal_user_job( pid_t pid, int sig );
char *generate_ckpt_name( int cluster, int proc, char *ext );
void get_ckpt_file();
int verify_ckpt_file();
char *get_user_env_param( char * );
void initial_bookeeping( int argc, char *argv[] );
void wait_for_debugger( int do_wait );
void send_ckpt_file();
void send_core_file();
void update_cpu_usage();
void final_shadow_update();
int cleanup();
int create_ckpt();
void start_job();
void request_ckpt();
void request_periodic_ckpt();
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
int exception_cleanup(int,int,char*);
extern "C" {
	void display_sigmask( char *name, sigset_t *mask );
	void display_cur_sigmask();
	void install_handler( void (*func)(int), sigset_t mask );
	void AllowAsynchEvents();
	void BlockAsynchEvents();
	char *sig_name( int );
	ReliSock	 *init_syscall_connection( int );

	/* these are the remote system calls that we use in the starter */
	int REMOTE_CONDOR_image_size(int kbytes);
	int REMOTE_CONDOR_subproc_status(int id, int *status, 
		struct rusage *use_p);
	int REMOTE_CONDOR_reallyexit(int *status, struct rusage *use_p);
	int REMOTE_CONDOR_send_rusage(struct rusage *use_p);
	int REMOTE_CONDOR_startup_info_request(STARTUP_INFO *s);
	int REMOTE_CONDOR_register_fs_domain(char *domain);
	int REMOTE_CONDOR_register_uid_domain(char *domain);
	int REMOTE_CONDOR_register_ckpt_server(char *host);
	int REMOTE_CONDOR_register_arch(char *arch);
	int REMOTE_CONDOR_register_opsys(char *opsys);
	int REMOTE_CONDOR_register_ckpt_platform(const char *platform, int len);
}
