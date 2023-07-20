/*
#  File:     server.c
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#   20 Mar 2004 - Original release.
#   23 Apr 2004 - Command parsing moved to a dedicated module.
#   25 Apr 2004 - Handling of Job arguments as list in classad
#                 Result buffer made persistent between sessions
#   29 Apr 2004 - Handling of Job arguments as list in classad removed
#                 Result buffer no longer persistant
#    7 May 2004 - 'Write' commands embedded in locks to avoid output corruption
#                 Uses dynamic strings to retrieve classad attributes' value
#   12 Jul 2004 - (prelz@mi.infn.it). Fixed quoting style for environment.
#   13 Jul 2004 - (prelz@mi.infn.it). Make sure an entire command is assembled 
#                                     before forwarding it.
#   20 Sep 2004 - Added support for Queue attribute.
#   12 Dec 2006 - (prelz@mi.infn.it). Added commands to cache proxy
#                 filenames.
#   23 Nov 2007 - (prelz@mi.infn.it). Access blah.config via config API.
#   31 Jan 2008 - (prelz@mi.infn.it). Add watches on a few needed processes
#                                     (bupdater and bnotifier for starters)
#   31 Mar 2008 - (rebatto@mi.infn.it) Async mode handling moved to resbuffer.c
#                                      Adapted to new resbuffer functions
#   17 Jun 2008 - (prelz@mi.infn.it). Add access to the job proxy stored
#                                     in the job registry.
#                                     Make job proxy optional.
#   15 Oct 2008 - (prelz@mi.infn.it). Make proxy renewal on worker nodes
#                                     optional via config.
#   18 Mar 2009 - (rebatto@mi.infn.it) Glexec references replaced with generic
#                                      mapping names. Constants' definitions
#                                      moved to mapped_exec.h.
#   15 Sep 2011 - (prelz@mi.infn.it). Optionally pass any submit attribute
#                                     to local configuration script.
#                                      
#
#  Description:
#   Serve a connection to a blahp client, performing appropriate
#   job operations according to client requests.
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
#
*/

#include "acconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <errno.h>
#include <netdb.h>
#include <libgen.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <assert.h>
#include <ctype.h>

#if defined(HAVE_GLOBUS)
#include "globus_gsi_credential.h"
#include "globus_gsi_proxy.h"
#endif

#include "blahpd.h"
#include "config.h"
#include "job_registry.h"
#include "classad_c_helper.h"
#include "commands.h"
#include "job_status.h"
#include "resbuffer.h"
#include "mapped_exec.h"
#include "proxy_hashcontainer.h"
#include "blah_utils.h"
#include "cmdbuffer.h"

#define COMMAND_PREFIX "-c"
#define JOBID_REGEXP            "(^|\n)BLAHP_JOBID_PREFIX([^\n]*)"
#define HOLD_JOB                1
#define RESUME_JOB              0
#define MAX_LRMS_NUMBER 	10
#define MAX_LRMS_NAME_SIZE	8
#define MAX_TEMP_ARRAY_SIZE              1000
#define MAX_PENDING_COMMANDS             500
#define DEFAULT_TEMP_DIR                 "/tmp"
 
#define NO_QUOTE     0
#define SINGLE_QUOTE 1
#define DOUBLE_QUOTE 2
#define INT_NOQUOTE  3

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  1
#endif

#ifndef VERSION
#define VERSION            "1.8.0"
#endif

const char *opt_format[] = {
	" %s %s",          /* NO_QUOTE */
	" %s '%s'",        /* SINGLE_QUOTE */
	" %s '\"%s\"'",    /* DOUBLE_QUOTE */
	" %s %d"           /* INT_NOQUOTE */
};

const char *statusstring[] = {
 "IDLE",
 "RUNNING",
 "REMOVED",
 "IDLE",
 "HELD",
};

/* Function prototypes */
char *get_command(int client_socket);
int set_cmd_list_option(char **command, classad_context cad, const char *attribute, const char *option);
int set_cmd_string_option(char **command, classad_context cad, const char *attribute, const char *option, const int quote_style);
int set_cmd_int_option(char **command, classad_context cad, const char *attribute, const char *option, const int quote_style);
int set_cmd_bool_option(char **command, classad_context cad, const char *attribute, const char *option, const int quote_style);
static char *limit_proxy(char* proxy_name, char *requested_name, char **error_message);
int getProxyInfo(char* proxname, char** subject, char** fqan);
int logAccInfo(char* jobId, char* server_lrms, classad_context cad, char* fqan, char* userDN, char** environment);
int CEReq_parse(classad_context cad, char* filename, char *proxysubject, char *proxyfqan);
char* outputfileRemaps(char *sb,char *sbrmp);
int check_TransferINOUT(classad_context cad, char **command, char *reqId, char **resultLine, char ***files_to_clean_up);
char *ConvertArgs(char* args, char sep);
void enqueue_result(char *res);

/* Global variables */
struct blah_managed_child {
	char *exefile;
	char *pidfile;
	char *sname;
	time_t lastfork;
};
static struct blah_managed_child *blah_children=NULL;
static int blah_children_count=0;

config_handle *blah_config_handle = NULL;
config_entry *blah_accounting_log_location = NULL;
config_entry *blah_accounting_log_umask = NULL;
job_registry_handle *blah_jr_handle = NULL;
static int server_socket;
static int exit_program = 0;
static pthread_mutex_t send_lock  = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t bfork_lock  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t blah_jr_lock  = PTHREAD_MUTEX_INITIALIZER;
pthread_attr_t cmd_threads_attr;

sem_t sem_total_commands;

char *blah_script_location;
char *blah_version;
static char lrmslist[MAX_LRMS_NUMBER][MAX_LRMS_NAME_SIZE];
static int  lrms_counter = 0;
static mapping_t current_mapping_mode = MEXEC_NO_MAPPING;
char *tmp_dir;
char *bssp = NULL;
int enable_condor_glexec = FALSE;
int require_proxy_on_submit = FALSE;
int disable_wn_proxy_renewal = FALSE;
int disable_proxy_user_copy = FALSE;
int disable_limited_proxy = FALSE;
int synchronous_termination = FALSE;

static char *mapping_parameter[MEXEC_PARAM_COUNT];

static char **submit_attributes_to_pass = NULL;
static int pass_all_submit_attributes = FALSE;

/* Check on good health of our managed children
 **/
void
check_on_children(struct blah_managed_child *children, const int count)
{
	FILE *pid;
	pid_t ch_pid;
	int junk;
	int i;
	int try_to_restart;
	int fret;
	static time_t calldiff=0;
	const time_t default_calldiff = 150;
	config_entry *ccld;
	time_t now;

	pthread_mutex_lock(&bfork_lock);
	if (calldiff <= 0)
	{
		ccld = config_get("blah_children_restart_interval",blah_config_handle);
		if (ccld != NULL) calldiff = atoi(ccld->value);
		if (calldiff <= 0) calldiff = default_calldiff;
	}

	time(&now);

	for (i=0; i<count; i++)
	{
		try_to_restart = 0;
		if ((pid = fopen(children[i].pidfile, "r")) == NULL)
		{
			if (errno != ENOENT) continue;
			else try_to_restart = 1;
		} else {
			if (fscanf(pid,"%d",&ch_pid) < 1) 
			{
				fclose(pid);
				continue;
			}
			if (kill(ch_pid, 0) < 0)
			{
				/* The child process disappeared. */
				if (errno == ESRCH) try_to_restart = 1;
			}
			fclose(pid);
		}
		if (try_to_restart)
		{
			/* Don't attempt to restart too often. */
			if ((now - children[i].lastfork) < calldiff) 
			{
				fprintf(stderr,"Restarting %s (%s) too frequently.\n",
					children[i].exefile, children[i].sname);
				fprintf(stderr,"Last restart %zu seconds ago (<%zu).\n",
					(now - children[i].lastfork), calldiff);
				continue;
			}
			fret = fork();
			if (fret == 0)
			{
				/* Child process. Exec exe file. */
				if (execl(children[i].exefile, children[i].exefile, NULL) < 0)
				{
					fprintf(stderr,"Cannot exec %s: %s\n",
						children[i].exefile,
						strerror(errno));
					exit(1);
				}
			} else if (fret < 0) {
				fprintf(stderr,"Cannot fork trying to start %s: %s\n",
					children[i].exefile,
					strerror(errno));
			}
			children[i].lastfork = now;
		}
	}

	/* Reap dead children. Yuck.*/
	while (waitpid(-1, &junk, WNOHANG) > 0) /* Empty loop */;

	pthread_mutex_unlock(&bfork_lock);
}

/* Free all tokens of a command
 * */
void
free_args(char **arg_array)
{
	char **arg_ptr;
	
	if (arg_array)
	{
		for (arg_ptr = arg_array; (*arg_ptr) != NULL; arg_ptr++)
			free(*arg_ptr);
		free(arg_array);
	}
}	
/* Main server function 
 * */
int
serveConnection(int cli_socket, char* cli_ip_addr)
{
	char *input_buffer;
	int get_cmd_res;
	char *reply;
	char *result;
	char *cmd_result;
	int exitcode = 0;
	pthread_t task_tid;
	int i, argc;
	char **argv;
	command_t *command;
#if 0
	char *needed_libs=NULL;
	char *old_ld_lib=NULL;
	char *new_ld_lib=NULL;
#endif
	config_entry *suplrms, *jre;
	char *next_lrms_s, *next_lrms_e;
	int lrms_len;
	char *children_names[3] = {"bupdater", "bnotifier", NULL};
	char **child_prefix;
	char *child_exe_conf, *child_pid_conf;
        config_entry *child_config_exe, *child_config_pid;
	int max_threaded_cmds = MAX_PENDING_COMMANDS;
	config_entry *max_threaded_conf;
	int n_threads_value;
	char *final_results;
	struct stat tmp_stat;
	job_registry_index_mode jr_mode;
	config_entry *check_children_interval_conf;
	int check_children_interval = -1; /* no timeout by default */
        config_entry *pass_attr;
	int virtualorg_found;
	char **attr;
	int n_attrs;
	int write_result;

	blah_config_handle = config_read(NULL);
	if (blah_config_handle == NULL)
	{
		fprintf(stderr, "Cannot access blah.config file in default locations ($BLAHPD_CONFIG_LOCATION, or $GLITE_LOCATION/etc or $BLAHPD_LOCATION/etc): ");
		perror("");
		exit(MALLOC_ERROR);
	}

	check_children_interval_conf = config_get("blah_check_children_interval",blah_config_handle);
	if (check_children_interval_conf != NULL)
		check_children_interval = atoi(check_children_interval_conf->value);

	/* Start with a 4 KB input buffer */
	if (cmd_buffer_init(cli_socket, 4096, check_children_interval) != CMDBUF_OK)
	{
		perror("Cannot allocate input buffer");
		exit(MALLOC_ERROR);
	}

	blah_accounting_log_location = config_get("BLAHPD_ACCOUNTING_INFO_LOG",blah_config_handle);
	blah_accounting_log_umask = config_get("blah_accounting_log_umask",blah_config_handle);
	max_threaded_conf = config_get("blah_max_threaded_cmds",blah_config_handle);
	if (max_threaded_conf != NULL) max_threaded_cmds = atoi(max_threaded_conf->value);

	for (i = 0; i < MEXEC_PARAM_COUNT; i++)
		mapping_parameter[i] = NULL;

	init_resbuffer();
	if (cli_socket == 0) server_socket = 1;
	else                 server_socket = cli_socket;

	/* Get values from environment */
	if ((result = getenv("GLITE_LOCATION")) == NULL)
	{
		result = DEFAULT_GLITE_LOCATION;
	}
	if ((tmp_dir = getenv("GAHP_TEMP")) == NULL)
	{
		tmp_dir  = DEFAULT_TEMP_DIR;
	}

/* In the Condor build of the blahp, we can find all the libraries we need
 * via the RUNPATH. Setting LD_LIBRARY_PATH can muck up the command line
 * tools for the local batch system.
 *
 * Similarly, in OSG, all Globus libraries are in the expected location.
 */
#if 0
	needed_libs = make_message("%s/lib:%s/externals/lib:%s/lib:/opt/lcg/lib", result, result, getenv("GLOBUS_LOCATION") ? getenv("GLOBUS_LOCATION") : "/opt/globus");
	old_ld_lib=getenv("LD_LIBRARY_PATH");
	if(old_ld_lib)
	{
		new_ld_lib =(char*)malloc(strlen(old_ld_lib) + 2 + strlen(needed_libs));
		if (new_ld_lib == NULL)
		{
			fprintf(stderr, "Out of memory\n");
			exit(MALLOC_ERROR);
		}
	  	sprintf(new_ld_lib,"%s;%s",old_ld_lib,needed_libs);
	  	setenv("LD_LIBRARY_PATH",new_ld_lib,1);
	}
	else
	 	 setenv("LD_LIBRARY_PATH",needed_libs,1);
#endif	
	blah_script_location = strdup(blah_config_handle->libexec_path);
	blah_version = make_message(RCSID_VERSION, VERSION, "poly,new_esc_format");
	require_proxy_on_submit = config_test_boolean(config_get("blah_require_proxy_on_submit",blah_config_handle));
	enable_condor_glexec = config_test_boolean(config_get("blah_enable_glexec_from_condor",blah_config_handle));
	disable_wn_proxy_renewal = config_test_boolean(config_get("blah_disable_wn_proxy_renewal",blah_config_handle));
	disable_proxy_user_copy = config_test_boolean(config_get("blah_disable_proxy_user_copy",blah_config_handle));
        disable_limited_proxy = config_test_boolean(config_get("blah_disable_limited_proxy",blah_config_handle));

	/* Scan configuration for submit attributes to pass to local script */
	pass_all_submit_attributes = config_test_boolean(config_get("blah_pass_all_submit_attributes",blah_config_handle));
	if (!pass_all_submit_attributes)
	{
		pass_attr = config_get("blah_pass_submit_attributes",blah_config_handle);
		if (pass_attr != NULL)
		{
			submit_attributes_to_pass = pass_attr->values;
		}
		virtualorg_found = FALSE;
		n_attrs = 0;
		for (attr = submit_attributes_to_pass; (attr != NULL) && ((*attr) != NULL); attr++)
		{
			if (strcmp(*attr, "VirtualOrganisation") == 0) virtualorg_found = TRUE;
			n_attrs++;
		}
		/* Make sure we have VirtualOrganisation here */
		if (!virtualorg_found)
		{
			submit_attributes_to_pass = realloc(submit_attributes_to_pass, sizeof(char *) * (n_attrs+2));
			if (submit_attributes_to_pass != NULL)
			{
				submit_attributes_to_pass[n_attrs] = strdup("VirtualOrganisation");
				submit_attributes_to_pass[n_attrs+1] = NULL;
				if (pass_attr != NULL)
				{
					pass_attr->values = submit_attributes_to_pass;
				}
			}
		}
		
	}
				
	if (enable_condor_glexec)
	{
		/* Enable condor/glexec commands */
		/* FIXME: should check/assert for success */
		command = find_command("CACHE_PROXY_FROM_FILE");
		if (command) command->cmd_handler = cmd_cache_proxy_from_file;
		command = find_command("USE_CACHED_PROXY");
		if (command) command->cmd_handler = cmd_use_cached_proxy;
		command = find_command("UNCACHE_PROXY");
		if (command) command->cmd_handler = cmd_uncache_proxy;
		/* Check that tmp_dir is group writable */
		if (stat(tmp_dir, &tmp_stat) >= 0)
		{
			if ((tmp_stat.st_mode & S_IWGRP) == 0)
			{
				if (chmod(tmp_dir,tmp_stat.st_mode|S_IWGRP|S_IRGRP|S_IXGRP)<0)
				{
					fprintf(stderr,"WARNING: cannot make %s group writable: %s\n",
					        tmp_dir, strerror(errno));
				}
			}
		}
	}

	suplrms = config_get("supported_lrms", blah_config_handle);

	if (suplrms != NULL)
	{
		next_lrms_s = suplrms->value;
		for(lrms_counter = 0; next_lrms_s < (suplrms->value + strlen(suplrms->value)) && lrms_counter < MAX_LRMS_NUMBER; )
		{
			next_lrms_e = strchr(next_lrms_s,','); 
			if (next_lrms_e == NULL) 
				next_lrms_e = suplrms->value + strlen(suplrms->value);
			lrms_len = (int)(next_lrms_e - next_lrms_s);
			if (lrms_len >= MAX_LRMS_NAME_SIZE)
				lrms_len = MAX_LRMS_NAME_SIZE-1;
			if (lrms_len > 0)
			{
				memcpy(lrmslist[lrms_counter],next_lrms_s,lrms_len);
				lrmslist[lrms_counter][lrms_len] = '\000';
				lrms_counter++;
			}
			if (*next_lrms_e == '\000') break;
			next_lrms_s = next_lrms_e + 1;
		}
	}
	
	jre = config_get("job_registry", blah_config_handle);
	if (jre != NULL)
	{
		jr_mode = BY_BLAH_ID;
		if (config_test_boolean(config_get("job_registry_use_mmap", blah_config_handle))) {
			jr_mode = BY_BLAH_ID_MMAP;
		}
		blah_jr_handle = job_registry_init(jre->value, jr_mode);
		if (blah_jr_handle != NULL)
		{
			/* Enable BLAH_JOB_STATUS_ALL/SELECT commands */
                        /* (served by the same function) */
			/* FIXME: should check/assert for success */
			command = find_command("BLAH_JOB_STATUS_ALL");
			if (command) command->cmd_handler = cmd_status_job_all;
			command = find_command("BLAH_JOB_STATUS_SELECT");
			if (command) command->cmd_handler = cmd_status_job_all; 
		}
	}

	/* Get list of BLAHPD children whose survival we care about. */

	blah_children_count = 0;
	for (child_prefix = children_names; (*child_prefix)!=NULL;
	     child_prefix++)
	{
		child_pid_conf = make_message("%s_pidfile",*child_prefix);
		child_exe_conf = make_message("%s_path",*child_prefix);
		if (child_exe_conf == NULL || child_pid_conf == NULL)
		{
			fprintf(stderr, "Out of memory\n");
			exit(MALLOC_ERROR);
		}
		child_config_exe = config_get(child_exe_conf,blah_config_handle);
		child_config_pid = config_get(child_pid_conf,blah_config_handle);
		
		if (child_config_exe != NULL && child_config_pid != NULL)
		{
			if (strlen(child_config_exe->value) <= 0 ||
			    strlen(child_config_pid->value) <= 0) 
			{
				free(child_exe_conf);
				free(child_pid_conf);
				continue;
			}

			blah_children = realloc(blah_children,
				(blah_children_count + 1) * 
				sizeof(struct blah_managed_child));
			if (blah_children == NULL)
			{
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}
			blah_children[blah_children_count].exefile = strdup(child_config_exe->value);
			blah_children[blah_children_count].pidfile = strdup(child_config_pid->value);
			blah_children[blah_children_count].sname = strdup(*child_prefix);
			if (blah_children[blah_children_count].exefile == NULL ||
			    blah_children[blah_children_count].pidfile == NULL ||
                            blah_children[blah_children_count].sname == NULL )
			{
				fprintf(stderr, "Out of memory\n");
				exit(MALLOC_ERROR);
			}
			blah_children[blah_children_count].lastfork = 0;
			blah_children_count++;
			
		}
		free(child_exe_conf);
		free(child_pid_conf);
	}

	if (blah_children_count>0) check_on_children(blah_children, blah_children_count);

	pthread_attr_init(&cmd_threads_attr);
	pthread_attr_setdetachstate(&cmd_threads_attr, PTHREAD_CREATE_DETACHED);

	sem_init(&sem_total_commands, 0, max_threaded_cmds);
	
	write_result = write(server_socket, blah_version, strlen(blah_version));
	if (write_result < 0) {
		exit_program = 1;
	}
	write_result = write(server_socket, "\r\n", 2);
	if (write_result < 0) {
		exit_program = 1;
	}
	while(!exit_program)
	{
		get_cmd_res = cmd_buffer_get_command(&input_buffer);
		if (get_cmd_res == CMDBUF_TIMEOUT)
		{
			if (blah_children_count>0) check_on_children(blah_children, blah_children_count);
		}
		else if (get_cmd_res == CMDBUF_OK)
		{
			if (parse_command(input_buffer, &argc, &argv) == 0)
				command = find_command(argv[0]);
			else
				command = NULL;

			if (command && (command->cmd_handler == cmd_unknown))
				command = NULL;

			if (command)
			{
				if (argc != (command->required_params + 1))
				{
					reply = make_message("E expected\\ %d\\ parameters,\\ %d\\ found\\r\n", command->required_params, argc-1);
				}
				else
				{
					if (command->threaded)
					{	
						/* Add the proxy parameters as last arguments */
						if (current_mapping_mode != MEXEC_NO_MAPPING) 
						{
							argv = (char **)realloc(argv, (argc + MEXEC_PARAM_COUNT + 1) * sizeof(char *));
							assert(argv);
							for (i = 0; i < MEXEC_PARAM_COUNT; i++) 
								argv[argc + i] = strdup(mapping_parameter[i]);
							argv[argc + MEXEC_PARAM_COUNT] = NULL;
						}

						if (sem_trywait(&sem_total_commands))
						{
							if (errno == EAGAIN)
								reply = make_message("F Threads\\ limit\\ reached\r\n");
							else
							{
								perror("sem_trywait()");
								exit(1);
							}
						}
						else if (pthread_create(&task_tid, &cmd_threads_attr, command->cmd_handler, (void *)argv))
						{
							reply = make_message("F Cannot\\ start\\ thread\r\n");
						}
						else
						{
							reply = make_message("S\r\n");
						}
						/* free argv in threaded function */
					}
					else
					{
						cmd_result = (char *)command->cmd_handler(argv);
						reply = make_message("%s\r\n", cmd_result);
						free(cmd_result);
						free_args(argv);
					}
				}
			}
			else
			{
				reply = make_message("E Unknown\\ command\r\n");
				free_args(argv);
			}

			pthread_mutex_lock(&send_lock);
			if (reply)
			{
				write_result = write(server_socket, reply, strlen(reply));
				free(reply);
				if (write_result < 0) {
					exit_program = 1;
				}
			}
			else
				/* WARNING: the command here could have been actually executed */
			{
				write_result = write(server_socket, "F Cannot\\ allocate\\ return\\ line\r\n", 34);
				if (write_result < 0) {
					exit_program = 1;
				}
			}
			pthread_mutex_unlock(&send_lock);
			
			free(input_buffer);
		}
		else /* cmd_buffer_get_command() returned an error */
		{
			if (synchronous_termination)
			{
				fprintf(stderr, "Waiting for threads to terminate.\n");
				for (;;)
				{
					if (sem_getvalue(&sem_total_commands, &n_threads_value) < 0)
					{
						fprintf(stderr, "Error getting value of sem_total_commands semaphore\n");
						exitcode = 1;
						break;
					}
					if (n_threads_value >= max_threaded_cmds)
					{
						final_results = get_lines();						
						if (final_results != NULL)
						{
							printf("%s\n",final_results);
							free(final_results);
						}
						exitcode = 0;
						break;
					}
					sleep(2); /* Carefully researched figure. */
				}
				
			} else {
				fprintf(stderr, "Connection closed by remote host\n");
				exitcode = 1;
			}
			break;
		}

	}

	if (cli_socket != 0) 
	{
		shutdown(cli_socket, SHUT_RDWR);
		close(cli_socket);
	}

	free(blah_script_location);
	free(blah_version);
	cmd_buffer_free();

	exit(exitcode);
}

