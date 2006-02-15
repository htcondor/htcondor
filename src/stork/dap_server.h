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
#ifndef _DAP_SERVER_H
#define _DAP_SERVER_H

#include "condor_common.h"
#include "condor_string.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "user_log.c++.h"
#include "dap_constants.h"
#include "sock.h"

int initializations();
int read_config_file();
int call_main();
void startup_check_for_requests_in_process();
void regular_check_for_requests_in_process();
void regular_check_for_rescheduled_requests();

int handle_stork_submit(Service *, int command, Stream *s);
int handle_stork_remove(Service *, int command, Stream *s);
int handle_stork_status(Service *, int command, Stream *s);
int handle_stork_list(Service *, int command, Stream *s);

int transfer_dap_reaper(Service *,int pid,int exit_status);
int reserve_dap_reaper(Service *,int pid,int exit_status);
int release_dap_reaper(Service *,int pid,int exit_status);
int requestpath_dap_reaper(Service *,int pid,int exit_status);

int write_requests_to_file(ReliSock * sock);
int remove_requests_from_queue (ReliSock * sock);
int send_dap_status_to_client (ReliSock * sock);
int list_queue (ReliSock * sock);

void remove_credential (char * dap_id);
char * get_credential_filename (char * dap_id);
int get_cred_from_credd (const char * request, void *& buff, int & size);

#if 0
int init_user_id_from_FQN (const char *owner);
#endif

#endif

