/*
#  File:     mapped_exec.c
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#    10 Mar 2009 - Original release
#
#  Description:
#    Executes a command, enabling optional "sudo-like" mechanism (like glexec or sudo itself).
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


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "blahpd.h"
#include "config.h"
#include "blah_utils.h"
#include "mapped_exec.h"

extern config_handle *blah_config_handle;
extern char **environ;

/* Internal functions */

static int
read_data(const int fdpipe, char **buffer)
{
	char local_buffer[1024];
	int char_count;
	int i;

	if (buffer == NULL) return(-1);
	if (*buffer == NULL) return(-1);

	if ((char_count = read(fdpipe, local_buffer, sizeof(local_buffer) - 1)) > 0)
	{
		if ((*buffer = (char *)realloc(*buffer, strlen(*buffer) + char_count + 1)) == NULL)
		{
			BLAHDBG("read_data: %s\n", "out of memory!");
			errno = ENOMEM;
			return(-1);
		}
		local_buffer[char_count] = '\000';

		/* Any stray NUL in the output string? Replace it with '.' */
		for (i=0; i<char_count; i++) if (local_buffer[i] == '\000') local_buffer[i] = '.';

		strcat(*buffer, local_buffer);
	}
	return(char_count);
}

static int
merciful_kill(pid_t pid, exec_cmd_t *cmd)
{
	int graceful_timeout = 20; /* Default value - overridden by config */
	config_entry *config_timeout;
	int tmp_timeout;
	int tsl;
	char *mapped_kill_cmd = "/bin/kill";
	config_entry *cfg_mapped_kill_cmd;
	char kill_args[32]; /* "-s SIGXXXX -YYYYY" - X = signal name, Y = pid  */
	int status = 0;
	int kill_status;

	if (blah_config_handle != NULL && 
	    (config_timeout = config_get("blah_graceful_kill_timeout", blah_config_handle)) != NULL)
	{
		tmp_timeout = atoi(config_timeout->value);
		if (tmp_timeout > 0) graceful_timeout = tmp_timeout;
	}

	/* Set the kill command */
	cmd->command = mapped_kill_cmd;
	cmd->append_to_command = kill_args;

	if ((cmd->delegation_type != MEXEC_NO_MAPPING) && (blah_config_handle != NULL))
	{
		cfg_mapped_kill_cmd = config_get("blah_graceful_kill_mappable_cmd", blah_config_handle);
		cmd->command = ( cfg_mapped_kill_cmd ? cfg_mapped_kill_cmd->value : mapped_kill_cmd );
	}

	/* verify that child is dead */
	for(tsl = 0; (waitpid(pid, &status, WNOHANG) == 0) &&
	              tsl < graceful_timeout; tsl++)
	{
		/* still alive, allow a few seconds 
		   then use brute force */
		sleep(1);
		if (tsl > (graceful_timeout/2)) 
		{
			/* Signal forked process group */
			if (cmd->delegation_type == MEXEC_NO_MAPPING)
				kill(-pid, SIGTERM);
			else
			{
				/* Warning: execute_cmd requires a leading space in its arguments */
				snprintf(kill_args, sizeof(kill_args), " -s SIGTERM -%d", pid); 
				execute_cmd(cmd);
			}
		}
	}

	if (tsl >= graceful_timeout && (waitpid(pid, &status, WNOHANG) == 0))
	{
		if (cmd->delegation_type == MEXEC_NO_MAPPING)
			kill_status = kill(-pid, SIGKILL);
		else
		{
			recycle_cmd(cmd);
			/* Warning: execute_cmd requires a leading space in its arguments */
			snprintf(kill_args, sizeof(kill_args), " -s SIGKILL -%d", pid); 
			execute_cmd(cmd);
			kill_status = cmd->exit_code;
		}

		if (kill_status == 0)
		{
			waitpid(pid, &status, 0);
		}
	}

	return(status);
}