/* Non threaded commands
 * --------
 * must return a pointer to a free-able string or NULL
 * */

void *
cmd_quit(void *args)
{
	char *result = NULL;
	
	exit_program = 1;
	result = strdup("S Server\\ exiting");
	return(result);
}

void *
cmd_commands(void *args)
{
    char *commands;
	char *result;

	if ((commands = known_commands()) != NULL)
	{
		result = make_message("S %s", commands);
		free(commands);
	}
	else
    {
		result = strdup("F known_commands()\\ returned\\ NULL");
	}
	return(result);
}

void *
cmd_unknown(void *args)
{
	/* Placeholder for commands that are not advertised via */
        /* known_commands and should therefore never be executed */
	return(NULL);
}
 
void *
cmd_cache_proxy_from_file(void *args)
{
	char **argv = (char **)args;
	char *proxyId = argv[1];
	char *proxyPath = argv[2];
	char *result;

	if (proxy_hashcontainer_append(proxyId, proxyPath) == NULL)
	{
		/* Error in insertion */
		result = strdup("F Internal\\ hash\\ error:\\ proxy\\ not\\ cached");
	} else 
	{
		result = strdup("S Proxy\\ cached");
	}
	return(result);
}

void *
cmd_use_cached_proxy(void *args)
{
	char **argv = (char **)args;
	char *glexec_argv[4];
	proxy_hashcontainer_entry *entry;
	char *proxyId = argv[1];
	char *result = NULL;
	char *junk;
	char *proxy_name, *proxy_dir, *escaped_proxy;
	int need_to_free_escaped_proxy = 0;
	struct stat proxy_stat;

	entry = proxy_hashcontainer_lookup(proxyId);
	if (entry == NULL)
	{
        result = strdup("F Cannot\\ find\\ required\\ proxyId");
	} else
	{
		glexec_argv[0] = argv[0];
		glexec_argv[1] = entry->proxy_file_name;
		glexec_argv[2] = entry->proxy_file_name;
		glexec_argv[3] = "0"; /* Limit the proxy */

		junk = (char *)cmd_set_glexec_dn((void *)glexec_argv);
		if (junk == NULL) 
		{
			result = strdup("F Glexec\\ proxy\\ not \\set\\ (internal\\ error)");
		} 
		else if (junk[0] == 'S')
		{ 
			free(junk);
			/* Check that the proxy dir is group writable */
			escaped_proxy = escape_spaces(entry->proxy_file_name);
			/* Out of memory: Try with the full name */
			if (!BLAH_DYN_ALLOCATED(escaped_proxy)) escaped_proxy = entry->proxy_file_name;
			else need_to_free_escaped_proxy = 1;

			proxy_name = strdup(entry->proxy_file_name);
			if (proxy_name != NULL)
			{
				proxy_dir = dirname(proxy_name);
				if (stat(proxy_dir, &proxy_stat) >= 0)
				{
					if ((proxy_stat.st_mode & S_IWGRP) == 0)
					{
						if (chmod(proxy_dir,proxy_stat.st_mode|S_IWGRP|S_IRGRP|S_IXGRP)<0)
						{
							result = make_message("S Glexec\\ proxy\\ set\\ to\\ %s\\ (could\\ not\\ set\\ dir\\ IWGRP\\ bit)",
									escaped_proxy);
						} else {
							result = make_message("S Glexec\\ proxy\\ set\\ to\\ %s\\ (dir\\ IWGRP\\ bit\\ set)",
									escaped_proxy);
						}
					} else {
						result = make_message("S Glexec\\ proxy\\ set\\ to\\ %s\\ (dir\\ IWGRP\\ bit\\ already\\ on)",
									escaped_proxy);
					}
				} else {
					result = make_message("S Glexec\\ proxy\\ set\\ to\\ %s\\ (cannot\\ stat\\ dirname)",
								escaped_proxy);

				}
				free(proxy_name);
			} else {
				result = make_message("S Glexec\\ proxy\\ set\\ to\\ %s\\ (Out\\ of\\ memory)",
							escaped_proxy);
			}
		} else {
            /* Return original cmd_set_glexec_dn() error */
			result = junk;
		}
	}
	if (need_to_free_escaped_proxy) free(escaped_proxy);
	return(result);
}

void *
cmd_uncache_proxy(void *args)
{
	char **argv = (char **)args;
	char *proxyId = argv[1];
	char *junk;
	char *result = NULL;

	proxy_hashcontainer_entry *entry;

	entry = proxy_hashcontainer_lookup(proxyId);
	if (entry == NULL)
	{
		result = strdup("F proxyId\\ not\\ found");
	} else
	{
                      /* Check for entry->proxy_file_name at the beginning */
                      /* of mapping_parameter[MEXEC_PARAM_SRCPROXY] to */
                      /* allow for limited proxies */
		if ( current_mapping_mode &&
		    ( strncmp(mapping_parameter[MEXEC_PARAM_SRCPROXY],
		              entry->proxy_file_name,
                              strlen(entry->proxy_file_name)) == 0) )
		{
			/* Need to unregister cached proxy from glexec */
                	junk = (char *)cmd_unset_glexec_dn(NULL);
					/* FIXME: need to check for error message in junk? */
                	if (junk != NULL) free(junk);
		}
		proxy_hashcontainer_unlink(proxyId);
		result = strdup("S Proxy\\ uncached");
	}
	return(result);
}

void *
cmd_version(void *args)
{
	char *result;

	result = make_message("S %s",blah_version);
	return(result);	
}

void *
cmd_async_on(void *args)
{
	char *result;

	set_async_mode(ASYNC_MODE_ON);
	result = strdup("S Async\\ mode\\ on");
	return(result);
}
			
void *
cmd_async_off(void *args)
{
	char *result;

	set_async_mode(ASYNC_MODE_OFF);
	result = strdup("S Async\\ mode\\ off");
	return(result);
}

void *
cmd_results(void *args)
{
	char *result;

	result = get_lines();
	return(result);
}

void
reset_mapping_parameters(void)
{
	int i;

	for (i = 0; i < MEXEC_PARAM_COUNT; i++)
		if (mapping_parameter[i])
		{
			free(mapping_parameter[i]);
			mapping_parameter[i] = NULL;
		}
	current_mapping_mode = MEXEC_NO_MAPPING;
}

void *
cmd_unset_glexec_dn(void *args)
{
	char *result;

	reset_mapping_parameters();
	result = make_message("S Glexec\\ mode\\ off");
	return(result);
}

void *
cmd_set_sudo_off(void *args)
{
	char *result;

	reset_mapping_parameters();
	result = make_message("S Sudo\\ mode\\ off");
	return(result);
}

void *
cmd_set_glexec_dn(void *args)
{

	char *result  = NULL;
	struct stat buf;
	char **argv = (char **)args;
	char *proxt4= argv[1];
	char *ssl_client_cert = argv[2];
	char *proxynameNew = NULL;
	
	if (current_mapping_mode != MEXEC_NO_MAPPING) reset_mapping_parameters();

	if((!stat(proxt4, &buf)) && (!stat(ssl_client_cert, &buf)))
	{
		mapping_parameter[MEXEC_PARAM_DELEGCRED] = strdup(ssl_client_cert);
		/* proxt4 must be limited for subsequent submission */		
		if(argv[3][0]=='0')
		{
                  if (((proxynameNew = limit_proxy(proxt4, NULL, NULL)) == NULL) ||
                      (disable_limited_proxy))
			{
				free(mapping_parameter[MEXEC_PARAM_DELEGCRED]);
				mapping_parameter[MEXEC_PARAM_DELEGCRED] = NULL;
				result = strdup("F Not\\ limiting\\ proxy\\ file");
			}
			else
				mapping_parameter[MEXEC_PARAM_SRCPROXY] = proxynameNew;
		}
		else
		{
			mapping_parameter[MEXEC_PARAM_SRCPROXY] = strdup(proxt4);
		}
		if (!result)
		{
			current_mapping_mode = MEXEC_GLEXEC;
			/* string version needed to pass it as char* parameter to threaded functions */
			mapping_parameter[MEXEC_PARAM_DELEGTYPE] = make_message("%d", current_mapping_mode);
			result = strdup("S Glexec\\ mode\\ on");
		}
	}
	else
	{
		result = strdup("F Cannot\\ stat\\ proxy\\ file");
	}
	return(result);
}

void *
cmd_set_sudo_id(void *args)
{
	char *result = NULL;
	char **argv = (char **)args;
	char *user_id = argv[1];

	if (current_mapping_mode != MEXEC_NO_MAPPING) reset_mapping_parameters();

	mapping_parameter[MEXEC_PARAM_DELEGCRED] = strdup(user_id);
	current_mapping_mode = MEXEC_SUDO;
	/* string version needed to pass it as char* parameter to threaded functions */
	mapping_parameter[MEXEC_PARAM_DELEGTYPE] = make_message("%d", current_mapping_mode);
	mapping_parameter[MEXEC_PARAM_SRCPROXY] = strdup("");

	result = strdup("S Sudo\\ mode\\ on");
	return(result);
}

/* Threaded commands
 * N.B.: functions must free argv before return
 * */

