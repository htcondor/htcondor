/*
#  File:     mtsafe_popen.c
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#    7 Sep 2005 - Original release
#
#  Description:
#   Implements a mutexed popen a pclose to be MT safe
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
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/poll.h>

#include "blahpd.h"
#include "blah_utils.h"

static pthread_mutex_t poperations_lock  = PTHREAD_MUTEX_INITIALIZER;
extern config_handle *blah_config_handle;
extern char **environ;
extern char *gloc;

int
init_poperations_lock(void)
{
	pthread_mutex_init(&poperations_lock, NULL);
}

FILE *
mtsafe_popen(const char *command, const char *type)
{
	FILE *result;

#ifdef MT_DEBUG
	int retcode;

	retcode = pthread_mutex_trylock(&poperations_lock);
	if (retcode)
	{
		fprintf(stderr, "Thread %d: another thread was popening\n", pthread_self());
#endif
	pthread_mutex_lock(&poperations_lock);
#ifdef MT_DEBUG
	}
#endif
	result = popen(command, type);
	pthread_mutex_unlock(&poperations_lock);
	return(result);
}

int
mtsafe_pclose(FILE *stream)
{
	int result;

#ifdef MT_DEBUG
	int retcode;

	retcode = pthread_mutex_trylock(&poperations_lock);
	if (retcode)
	{
		fprintf(stderr, "Thread %d: another thread was pclosing\n", pthread_self());
#endif
	pthread_mutex_lock(&poperations_lock);
#ifdef MT_DEBUG
	}
#endif
	result = pclose(stream);
	pthread_mutex_unlock(&poperations_lock);
	return(result);
}



/* Utility functions */
int
glexec_kill_pid(pid_t pid, int sig, const char *cmd, char *const env[])
{
	char *kill_cmd;
	char *kill_out, *kill_err;
	char *kill_cmd_format="%s %s -%d %d";
	config_entry *cfg_kill_cmd_format;
	int nalloc;
	int kill_status;

	if (blah_config_handle != NULL)
	{
		cfg_kill_cmd_format=config_get("blah_graceful_kill_glexecable_cmd_format",blah_config_handle);
		if (cfg_kill_cmd_format != NULL) kill_cmd_format = cfg_kill_cmd_format->value;
	}

        nalloc = snprintf(NULL, 0, kill_cmd_format, gloc, cmd, sig, pid) + 1;

        kill_cmd = (char *) malloc (nalloc);
        if (kill_cmd == NULL)
	{
		errno = ENOMEM;
		return -1;
	}

	snprintf(kill_cmd, nalloc, kill_cmd_format, gloc, cmd, sig, pid);

        kill_status = exe_getouterr(kill_cmd, env, &kill_out, &kill_err);

	free(kill_cmd);
	if (kill_out) free(kill_out);
	if (kill_err) free(kill_err);
	
	return(kill_status);
}

int
merciful_kill(pid_t pid, int kill_via_glexec, 
              const char *glexeced_command, 
              char *const glexec_environment[])
{
	int graceful_timeout = 20; /* Default value - overridden by config */
	int status=0;
	config_entry *config_timeout;
	int tmp_timeout;
	int tsl;
	char *glexec_kill_cmd = "/bin/kill";
	config_entry *cfg_glexec_kill_cmd;
	int kill_status;

	if (blah_config_handle != NULL && 
	    (config_timeout=config_get("blah_graceful_kill_timeout",blah_config_handle)) != NULL)
	{
		tmp_timeout = atoi(config_timeout->value);
		if (tmp_timeout > 0) graceful_timeout = tmp_timeout;
	}

	if (kill_via_glexec && blah_config_handle != NULL)
	{
		cfg_glexec_kill_cmd=config_get("blah_graceful_kill_glexecable_cmd",blah_config_handle);
		if (cfg_glexec_kill_cmd != NULL) glexec_kill_cmd = cfg_glexec_kill_cmd->value;
	}

	if (kill_via_glexec &&
            strncmp(glexeced_command, glexec_kill_cmd, strlen(glexec_kill_cmd))==0)
	{
		/* Glexec'ing kill for a glexeced kill is overkill */
		/* (can lead to a cascade of attempts). Disable that. */
		kill_via_glexec = 0;
	}

	/* verify that child is dead */
	for(tsl = 0; (waitpid(pid, &status, WNOHANG) == 0) &&
	              tsl < graceful_timeout; tsl++)
	{
		/* still alive, allow a few seconds 
		   than use brute force */
		sleep(1);
		if (tsl > (graceful_timeout/2)) 
		{
			/* Signal forked process group */
			if (kill_via_glexec)
			{
				glexec_kill_pid(-pid, SIGTERM, glexec_kill_cmd, 
						glexec_environment);
				
			}
			else kill(-pid, SIGTERM);
		}
	}

	if (tsl >= graceful_timeout && (waitpid(pid, &status, WNOHANG) == 0))
	{
		if (kill_via_glexec)
		{
			glexec_kill_pid(-pid, SIGKILL, glexec_kill_cmd,	glexec_environment);
		}
		else kill_status = kill(-pid, SIGKILL);

		if (kill_status == 0)
		{
			waitpid(pid, &status, 0);
		}
	}

	return(status);
}