char *
escape_wordexp_special_chars(char *in)
{
  char *special="|&;<>(){}";
  int inside_double_quotes;
  int inside_single_quotes;
  int inside_command_subst;

  int special_count;

  char *ret;
  char *cur, *wcur, prev;

  /* Count special char occurrences first */
  special_count = 0;
  cur = in;
  while ((cur = strpbrk(cur, special)) != NULL) 
   {
    special_count++;
    cur++;
   }

  if (special_count == 0) return NULL;

  ret = (char *)malloc(strlen(in) + special_count + 1);
  if (ret == NULL) return NULL;


  inside_double_quotes = FALSE;
  inside_single_quotes = FALSE;
  inside_command_subst = FALSE;

  for (wcur=ret,prev='\000',cur=in; *cur != '\000'; cur++)
   {
    if (*cur == '\'')
     {
      if (inside_single_quotes)
       {
        if (prev != '\\')
         {
          inside_single_quotes = FALSE;
         }
       }
      else if (!inside_double_quotes && !inside_command_subst)
       {
        inside_single_quotes = TRUE;
       }
     }
    if (*cur == '"')
     {
      if (inside_double_quotes)
       {
        if (prev != '\\')
         {
          inside_double_quotes = FALSE;
         }
       }
      else if (!inside_single_quotes && !inside_command_subst)
       {
        inside_double_quotes = TRUE;
       }
     }
    if (*cur == '(')
     {
      if (!inside_single_quotes && !inside_double_quotes &&
          prev == '$')
       {
        inside_command_subst = TRUE;
       }
     }

    if (strchr(special, *cur) != NULL)
     {
      if (!inside_single_quotes && !inside_double_quotes &&
          !inside_command_subst)
       {
        /* Escape special character */
        *wcur = '\\';
        wcur++;
       }
     }

    /* This check must follow the special character escape check */
    /* as ')' is one of the special characters */
    if (*cur == ')')
     {
      if (inside_command_subst)
       {
        if (prev != '\\')
         {
          inside_command_subst = FALSE;
         }
       }
     }

    *wcur = *cur;
    wcur++;
    prev = *cur;
   }
  *wcur = '\000';

  return ret;
}

/* Exported functions */