#define CMD_SUBMIT_JOB_ARGS 2
void *
cmd_submit_job(void *args)
{
	char *escpd_cmd_out, *escpd_cmd_err;
	int retcod;
	char *command = NULL;
	char jobId[JOBID_MAX_LEN];
	char *resultLine=NULL;
	char **argv = (char **)args;
	classad_context cad;
	char *reqId = argv[1];
	char *jobDescr = argv[2];
	char *server_lrms = NULL;
	int result;
	char *proxyname = NULL;
	char *iwd = NULL;
	char *proxysubject = NULL;
	char *proxyfqan = NULL;
	char *proxynameNew   = NULL;
	char *log_proxy = NULL;
	char *command_ext = NULL;
	char *tmp_subject, *tmp_fqan;
	int enable_log = 0;
	struct timeval ts;
	char *req_file=NULL;
	char **cur_file;
	regex_t regbuf;
	regmatch_t pmatch[3];
	char *arguments=NULL;
	char *conv_arguments=NULL;
	char *environment=NULL;
	char *conv_environment=NULL;
	exec_cmd_t submit_command = EXEC_CMD_DEFAULT;
	char *saved_proxyname=NULL;
	char **inout_files = NULL;

        if (blah_children_count>0) check_on_children(blah_children, blah_children_count);

	/* Parse the job description classad */
	if ((cad = classad_parse(jobDescr)) == NULL)
	{
		/* PUSH A FAILURE */
		resultLine = make_message("%s 1 Error\\ parsing\\ classad N/A", reqId);
		goto cleanup_argv;
	}

	/* Get the lrms type from classad attribute "gridtype" */
	if (classad_get_dstring_attribute(cad, "gridtype", &server_lrms) != C_CLASSAD_NO_ERROR)
	{
		/* PUSH A FAILURE */
		resultLine = make_message("%s 1 Missing\\ gridtype\\ in\\ submission\\ classAd N/A", reqId);
		goto cleanup_cad;
	}

	/* These two attributes are used in case the proxy supplied in */
        /* 'x509UserProxy' is not readable by the BLAH user */
        /* (e.g. when SUDO is used) */
	classad_get_dstring_attribute(cad, "x509UserProxySubject", &proxysubject);
	classad_get_dstring_attribute(cad, "x509UserProxyFQAN", &proxyfqan);

	/* Get the proxy name from classad attribute "X509UserProxy" */
	if (classad_get_dstring_attribute(cad, "x509UserProxy", &proxyname) != C_CLASSAD_NO_ERROR)
	{
		if (require_proxy_on_submit)
		{
			/* PUSH A FAILURE */
			resultLine = make_message("%s 1 Missing\\ x509UserProxy\\ in\\ submission\\ classAd N/A", reqId);
			goto cleanup_lrms;
		} else {
			proxyname = NULL;
		}
	}
	/* If the proxy is a relative path, we must prepend the Iwd to make it absolute */
	if (proxyname && proxyname[0] != '/') {
		if (classad_get_dstring_attribute(cad, "Iwd", &iwd) == C_CLASSAD_NO_ERROR) {
			size_t iwdlen = strlen(iwd);
			size_t proxylen = iwdlen + strlen(proxyname) + 1;
			char *proxynameTmp;
			proxynameTmp = malloc(proxylen + 1);
			if (!proxynameTmp) {
				resultLine = make_message("%s 1 Malloc\\ failure N/A", reqId);
				goto cleanup_lrms;
			}
			memcpy(proxynameTmp, iwd, iwdlen);
			proxynameTmp[iwdlen] = '/';
			strcpy(proxynameTmp+iwdlen+1, proxyname);
			free(proxyname);
			free(iwd);
			iwd = NULL;
			proxyname = proxynameTmp;
			proxynameTmp = NULL;
		} else {
			resultLine = make_message("%s 1 Relative\\ x509UserProxy\\ specified\\ without\\ Iwd N/A", reqId);
			goto cleanup_lrms;
		}
	}

	/* If there are additional arguments, we have to map on a different id */
	if(argv[CMD_SUBMIT_JOB_ARGS + 1] != NULL)
	{
		submit_command.delegation_type = atoi(argv[CMD_SUBMIT_JOB_ARGS + 1 + MEXEC_PARAM_DELEGTYPE]);
		submit_command.delegation_cred = argv[CMD_SUBMIT_JOB_ARGS + 1 + MEXEC_PARAM_DELEGCRED];
		if ((proxyname != NULL) && (!disable_proxy_user_copy))
		{
			if ((atoi(argv[CMD_SUBMIT_JOB_ARGS + 1 + MEXEC_PARAM_DELEGTYPE ]) != MEXEC_GLEXEC))
			{
				saved_proxyname = strdup(proxyname);
				submit_command.source_proxy = saved_proxyname;
				proxynameNew = make_message("%s.mapped", proxyname);
			}
			else
			{
				submit_command.source_proxy = argv[CMD_SUBMIT_JOB_ARGS + 1 + MEXEC_PARAM_SRCPROXY];
				proxynameNew = make_message("%s.glexec", proxyname);
			}
			/* Add the target proxy - cause glexec or sudo to move it to another file */
			if (proxynameNew)
			{
				free(proxyname);
				proxyname = proxynameNew;
				submit_command.dest_proxy = proxyname;
				log_proxy = submit_command.source_proxy;
			}
			else
			{
				fprintf(stderr, "blahpd: out of memory! Exiting...\n");
				exit(MALLOC_ERROR);
			}
		}
	}
	else if ((proxyname) != NULL && (!disable_limited_proxy))
	{
		/* not in glexec mode: need to limit the proxy */
		char *errmsg = NULL;
		if((proxynameNew = limit_proxy(proxyname, NULL, &errmsg)) == NULL)
		{
			/* PUSH A FAILURE */
			char * escaped_errmsg = (errmsg) ? escape_spaces(errmsg) : NULL;
			if (escaped_errmsg) resultLine = make_message("%s 1 Unable\\ to\\ limit\\ the\\ proxy\\ (%s) N/A", reqId, escaped_errmsg);
			else resultLine = make_message("%s 1 Unable\\ to\\ limit\\ the\\ proxy N/A", reqId);
			if (errmsg) free(errmsg);
			goto cleanup_proxyname;
		}
		free(proxyname);
		proxyname = proxynameNew;
		log_proxy = proxynameNew;
	}

	if (log_proxy != NULL)
	{
		/* DGAS accounting */
		if (getProxyInfo(log_proxy, &tmp_subject, &tmp_fqan))
		{
			/* PUSH A FAILURE */
			resultLine = make_message("%s 1 Credentials\\ not\\ valid N/A", reqId);
			goto cleanup_command;
		}
		/* Subject and FQAN read from a valid proxy take precedence */
		/* over those supplied in the submit command. */
		if (tmp_subject != NULL)
		{
			if (proxysubject != NULL) free(proxysubject);
			proxysubject = tmp_subject;
		}
		if (tmp_fqan != NULL)
		{
			if (proxyfqan != NULL) free(proxyfqan);
			proxyfqan = tmp_fqan;
		}
	}
	if ((proxysubject != NULL) && (proxyfqan != NULL)) 
	{
		enable_log = 1;
	}

	command = make_message("%s/%s_submit.sh", blah_script_location, server_lrms);

	if (command == NULL)
	{
		/* PUSH A FAILURE */
		resultLine = make_message("%s 1 Out\\ of\\ Memory N/A", reqId);
		goto cleanup_proxyname;
	}

	/* add proxy name and/or subjects if present */
	command_ext = NULL;
	if (proxyname != NULL)
	{
		command_ext = make_message("%s -x %s -u \"%s\" ", command, proxyname, proxysubject);
		if (command_ext == NULL)
		{
			/* PUSH A FAILURE */
			resultLine = make_message("%s 1 Out\\ of\\ memory\\ parsing\\ classad N/A", reqId);
			goto cleanup_command;
		}
	} else if (proxysubject != NULL) {
		command_ext = make_message("%s -u \"%s\" ", command, proxysubject);
		if (command_ext == NULL)
		{
			/* PUSH A FAILURE */
			resultLine = make_message("%s 1 Out\\ of\\ memory\\ parsing\\ classad N/A", reqId);
			goto cleanup_command;
		}
	}
	if (command_ext != NULL)
	{
		/* Swap new command in */
		free(command);
		command = command_ext;
	}

	/* Add command line option to explicitely disable proxy renewal */
	/* if requested. */
	if (disable_wn_proxy_renewal)
	{
		command_ext = make_message("%s -r no", command);
		if (command_ext == NULL)
		{
			/* PUSH A FAILURE */
			resultLine = make_message("%s 1 Out\\ of\\ memory\\ parsing\\ classad N/A", reqId);
			goto cleanup_command;
		}
		/* Swap new command in */
		free(command);
		command = command_ext;
	}

	/* Cmd attribute is mandatory: stop on any error */
	if (set_cmd_string_option(&command, cad, "Cmd", COMMAND_PREFIX, NO_QUOTE) != C_CLASSAD_NO_ERROR)
	{
		/* PUSH A FAILURE */
		resultLine = make_message("%s 7 Cannot\\ parse\\ Cmd\\ attribute\\ in\\ classad N/A", reqId);
		goto cleanup_command;
	}

	/* temporary directory path*/
	command_ext = make_message("%s -T %s", command, tmp_dir);
	if (command_ext == NULL)
	{
		/* PUSH A FAILURE */
		resultLine = make_message("%s 1 Out\\ of\\ memory\\ parsing\\ classad N/A", reqId);
		goto cleanup_command;
	}
	/* Swap new command in */
	free(command);
	command = command_ext;
	if(check_TransferINOUT(cad,&command,reqId,&resultLine,&inout_files))
	{
		goto cleanup_inoutfiles;
	}

	/* Set the CE requirements */
	gettimeofday(&ts, NULL);
	req_file = make_message("%s/ce-req-file-%ld%ld",tmp_dir, ts.tv_sec, ts.tv_usec);
	if(CEReq_parse(cad, req_file, proxysubject, proxyfqan) >= 0)
	{
		command_ext = make_message("%s -C %s", command, req_file);
		if (command_ext == NULL)
		{
			/* PUSH A FAILURE */
			resultLine = make_message("%s 1 Out\\ of\\ memory\\ parsing\\ classad N/A", reqId);
			goto cleanup_req;
		}
		/* Swap new command in */
		free(command);
		command = command_ext;
	}

	/* All other attributes are optional: fail only on memory error 
	   IMPORTANT: Args must alway be the last!
	*/
	if ((set_cmd_string_option(&command, cad, "In",         "-i", NO_QUOTE)      == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_string_option(&command, cad, "Out",        "-o", NO_QUOTE)      == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_string_option(&command, cad, "Err",        "-e", NO_QUOTE)      == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_string_option(&command, cad, "Iwd",        "-w", NO_QUOTE)      == C_CLASSAD_OUT_OF_MEMORY) ||
//	    (set_cmd_string_option(&command, cad, "Env",        "-v", SINGLE_QUOTE)  == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_string_option(&command, cad, "Queue",      "-q", NO_QUOTE)      == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_int_option   (&command, cad, "MICNumber",  "-P", INT_NOQUOTE)   == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_int_option   (&command, cad, "GPUNumber",  "-g", INT_NOQUOTE)   == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_string_option(&command, cad, "GPUMode",    "-G", NO_QUOTE)      == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_string_option(&command, cad, "GPUModel",   "-M", SINGLE_QUOTE)  == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_int_option   (&command, cad, "NodeNumber", "-n", INT_NOQUOTE)   == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_bool_option  (&command, cad, "WholeNodes", "-z", NO_QUOTE)      == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_int_option   (&command, cad, "HostNumber", "-h", INT_NOQUOTE)   == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_int_option   (&command, cad, "SMPGranularity", "-S", INT_NOQUOTE) == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_int_option   (&command, cad, "HostSMPSize", "-N", INT_NOQUOTE)  == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_bool_option  (&command, cad, "StageCmd",   "-s", NO_QUOTE)      == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_string_option(&command, cad, "ClientJobId","-j", NO_QUOTE)      == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_string_option(&command, cad, "JobDirectory","-D", NO_QUOTE)      == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_string_option(&command, cad, "BatchProject", "-A", NO_QUOTE) == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_int_option(&command, cad, "BatchRuntime", "-t", INT_NOQUOTE) == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_string_option(&command, cad, "BatchExtraSubmitArgs", "-a", SINGLE_QUOTE) == C_CLASSAD_OUT_OF_MEMORY) ||
	    (set_cmd_int_option(&command, cad, "RequestMemory", "-m", INT_NOQUOTE) == C_CLASSAD_OUT_OF_MEMORY))
//	    (set_cmd_string_option(&command, cad, "Args",      	"--", SINGLE_QUOTE)      == C_CLASSAD_OUT_OF_MEMORY))
	{
		/* PUSH A FAILURE */
		resultLine = make_message("%s 1 Out\\ of\\ memory\\ parsing\\ classad N/A", reqId);
		goto cleanup_req;
	}

	/* if present, "environment" attribute must be used instead of "env" */
	if ((result = classad_get_dstring_attribute(cad, "environment", &environment)) == C_CLASSAD_NO_ERROR)
	{
		if (environment[0] != '\000')
		{
			conv_environment = ConvertArgs(environment, ' ');
			/* fprintf(stderr, "DEBUG: args conversion <%s> to <%s>\n", environment, conv_environment); */
			if (conv_environment != NULL)
			{
				command_ext = make_message("%s -V %s", command, conv_environment);
				free(conv_environment);
			}
			if ((conv_environment == NULL) || (command_ext == NULL))
			{
				/* PUSH A FAILURE */
				resultLine = make_message("%s 1 Out\\ of\\ memory\\ parsing\\ classad N/A", reqId);
				free(environment);
				goto cleanup_req;
			}
			/* Swap new command in */
			free(command);
			command = command_ext;
		}
		free(environment);
	}
	else /* use Env old syntax */
	{
		if(set_cmd_string_option(&command, cad, "Env","-v", SINGLE_QUOTE) == C_CLASSAD_OUT_OF_MEMORY)
		{
			/* PUSH A FAILURE */
			resultLine = make_message("%s 1 Out\\ of\\ memory\\ parsing\\ classad N/A", reqId);
			goto cleanup_req;
		}
	}

	/* if present, "arguments" attribute must be used instead of "args" */
	if ((result = classad_get_dstring_attribute(cad, "Arguments", &arguments)) == C_CLASSAD_NO_ERROR)
	{
		if (arguments[0] != '\000')
		{
			conv_arguments = ConvertArgs(arguments, ' ');
			/* fprintf(stderr, "DEBUG: args conversion <%s> to <%s>\n", arguments, conv_arguments); */
			if (conv_arguments)
			{
				command_ext = make_message("%s -- %s", command, conv_arguments);
				free(conv_arguments);
			}
			if ((conv_arguments == NULL) || (command_ext == NULL))
			{
				/* PUSH A FAILURE */
				resultLine = make_message("%s 1 Out\\ of\\ memory\\ creating\\ submission\\ command N/A", reqId);
				goto cleanup_req;
			}
			/* Swap new command in */
			free(command);
			command = command_ext;
		}
		free(arguments);
	}
	else /* use Args old syntax */
	{
		if (set_cmd_string_option(&command, cad, "Args","--", SINGLE_QUOTE) == C_CLASSAD_OUT_OF_MEMORY)
		{
			/* PUSH A FAILURE */
			resultLine = make_message("%s 1 Out\\ of\\ memory\\ parsing\\ classad N/A", reqId);
			goto cleanup_req;
		}
	}

	/* Execute the submission command */
	submit_command.command = command;
	retcod = execute_cmd(&submit_command);

	if (retcod != 0)
	{
		escpd_cmd_err = escape_spaces(strerror(errno));
		resultLine = make_message("%s 3 Error\\ executing\\ the\\ submission\\ command:\\ %s", reqId, escpd_cmd_err);
		if (escpd_cmd_err) free(escpd_cmd_err);
		goto cleanup_cmd_out;
	}

	else if (submit_command.exit_code != 0)
	{
		/* PUSH A FAILURE */
		escpd_cmd_out = escape_spaces(submit_command.output);
		escpd_cmd_err = escape_spaces(submit_command.error);
		resultLine = make_message("%s %d submission\\ command\\ failed\\ (exit\\ code\\ =\\ %d)\\ (stdout:%s)\\ (stderr:%s) N/A",
		                            reqId, submit_command.exit_code, submit_command.exit_code, escpd_cmd_out, escpd_cmd_err);
		if (BLAH_DYN_ALLOCATED(escpd_cmd_out)) free(escpd_cmd_out);
		if (BLAH_DYN_ALLOCATED(escpd_cmd_err)) free(escpd_cmd_err);
		goto cleanup_cmd_out;
	}


	if (regcomp(&regbuf, JOBID_REGEXP, REG_EXTENDED) != 0)
	{
		/* this mean a bug in the regexp, cannot do anything further */
		fprintf(stderr, "Fatal: cannot compile regexp\n");
		exit(1);
	}

	if (regexec(&regbuf, submit_command.output, 3, pmatch, 0) != 0)
	{
		/* PUSH A FAILURE */
		escpd_cmd_out = escape_spaces(submit_command.output);
		escpd_cmd_err = escape_spaces(submit_command.error);
		resultLine = make_message("%s 8 no\\ jobId\\ in\\ submission\\ script's\\ output\\ (stdout:%s)\\ (stderr:%s) N/A",
		                          reqId, escpd_cmd_out, escpd_cmd_err);
		if (BLAH_DYN_ALLOCATED(escpd_cmd_out)) free(escpd_cmd_out);
		if (BLAH_DYN_ALLOCATED(escpd_cmd_err)) free(escpd_cmd_err);
		goto cleanup_regbuf;
	}

	submit_command.output[pmatch[2].rm_eo] = '\000';
	strncpy(jobId, submit_command.output + pmatch[2].rm_so, sizeof(jobId) - 1);

	/* PUSH A SUCCESS */
	resultLine = make_message("%s 0 No\\ error %s", reqId, jobId);
	
	/* DGAS accounting */
	if (enable_log)
		logAccInfo(jobId, server_lrms, cad, proxyfqan, proxysubject, argv + CMD_SUBMIT_JOB_ARGS + 1);

	/* Free up all arguments and exit (exit point in case of error is the label
	   pointing to last successfully allocated variable) */
cleanup_regbuf:
	regfree(&regbuf);
cleanup_cmd_out:
	cleanup_cmd(&submit_command);
cleanup_req:
	if (req_file != NULL)
	{
		unlink(req_file);
		free(req_file);
	}
cleanup_inoutfiles:
	if (inout_files != NULL)
	{
		for(cur_file = inout_files; *cur_file != NULL; cur_file++)
		{
			unlink(*cur_file);
			free(*cur_file);
		}
		free(inout_files);
	}
cleanup_command:
	if (command) free(command);
cleanup_proxyname:
	if (proxyname != NULL) free(proxyname);
	if (saved_proxyname != NULL) free(saved_proxyname);
cleanup_lrms:
	if (proxysubject != NULL) free(proxysubject);
	if (proxyfqan != NULL) free(proxyfqan);
	free(server_lrms);
cleanup_cad:
	classad_free(cad);
cleanup_argv:
	free_args(argv);
	if (resultLine)
	{
		enqueue_result(resultLine);
		free(resultLine);
	}
	else
	{
		fprintf(stderr, "blahpd: out of memory! Exiting...\n");
		exit(MALLOC_ERROR);
	}
	sem_post(&sem_total_commands);
	return NULL;
}

