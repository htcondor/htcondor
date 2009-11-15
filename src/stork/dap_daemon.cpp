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

/* ============================================================================
 * ReqEx_daemon_main.C - Request Executer Daemon
 * Reliable File Transfer through SRB
 * by Tevfik Kosar <kosart@cs.wisc.edu>
 * University of Wisconsin - Madison
 * March 2000
 * ==========================================================================*/

#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "dap_server.h"
#include "subsystem_info.h"

// --------------
DECL_SUBSYSTEM( "STORK", SUBSYSTEM_TYPE_DAEMON );	// used by Daemon Core
// --------------

// command line arguments are read into these variables
char * logfilename = "storkserver.log";
char * xmllogfilename = NULL;
char * userlogfilename = NULL;
char * clientagenthost = NULL;

// Timers
int IdleJobMonitorInterval;
int IdleJobMonitorTid;
int HungJobMonitorInterval;
int HungJobMonitorTid;
int RescheduledJobMonitorInterval;
int RescheduledJobMonitorTid;
int LowWaterTid;

extern int  transfer_dap_reaper_id, reserve_dap_reaper_id;
extern int  release_dap_reaper_id;
extern int  requestpath_dap_reaper_id;
extern int  low_water_reaper_id;


/* ==========================================================================
 * ==========================================================================*/
void Usage()
{
  fprintf(stderr,
	  "==============================================================\n");
  fprintf(stderr,"USAGE: stork_server\n"); 
  fprintf(stderr,"   [ -t           ] // output to stdin\n");
  fprintf(stderr,"   [ -p           ] // port on which to run Stork Server\n");
  fprintf(stderr,"   [ -help        ] // stork help screen\n");
  fprintf(stderr,"   [ -Config      ] // stork config file\n");
  fprintf(stderr,"   [ -Serverlog   ] // stork server log in ClassAds\n");
  fprintf(stderr,"   [ -Xmllog      ] // stork server log in XML format\n");
  fprintf(stderr,"   [ -Userlog     ] // stork userlog in XMLformat\n");
  fprintf(stderr,"   [ -Clientagent ] // host where client agent is running\n");
  fprintf(stderr,
	  "==============================================================\n");
  exit (1);
  
}

/* ==========================================================================
 * Parse command line arguments
 * ==========================================================================*/
void parse_arguments(int argc, char **argv)
{

  //check number of arguments
  /*
  if (argc < 2 ){
    Usage();
  }
  */
  for (int i = 1; i < argc; i++) {
    if (!strcmp("-Serverlog", argv[i])) {
      i++;
      if (argc <= i) {
	dprintf( D_ALWAYS, "No daplog file specified!\n" );
	Usage();
      }
      logfilename = argv[i];
    }
    else if (!strcmp("-Xmllog", argv[i])) {
      i++;
      if (argc <= i) {
	dprintf( D_ALWAYS, "No xmllog file specified!\n" );
	Usage();
      }
      xmllogfilename = argv[i];
    }
    else if (!strcmp("-Userlog", argv[i])) {
      i++;
      if (argc <= i) {
	dprintf( D_ALWAYS, "No userlog file specified!\n" );
	Usage();
      }
      userlogfilename = argv[i];
    }
    else if (!strcmp("-Clientagent", argv[i])) {
      i++;
      if (argc <= i) {
	dprintf( D_ALWAYS, "No Client Agent hostname specified!\n" );
	Usage();
      }
      clientagenthost = argv[i];
    }
    else if (!strcmp("-p", argv[i])) {
      i++;
      // Let DC take care of this
    }
    else{
      Usage();
    }
    
  }
      
  //dump the configuration
  dprintf(D_ALWAYS,
	  "==============================================================\n");
  dprintf(D_ALWAYS,
	  "              CONDOR_DATA_MANAGER CONFIGURATION:                 \n");
  dprintf(D_ALWAYS,
	   "==============================================================\n");
  
  //read the logfilename
  dprintf(D_ALWAYS,"DaP log file     : %s\n", logfilename);
  dprintf(D_ALWAYS,"Userlog file     : %s\n", userlogfilename);
  dprintf(D_ALWAYS,"XML log file     : %s\n", xmllogfilename);
  dprintf(D_ALWAYS,"Client Agent host: %s\n", clientagenthost);

//  sprintf(logfilename,"%s",argv[1]);
  
  dprintf(D_ALWAYS,
	  "==============================================================\n");
}

#include "stork-lm.h"

/* ============================================================================
 * initializations for the reqex_daemon
 * ==========================================================================*/