int
read_data(const int fdpipe, char **buffer)
{
	char local_buffer[1024];
	int char_count;
	int i;

	if (*buffer == NULL) return(-1);

	if ((char_count = read(fdpipe, local_buffer, sizeof(local_buffer) - 1)) > 0)
	{
		if ((*buffer = (char *)realloc(*buffer, strlen(*buffer) + char_count + 1)) == NULL)
		{
			fprintf(stderr, "out of memory!\n");
			exit(1);
		}
		local_buffer[char_count] = '\000';
		/* Any stray NUL in the output string ? */
		for (i=0; i<char_count; i++) if (local_buffer[i] == '\000') local_buffer[i]='.';
		strcat(*buffer, local_buffer);
	}
	return(char_count);
}

/* This function forks a new process to run a command, and returns
   the exit code and the stdout in an allocated buffer.
   The caller must take care of freing the buffer.
*/

#define MAX_ENV_SIZE 8192
#define BUFFERSIZE 2048

int
exe_getouterr(char *const command, char *const environment[], char **cmd_output, char **cmd_error)
{
	int fdpipe_stdout[2];
	int fdpipe_stderr[2];
	struct pollfd pipe_poll[2];
	int poll_timeout = 30000; /* 30 seconds by default */
	pid_t pid, process_group;
	int child_running, status, exitcode;
	char **envcopy = NULL;
	int envcopy_size;
	int i = 0;
	char buffer[BUFFERSIZE];
	char *killed_format = "%s <blah> killed by signal %s %d.\n";
	char *killed_for_timeout = "%s <blah> exe_getouterr: %d seconds timeout expired, killing child process.\n";
	char *killed_for_poll_signal = "%s <blah> exe_getouterr: poll() got an unknown event (stdout 0x%04X - stderr: 0x%04X).\n";
        char *signal_name="";
	char *new_cmd_error;
	wordexp_t args;
	config_entry *config_timeout;
	int tmp_timeout;
	int successful_read;
	int kill_via_glexec = 0;
	char *glexeced_cmd = NULL;

	if (blah_config_handle != NULL && 
	    (config_timeout=config_get("blah_child_poll_timeout",blah_config_handle)) != NULL)
	{
		tmp_timeout = atoi(config_timeout->value);
		if (tmp_timeout > 0) poll_timeout = tmp_timeout*1000;
	}

	if (cmd_error == NULL || cmd_output == NULL) return(-1);

	*cmd_output = NULL;
	*cmd_error = NULL;

	/* Is this a glexec'ed command ? */
	if (strncmp(command, gloc, strlen(gloc)) == 0)
	{
		kill_via_glexec = 1;
		glexeced_cmd = command + strlen(gloc);
		while (*glexeced_cmd == ' ') glexeced_cmd++;
	}

	/* Copy original environment */
	for (envcopy_size = 0; environ[envcopy_size] != NULL; envcopy_size++)
	{
		envcopy = (char **)realloc(envcopy, sizeof(char *) * (envcopy_size + 1));
		envcopy[envcopy_size] = strdup(environ[envcopy_size]);
	}

	/* Add the required environment */
	if (environment)
		for(i = 0; environment[i] != NULL; i++)
		{
			envcopy = (char **)realloc(envcopy, sizeof(char *) * (envcopy_size + i + 1));
			envcopy[envcopy_size + i] = strdup(environment[i]);
		}

	/* Add the NULL terminator */
	envcopy = (char **)realloc(envcopy, sizeof(char *) * (envcopy_size + i + 1));
	envcopy[envcopy_size + i] = (char *)NULL;

	/* Do the shell expansion */
	if(i = wordexp(command, &args, WRDE_NOCMD))
	{
		fprintf(stderr,"wordexp: unable to parse the command line \"%s\" (error %d)\n", command, i);
		return(1);
	}

#ifdef EXE_GETOUT_DEBUG
	fprintf(stderr, "DEBUG: blahpd invoking the command '%s'\n", command);
#endif

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
			execve(args.we_wordv[0], args.we_wordv, envcopy);
			exit(errno);

		default: /* Parent process */
			/* Close unused pipes */
			close(fdpipe_stdout[1]);
			close(fdpipe_stderr[1]);

			/* Free the copy of the environment */
			for (i = 0; envcopy[i] != NULL; i++)
				free(envcopy[i]);
			free(envcopy);

			/* Free the wordexp'd args */
			wordfree(&args);

			/* Initialise empty stderr and stdout */
			if ((*cmd_output = (char *)malloc(sizeof(char))) == NULL ||
			    (*cmd_error = (char *)malloc(sizeof(char))) == NULL)
			{
				fprintf(stderr, "out of memory!\n");
				exit(1);
			}
			*cmd_output[0] = '\000';
			*cmd_error[0] = '\000';


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
						perror("poll()");
						status = merciful_kill(pid, kill_via_glexec, glexeced_cmd, environment);
						child_running = 0;
						break;

					case 0: /* timeout occurred */
						/* add a message to stderr */
						new_cmd_error = make_message(killed_for_timeout, *cmd_error, poll_timeout/1000);
						if (new_cmd_error != NULL)
						{
							free(*cmd_error);
							*cmd_error = new_cmd_error;
						}
						else
							/* if memory low, print message directly on sdterr */
							fprintf(stderr, killed_for_timeout, *cmd_error, poll_timeout/1000);
						/* kill the child process */
						status = merciful_kill(pid, kill_via_glexec, glexeced_cmd, environment);
						child_running = 0;
						break;

					default: /* some event occurred */
						successful_read = 0;
						if (pipe_poll[0].revents & POLLIN)
						{
							if (read_data(pipe_poll[0].fd, cmd_output)>=0)
								successful_read = 1;
						}
						if (pipe_poll[1].revents & POLLIN)
						{
							if (read_data(pipe_poll[1].fd, cmd_error)>=0)
								successful_read = 1;
						}
						if (successful_read == 0)
						{
							/* add a message to stderr if the signal is not POLLHUP */
							if (!((pipe_poll[0].revents & POLLHUP) || (pipe_poll[1].revents & POLLHUP)))
							{
								new_cmd_error = make_message(killed_for_poll_signal, *cmd_error, pipe_poll[0].revents, pipe_poll[1].revents);
								if (new_cmd_error != NULL)
								{
									free(*cmd_error);
									*cmd_error = new_cmd_error;
								}
								else
									/* if memory low, print message directly on sdterr */
									fprintf(stderr, killed_for_poll_signal, *cmd_error, pipe_poll[0].revents, pipe_poll[1].revents);
							}
							/* kill the child process */
							status = merciful_kill(pid, kill_via_glexec, glexeced_cmd, environment);
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
				new_cmd_error = make_message(killed_format, *cmd_error, signal_name, exitcode);
				exitcode = -exitcode;
				if (new_cmd_error != NULL)
				{
					free(*cmd_error);
					*cmd_error = new_cmd_error;
				}
			}
			else
			{
				fprintf(stderr, "exe_getout: Child process terminated abnormally\n");
				return -1;
			}
			return(exitcode);	
	}
}

int
exe_getout(char *const command, char *const environment[], char **cmd_output)
{
	int result;
	char *cmd_error;

	result = exe_getouterr(command, environment, cmd_output, &cmd_error);
	if (cmd_error)
	{
		if (*cmd_error != '\000')
			fprintf(stderr, "blahpd:exe_getouterr() stderr caught for command <%s>:\n%s\n", command, cmd_error);
		free (cmd_error);
	}
	return(result);
}