#define CMD_PING_ARGS 2
void *
cmd_ping(void* args)
{
	int retcod;
	long script_code;
	char *err_msg;
	char *escpd_cmd_out, *escpd_cmd_err;
	char *resultLine = NULL;
	char **argv = (char **)args;
	char *reqId = argv[1];
	char *lrms = argv[2];
	exec_cmd_t ping_command = EXEC_CMD_DEFAULT;

	/* Prepare the ping command */
	ping_command.command = make_message("%s/%s_ping.sh", blah_script_location, lrms);
	if (ping_command.command == NULL)
	{
		/* PUSH A FAILURE */
		resultLine = make_message("%s 1 Cannot\\ allocate\\ memory\\ for\\ the\\ command\\ string", reqId);
		goto cleanup_argv;
	}
	if (argv[CMD_PING_ARGS + 1] != NULL)
	{
		ping_command.delegation_type = atoi(argv[CMD_PING_ARGS + 1 + MEXEC_PARAM_DELEGTYPE]);
		ping_command.delegation_cred = argv[CMD_PING_ARGS + 1 + MEXEC_PARAM_DELEGCRED];
	}

	/* Execute the command */
	retcod = execute_cmd(&ping_command);

	if (retcod != 0)
	{
		escpd_cmd_err = escape_spaces(strerror(errno));
		resultLine = make_message("%s 3 Error\\ executing\\ the\\ ping\\ command:\\ %s", reqId, escpd_cmd_err);
		if (escpd_cmd_err) free(escpd_cmd_err);
		goto cleanup_command;
	}

	if (ping_command.exit_code != 0)
	{
		/* PUSH A FAILURE */
		escpd_cmd_out = escape_spaces(ping_command.output);
		escpd_cmd_err = escape_spaces(ping_command.error);
		resultLine = make_message("%s %d Ping\\ command\\ failed\\ (stdout:%s)\\ (stderr:%s)",
		                           reqId, ping_command.exit_code, escpd_cmd_out, escpd_cmd_err);
		if (BLAH_DYN_ALLOCATED(escpd_cmd_out)) free(escpd_cmd_out);
		if (BLAH_DYN_ALLOCATED(escpd_cmd_err)) free(escpd_cmd_err);
		goto cleanup_command;
	}

	script_code = strtol(ping_command.output, &err_msg, 10);
	while (*err_msg && isspace(*err_msg)) err_msg++;
	err_msg = escape_spaces(err_msg);
	resultLine = make_message("%s %ld %s", reqId, script_code, err_msg);
	if (BLAH_DYN_ALLOCATED(err_msg)) free(err_msg);
	/* resultLine = make_message("%s 0 No\\ Error", reqId); */

	/* Free up all arguments and exit (exit point in case of error is the label
	   pointing to last successfully allocated variable) */
cleanup_command:
	cleanup_cmd(&ping_command);
	free(ping_command.command);
cleanup_argv:
	free_args(argv);
	if(resultLine)
	{
		enqueue_result(resultLine);
		free (resultLine);
	}
	sem_post(&sem_total_commands);
	return NULL;
}

#define CMD_CANCEL_JOB_ARGS 2
void *
cmd_cancel_job(void* args)
{
	int retcod;
	char *escpd_cmd_out, *escpd_cmd_err;
	char *begin_res;
	char *end_res;
	int res_length;
	char *resultLine = NULL;
	char **argv = (char **)args;
	job_registry_split_id *spid;
	char *reqId = argv[1];
	exec_cmd_t cancel_command = EXEC_CMD_DEFAULT;

	/* Split <lrms> and actual job Id */
	if((spid = job_registry_split_blah_id(argv[2])) == NULL)
	{
		/* PUSH A FAILURE */
		resultLine = make_message("%s 2 Malformed\\ jobId\\ %s\\ or\\ out\\ of\\ memory", reqId, argv[2]);
		goto cleanup_argv;
	}

	/* Prepare the cancellation command */
	cancel_command.command = make_message("%s/%s_cancel.sh %s", blah_script_location, spid->lrms, spid->script_id);
	if (cancel_command.command == NULL)
	{
		/* PUSH A FAILURE */
		resultLine = make_message("%s 1 Cannot\\ allocate\\ memory\\ for\\ the\\ command\\ string", reqId);
		goto cleanup_lrms;
	}
	if (argv[CMD_CANCEL_JOB_ARGS + 1] != NULL)
	{
		cancel_command.delegation_type = atoi(argv[CMD_CANCEL_JOB_ARGS + 1 + MEXEC_PARAM_DELEGTYPE]);
		cancel_command.delegation_cred = argv[CMD_CANCEL_JOB_ARGS + 1 + MEXEC_PARAM_DELEGCRED];
	}

	/* Execute the command */
	retcod = execute_cmd(&cancel_command);

	if (retcod != 0)
	{
		escpd_cmd_err = escape_spaces(strerror(errno));
		resultLine = make_message("%s 3 Error\\ executing\\ the\\ cancel\\ command:\\ %s", reqId, escpd_cmd_err);
		if (escpd_cmd_err) free(escpd_cmd_err);
		goto cleanup_command;
	}

	if (cancel_command.exit_code != 0)
	{
		/* PUSH A FAILURE */
		escpd_cmd_out = escape_spaces(cancel_command.output);
		escpd_cmd_err = escape_spaces(cancel_command.error);
		resultLine = make_message("%s %d Cancellation\\ command\\ failed\\ (stdout:%s)\\ (stderr:%s)",
		                           reqId, cancel_command.exit_code, escpd_cmd_out, escpd_cmd_err);
		if (BLAH_DYN_ALLOCATED(escpd_cmd_out)) free(escpd_cmd_out);
		if (BLAH_DYN_ALLOCATED(escpd_cmd_err)) free(escpd_cmd_err);
		goto cleanup_command;
	}	

	/* Multiple job cancellation */
	res_length = strlen(cancel_command.output);
	for (begin_res = cancel_command.output; (end_res = memchr(cancel_command.output, '\n', res_length)); begin_res = end_res + 1)
	{
		*end_res = 0;
		resultLine = make_message("%s%s", reqId, begin_res);
		enqueue_result(resultLine);
		free(resultLine);
		resultLine = NULL;
	}

	/* Free up all arguments and exit (exit point in case of error is the label
	   pointing to last successfully allocated variable) */
cleanup_command:
	cleanup_cmd(&cancel_command);
	free(cancel_command.command);
cleanup_lrms:
	job_registry_free_split_id(spid);
cleanup_argv:
	free_args(argv);
	if(resultLine)
	{
		enqueue_result(resultLine);
		free (resultLine);
	}
	sem_post(&sem_total_commands);
	return NULL;
}

#define CMD_STATUS_JOB_ARGS 2
void*
cmd_status_job(void *args)
{
	classad_context status_ad[MAX_JOB_NUMBER];
	char *str_cad;
	char *esc_str_cad;
	char *resultLine;
	char **argv = (char **)args;
	char errstr[MAX_JOB_NUMBER][ERROR_MAX_LEN];
	char *esc_errstr;
	char *reqId = argv[1];
	char *jobDescr = argv[2];
	int jobStatus, retcode;
	int i, job_number;

	if (blah_children_count>0) check_on_children(blah_children, blah_children_count);

	retcode = get_status(jobDescr, status_ad, argv + CMD_STATUS_JOB_ARGS + 1, errstr, 0, &job_number);
	if (!retcode)
	{
		for(i = 0; i < job_number; i++)
		{
			if (status_ad[i] != NULL)
			{
				classad_get_int_attribute(status_ad[i], "JobStatus", &jobStatus);
				str_cad = classad_unparse(status_ad[i]);
				esc_str_cad = escape_spaces(str_cad);
				esc_errstr = escape_spaces(errstr[i]);
				if (job_number > 1)
					resultLine = make_message("%s.%d %d %s %d %s", reqId, i, retcode, esc_errstr, jobStatus, esc_str_cad);
				else
					resultLine = make_message("%s %d %s %d %s", reqId, retcode, esc_errstr, jobStatus, esc_str_cad);
				classad_free(status_ad[i]);
				free(str_cad);
				if (BLAH_DYN_ALLOCATED(esc_str_cad)) free(esc_str_cad);
			}
			else
			{
				esc_errstr = escape_spaces(errstr[i]);
				if(job_number > 1)
					resultLine = make_message("%s.%d 1 %s 0 N/A", reqId, i, esc_errstr);
				else
					resultLine = make_message("%s 1 %s 0 N/A", reqId, esc_errstr);
			}
			if (BLAH_DYN_ALLOCATED(esc_errstr)) free(esc_errstr);
			enqueue_result(resultLine);
			free(resultLine);
		}
	}
	else
	{
		esc_errstr = escape_spaces(errstr[0]);
		resultLine = make_message("%s %d %s 0 N/A", reqId, retcode, esc_errstr);
		enqueue_result(resultLine);
		free(resultLine);
		if (BLAH_DYN_ALLOCATED(esc_errstr)) free(esc_errstr);
	}

	/* Free up all arguments */
	free_args(argv);
	sem_post(&sem_total_commands);
	return NULL;
}

void*
cmd_status_job_all(void *args)
{
	char *resultLine=NULL;
	char *str_cad=NULL;
	char *en_cad, *new_str_cad;
	int  str_cad_app, str_cad_len=0;
	char *esc_str_cad, *esc_errstr;
	char **argv = (char **)args;
	char *reqId = argv[1];
	char *selectad = argv[2]; /* May be NULL */
	classad_expr_tree selecttr = NULL;
	FILE *fd;
	job_registry_entry *en;
	int select_ret, select_result;

	if (blah_children_count>0) check_on_children(blah_children, blah_children_count);

	/* File locking will not protect threads in the same */
	/* process. */
	pthread_mutex_lock(&blah_jr_lock);

	fd = job_registry_open(blah_jr_handle, "r");
	if (fd == NULL)
	{
	  	/* Report error opening registry. */
		esc_errstr = escape_spaces(strerror(errno));
		resultLine = make_message("%s 1 Cannot\\ open\\ BLAH\\ job\\ registry:\\ %s N/A", reqId, esc_errstr);
		if (BLAH_DYN_ALLOCATED(esc_errstr)) free(esc_errstr);
		goto wrap_up;
	}
	if (job_registry_rdlock(blah_jr_handle, fd) < 0)
	{
	  	/* Report error locking registry. */
		esc_errstr = escape_spaces(strerror(errno));
		resultLine = make_message("%s 1 Cannot\\ lock\\ BLAH\\ job\\ registry:\\ %s N/A", reqId, esc_errstr);
		if (BLAH_DYN_ALLOCATED(esc_errstr)) free(esc_errstr);
		goto wrap_up;
	}

	if (selectad != NULL)
	{
		selecttr = classad_parse_expr(selectad);
	}

	while ((en = job_registry_get_next(blah_jr_handle, fd)) != NULL)
	{
		en_cad = job_registry_entry_as_classad(blah_jr_handle, en);
		if (en_cad != NULL)
		{
			if (selecttr != NULL)
			{
				select_ret = classad_evaluate_boolean_expr(en_cad,selecttr,&select_result);
				if ((select_ret == C_CLASSAD_NO_ERROR && !select_result) ||
				     select_ret != C_CLASSAD_NO_ERROR)
				{
					free(en_cad);
					free(en);
					continue;
				}
			}
			str_cad_app = str_cad_len;
			str_cad_len += (strlen(en_cad)+1);
			new_str_cad = realloc(str_cad,str_cad_len);
			if (new_str_cad == NULL)
			{
				free(str_cad);
				free(en_cad);
				free(en);
				resultLine = make_message("%s 1 Out\\ of\\ memory\\ servicing\\ status_all\\ request N/A", reqId);
				goto wrap_up;
			}
			str_cad = new_str_cad;
			if (str_cad_app > 0)
			{
				strcat(str_cad,";"); 
			} else {
				str_cad[0] = '\000';
			}
			strcat(str_cad, en_cad);
			free(en_cad);
		}
		free(en);
	}
	if (str_cad != NULL)
	{
		esc_str_cad = escape_spaces(str_cad);
		if (esc_str_cad != NULL)
			resultLine = make_message("%s 0 No\\ error [%s]", reqId, esc_str_cad);
		else resultLine = make_message("%s 1 Out\\ of\\ memory\\ servicing\\ status_all\\ request N/A", reqId);
		free(str_cad);
		if (BLAH_DYN_ALLOCATED(esc_str_cad)) free(esc_str_cad);
	} else {
		resultLine = make_message("%s 0 No\\ error []", reqId);
	}

wrap_up:
	if (selecttr != NULL) classad_free_tree(selecttr);
	if (fd != NULL) fclose(fd);
	pthread_mutex_unlock(&blah_jr_lock);

	/* Free up all arguments */
	free_args(argv);
	if(resultLine)
	{
		enqueue_result(resultLine);
		free (resultLine);
	}
	sem_post(&sem_total_commands);
	return NULL;
}

