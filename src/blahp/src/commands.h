/*
#  File:     commands.h
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Copyright (c) Members of the EGEE Collaboration. 2007-2010. 
#
#    See http://www.eu-egee.org/partners/ for details on the copyright
#    holders.  
#  
#    Licensed under the Apache License, Version 2.0 (the "License"); 
#    you may not use this file except in compliance with the License. 
#    You may obtain a copy of the License at 
#  
#        http://www.apache.org/licenses/LICENSE-2.0 
#  
#    Unless required by applicable law or agreed to in writing, software 
#    distributed under the License is distributed on an "AS IS" BASIS, 
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
#    See the License for the specific language governing permissions and 
#    limitations under the License.
#
*/

/* Command structure */
#define COMMAND_MAX_LEN 50
typedef struct command_s {
	char    cmd_name[COMMAND_MAX_LEN];
	int     required_params;
	int     threaded;
	void    *(*cmd_handler)(void *);
} command_t;

/* Command handlers prototypes
 * */
void *cmd_ping(void *args);
void *cmd_submit_job(void *args);
void *cmd_cancel_job(void *args);
void *cmd_status_job(void *args);
void *cmd_status_job_all(void *args);
void *cmd_renew_proxy(void *args);
void *cmd_send_proxy_to_worker_node(void *args);
void *cmd_quit(void *args);
void *cmd_version(void *args);
void *cmd_commands(void *args);
void *cmd_async_on(void *args);
void *cmd_async_off(void *args);
void *cmd_results(void *args);
void *cmd_hold_job(void *args);
void *cmd_resume_job(void *args);
void *cmd_get_hostport(void *args);
void *cmd_set_glexec_dn(void *args);
void *cmd_unset_glexec_dn(void *args);
void *cmd_set_sudo_id(void *args);
void *cmd_set_sudo_off(void *args);
void *cmd_unknown(void *args);
void *cmd_cache_proxy_from_file(void *args);
void *cmd_use_cached_proxy(void *args);
void *cmd_uncache_proxy(void *args);


/* Function prototypes
 * */
command_t *find_command(const char *cmd);
char *known_commands(void);
int parse_command(const char *cmd, int *argc, char ***argv);