int main_init(int argc, char **argv)
{
	parse_arguments(argc, argv);

	//register timers (start_time,period,....)
	/*  daemonCore->Register_Timer(0,
		(TimerHandler)initializations,
		"initializations");*/

	IdleJobMonitorInterval =
		param_integer(
			"STORK_IDLE_JOB_MONITOR",
			STORK_IDLE_JOB_MONITOR_DEFAULT,
			STORK_IDLE_JOB_MONITOR_MIN);
	IdleJobMonitorTid = 
		daemonCore->Register_Timer(
			1,							// deltawhen
			IdleJobMonitorInterval,		// period
			call_main,	// event
			"call_main");				// description

	daemonCore->Register_Command(STORK_SUBMIT, "STORK_SUBMIT",
								 (CommandHandler)&handle_stork_submit,
								 "handle_stork_submit", NULL,WRITE);

	daemonCore->Register_Command(STORK_STATUS, "STORK_STATUS",
								 (CommandHandler)&handle_stork_status,
								 "handle_stork_status", NULL,WRITE);


	daemonCore->Register_Command(STORK_LIST, "STORK_LIST",
								 (CommandHandler)&handle_stork_list,
								 "handle_stork_list", NULL,WRITE);

	daemonCore->Register_Command(STORK_REMOVE, "STORK_REMOVE",
								 (CommandHandler)&handle_stork_remove,
								 "handle_stork_remove", NULL,WRITE);
  
	HungJobMonitorInterval =
		param_integer(
			"STORK_HUNG_JOB_MONITOR",
			STORK_HUNG_JOB_MONITOR_DEFAULT,
			STORK_HUNG_JOB_MONITOR_MIN);
	HungJobMonitorTid = 
		daemonCore->Register_Timer(
			4,							// deltawhen
			HungJobMonitorInterval,		// period
			regular_check_for_requests_in_process,
			"check_for_requests_in_process");

	// SC05 Hackery
#if SC2005_DEMO
	LowWaterTid =
		daemonCore->Register_Timer(
			1,							// deltawhen
			60,		// period
			low_water_timer,
			"low_water_timer");
#endif

	RescheduledJobMonitorInterval =
		param_integer(
			"STORK_RESCHEDULED_JOB_MONITOR",
			STORK_RESCHEDULED_JOB_MONITOR_DEFAULT,
			STORK_RESCHEDULED_JOB_MONITOR_MIN);
	RescheduledJobMonitorTid = 
		daemonCore->Register_Timer(
			7,							// deltawhen
			RescheduledJobMonitorInterval,	// period
			regular_check_for_rescheduled_requests,
			"regular_check_for_rescheduled_requests");

	//register reaper functions
	transfer_dap_reaper_id =
		daemonCore->Register_Reaper("transfer_dap_reaper",
									(ReaperHandler)transfer_dap_reaper,
									"reaper for transfer DaP");

	reserve_dap_reaper_id =
		daemonCore->Register_Reaper("reserve_dap_reaper",
									(ReaperHandler)reserve_dap_reaper,
									"reaper for reserve DaP");

	release_dap_reaper_id =
		daemonCore->Register_Reaper("release_dap_reaper",
									(ReaperHandler)release_dap_reaper,
									"reaper for release DaP");

	requestpath_dap_reaper_id =
		daemonCore->Register_Reaper("requestpath_dap_reaper",
									(ReaperHandler)requestpath_dap_reaper,
									"reaper for requestpath DaP");

	// SC05 Hackery
#if SC2005_DEMO
	low_water_reaper_id =
		daemonCore->Register_Reaper("low_water_reaper",
									(ReaperHandler)low_water_reaper,
									"reaper for low_water");
#endif

	if (!initializations()) {
		DC_Exit (1);
		return FALSE;
	}

	return TRUE;
}

/* ============================================================================
 * reconfigure reqex_daemon
 * ==========================================================================*/
int main_config(bool v)
{
	(void) v;
	dprintf(D_ALWAYS,"RECONFIGURING ......\n");
	read_config_file();
	return TRUE;
}

/* ============================================================================
 * shutdown reqex_daemon fast
 * ==========================================================================*/
int main_shutdown_fast(void)
{
	dprintf(D_ALWAYS,"SHUTDOWN FAST ....\n");
	terminate(TERMINATE_FAST);
  
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

/* ============================================================================
 * shutdown reqex_daemon gracefully
 * ==========================================================================*/
int main_shutdown_graceful()
{
	dprintf(D_ALWAYS,"SHUTDOWN GRACEFULLY ....\n");
	terminate(TERMINATE_GRACEFUL);

	DC_Exit(0);
	return TRUE;	// to satisfy c++
}


void main_pre_dc_init(int argc, char **argv)
{
	(void) argc;
	(void) argv;
}

int
handle_stork_submit_more(Service *, Stream *s)
{
	return write_requests_to_file((ReliSock*)s);
}

int 
handle_stork_submit(Service *, int command, Stream *s)
{
	dprintf (D_COMMAND, "handle_stork_submit (%d)\n", command);
	if (s->type( ) != Stream::reli_sock ) {
		dprintf (D_ALWAYS, "ERROR: Socket not of type reli_sock!\n");
		return FALSE;
	}
	int rc = write_requests_to_file ((ReliSock*)s);
	if(rc == KEEP_STREAM) {
		daemonCore->Register_Socket(s,
									"STORK_SUBMIT",
									(SocketHandler)&handle_stork_submit_more,
									"handle_stork_submit", NULL, WRITE);

	}
	return rc;
}


void
main_pre_command_sock_init( void )
{
}


int
handle_stork_list(Service *, int command, Stream *s)
{
	dprintf (D_COMMAND, "handle_stork_list (%d)\n", command);
	if (s->type( ) != Stream::reli_sock ) {
		dprintf (D_ALWAYS, "ERROR: Socket not of type reli_sock!\n");
		return FALSE;
	}

	return list_queue((ReliSock*)s);
}


int
handle_stork_remove(Service *, int command, Stream *s)
{
	dprintf (D_COMMAND, "handle_stork_remove (%d)\n", command );
	if (s->type( ) != Stream::reli_sock ) {
		dprintf (D_ALWAYS, "ERROR: Socket not of type reli_sock!\n");
		return FALSE;
	}

	return remove_requests_from_queue ((ReliSock*)s);
}


int
handle_stork_status(Service *, int command, Stream *s)
{
	dprintf (D_COMMAND, "handle_stork_status (%d)\n", command );
	if (s->type( ) != Stream::reli_sock ) {
		dprintf (D_ALWAYS, "ERROR: Socket not of type reli_sock!\n");
		return FALSE;
	}

	return send_dap_status_to_client((ReliSock*)s);
}