int
get_status_and_old_proxy(int map_mode, char *jobDescr, const char *proxyFileName,
			char **status_argv, char **old_proxy,
			char **workernode, char **error_string)
{
	char *r_old_proxy=NULL;
	classad_context status_ad[MAX_JOB_NUMBER];
	char errstr[MAX_JOB_NUMBER][ERROR_MAX_LEN];
	int jobNumber=0, jobStatus;
	job_registry_entry *ren;
	int i;
	const char *proxy_ext = NULL;

	if (old_proxy == NULL) return(-1);
	*old_proxy = NULL;
	if (workernode == NULL) return(-1);
	*workernode = NULL;
	if (error_string != NULL) *error_string = NULL;

	/* Look up job registry first, if configured. */
	if (blah_jr_handle != NULL)
	{
		/* File locking will not protect threads in the same */
		/* process. */
		pthread_mutex_lock(&blah_jr_lock);
		if ((ren=job_registry_get(blah_jr_handle, jobDescr)) != NULL)
		{
			*old_proxy = job_registry_get_proxy(blah_jr_handle, ren);
			if (*old_proxy != NULL && ren->renew_proxy == 0)
			{
				free(ren);
				pthread_mutex_unlock(&blah_jr_lock);
				return 1; /* 'local' state */
			} 
			jobStatus = ren->status;
			*workernode = strdup(ren->wn_addr);
			free(ren);
			pthread_mutex_unlock(&blah_jr_lock);
			return jobStatus;	
		}
		pthread_mutex_unlock(&blah_jr_lock);
	}

	/* FIXMEPRREG: this entire, complex recipe to retrieve */
	/* FIXMEPRREG: the job proxy and status can be removed once */
	/* FIXMEPRREG: the job registry is used throughout. */

	switch (map_mode) {
	case MEXEC_GLEXEC:
		proxy_ext = ".glexec";
		break;
	case MEXEC_SUDO:
		proxy_ext = ".mapped";
		break;
	case MEXEC_NO_MAPPING:
	default:
		proxy_ext = ".lmt";
	}

	if ((r_old_proxy = make_message("%s%s", proxyFileName, proxy_ext)) == NULL)
	{
		fprintf(stderr, "Out of memory.\n");
		exit(MALLOC_ERROR);
	}
	if (access(r_old_proxy, R_OK) == -1)
	{
		r_old_proxy[strlen(proxyFileName)] = '\0';
		if (access(r_old_proxy, R_OK) == -1)
		{
			free(r_old_proxy);
			return -1;
		}
	}
	*old_proxy = r_old_proxy;

	/* If we have a proxy, and proxy renewal on worker nodes was */
	/* disabled, we only need to deal with the proxy locally. */
	/* We don't need to spend time checking on job status. */
	if (disable_wn_proxy_renewal) return 1; /* 'Local' renewal only. */

	/* If we reach here we have a proxy *and* we have */
	/* to check on the job status */
	get_status(jobDescr, status_ad, status_argv, errstr, 1, &jobNumber);

	if (jobNumber > 0 && (!strcmp(errstr[0], "No Error")))
	{
		classad_get_int_attribute(status_ad[0], "JobStatus", &jobStatus);
		classad_get_dstring_attribute(status_ad[0], "WorkerNode", workernode);
		for (i=0; i<jobNumber; i++) if (status_ad[i]) classad_free(status_ad[i]);
		return jobStatus;
	}
	if (error_string != NULL)
	{
		*error_string = escape_spaces(errstr[0]);	
	}
	if (jobNumber > 0) 
	{
		for (i=0; i<jobNumber; i++) if (status_ad[i]) classad_free(status_ad[i]);
	}
	return -1;
}

#define CMD_RENEW_PROXY_ARGS 3
void *
cmd_renew_proxy(void *args)
{
	char *resultLine;
	char **argv = (char **)args;
	char *reqId = argv[1];
	char *jobDescr = argv[2];
	char *proxyFileName = argv[3];
	char *workernode = NULL;
	char *old_proxy = NULL;
	int old_proxy_len;
	
	int i, jobStatus, retcod, count;
	char *error_string = NULL;
	int use_glexec = 0;
	int use_mapping = 0;
	exec_cmd_t exe_command = EXEC_CMD_DEFAULT;
	char *limited_proxy_name = NULL;

	if (argv[CMD_RENEW_PROXY_ARGS + 1] != NULL) {
		use_mapping = atoi(argv[CMD_RENEW_PROXY_ARGS + 1 + MEXEC_PARAM_DELEGTYPE ]);
		use_glexec = use_mapping == MEXEC_GLEXEC;
	}

	if (blah_children_count>0) check_on_children(blah_children, blah_children_count);

	jobStatus=get_status_and_old_proxy(use_mapping, jobDescr, proxyFileName, argv + CMD_RENEW_PROXY_ARGS + 1, &old_proxy, &workernode, &error_string);
	old_proxy_len = -1;
	if (old_proxy != NULL) old_proxy_len = strlen(old_proxy);
	if (!use_mapping && disable_limited_proxy && disable_wn_proxy_renewal)
	{
		/* Nothing needs to be done with the proxy */
		resultLine = make_message("%s 0 Proxy\\ renewed", reqId);
		if (old_proxy != NULL) free(old_proxy);
	}
	else if ((jobStatus < 0) || (old_proxy == NULL) || (old_proxy_len <= 0))
	{
		resultLine = make_message("%s 1 Cannot\\ locate\\ old\\ proxy:\\ %s", reqId, error_string);
		if (BLAH_DYN_ALLOCATED(error_string)) free(error_string);
		if (old_proxy != NULL) free(old_proxy);
		if (workernode != NULL) free(workernode);
	}
	else
	{
		if (BLAH_DYN_ALLOCATED(error_string)) free(error_string);
		switch(jobStatus)
		{
			case 1: /* job queued: copy the proxy locally */
                               if (!use_mapping)
				{
                                 if (!disable_limited_proxy)
                                   {
					limit_proxy(proxyFileName, old_proxy, NULL);
                                   }
                                 resultLine = make_message("%s 0 Proxy\\ renewed", reqId);
				}
				else
				{
					exe_command.delegation_type = atoi(argv[CMD_RENEW_PROXY_ARGS + 1 + MEXEC_PARAM_DELEGTYPE]);
					exe_command.delegation_cred = argv[CMD_RENEW_PROXY_ARGS + 1 + MEXEC_PARAM_DELEGCRED];
					if ((use_glexec) || (disable_limited_proxy))
					{
						exe_command.source_proxy = argv[CMD_RENEW_PROXY_ARGS + 1 + MEXEC_PARAM_SRCPROXY];
					} else {
                                                limited_proxy_name = limit_proxy(proxyFileName, NULL, NULL);
                                                exe_command.source_proxy = limited_proxy_name;

					}
					exe_command.dest_proxy = old_proxy;
					if (exe_command.source_proxy == NULL)
					{
						resultLine = make_message("%s 1 renew\\ failed\\ (error\\ limiting\\ proxy)", reqId);
					} else {
						retcod = execute_cmd(&exe_command);
						if (retcod == 0)
						{
							resultLine = make_message("%s 0 Proxy\\ renewed", reqId);
						} else {
							resultLine = make_message("%s 1 user\\ mapping\\ command\\ failed\\ (exitcode==%d)", reqId, retcod);
						}
					}
					if (limited_proxy_name != NULL) free(limited_proxy_name);
					cleanup_cmd(&exe_command);
				}
				break;

			case 2: /* job running: send the proxy to remote host */
				if (workernode != NULL && strcmp(workernode, ""))
				{
					/* Add the worker node argument to argv and invoke cmd_send_proxy_to_worker_node */
					for(count = CMD_RENEW_PROXY_ARGS + 1; argv[count]; count++);
					argv = (char **)realloc(argv, sizeof(char *) * (count + 2));
					if (argv != NULL)
					{
						/* Make room for the workernode argument at i==CMD_RENEW_PROXY_ARGS+1. */
						argv[count+1] = 0;
						for(i = count; i > (CMD_RENEW_PROXY_ARGS+1); i--) argv[i] = argv[i-1];
						/* workernode will be freed inside cmd_send_proxy_to_worker_node */
						/* also the semaphore will be released there */
						argv[CMD_RENEW_PROXY_ARGS+1] = workernode;
						cmd_send_proxy_to_worker_node((void *)argv);
						if (old_proxy != NULL) free(old_proxy);
						return NULL;
					}
					else
					{
						fprintf(stderr, "blahpd: out of memory! Exiting...\n");
						exit(MALLOC_ERROR);
					}
				}
				else
				{
					resultLine = make_message("%s 1 Cannot\\ retrieve\\ executing\\ host", reqId);
				}
				break;
			case 3: /* job deleted */
				/* no need to refresh the proxy */
				resultLine = make_message("%s 0 No\\ proxy\\ to\\ renew\\ -\\ Job\\ was\\ deleted", reqId);
				break;
			case 4: /* job completed */
				/* no need to refresh the proxy */
				resultLine = make_message("%s 0 No\\ proxy\\ to\\ renew\\ -\\ Job\\ completed", reqId);
				break;
			case 5: /* job hold */
				/* FIXME not yet supported */
				resultLine = make_message("%s 0 No\\ support\\ for\\ renewing\\ held\\ jobs\\ yet", reqId);
				break;
			default:
				resultLine = make_message("%s 1 Job\\ is\\ in\\ an\\ unknown\\ status\\ (%d)", reqId, jobStatus);
		}
		if (old_proxy != NULL) free(old_proxy);
		if (workernode != NULL) free(workernode);
	}
		
	if (resultLine)
	{
		enqueue_result(resultLine);
		free(resultLine);
	}
	else
	{
		fprintf(stderr, "blahpd: out of memory! Exiting...\n");
		exit(MALLOC_ERROR);
	}
	
	/* Free up all arguments */
	free_args(argv);
	sem_post(&sem_total_commands);
	return NULL;
}

#define CMD_SEND_PROXY_TO_WORKER_NODE_ARGS 4
void *
cmd_send_proxy_to_worker_node(void *args)
{
	char *resultLine;
	char **argv = (char **)args;
	char *reqId = argv[1];
	char *jobDescr = argv[2];
	char *proxyFileName = argv[3];
	char *workernode = argv[4];
#if defined(HAVE_GLOBUS)
	char *ld_path = NULL;
#endif
	int retcod;
	
	char *error_string = NULL;
	char *proxyFileNameNew = NULL;
	exec_cmd_t exe_command = EXEC_CMD_DEFAULT;

	char *delegate_switch;

	int use_mapping;
	int use_glexec;

	use_mapping = (argv[CMD_RENEW_PROXY_ARGS + 1] != NULL);
	use_glexec = ( use_mapping &&
                     (atoi(argv[CMD_RENEW_PROXY_ARGS + 1 + MEXEC_PARAM_DELEGTYPE ]) == MEXEC_GLEXEC) );

	if (workernode != NULL && strcmp(workernode, ""))
	{
               if (!use_glexec)
		{
                  if (disable_limited_proxy)
                    {
                        proxyFileNameNew = strdup(proxyFileName);
                    }
                  else
                    {
			proxyFileNameNew = limit_proxy(proxyFileName, NULL, NULL);
                    }
		}
		else
			proxyFileNameNew = strdup(argv[CMD_SEND_PROXY_TO_WORKER_NODE_ARGS + MEXEC_PARAM_SRCPROXY + 1]);

#if defined(HAVE_GLOBUS)
		/* Add the globus library path */
		ld_path = make_message("LD_LIBRARY_PATH=%s/lib",
		                           getenv("GLOBUS_LOCATION") ? getenv("GLOBUS_LOCATION") : "/opt/globus");
		push_env(&exe_command.environment, ld_path);
		free(ld_path);
#endif

		delegate_switch = "";
		if (config_test_boolean(config_get("blah_delegate_renewed_proxies",blah_config_handle)))
			delegate_switch = "delegate_proxy";

		exe_command.command = make_message("%s/BPRclient %s %s %s %s",
		                       blah_script_location, proxyFileNameNew, jobDescr, workernode, delegate_switch); 
		free(proxyFileNameNew);

		retcod = execute_cmd(&exe_command);
		if (exe_command.output)
		{
			error_string = escape_spaces(exe_command.output);
		}
		else
		{
			error_string = strdup("Cannot\\ execute\\ BPRclient");
		}
		free(exe_command.command);
		cleanup_cmd(&exe_command);
		
		resultLine = make_message("%s %d %s", reqId, retcod, error_string);
		if (BLAH_DYN_ALLOCATED(error_string)) free(error_string);
	}
	else
	{
		resultLine = make_message("%s 1 Worker\\ node\\ empty.", reqId);
	}
		
	if (resultLine)
	{
		enqueue_result(resultLine);
		free(resultLine);
	}
	else
	{
		fprintf(stderr, "blahpd: out of memory! Exiting...\n");
		exit(MALLOC_ERROR);
	}
	
	/* Free up all arguments */
	free_args(argv);
	sem_post(&sem_total_commands);
	return NULL;
}

void
hold_res_exec(char* jobdescr, char* reqId, char* action, int status, char **argv )
{
	int retcod;
	char *escpd_cmd_out, *escpd_cmd_err;
	char *resultLine = NULL;
	job_registry_split_id *spid;
	exec_cmd_t hold_command = EXEC_CMD_DEFAULT;

	/* Split <lrms> and actual job Id */
	if((spid = job_registry_split_blah_id(jobdescr)) == NULL)
	{
		/* PUSH A FAILURE */
		resultLine = make_message("%s 2 Malformed\\ jobId\\ %s\\ or\\ out\\ of\\ memory", reqId, jobdescr);
		goto cleanup_argv;
	}

	if (argv[MEXEC_PARAM_DELEGTYPE] != NULL)
	{
		hold_command.delegation_type = atoi(argv[MEXEC_PARAM_DELEGTYPE]);
		hold_command.delegation_cred = argv[MEXEC_PARAM_DELEGCRED];
	}
	
	if(!strcmp(action,"hold"))
	{
		hold_command.command = make_message("%s/%s_%s.sh %s %d", blah_script_location, spid->lrms, action, spid->script_id, status);
	}
	else
	{
		hold_command.command = make_message("%s/%s_%s.sh %s", blah_script_location, spid->lrms, action, spid->script_id);
	}

	if (hold_command.command == NULL)
	{
		/* PUSH A FAILURE */
		resultLine = make_message("%s 1 Cannot\\ allocate\\ memory\\ for\\ the\\ command\\ string", reqId);
		goto cleanup_lrms;
	}

	/* Execute the command */
	retcod = execute_cmd(&hold_command);

	if (retcod != 0)
	{
		resultLine = make_message("%s 1 Cannot\\ execute\\ %s\\ script", reqId, hold_command.command);
		goto cleanup_command;
	}

	if (hold_command.exit_code != 0)
	{
		escpd_cmd_out = escape_spaces(hold_command.output);
		escpd_cmd_err = escape_spaces(hold_command.error);
		resultLine = make_message("%s %d Job\\ %s:\\ %s\\ %s\\ command\\ failed\\ (stdout:%s)\\ (stderr:%s)",
		                          reqId, hold_command.exit_code, statusstring[status - 1], spid->lrms, action, escpd_cmd_out, escpd_cmd_err);
		if (BLAH_DYN_ALLOCATED(escpd_cmd_out)) free(escpd_cmd_out);
		if (BLAH_DYN_ALLOCATED(escpd_cmd_err)) free(escpd_cmd_err);
	}
	else
		resultLine = make_message("%s %d No\\ error", reqId, retcod);

	cleanup_cmd(&hold_command);

	/* Free up all arguments and exit (exit point in case of error is the label
	   pointing to last successfully allocated variable) */
cleanup_command:
	free(hold_command.command);
cleanup_lrms:
	job_registry_free_split_id(spid);
cleanup_argv:
	if(resultLine)
	{
		enqueue_result(resultLine);
		free(resultLine);
	}
	else
	{
		fprintf(stderr, "blahpd: out of memory! Exiting...\n");
		exit(MALLOC_ERROR);
	}
	
	/* argv is cleared in the calling function */
	return;
}

