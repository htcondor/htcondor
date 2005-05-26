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

int init_user_id_from_FQN (const char *owner);
#endif