int
execute_cmd(exec_cmd_t *cmd)
{
	int fdpipe_stdout[2];
	int fdpipe_stderr[2];
	struct pollfd pipe_poll[2];
	int poll_timeout = 30000; /* 30 seconds by default */
	pid_t pid, process_group;
	int child_running, status, exitcode;
	char *killed_format = "%s <blah> killed by signal %s %d.\n";
	char *killed_for_timeout = "%s <blah> execute_cmd: %d seconds timeout expired, killing child process.\n";
	char *killed_for_poll_signal = "%s <blah> execute_cmd: poll() got an unknown event (stdout 0x%04X - stderr: 0x%04X).\n";
	char *signal_name = "";
	char *new_cmd_error;
	config_entry *config_timeout;
	int tmp_timeout;
	int successful_read;
	
	char *id_mapping_command = NULL;
	config_entry *cfg_id_mapping_command;
	char *env_strbuffer;
	int proxy_fd = -1;

	char *command = NULL;
	char *command_tmp = NULL;
	wordexp_t args;
	int wordexp_err;
	env_t cmd_env = NULL;

	exec_cmd_t kill_command = EXEC_CMD_DEFAULT;
	exec_cmd_t cp_proxy_command = EXEC_CMD_DEFAULT;

	/* Sanity checks */
	if (cmd->command == NULL && (cmd->delegation_type == MEXEC_NO_MAPPING || cmd->dest_proxy == NULL))
	{
		/* command can be NULL only to create the mapped user's proxy */
		BLAHDBG("execute_cmd: %s\n", "null command specified."); 
		errno = ENOEXEC;
		return(-1);
	}
	if ((cmd->delegation_type != MEXEC_NO_MAPPING) && (cmd->delegation_cred == NULL))
	{
		BLAHDBG("execute_cmd: %s\n", "ID mapping with null credential requested."); 
		errno = EINVAL;
		return(-1);
	}

	/* Environment setup */
	if (cmd->copy_original_env) append_env(&cmd_env, environ);
	if (cmd->environment) append_env(&cmd_env, cmd->environment);

	/* Set the appropriate command for the delegation type */
	switch (cmd->delegation_type)
	{
	case MEXEC_NO_MAPPING:
		/* cmd->command can't be NULL here (see Sanity checks) */
		command = strdup(cmd->command);
		break;

	case MEXEC_GLEXEC:
		if ((cfg_id_mapping_command = config_get("blah_id_mapping_command_glexec", blah_config_handle)) == NULL)
		{
			id_mapping_command = DEFAULT_GLEXEC_COMMAND;
			BLAHDBG("execute_cmd: ID mapping requested but no blah_id_mapping_command_glexec found in config file. Using default value: %s\n", id_mapping_command);
		}
		else
			id_mapping_command = cfg_id_mapping_command->value;

		/* /bin/pwd is used here as default as it's ordinarily */
		/* allowed to execute by GLEXEC configs. Take caution */
		/* by making sure it's added to GLEXEC allowed commands */
		/* before replacing it. */
		command = make_message("%s %s", id_mapping_command, cmd->command ? cmd->command : "/bin/pwd");
		push_env(&cmd_env, "GLEXEC_MODE=lcmaps_get_account");
		env_strbuffer = make_message("GLEXEC_CLIENT_CERT=%s", cmd->delegation_cred);
		push_env(&cmd_env, env_strbuffer);
		if (env_strbuffer) free(env_strbuffer);

		/* Let glexec take care of copying the source proxy to a new one, readable by the mapped user */ 
		if (cmd->source_proxy)
		{
			env_strbuffer = make_message("GLEXEC_SOURCE_PROXY=%s", cmd->source_proxy);
			push_env(&cmd_env, env_strbuffer);
			if (env_strbuffer) free(env_strbuffer);
		}
		if (cmd->dest_proxy)
		{
			env_strbuffer = make_message("GLEXEC_TARGET_PROXY=%s", cmd->dest_proxy);
			push_env(&cmd_env, env_strbuffer);
			if (env_strbuffer) free(env_strbuffer);
		}
		break;

	case MEXEC_SUDO:
		if ((cfg_id_mapping_command = config_get("blah_id_mapping_command_sudo", blah_config_handle)) == NULL)
		{
			id_mapping_command = DEFAULT_SUDO_COMMAND;
			BLAHDBG("execute_cmd: ID mapping requested but no blah_id_mapping_command_sudo found in config file. Using default value: %s\n", id_mapping_command);
		}
		else
			id_mapping_command = cfg_id_mapping_command->value;
		if (cmd->source_proxy && (strlen(cmd->source_proxy) > 0))
		{
			if (cmd->special_cmd == MEXEC_PROXY_COMMAND)
			{
				/* Open the file to pass it to the child */
				proxy_fd = open(cmd->source_proxy, O_RDONLY);
				if (proxy_fd == -1)
				{
					BLAHDBG("execute_cmd: cannot open source proxy <%s>\n", cmd->source_proxy);
					return(-1);
				}
			}
			else
			{
				/* In two steps, create a copy of the proxy, owned by the mapped user */
				/* First, use dd to make a temporary copy if the file */
				env_strbuffer = make_message("/bin/dd of=%s.tmp", cmd->dest_proxy); /*FIXME: find a better tmp name*/
				cp_proxy_command.command = env_strbuffer;
				cp_proxy_command.delegation_type = cmd->delegation_type;
				cp_proxy_command.delegation_cred = cmd->delegation_cred;
				cp_proxy_command.source_proxy = cmd->source_proxy;
				cp_proxy_command.special_cmd = MEXEC_PROXY_COMMAND; /* avoid infinite recursion */
				execute_cmd(&cp_proxy_command);
				free(env_strbuffer);

				/* Use mv to atomically rotate the temporary proxy to the final target */
				recycle_cmd(&cp_proxy_command);
				env_strbuffer = make_message("/bin/mv %s.tmp %s", cmd->dest_proxy, cmd->dest_proxy);
				cp_proxy_command.command = env_strbuffer;
				cp_proxy_command.source_proxy = NULL; /* avoid infinite recursion */
				cp_proxy_command.special_cmd = MEXEC_NORMAL_COMMAND;
				execute_cmd(&cp_proxy_command);
				free(env_strbuffer);
				cleanup_cmd(&cp_proxy_command);
			}
		}
		if (cmd->command)
			command = make_message("%s -u %s %s", id_mapping_command, cmd->delegation_cred, cmd->command);
		else
			return(0); /* nothing to execute */
		break;
	}

	/* Add the optional append string */
	if (cmd->append_to_command)
	{
		command_tmp = make_message("%s%s", command, cmd->append_to_command); /* N.B. intentionally no space! */
		free(command);
		command = command_tmp;
	}

	/* Get the timeout from config file */
	if (blah_config_handle != NULL && 
	    (config_timeout = config_get("blah_child_poll_timeout", blah_config_handle)) != NULL)
	{
		tmp_timeout = atoi(config_timeout->value);
		if (tmp_timeout > 0) poll_timeout = tmp_timeout * 1000;
	}

	/* Escape special characters, as per wordexp(3) manpage*/
	command_tmp = escape_wordexp_special_chars(command);
	if (command_tmp != NULL)
	{
		free(command);
		command = command_tmp;
	}

	/* Do the shell expansion */
	if((wordexp_err = wordexp(command, &args, WRDE_NOCMD)))
	{
		fprintf(stderr,"wordexp: unable to parse the command line \"%s\" (error %d)\n", command, wordexp_err);
		return(1);
	}
	BLAHDBG("execute_cmd: will execute the command <%s>\n", command);

	/* Create the pipes to read the child streams */ 
	if (pipe(fdpipe_stdout) == -1)
	{
		perror("pipe() for stdout");
		return(-1);       
	}
	if (pipe(fdpipe_stderr) == -1)
	{
		perror("pipe() for stderr");
		return(-1);       
	}

	/* Let's fork! */
	switch(pid = fork())
	{
		case -1:
			perror("fork");
			return(-1);

		case 0: /* Child process */
			/* CAUTION: fork was invoked from within a thread!
			 * Do NOT use any fork-unsafe function! */

			/* Set up process group so that the resulting process tree can be signaled. */
			if (((process_group = setsid()) == -1) ||
			     (process_group != getpid()))
			{
				fprintf(stderr,"Error: setsid() returns %d. getpid returns %d: ", process_group, getpid());
				perror("");
				exit(1);       
			}

			/* Connect stdin to the proxy file if opened */
			if (proxy_fd != -1)
			{
				if (dup2(proxy_fd, STDIN_FILENO) == -1)
				{
					perror("dup2() stdin");
					exit(1);       
				}
				BLAHDBG("%s\n", "Proxy fd dupped on stdin. Setting umask to 0777");
				umask(077);
			}

			/* Connect stdout & stderr to the pipes */
			if (dup2(fdpipe_stdout[1], STDOUT_FILENO) == -1)
			{
				perror("dup2() stdout");
				exit(1);       
			}
			if (dup2(fdpipe_stderr[1], STDERR_FILENO) == -1)
			{
				perror("dup2() stderr");
				exit(1);       
			}

			/* Close unused pipes */
			close(fdpipe_stdout[0]);
			close(fdpipe_stdout[1]);
			close(fdpipe_stderr[0]);
			close(fdpipe_stderr[1]);

			/* Execute the command */
			execve(args.we_wordv[0], args.we_wordv, cmd_env);

			/* If we are still here, execve failed */
			fprintf(stderr, "%s: %s", args.we_wordv[0], strerror(errno));
			exit(errno);

		default: /* Parent process */
			/* Close unused pipes */
			close(fdpipe_stdout[1]);
			close(fdpipe_stderr[1]);

			/* Free the copy of the environment */
			free_env(&cmd_env);

			/* Free the command */
			free(command);

			/* Free the wordexp'd args */
			wordfree(&args);

			/* Close the proxy file if it was opened */
			if (proxy_fd != -1) close (proxy_fd);

			/* Prepare the kill command */
			if (cmd->special_cmd == MEXEC_KILL_COMMAND)
				/* Avoid infinite recursion of kill commands */
				kill_command.delegation_type = MEXEC_NO_MAPPING;
			else
			{
				kill_command.delegation_type = cmd->delegation_type;
				kill_command.delegation_cred = cmd->delegation_cred;
			}
			kill_command.special_cmd = MEXEC_KILL_COMMAND;

			/* Initialise empty stderr and stdout */
			if ((cmd->output = (char *)malloc(sizeof(char))) == NULL ||
			    (cmd->error = (char *)malloc(sizeof(char))) == NULL)
			{
				fprintf(stderr, "out of memory!\n");
				exit(1);
			}
			cmd->output[0] = '\000';
			cmd->error[0] = '\000';

			child_running = 1;
			while(child_running)
			{
				/* Initialize fdpoll structures */
				pipe_poll[0].fd = fdpipe_stdout[0];
				pipe_poll[0].events = ( POLLIN | POLLERR | POLLHUP | POLLNVAL );
				pipe_poll[0].revents = 0;
				pipe_poll[1].fd = fdpipe_stderr[0];
				pipe_poll[1].events = ( POLLIN | POLLERR | POLLHUP | POLLNVAL );
				pipe_poll[1].revents = 0;
				switch(poll(pipe_poll, 2, poll_timeout))
				{
				case -1: /* poll error */
					if (errno == EINTR) continue; /*poll() was interrupted by a signal. */
					perror("execute_cmd: poll()");
					status = merciful_kill(pid, &kill_command);
					child_running = 0;
					break;

				case 0: /* timeout occurred */
					/* add a message to stderr */
					new_cmd_error = make_message(killed_for_timeout, cmd->error, poll_timeout/1000);
					if (new_cmd_error != NULL)
					{
						free(cmd->error);
						cmd->error = new_cmd_error;
					}
					else
						/* if memory low, print message directly on stderr */
						fprintf(stderr, killed_for_timeout, cmd->error, poll_timeout/1000);
					/* kill the child process */
					status = merciful_kill(pid, &kill_command);
					child_running = 0;
					break;

				default: /* some event occurred */
					successful_read = 0;
					if (pipe_poll[0].revents & POLLIN)
					{
						if (read_data(pipe_poll[0].fd, &(cmd->output))>=0)
							successful_read = 1;
					}
					if (pipe_poll[1].revents & POLLIN)
					{
						if (read_data(pipe_poll[1].fd, &(cmd->error))>=0)
							successful_read = 1;
					}
					if (successful_read == 0)
					{
						/* add a message to stderr if the signal is not POLLHUP */
						if (!((pipe_poll[0].revents & POLLHUP) || (pipe_poll[1].revents & POLLHUP)))
						{
							new_cmd_error = make_message(killed_for_poll_signal, cmd->error, pipe_poll[0].revents, pipe_poll[1].revents);
							if (new_cmd_error != NULL)
							{
								free(cmd->error);
								cmd->error = new_cmd_error;
							}
							else
								/* if memory low, print message directly on stderr */
								fprintf(stderr, killed_for_poll_signal, cmd->error, pipe_poll[0].revents, pipe_poll[1].revents);
						}
						/* kill the child process */
						status = merciful_kill(pid, &kill_command);
						child_running = 0;
						break;
					}
				}
			}

			close(fdpipe_stdout[0]);
			close(fdpipe_stderr[0]);

			if (WIFEXITED(status))
			{
				exitcode = WEXITSTATUS(status);
			}
			else if (WIFSIGNALED(status))
			{
				exitcode = WTERMSIG(status);
#ifdef _GNU_SOURCE
				signal_name = strsignal(exitcode);
#endif
				/* Append message to the stderr */
				new_cmd_error = make_message(killed_format, cmd->error, signal_name, exitcode);
				exitcode = -exitcode;
				if (new_cmd_error != NULL)
				{
					free(cmd->error);
					cmd->error = new_cmd_error;
				}
			}
			else
			{
				fprintf(stderr, "execute_cmd: Child process terminated abnormally\n");
				return(-1);
			}
			cmd->exit_code = exitcode;
			return(0);	
	}
}


void
recycle_cmd(exec_cmd_t *cmd)
{
	if (cmd->output)
	{
		free(cmd->output);
		cmd->output = NULL;
	}
	if (cmd->error)
	{
		free(cmd->error);
		cmd->error = NULL;
	}
	cmd->exit_code = 0;
}

void
cleanup_cmd(exec_cmd_t *cmd)
{
	/* Free all the memory allocated by the execution and env functions */
	recycle_cmd(cmd);
	free_env(&(cmd->environment));
}