#define HOLD_RESUME_ARGS 2
void
hold_resume(void* args, int action )
{
	classad_context status_ad[MAX_JOB_NUMBER];
	char **argv = (char **)args;
	char errstr[MAX_JOB_NUMBER][ERROR_MAX_LEN];
	char *resultLine = NULL;
	int jobStatus, retcode;
	char *reqId = argv[1];
	char jobdescr[MAX_JOB_NUMBER][JOBID_MAX_LEN + 1];
	int i,job_number;
	char *dummyargv = argv[2];
	char *tmpjobdescr=NULL;
	char *pointer;

	/* job status check */
	retcode = get_status(dummyargv, status_ad, argv + HOLD_RESUME_ARGS + 1, errstr, 0, &job_number);
	/* if multiple jobs are present their id must be extracted from argv[2] */
	i=0;
#if 1
	tmpjobdescr = strtok_r(dummyargv," ", &pointer);
	strncpy(jobdescr[0], tmpjobdescr, JOBID_MAX_LEN);
	if(job_number>1)
	{
		for (i=1; i<job_number; i++)
		{
		        tmpjobdescr = strtok_r(NULL," ", &pointer);
		        strncpy(jobdescr[i], tmpjobdescr, JOBID_MAX_LEN);
		}
	}
#else
	strncpy(jobdescr[0], dummyargv, JOBID_MAX_LEN);
#endif
	if (!retcode)
	{
		for (i=0; i<job_number; i++)
		{
		        if(job_number>1)
		        {
		                reqId = make_message("%s.%d",argv[1],i);
		        }else
				reqId = strdup(argv[1]);
		        if(classad_get_int_attribute(status_ad[i], "JobStatus", &jobStatus) == C_CLASSAD_NO_ERROR)
		        {
		                switch(jobStatus)
		                {
		                        case 1:/* IDLE */
		                                if(action == HOLD_JOB)
		                                {
		                                        hold_res_exec(jobdescr[i], reqId, "hold", 1, argv + HOLD_RESUME_ARGS + 1);
		                                }
		                                else
		                                if ((resultLine = make_message("%s 1 Job\\ Idle\\ jobId\\ %s", reqId,jobdescr[i])))
		                                {
		                                        enqueue_result(resultLine);
		                                        free(resultLine);
		                                }
		                        break;
		                        case 2:/* RUNNING */
		                                if(action == HOLD_JOB)
		                                {
		                                        hold_res_exec(jobdescr[i], reqId, "hold", 2, argv + HOLD_RESUME_ARGS + 1);
		                                }else
		                                if ((resultLine = make_message("%s 1 \\ Job\\ Running\\ jobId\\ %s", reqId, jobdescr[i])))
		                                {
		                                        enqueue_result(resultLine);
		                                        free(resultLine);
		                                }
		                        break;
		                        case 3:/* REMOVED */
		                                if ((resultLine = make_message("%s 1 Job\\ Removed\\ jobId\\ %s", reqId, jobdescr[i])))
		                                {
		                                        enqueue_result(resultLine);
		                                        free(resultLine);
		                                }
		                        break;
		                        case 4:/* COMPLETED */
		                                if ((resultLine = make_message("%s 1 Job\\ Completed\\ jobId\\ %s", reqId, jobdescr[i])))
		                                {
		                                        enqueue_result(resultLine);
		                                        free(resultLine);
		                                }
		                        break;
		                        case 5:/* HELD */
		                                if(action == RESUME_JOB)
		                                        hold_res_exec(jobdescr[i], reqId, "resume", 5, argv + HOLD_RESUME_ARGS + 1);
		                                else
		                                if ((resultLine = make_message("%s 0 Job\\ Held\\ jobId\\ %s", reqId, jobdescr[i])))
		                                {
		                                        enqueue_result(resultLine);
		                                        free(resultLine);
		                                }
		                        break;
		                }
		        }else
		        if ((resultLine = make_message("%s 1 %s", reqId, errstr[i])))
		        {
		                enqueue_result(resultLine);
		                free(resultLine);
		        }
			if(reqId) {free(reqId);reqId=NULL;}
		}
	}else
	{
		resultLine = make_message("%s %d %s", reqId, retcode, errstr[0]);
		enqueue_result(resultLine);
		free(resultLine);
	}
	free(dummyargv);
	if (reqId) free(reqId);
	return;
}

void *
cmd_hold_job(void* args)
{
	hold_resume(args,HOLD_JOB);
	sem_post(&sem_total_commands);
	return NULL;
}

void *
cmd_resume_job(void* args)
{
	hold_resume(args,RESUME_JOB);
	sem_post(&sem_total_commands);
	return NULL;
}

void *
cmd_get_hostport(void *args)
{
	char **argv = (char **)args;
	char *reqId = argv[1];
	char *resultLine;
	char *resultLine_tmp;
	char *cmd_out;
	int  retcode;
	exec_cmd_t exe_command = EXEC_CMD_DEFAULT;
	int i;

	if (blah_children_count>0) check_on_children(blah_children, blah_children_count);

	if (lrms_counter)
	{
		resultLine = make_message("%s 0 ", reqId);
		for(i = 0; i < lrms_counter; i++)
		{        
			exe_command.command = make_message("%s/%s_status.sh -n", blah_script_location, lrmslist[i]);
			retcode = execute_cmd(&exe_command);
			cmd_out = exe_command.output;
			if (retcode || !cmd_out)
			{
				free(resultLine);
				resultLine = make_message("%s 1 Unable\\ to\\ retrieve\\ the\\ port\\ (exit\\ code\\ =\\ %d) ", reqId, retcode);
				break;
			}
			if (cmd_out[strlen(cmd_out) - 1] == '\n') cmd_out[strlen(cmd_out) - 1] = '\0';
			resultLine_tmp = make_message("%s%s/%s ", resultLine, lrmslist[i], strlen(cmd_out) ? cmd_out : "Error\\ reading\\ host:port");
			free(resultLine);
			resultLine = resultLine_tmp;
			recycle_cmd(&exe_command);
		}
		/* remove the trailing space */
		resultLine[strlen(resultLine) - 1] = '\0';
		cleanup_cmd(&exe_command);
	}
	else
	{
		resultLine = make_message("%s 1 No\\ LRMS\\ found", reqId);
	}

	enqueue_result(resultLine);
	free(resultLine);
	free_args(argv);
	sem_post(&sem_total_commands);
	return NULL;
}

/* Utility functions
 * */

void
enqueue_result(char *res)
{
	if (push_result(res))
	{
		int write_result;
		pthread_mutex_lock(&send_lock);
		write_result = write(server_socket, "R\r\n", 3);
		if (write_result < 0) {
			exit_program = 1;
		}
		pthread_mutex_unlock(&send_lock);
	}
	return;
}

int
set_cmd_string_option(char **command, classad_context cad, const char *attribute, const char *option, const int quote_style)
{
	char *argument;
	char *to_append = NULL;
	char *new_command;
	int result;
	
	if ((result = classad_get_dstring_attribute(cad, attribute, &argument)) == C_CLASSAD_NO_ERROR)
	{
		if (strlen(argument) > 0)
		{
			if ((to_append = make_message(opt_format[quote_style], option, argument)) == NULL)
				result = C_CLASSAD_OUT_OF_MEMORY;
			free (argument);
		}
		else
			result = C_CLASSAD_VALUE_NOT_FOUND;
	}

	if (result == C_CLASSAD_NO_ERROR)
	{
		if ((new_command = (char *) realloc (*command, strlen(*command) + strlen(to_append) + 1)))
		{
			strcat(new_command, to_append);
			*command = new_command;
		}
		else
		{
			result = C_CLASSAD_OUT_OF_MEMORY;
		}
	}

	if (to_append) free (to_append);
	return(result);
}

int
set_cmd_int_option(char **command, classad_context cad, const char *attribute, const char *option, const int quote_style)
{
	int argument;
	char *to_append = NULL;
	char *new_command;
	int result;
	
	if ((result = classad_get_int_attribute(cad, attribute, &argument)) == C_CLASSAD_NO_ERROR)
	{
		if ((to_append = make_message(opt_format[quote_style], option, argument)) == NULL)
		{
			result = C_CLASSAD_OUT_OF_MEMORY;
		}
	}

	if (result == C_CLASSAD_NO_ERROR)
	{
		if ((new_command = (char *) realloc (*command, strlen(*command) + strlen(to_append) + 1)))
		{
			strcat(new_command, to_append);
			*command = new_command;
		}
		else
		{
			result = C_CLASSAD_OUT_OF_MEMORY;
		}
	}

	if (to_append) free (to_append);
	return(result);
}

int
set_cmd_bool_option(char **command, classad_context cad, const char *attribute, const char *option, const int quote_style)
{
	const char *str_yes = "yes";
	const char *str_no  = "no";
	int attr_value;
	const char *argument;
	char *to_append = NULL;
	char *new_command;
	int result;
	
	if ((result = classad_get_bool_attribute(cad, attribute, &attr_value)) == C_CLASSAD_NO_ERROR)
	{
		argument = (attr_value ? str_yes : str_no);
		if ((to_append = make_message(opt_format[quote_style], option, argument)) == NULL)
			result = C_CLASSAD_OUT_OF_MEMORY;
	}

	if (result == C_CLASSAD_NO_ERROR)
	{
		if ((new_command = (char *) realloc (*command, strlen(*command) + strlen(to_append) + 1)))
		{
			strcat(new_command, to_append);
			*command = new_command;
		}
		else
		{
			result = C_CLASSAD_OUT_OF_MEMORY;
		}
	}

	if (to_append) free (to_append);
	return(result);
}


int
set_cmd_list_option(char **command, classad_context cad, const char *attribute, const char *option)
{
	char **list_cont;
	char **str_ptr;
	char *to_append = NULL;
	char *reallocated;
	int result;
	
	if ((result = classad_get_string_list_attribute(cad, attribute, &list_cont)) == C_CLASSAD_NO_ERROR)
	{
		if ((to_append = strdup(option)))
		{
			for (str_ptr = list_cont; (*str_ptr) != NULL; str_ptr++)
			{
				if ((reallocated = (char *) realloc (to_append, strlen(*str_ptr) + strlen(to_append) + 2)))
				{
					to_append = reallocated;
					strcat(to_append, " ");
					strcat(to_append, *str_ptr);
				}
				else
				{
					result = C_CLASSAD_OUT_OF_MEMORY;
					break;
				}					
			}
		}
		else /* strdup failed */
			result = C_CLASSAD_OUT_OF_MEMORY;

		classad_free_string_list(list_cont);
	}

	if (result == C_CLASSAD_NO_ERROR)
		if ((reallocated = (char *) realloc (*command, strlen(*command) + strlen(to_append) + 1)))
		{
			strcat(reallocated, to_append);
			*command = reallocated;
		}

	if (to_append) free (to_append);
	return(result);
}
 
const char *grid_proxy_errmsg = NULL;

int activate_globus()
{
#if !defined(HAVE_GLOBUS)
	grid_proxy_errmsg = "Globus support disabled";
	return -1;
#else
	static int active = 0;

	if (active) {
		return 0;
	}

	if ( globus_thread_set_model( "pthread" ) ) {
		grid_proxy_errmsg = "failed to activate Globus";
		return -1;
	}

	if ( globus_module_activate(GLOBUS_GSI_CREDENTIAL_MODULE) ) {
		grid_proxy_errmsg = "failed to activate Globus";
		return -1;
	}

	if ( globus_module_activate(GLOBUS_GSI_PROXY_MODULE) ) {
		grid_proxy_errmsg = "failed to activate Globus";
		return -1;
	}

	active = 1;
	return 0;
#endif
}

/* Returns lifetime left on proxy, in seconds.
 * 0 means proxy is expired.
 * -1 means an error occurred.
 */
int grid_proxy_info(const char *proxy_filename)
{
#if !defined(HAVE_GLOBUS)
	grid_proxy_errmsg = "Globus support disabled";
	return -1;
#else
	globus_gsi_cred_handle_t handle = NULL;
	time_t time_left = -1;

	if ( activate_globus() < 0 ) {
		return -1;
	}

	if (globus_gsi_cred_handle_init(&handle, NULL)) {
		grid_proxy_errmsg = "failed to initialize Globus data structures";
		goto cleanup;
	}

	// We should have a proxy file, now, try to read it
	if (globus_gsi_cred_read_proxy(handle, proxy_filename)) {
		grid_proxy_errmsg = "unable to read proxy file";
		goto cleanup;
	}

	if (globus_gsi_cred_get_lifetime(handle, &time_left)) {
		grid_proxy_errmsg = "unable to extract expiration time";
		goto cleanup;
	}

	if ( time_left < 0 ) {
		time_left = 0;
	}

 cleanup:
	if (handle) {
		globus_gsi_cred_handle_destroy(handle);
	}

	return time_left;
#endif
}

/* Writes new proxy derived from existing one. Argument lifetime is the
 * number of seconds until expiration for the new proxy. A 0 lifetime
 * means the same expiration time as the source proxy.
 * Returns 0 on success and -1 on error.
 */
int grid_proxy_init(const char *src_filename, char *dst_filename,
					int lifetime)
{
#if !defined(HAVE_GLOBUS)
	grid_proxy_errmsg = "Globus support disabled";
	return -1;
#else
	globus_gsi_cred_handle_t src_handle = NULL;
	globus_gsi_cred_handle_t dst_handle = NULL;
	globus_gsi_proxy_handle_t dst_proxy_handle = NULL;
	int rc = -1;
	time_t src_time_left = -1;
	globus_gsi_cert_utils_cert_type_t cert_type = GLOBUS_GSI_CERT_UTILS_TYPE_LIMITED_PROXY;

	if ( activate_globus() < 0 ) {
		return -1;
	}

	if (globus_gsi_cred_handle_init(&src_handle, NULL)) {
		grid_proxy_errmsg = "failed to initialize Globus data structures";
		goto cleanup;
	}

	// We should have a proxy file, now, try to read it
	if (globus_gsi_cred_read_proxy(src_handle, src_filename)) {
		grid_proxy_errmsg = "unable to read proxy file";
		goto cleanup;
	}

	if (globus_gsi_cred_get_lifetime(src_handle, &src_time_left)) {
		grid_proxy_errmsg = "unable to extract expiration time";
		goto cleanup;
	}
	if ( src_time_left < 0 ) {
		src_time_left = 0;
	}

	if (globus_gsi_proxy_handle_init( &dst_proxy_handle, NULL )) {
		grid_proxy_errmsg = "failed to initialize Globus data structures";
		goto cleanup;
	}

		// lifetime == desired dst lifetime
		// src_time_left == time left on src
	if ( lifetime == 0 || lifetime > src_time_left ) {
		lifetime = src_time_left;
	}
	if (globus_gsi_proxy_handle_set_time_valid( dst_proxy_handle, lifetime/60 )) {
		grid_proxy_errmsg = "unable to set proxy expiration time";
		goto cleanup;
	}

	if (globus_gsi_proxy_handle_set_type( dst_proxy_handle, cert_type)) {
		grid_proxy_errmsg = "unable to set proxy type";
		goto cleanup;
	}

	if (globus_gsi_proxy_create_signed( dst_proxy_handle, src_handle, &dst_handle)) {
		grid_proxy_errmsg = "unable to generate proxy";
		goto cleanup;
	}

	if (globus_gsi_cred_write_proxy( dst_handle, dst_filename )) {
		grid_proxy_errmsg = "unable to write proxy file";
		goto cleanup;
	}

	rc = 0;

 cleanup:
	if (src_handle) {
		globus_gsi_cred_handle_destroy(src_handle);
	}
	if (dst_handle) {
		globus_gsi_cred_handle_destroy(dst_handle);
	}
	if ( dst_handle ) {
		globus_gsi_proxy_handle_destroy( dst_proxy_handle );
	}

	return rc;
#endif
}

