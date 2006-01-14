/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#ifndef __UTIL_LIB_PROTO_H
#define __UTIL_LIB_PROTO_H

#include "condor_status.h"		/* For STATUS_LINE */
#include "condor_config.h"
#include "condor_getmnt.h"
#include "condor_types.h"
#include "condor_ckpt_name.h"

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef __CEXTRACT__
#if defined(HAS_PROTO) || defined(__STDC__) || defined(__cplusplus) || !defined(Solaris)

int blankline ( char *str );
char * gen_exec_name ( int cluster, int proc, int subproc );

int hash ( register char *string, register int size );
char * getline ( FILE *fp );

char* getExecPath();

int rotate_file(const char *old_filename, const char *new_filename);
int copy_file(const char *old_filename, const char *new_filename);

void schedule_event ( int month, int day, int hour, int minute, int second, void (*func)() );
void event_mgr ( void );
void StartRecording ( void );
void CompleteRecording ( long numberOfBytes );
void ProcessLogging ( int request, int extraInteger );
void detach ( void );
int do_connect ( const char *host, const char *service, u_int port );
int udp_connect ( char *host, u_int port );
void dprintf ( int flags, char* fmt, ... );
FILE * debug_lock ( void );
void debug_unlock ( void );
void _EXCEPT_ ( char *fmt, ... );
int flock ( int fd, int op );
int getdtablesize ( void );

#ifndef WIN32  // on WIN32, it messes with our macro in condor_sys_nt.h
char* strupr( char* );
#endif

#ifndef WIN32	// on WIN32, it messes with our getwd macro in condor_sys_nt.h
char * getwd ( char *path );
#endif

char * ltrunc ( register char *str );
int set_machine_status ( int status );
int get_machine_status ( void );
int mkargv ( int *argc, char *argv[], char *line );
int display_proc_short ( PROC *proc );
char * format_time ( float fp_secs );
int display_proc_long ( PROC *proc );
int display_v2_proc_long ( PROC *proc );

int display_status_line ( STATUS_LINE *line, FILE *fp );
char * shorten ( char *state );
int free_status_line ( STATUS_LINE *line );
int print_header ( FILE *fp );
char * format_seconds ( int t_sec );
char * substr ( char *string, char *pattern );
void update_rusage( register struct rusage *ru1, register struct rusage *ru2 );
int sysapi_swap_space ( void );
int sysapi_disk_space(const char *filename);
int calc_disk_needed( PROC * proc );
PROC *ConstructProc( int, PROC *);

FILE *my_popenv( char *const argv[], const char * mode );
int my_systemv( char *const argv[] ); 
int my_pclose( FILE *fp );
int my_spawnl( const char* cmd, ... );
int my_spawnv( const char* cmd, char *const argv[] );

#else /* HAS_PROTO */

int blankline ();
char * gen_exec_name ();

char* getExecPath();

int hash ();
char * getline ();


void schedule_event ();
void event_mgr ();
void StartRecording ();
void CompleteRecording ();
void ProcessLogging ();
void detach ();
int do_connect ();
int udp_connect ();
void dprintf ();
FILE * debug_lock ();
void debug_unlock ();
void _EXCEPT_ ();
int flock ();
int getdtablesize ();
int getmnt ();
/* int getpagesize (); */
char * getwd ();
char * ltrunc ();
int set_machine_status ();
int get_machine_status ();
int mkargv ();
int display_proc_short ();
char * format_time ();
int display_proc_long ();
int display_v2_proc_long ();
#ifndef HPUX9
int setlinebuf ();
#endif
int display_status_line ();
char * shorten ();
int free_status_line ();
int print_header ();
char * format_seconds ();
char * strdup ();
char * substr ();
int update_rusage ();
int sysapi_swap_space ();
int sysapi_disk_space();
int calc_disk_needed();
PROC *ConstructProc();

#endif /* HAS_PROTO */
#endif /* __CEXTRACT__ */

#if defined(__cplusplus)
}		/* End of extern "C" declaration */
#endif

#endif