static char *
limit_proxy(char* proxy_name, char *limited_proxy_name, char **error_message)
{
	int seconds_left;
	int res;
	char *limit_command_output;
	int tmpfd;
	int get_lock_on_limited_proxy;
	struct flock plock;
	FILE *fpr;
	char *limited_proxy_made_up_name=NULL;

#if !defined(HAVE_GLOBUS)
	if (error_message) {
		*error_message = strdup("Globus support disabled");
	}
	return NULL;
#endif

	if (limited_proxy_name == NULL)
	{
		/* If no name is supplied, figure one out */
		limited_proxy_made_up_name = make_message("%s.lmt", proxy_name);
		if (limited_proxy_made_up_name == NULL) return (NULL);
		limited_proxy_name = limited_proxy_made_up_name;
	}

	/* Sanity check - make sure the destination is writable and the source exists */
	tmpfd = open(limited_proxy_name, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
	if (tmpfd == -1)
	{
		char * errmsg = make_message("Unable to create limited proxy file (%s):"
		    " errno=%d, %s", limited_proxy_name, errno, strerror(errno));
		if (limited_proxy_made_up_name != NULL) free(limited_proxy_made_up_name);
		if (!errmsg) return(NULL);
		if (error_message) *error_message = errmsg; else free(errmsg);
		return NULL;
	}
	else
	{
		close(tmpfd);
	}
	if ((tmpfd = open(proxy_name, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR)) == -1)
	{
		char * errmsg = make_message("Unable to read proxy file (%s):" 
		    " errno=%d, %s", proxy_name, errno, strerror(errno));
		if (limited_proxy_made_up_name != NULL) free(limited_proxy_made_up_name);
		if (!errmsg) return(NULL);
		if (error_message) *error_message = errmsg; else if (errmsg) free(errmsg);
		return NULL;
	}
	else
	{
		close(tmpfd);
	}

	seconds_left = grid_proxy_info( proxy_name );
	if ( seconds_left < 0 ) {
		perror("blahpd error reading proxy lifetime");
		return NULL;
	}

	limit_command_output = make_message("%s_XXXXXX", limited_proxy_name);
	if (limit_command_output != NULL)
	{
		tmpfd = mkstemp(limit_command_output);
		if (tmpfd < 0)
		{
			/* Fall back to direct file creation - it may work */
			free(limit_command_output);
			limit_command_output = limited_proxy_name;
		}
		else
		{
			close(tmpfd);
			/* Make sure file gets created by grid-proxy-init */
			unlink(limit_command_output);
		}
	}
        
	get_lock_on_limited_proxy = config_test_boolean(config_get("blah_get_lock_on_limited_proxies",blah_config_handle));

	if (seconds_left <= 0) {
		/* Something's wrong with the current proxy - use defaults */
		seconds_left = 12*60*60;
	}

 	if ((limit_command_output == limited_proxy_name) &&
	    get_lock_on_limited_proxy)
	{
		fpr = fopen(limited_proxy_name, "a");
		if (fpr == NULL)
		{
			fprintf(stderr, "blahpd limit_proxy: Cannot open %s in append mode to obtain file lock: %s\n", limited_proxy_name, strerror(errno));
			char * errmsg = make_message("Cannot open %s in append mode to obtain file lock: %s", limited_proxy_name, strerror(errno));
			if (limited_proxy_made_up_name != NULL) free(limited_proxy_made_up_name);
			if (error_message && errmsg) *error_message= errmsg; else if (errmsg) free(errmsg);
			return(NULL);
		}
		/* Acquire lock on limited proxy */
  		plock.l_type = F_WRLCK;
		plock.l_whence = SEEK_SET;
		plock.l_start = 0;
		plock.l_len = 0; /* Lock whole file */
  		if (fcntl(fileno(fpr), F_SETLKW, &plock) < 0)
		{
			fclose(fpr);
			fprintf(stderr, "blahpd limit_proxy: Cannot obtain write file lock on %s: %s\n", limited_proxy_name, strerror(errno));
			char * errmsg = make_message("Cannot obtain write file lock on %s: %s", limited_proxy_name, strerror(errno));
			if (limited_proxy_made_up_name != NULL) free(limited_proxy_made_up_name);
			if (error_message && errmsg) *error_message= errmsg; else if (errmsg) free(errmsg);
			return(NULL);
		}
	} 

	res = grid_proxy_init( proxy_name, limit_command_output, seconds_left );

 	if ((limit_command_output == limited_proxy_name) &&
	    get_lock_on_limited_proxy)
	{
		/* Release lock by closing file*/
		fclose(fpr);
	}

	if (res != 0) 
	{
		if (limit_command_output != limited_proxy_name)
			free(limit_command_output);
		if (limited_proxy_made_up_name != NULL) free(limited_proxy_made_up_name);
		return(NULL);
	}

	if (limit_command_output != limited_proxy_name)
	{
 		if (get_lock_on_limited_proxy)
		{
			fpr = fopen(limited_proxy_name, "a");
			if (fpr == NULL)
			{
				fprintf(stderr, "blahpd limit_proxy: Cannot open %s in append mode to obtain file lock: %s\n", limited_proxy_name, strerror(errno));
				unlink(limit_command_output);
				char * errmsg = make_message("Cannot open %s in append mode to obtain file lock: %s", limited_proxy_name, strerror(errno));
				free(limit_command_output);
				if (limited_proxy_made_up_name != NULL) free(limited_proxy_made_up_name);
				if (error_message && errmsg) *error_message= errmsg; else if (errmsg) free(errmsg);
				return(NULL);
			}	
			/* Acquire lock on limited proxy */
  			plock.l_type = F_WRLCK;
			plock.l_whence = SEEK_SET;
			plock.l_start = 0;
			plock.l_len = 0; /* Lock whole file */
  			if (fcntl(fileno(fpr), F_SETLKW, &plock) < 0)
			{
				fclose(fpr);
				fprintf(stderr, "blahpd limit_proxy: Cannot obtain write file lock on %s: %s\n", limited_proxy_name, strerror(errno));
				char * errmsg = make_message("Cannot obtain write file lock on %s: %s", limited_proxy_name, strerror(errno));
				unlink(limit_command_output);
				free(limit_command_output);
				if (limited_proxy_made_up_name != NULL) free(limited_proxy_made_up_name);
				if (error_message && errmsg) *error_message= errmsg; else free(errmsg);
				return(NULL);
			}
		}

		/* Rotate limited proxy in place via atomic rename */
		res = rename(limit_command_output, limited_proxy_name);
		if (res < 0) unlink(limit_command_output);

 		if (get_lock_on_limited_proxy) fclose(fpr); /* Release lock */

		free(limit_command_output);
	}
	return(limited_proxy_name);
}


int
logAccInfo(char* jobId, char* server_lrms, classad_context cad, char* fqan, char* userDN, char** environment)
{
	int result=0;
	char *gridjobid=NULL;
	char *clientjobid=NULL, *clientjobidstr=NULL;
	char *ce_id=NULL;
	char *ce_idtmp=NULL;
	char date_str[MAX_TEMP_ARRAY_SIZE];
	time_t tt;
	struct tm *t_m=NULL;
	char host_name[MAX_TEMP_ARRAY_SIZE];
	char *lrms_jobid=NULL;
	char *bs;
	char *queue=NULL;
	char *uid;
	FILE *logf;
	char *logf_name;
	mode_t saved_umask;
	mode_t log_umask = 0007;
	struct flock plock;
	job_registry_split_id *spid;
	exec_cmd_t exe_command = EXEC_CMD_DEFAULT;
	int retcode = 1; /* Nonzero is failure */

	if (blah_accounting_log_location == NULL) return retcode; /* No location to write to. */

	/* Submission time */
	time(&tt);
	t_m = gmtime(&tt);

	logf_name = make_message("%s-%04d%02d%02d", blah_accounting_log_location->value, 1900+t_m->tm_year, t_m->tm_mon+1, t_m->tm_mday);
	if (logf_name == NULL) return retcode;

	snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d %02d:%02d:%02d", 1900+t_m->tm_year,
	                  t_m->tm_mon+1, t_m->tm_mday, t_m->tm_hour, t_m->tm_min, t_m->tm_sec);

	/* These data must be logged in the log file:
	 "timestamp=<submission time to LRMS>" "userDN=<user's DN>" "userFQAN=<user's FQAN>"
	 "ceID=<CE ID>" "jobID=<grid job ID>" "lrmsID=<LRMS job ID>"
	*/

	/* grid jobID  : if we are here we suppose that the edg_jobid is present, if not we log an empty string*/
	classad_get_dstring_attribute(cad, "edg_jobid", &gridjobid);
	if ((*environment) && (gridjobid == NULL))
	{
		classad_get_dstring_attribute(cad, "uniquejobid", &gridjobid);
	}
	if (gridjobid == NULL) gridjobid = strdup("");

	/* 'Client' (e.g. 'Cream') jobID */
	classad_get_dstring_attribute(cad, "ClientJobId", &clientjobid);
	if (clientjobid != NULL)
	{
		clientjobidstr = make_message(" \"clientID=%s\"", clientjobid);
		free(clientjobid);
	}
	if (clientjobidstr == NULL) clientjobidstr = strdup("");

	/* lrms job ID */
	if((spid = job_registry_split_blah_id(jobId)) == NULL)
	{
		fprintf(stderr, "blahpd: logAccInfo called with bad jobId <%s>. Nothing will be logged\n", jobId);
		goto free_1;
	}
	lrms_jobid = spid->proxy_id;

	/* Ce ID */
	bs = spid->lrms;
	classad_get_dstring_attribute(cad, "CeID", &ce_id);
	if(!ce_id) 
	{
		gethostname(host_name, sizeof(host_name));
		classad_get_dstring_attribute(cad, "Queue", &queue);
		if(queue)
		{
			ce_id=make_message("%s:2119/blah-%s-%s",host_name,bs,queue);
			free(queue);
		}
		else
			ce_id=make_message("%s:2119/blah-%s-",host_name,bs);
	}
	else
	{
		classad_get_dstring_attribute(cad, "Queue", &queue);
		if(queue&&(strncmp(&ce_id[strlen(ce_id) - strlen(queue)],queue,strlen(queue))))
		{ 
			ce_idtmp=make_message("%s-%s",ce_id,queue);
			free(ce_id);
			ce_id=ce_idtmp;
		}
		if (queue) free(queue);
	}
	if(*environment)
	{
	 	/* need to fork and glexec an id command to obtain real user */
		exe_command.delegation_type = atoi(environment[MEXEC_PARAM_DELEGTYPE]);
		exe_command.delegation_cred = environment[MEXEC_PARAM_DELEGCRED];
		exe_command.command = make_message("/usr/bin/id -u");
		result = execute_cmd(&exe_command);
		free(exe_command.command);
		if (result != 0 || exe_command.exit_code != 0) {
			goto free_2;
		}
		uid = strdup(exe_command.output);
		cleanup_cmd(&exe_command);
		uid[strlen(uid)-1] = 0;
	}
	else
		uid = make_message("%d", getuid());

	if (blah_accounting_log_umask != NULL) {
		log_umask = strtol(blah_accounting_log_umask->value, (char **) NULL, 8);
	}

	saved_umask = umask(log_umask);
	logf = fopen(logf_name, "a");
	umask(saved_umask);

	if (logf == NULL) goto free_3;

	/* Try acquiring a write lock on the file */
  	plock.l_type = F_WRLCK;
	plock.l_whence = SEEK_SET;
	plock.l_start = 0;
	plock.l_len = 0; /* Lock whole file */
  	if (fcntl(fileno(logf), F_SETLKW, &plock) >= 0) {
		if (fprintf(logf, "\"timestamp=%s\" \"userDN=%s\" %s \"ceID=%s\" \"jobID=%s\" \"lrmsID=%s\" \"localUser=%s\"%s\n", date_str, userDN, fqan, ce_id, gridjobid, lrms_jobid, uid, clientjobidstr) >= 0) retcode = 0;
	}

	fclose(logf);

free_3:
	free(uid);

free_2:
	if (ce_id) free(ce_id);
	job_registry_free_split_id(spid);

free_1:
	if(clientjobidstr) free(clientjobidstr);
	if(gridjobid) free(gridjobid);
	free(logf_name);

	return retcode;
}

int
getProxyInfo(char* proxname, char** subject, char** fqan)
{
	int  slen=0;
	char *begin_res;
	char *end_res;
	char *fqanlong;
	char *new_fqanlong;
	exec_cmd_t exe_command = EXEC_CMD_DEFAULT;

	if ((fqan == NULL) || (subject == NULL)) return 1;
	*fqan = NULL;
	*subject = NULL;

	exe_command.command = make_message("%s/voms-proxy-info -file %s ", blah_script_location, proxname);
	exe_command.append_to_command = "-subject";
	if (execute_cmd(&exe_command) != 0)
		return 1;
	fqanlong = exe_command.output;
	slen = strlen(fqanlong);
	if (fqanlong[slen-1] == '\n') 
	{
		fqanlong[slen-1] = '\000';
		slen--;
	}

	/* example:
	   subject= /C=IT/O=INFN/OU=Personal Certificate/L=Milano/CN=Giuseppe Fiorentino/Email=giuseppe.fiorentino@mi.infn.it/CN=proxy
	   CN=proxy, CN=limited proxy must be removed from the 
           tail of the string
	*/
	while(1)
	{
		if (!strncmp(&fqanlong[slen - 9],"/CN=proxy",9))
		{
		      memset(&fqanlong[slen - 9],0,9);
		      slen -=9;
		}else
		if (!strncmp(&fqanlong[slen - 17],"/CN=limited proxy",17))
		{
		      memset(&fqanlong[slen - 17],0,17);
		      slen -=17;
		}else
		          break;
	}
	*subject = strdup(fqanlong);
	recycle_cmd(&exe_command);
	exe_command.append_to_command = "-fqan";
	if (execute_cmd(&exe_command) != 0)
		return 1;
	fqanlong = NULL;
	for (begin_res = exe_command.output; (end_res = strchr(begin_res, '\n')); begin_res = end_res + 1)
	{
		*end_res = 0;
		if (fqanlong == NULL)
		{
			fqanlong = make_message("\"userFQAN=%s\"", begin_res);
		} else {
			new_fqanlong = make_message("%s \"userFQAN=%s\"", fqanlong, begin_res);
			if (new_fqanlong != NULL) 
			{
				free(fqanlong);
				fqanlong = new_fqanlong;
			}
		}
	}
	if (fqanlong != NULL)
	{
		*fqan = fqanlong;
	} else {
		if (strlen(begin_res) > 0)
		{
	        	*fqan = make_message("\"userFQAN=%s\"", begin_res);
		} else {
	        	*fqan = strdup("\"userFQAN=\"");
		}
	}
	cleanup_cmd(&exe_command);
	BLAHDBG("DEBUG: getProxyInfo returns: subject:%s  ", *subject);
	BLAHDBG("fqan:  %s\n", *fqan);

	return 0;
}

int CEReq_parse(classad_context cad, char* filename, 
                char *proxysubject, char *proxyfqan)
{
	FILE *req_file=NULL;
	char **reqstr=NULL;
	char **creq;
	char *attr=NULL;
	int n_written=-1;
	config_entry *aen;
	int i;
	char **reqattr=NULL;
	int clret;

	if (pass_all_submit_attributes)
	{
		classad_get_attribute_names(cad, &reqattr);
	} else {
		reqattr = submit_attributes_to_pass;
	}

	for (creq = reqattr; ((creq != NULL) && ((*creq) != NULL)); creq++)
	{
		clret = classad_get_dstring_attribute(cad, *creq, &attr);
		if((clret == C_CLASSAD_NO_ERROR) && (attr != NULL))
		{
 			if (req_file== NULL)
			{
				req_file=fopen(filename,"w");
				if(req_file==NULL) return -1;
			}
			if (strcasecmp(*creq, "x509UserProxySubject") == 0)
			{
				fprintf(req_file, "%s='%s'\n", *creq, proxysubject);
			} else if (strcasecmp(*creq, "x509UserProxyFQAN") == 0) {
				fprintf(req_file, "%s='%s'\n", *creq, proxyfqan);
			} else {
				fprintf(req_file, "%s='%s'\n", *creq, attr);
			}
			free(attr);
			n_written++;
		}
	}

	if (pass_all_submit_attributes)
	{
		classad_free_results(reqattr);
	}

	/* Look up any configured attribute */
	aen = config_get("local_submit_attributes_setup_args",blah_config_handle);
	if (aen != NULL)
	{
		if (aen->n_values > 0)
		{
			for(i=0; i<aen->n_values; i++)
				unwind_attributes(cad,aen->values[i],&reqstr);
		} else {
			unwind_attributes(cad,aen->value,&reqstr);
		}
	}

	/* Look up 'CERequirements' last so that it takes precedence */
	unwind_attributes(cad,"CERequirements",&reqstr);

	if (reqstr != NULL)
	{
		for(creq=reqstr; (*creq)!=NULL; creq++)
		{
			if (req_file== NULL)
			{
				req_file=fopen(filename,"w");
				if(req_file==NULL) goto free_reqstr;
			}
			fwrite(*creq ,1, strlen(*creq), req_file);
			fwrite("\n" ,1, strlen("\n"), req_file);
			n_written++;
		}
	}
	if (req_file != NULL) fclose(req_file);

free_reqstr:
	classad_free_results(reqstr);
	return n_written;
}

int check_TransferINOUT(classad_context cad, char **command, char *reqId, char **resultLine, char ***files_to_clean_up)
{
        int result;
        char *tmpIOfilestring = NULL;
        FILE *tmpIOfile = NULL;
        char *superbuffer = NULL;
        char *superbufferRemaps = NULL;
        char *superbufferTMP = NULL;
        char *iwd = NULL;
	size_t  iwd_alloc = 256;
        struct timeval ts;
        size_t i=0;
		int cs=0,fc=0;
        char *cur, *next_comma;
        int cur_len;
        char *newptr=NULL;
        char *newptr1=NULL;
        char *newptr2=NULL;
        int cleanup_file_index = 0;
        /* timetag to have unique Input and Output file lists */
        gettimeofday(&ts, NULL);

	/* Set up a NULL-terminated string array for temp file cleanup */
	if (files_to_clean_up != NULL)
	{
		*files_to_clean_up = (char **)calloc(4,sizeof(char *));
	}

	/* write files in InputFileList */
	result = classad_get_dstring_attribute(cad, "TransferInput", &superbuffer);
	if (result == C_CLASSAD_NO_ERROR)
	{
		classad_get_dstring_attribute(cad, "Iwd", &iwd);
		/* very tempting, but it's a linux-only extension to POSIX:
		if (iwd == NULL) iwd = getcwd(NULL, 0); */
		if(iwd == NULL)
		{
			/* Try to set iwd to the current directory */
			iwd = (char *)malloc(iwd_alloc);
			while (iwd != NULL)
			{
				if (getcwd(iwd, iwd_alloc) == NULL)
				{
					if (errno == ERANGE)
					{
						iwd_alloc += 256;
						iwd = (char *)realloc(iwd, iwd_alloc);
					} else {
						free(iwd);
						iwd = NULL;
					}
				}
				else break;
			}
		}

		if(iwd == NULL)
		{
			/* PUSH A FAILURE */
			if (resultLine != NULL) *resultLine = make_message("%s 1 Iwd\\ not\\ specified\\ and\\ failed\\ to\\ determine\\ current\\ dir. N/A", reqId);
			free(superbuffer);
			return 1;
		}
		tmpIOfilestring = make_message("%s/%s_%s_%ld%ld", tmp_dir, "InputFileList",reqId, ts.tv_sec, ts.tv_usec);
		tmpIOfile = fopen(tmpIOfilestring, "w");
		if(tmpIOfile == NULL)
		{
			/* PUSH A FAILURE */
			if (resultLine != NULL) *resultLine = make_message("%s 1 Error\\ opening\\ %s N/A", reqId,tmpIOfilestring);
			free(tmpIOfilestring);
			free(superbuffer);
			free(iwd);
			return 1;
		}

		cs = 0;
		cur = superbuffer; 
		while ((next_comma = strchr(cur, ',')) != NULL)
		{
			if (next_comma > cur)
			{
				if (*cur != '/')
				{
					fc = fprintf(tmpIOfile, "%s/", iwd);
					if (fc < 0) cs = fc;
				}
				cur_len = next_comma - cur;
				fc = fprintf(tmpIOfile, "%*.*s\n", cur_len, cur_len, cur);
				if (fc < 0) cs = fc;
			}
			cur = next_comma + 1;
		}
		if (strlen(cur) > 0)
		{
			if (*cur != '/')
			{
				fc = fprintf(tmpIOfile, "%s/%s\n", iwd, cur);
				if (fc < 0) cs = fc;
			} else {
				fc = fprintf(tmpIOfile, "%s\n", cur);
				if (fc < 0) cs = fc;
			}
		}

		if (cs < 0)
		{
			/* PUSH A FAILURE */
			if (resultLine != NULL) *resultLine = make_message("%s 1 Error\\ writing\\ to\\ %s N/A", reqId,tmpIOfilestring);
			fclose(tmpIOfile);
			unlink(tmpIOfilestring);
			free(tmpIOfilestring);
			free(superbuffer);
			free(iwd);
			return 1;
		}

		newptr = make_message("%s -I %s", *command, tmpIOfilestring);
		fclose(tmpIOfile);
		if (*files_to_clean_up != NULL) {
			(*files_to_clean_up)[cleanup_file_index++] = tmpIOfilestring;
		} else {
			free(tmpIOfilestring);
		}
		free(superbuffer);
		free(iwd);
	}

        if(newptr==NULL) newptr = *command;
        cs = 0;
        result = classad_get_dstring_attribute(cad, "TransferOutput", &superbuffer);
        if (result == C_CLASSAD_NO_ERROR)
        {

                if(classad_get_dstring_attribute(cad, "TransferOutputRemaps", &superbufferRemaps) == C_CLASSAD_NO_ERROR)
                {
                        superbufferTMP = strdup(superbuffer);
                }
                tmpIOfilestring = make_message("%s/%s_%s_%ld%ld", tmp_dir, "OutputFileList",reqId,ts.tv_sec, ts.tv_usec);
                tmpIOfile = fopen(tmpIOfilestring, "w");
                if(tmpIOfile == NULL)
                {
                        /* PUSH A FAILURE */
                        if (resultLine != NULL) *resultLine = make_message("%s 1 Error\\ opening\\ %s N/A", reqId,tmpIOfilestring);
                        free(tmpIOfilestring);
                        if (superbufferRemaps != NULL) free(superbufferRemaps);
                        if (superbufferTMP != NULL) free(superbufferTMP);
                        free(superbuffer);
						if(*command != newptr) free(newptr);
                        return 1;
                }

                for(i =0; i < strlen(superbuffer); i++){if (superbuffer[i] == ',')superbuffer[i] ='\n'; }
                cs = fwrite(superbuffer,1 , strlen(superbuffer), tmpIOfile);
                if((int)strlen(superbuffer) != cs)
                {
                        /* PUSH A FAILURE */
                        if (resultLine != NULL) *resultLine = make_message("%s 1 Error\\ writing\\ in\\ %s N/A", reqId,tmpIOfilestring);
                        fclose(tmpIOfile);
                        unlink(tmpIOfilestring);
                        free(tmpIOfilestring);
                        if (superbufferRemaps != NULL) free(superbufferRemaps);
                        if (superbufferTMP != NULL) free(superbufferTMP);
                        free(superbuffer);
						if(*command != newptr) free(newptr);
			return 1;
                }
                fwrite("\n",1,1,tmpIOfile);
                newptr1 = make_message("%s -O %s", newptr, tmpIOfilestring);
                fclose(tmpIOfile);
		if (files_to_clean_up && (*files_to_clean_up != NULL)) {
			(*files_to_clean_up)[cleanup_file_index++] = tmpIOfilestring;
		} else {
			free(tmpIOfilestring);
		}
                free(superbuffer);
        }
        if(superbufferTMP != NULL)
        {
                superbuffer = outputfileRemaps(superbufferTMP,superbufferRemaps);
                if(superbuffer == NULL)
                {
                        /* PUSH A FAILURE */
                        if (resultLine != NULL) *resultLine = make_message("%s 1 Out\\ of\\ memory\\ computing\\ file\\ remaps N/A", reqId);
                        free(superbufferRemaps);
                        free(superbufferTMP);
						if(*command != newptr) free(newptr);
						if (newptr1) free(newptr1);	
                        return 1;
                }

                tmpIOfilestring = make_message("%s/%s_%s_%ld%ld", tmp_dir, "OutputFileListRemaps",reqId,ts.tv_sec, ts.tv_usec);
                tmpIOfile = fopen(tmpIOfilestring, "w");
                if(tmpIOfile == NULL)
                {
                        /* PUSH A FAILURE */
                        if (resultLine != NULL) *resultLine = make_message("%s 1 Error\\ opening\\ %s N/A", reqId,tmpIOfilestring);
                        free(tmpIOfilestring);
                        free(superbuffer);
                        free(superbufferTMP);
                        free(superbufferRemaps);
						if(*command != newptr) free(newptr);
						if (newptr1) free(newptr1);	
                        return 1;
                }

                for(i =0; i < strlen(superbuffer); i++){if (superbuffer[i] == ',')superbuffer[i] ='\n'; }
                cs = fwrite(superbuffer,1 , strlen(superbuffer), tmpIOfile);
                if((int)strlen(superbuffer) != cs)
                {
                        /* PUSH A FAILURE */
                        if (resultLine != NULL) *resultLine = make_message("%s 1 Error\\ writing\\ in\\ %s N/A", reqId,tmpIOfilestring);
                        fclose(tmpIOfile);
                        unlink(tmpIOfilestring);
                        free(tmpIOfilestring);
                        free(superbuffer);
                        free(superbufferTMP);
                        free(superbufferRemaps);
						if(*command != newptr) free(newptr);
						if (newptr1) free(newptr1);	
                        return 1;
                }
                fwrite("\n",1,1,tmpIOfile);
                newptr2 = make_message("%s -R %s", newptr1, tmpIOfilestring);
                fclose(tmpIOfile);
		if (*files_to_clean_up != NULL)
			(*files_to_clean_up)[cleanup_file_index++] = tmpIOfilestring;
                free(superbuffer);
                free(superbufferTMP);
                free(superbufferRemaps);
	}

        if(newptr2)
        {
                 free(newptr1);
                 if(*command != newptr) free(newptr);
                 free(*command);
                 *command = newptr2;
        }
        else
        if(newptr1)
        {
                 if(*command != newptr) free(newptr);
                 free(*command);
                 *command=newptr1;
         }else
         if(*command != newptr)
         {
                free(*command);
                *command=newptr;
         }

        return 0;
}

char*  outputfileRemaps(char *sb,char *sbrmp)
{
        /*
                Files in TransferOutputFile attribute are remapped on TransferOutputFileRemaps
                Example of possible remapping combinations:
                out1,out2,out3,log/out4,log/out5,out6,/tmp/out7
                out2=out2a,out3=/my/data/dir/out3a,log/out5=/my/log/dir/out5a,out6=sub/out6
        */
        char *newbuffer = NULL;
        char *tstr = NULL;
        char *tstr2 = NULL;
        int sblen = strlen(sb);
        int sbrmplen = strlen(sbrmp);
        int i = 0, j = 0, endstridx = 0, begstridx = 0, endstridx1 = 0, begstridx1 = 0, strmpd = 0, blen = 0, last = 0;
        for( i = 0; i <= sblen ; i++)
        {
                if((sb[i] == ',')||(i == sblen - 1))
                {
                        /* Files to be remapped are extracted*/
                        if(i < sblen )
                        {
                                endstridx = (i ==(sblen - 1) ? i: i - 1);
                                tstr = malloc(endstridx  - begstridx + 2);
                                strncpy(tstr,&sb[begstridx],endstridx  - begstridx + 1);
                                tstr[endstridx  - begstridx + 1] = '\0';
                                if(i < sblen )
                                begstridx = i + 1;
                        }else
                        {
                                endstridx = i;
                                tstr = malloc(endstridx  - begstridx + 2);
                                strncpy(tstr,&sb[begstridx],endstridx  - begstridx + 1);
                                tstr[endstridx  - begstridx + 1] = '\0';
                                last = 1;
                        }
                        /* Search if the file must be remapped */
                        for(j = 0; j <= sbrmplen;)
                        {
                                endstridx1=0;
                                if(!strncmp(&sbrmp[j],tstr,strlen(tstr)))
                                {
					/* Argument boundary checks */
					if ((j > 0) && (sbrmp[j-1] != ';')) { j++; continue; }
					if (sbrmp[j + strlen(tstr)] != '=') { j++; continue; }
                                        begstridx1 = j + strlen(tstr) + 1;
               /* separator ; */        while((sbrmp[begstridx1 + endstridx1] != ';')&&((begstridx1 + endstridx1) <= sbrmplen ))
                                                 endstridx1++;
                                        if(begstridx1 + endstridx1 <= sbrmplen + 1)
                                        {
                                                tstr2=malloc(endstridx1 + 1);
                                                memset(tstr2,0,endstridx1);
                                                strncpy(tstr2,&sbrmp[begstridx1],endstridx1);
                                                tstr2[endstridx1]='\0';
                                                strmpd = 1;
                                        }
                                        begstridx1 = 0;
                                        break;
                                }else j++;
                        }
                        if(newbuffer)
                                blen = strlen(newbuffer);
                        if(strmpd == 0)
                        {
                                newbuffer =(char*) realloc((void*)newbuffer, blen + strlen(tstr) + 2);
                                assert(newbuffer);
                                strcpy(&newbuffer[blen],tstr);
                                newbuffer[blen + strlen(tstr)] = (last? 0: ',');
                                newbuffer[blen + strlen(tstr) + 1] = '\0';
                                memset((void*)tstr,0,strlen(tstr));
                        }
                        else
                        {
                                newbuffer = (char*) realloc((void*)newbuffer, blen + strlen(tstr2) + 2);
                                assert(newbuffer);
                                strcpy(&newbuffer[blen],tstr2);
                                newbuffer[blen + strlen(tstr2)] = (last? 0: ',');
                                newbuffer[blen + strlen(tstr2) + 1] = '\0';
                                memset((void*)tstr2,0,strlen(tstr2));
                                memset((void*)tstr,0,strlen(tstr));
                        }
                        strmpd = 0;
                        if (tstr2){free(tstr2); tstr2=NULL;}
                        if (tstr) {free(tstr); tstr=NULL;}
                }
        }

        return newbuffer;
}

#define SINGLE_QUOTE_CHAR '\''
#define DOUBLE_QUOTE_CHAR '\"'
#define CONVARG_OPENING        "\"\\\""
#define CONVARG_OPENING_LEN    3
#define CONVARG_CLOSING        "\\\"\"\000"
#define CONVARG_CLOSING_LEN    4
#define CONVARG_QUOTSEP        "\\\"%c\\\""
#define CONVARG_QUOTSEP_LEN    5
#define CONVARG_DBLQUOTESC     "\\\\\\\""
#define CONVARG_DBLQUOTESC_LEN 4

char*
ConvertArgs(char* original, char separator)
{
	/* example arguments
	args(1)="a '''b''' 'c d' e' 'f ''''" ---> "a;\'b\';c\ d;e\ f;\'"
	args(1)="'''b''' 'c d'" --> "\'b\';c\ d"
	*/

	int inside_quotes = 0;
	char *result;
	int i, j;
	int orig_len;

	char quoted_sep[CONVARG_QUOTSEP_LEN + 1];
	sprintf(quoted_sep, CONVARG_QUOTSEP, separator);

	orig_len = strlen(original);
	if (orig_len == 0) return NULL;

        /* Worst case is <a> --> <"\"a\""> */
	result = (char *)malloc(orig_len * 10);
	if (result == NULL) return NULL;

	memcpy(result, CONVARG_OPENING, CONVARG_OPENING_LEN);
	j = CONVARG_OPENING_LEN;

	for(i=0; i < orig_len; i++)
	{
		if(original[i] == SINGLE_QUOTE_CHAR && !inside_quotes)
		{	/* the quote is an opening quote */
			inside_quotes = 1;
		}
		else if (original[i] == SINGLE_QUOTE_CHAR)
		{	/* a quote inside quotes... */
			if ((i+1) < orig_len && original[i+1] == SINGLE_QUOTE_CHAR) 
			{	/* the quote is a literal, copy and skip */
				result[j++] = original[i++];
			}
			else
			{	/* the quote is a closing quote */
				inside_quotes = 0;
			}
		}
		else if (original[i] == ' ')
		{	/* a blank... */
			if (inside_quotes)
			{	/* the blank is a literal, copy */
				result[j++] = original[i];
			}
			else
			{	/* the blank is a separator */
				memcpy(result + j, quoted_sep, CONVARG_QUOTSEP_LEN);
				j += CONVARG_QUOTSEP_LEN;
			}
		}
		else if (original[i] == DOUBLE_QUOTE_CHAR)
		{	/* double quotes need to be triple-escaped to make it to the submit file */
			memcpy(result + j, CONVARG_DBLQUOTESC, CONVARG_DBLQUOTESC_LEN);
			j += CONVARG_DBLQUOTESC_LEN;
		}
		else if ((original[i] == '(') || (original[i] == ')') || (original[i] == '&'))
		{	/* Must escape a few meta-characters for wordexp */
			result[j++] = '\\';
			result[j++] = original[i];
		}
		else
		{	/* plain copy from the original */
			result[j++] = original[i];
		}
	}
	memcpy(result + j, CONVARG_CLOSING, CONVARG_CLOSING_LEN);

	return(result);
}

