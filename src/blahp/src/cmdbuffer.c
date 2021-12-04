/*
#  File:     cmdbuffer.c
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#   25 Aug 2011 - Original release
#   31 Aug 2011 - Test code incorporated
#
#  Description:
#   Get commands from a file descriptor, buffering if needed
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

#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include "cmdbuffer.h"

#define BUFF_INCR_STEP 1024
#define BUFF_INCR_LIMIT (1024*BUFF_INCR_STEP)
#define CMDBUF_IS_SEP(x) ((x=='\n')||(x=='\r')||(x=='\000'))

static char *cmd_queue_start = NULL; /* Pointer to the allocated buffer */
static size_t cmd_queue_size = 0;    /* Total amount of space in the buffer */
static size_t cmd_queue_len = 0;     /* Length of valid data in the buffer */
static size_t cmd_current = 0;       /* Beginning of the first unserved command
                                        in the buffer (offset)*/
static struct pollfd cmd_fds[1];     /* File descriptor of the connected stream */
static int cmd_timeout = -1;         /* poll() timeout */


/* Initialise static data and structures
 * */
int
cmd_buffer_init(const int fd, const size_t bufsize, const int timeout)
{
	if ((cmd_queue_start = (char*)malloc(bufsize)) == NULL)
		return (CMDBUF_ERROR_NOMEM);
	cmd_queue_size = bufsize;
	cmd_queue_len = 0;
	cmd_current = 0;
	cmd_fds[0].fd = fd;
	cmd_fds[0].events = POLLIN;
	/* Don't allow timeout = 0 (it would be a busy loop) */
	cmd_timeout = timeout > 0 ? timeout * 1000 : -1;
	return(CMDBUF_OK);
}


/* Get a command from the buffer
 * */
int
cmd_buffer_get_command(char **command)
{
	char *tmp_realloc;
	size_t endcmd;
	ssize_t space_left;
	ssize_t chunk_len; 
	int pollret;

	/* The queue must be initialised first */
	if (cmd_queue_start == NULL) return (CMDBUF_ERROR_NOBUFFER);

	endcmd = cmd_current;
	for (;;)
	{
		/* First, search for a command in the current queue */
		while(endcmd < cmd_queue_len)
		{
			if (CMDBUF_IS_SEP(cmd_queue_start[endcmd]))
			{
				/* We've found a command, save it */
				cmd_queue_start[endcmd] = '\000';
				*command = strdup(cmd_queue_start + cmd_current);
				if (*command == NULL) return (CMDBUF_ERROR_NOMEM);

				/* Look for the beginning of the next command */
				for(cmd_current = endcmd + 1; cmd_current < cmd_queue_len; cmd_current++)
					if (! CMDBUF_IS_SEP(cmd_queue_start[cmd_current])) break;
				if (cmd_current >= cmd_queue_len)
				{
					/* There are no more commands, we can reuse the space */
					cmd_queue_len = 0;
					cmd_current = 0;
					endcmd = 0;
				}
				return (CMDBUF_OK);
			}
			endcmd++;
		}

		/* No command was found, wait for some more data */
		pollret = poll(cmd_fds, 1, cmd_timeout);
		if (pollret < 0)
		{
			if (errno != 0 && errno != EINTR)
				return(CMDBUF_ERROR_POLL);
			else
				return(CMDBUF_TIMEOUT);
		} else if (pollret == 0)
		{
		 	/* Timeout occurred */
			return(CMDBUF_TIMEOUT);
		}

		/* Check the remaining space in the queue */
		/* If it's lower than 50% of BUFF_INCR_STEP take some action */
		space_left = cmd_queue_size - cmd_queue_len;
		if (space_left < BUFF_INCR_STEP/2)
		{
			/* Try to recycle the leading space */
			if (cmd_current > 0)
			{
				memmove(cmd_queue_start, cmd_queue_start + cmd_current, cmd_queue_len - cmd_current);
				cmd_queue_len -= cmd_current;
				endcmd -= cmd_current;
				space_left += cmd_current;
				cmd_current = 0;
			}
			else
			/* Realloc the buffer with more space */
			{
				if (cmd_queue_size + BUFF_INCR_STEP > BUFF_INCR_LIMIT)
					return(CMDBUF_ERROR_NOMEM);
				cmd_queue_size += BUFF_INCR_STEP;
				tmp_realloc = (char *) realloc (cmd_queue_start, cmd_queue_size);
				if (tmp_realloc == NULL) return(CMDBUF_ERROR_NOMEM);
				cmd_queue_start = tmp_realloc;
				space_left += BUFF_INCR_STEP;
			}
		}

		/* Read at most space_left bytes of incoming data 
		 * and save them at the end of the buffer */
		chunk_len = read(cmd_fds[0].fd, cmd_queue_start + cmd_queue_len, space_left);
		if (chunk_len > 0)
		{
			cmd_queue_len += chunk_len;
		}
		else
		{
			/* read() was unsuccessful */
			return(CMDBUF_ERROR_READ);
		}
	} /* end for(;;) */
}


/* Free the buffer
 * */
int
cmd_buffer_free(void)
{
	if (cmd_queue_start) free (cmd_queue_start);
	return (CMDBUF_OK);
}


#ifdef CMDBUF_DEBUG
/* ------ TEST CODE HERE -------
#
#  Description:
#   Test the command buffer by sending a set of strings of different
#   size with random intervals. String are splitted with a given
#   frequency in order to test buffering of partial commands.
#
#   Compile with -DCMDBUF_DEBUG option, e.g.
#   $ gcc -o test_cmdbuffer -DCMDBUF_DEBUG cmdbuffer.c 
#
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <getopt.h>

#define NUM_COMMANDS 8
const char *commands[] = {
	"TEST STRING 1 TEST STRING 1 TEST STRING 1 TEST STRING 1 TEST STRING 1",
	"TEST STRING 2 TEST STRING 2 TEST STRING 2 TEST STRING 2",
	"TEST STRING 3 VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND"\
	  "VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND VERY LONG COMMAND",
	"TEST COMMAND 4 TEST COMMAND 4",
	"TEST COMMAND 5 TEST COMMAND 5 TEST COMMAND 5",
	"TEST COMMAND 6",
	"TEST COMMAND 7 TEST COMMAND 7 TEST COMMAND 7 TEST COMMAND 7",
	"TEST COMMAND 8 TEST COMMAND 8 TEST COMMAND 8 TEST COMMAND 8 TEST COMMAND 8"\
	"TEST COMMAND 8 TEST COMMAND 8 TEST COMMAND 8 TEST COMMAND 8 TEST COMMAND 8"\
	"TEST COMMAND 8 TEST COMMAND 8 TEST COMMAND 8 TEST COMMAND 8 TEST COMMAND 8"
};

/* Dump internals
 * */
int
cmd_buffer_dump_internals(void)
{
	int i = 0;
	char *c;

	fprintf(stderr, "  *** DUMP INTERNALS ***\n");
	fprintf(stderr, "  Buffer content:\n  ");
	for (c = cmd_queue_start; c < cmd_queue_start + cmd_queue_len; c++)
	{
		if (i++ == 8) { fprintf(stderr, "\n  "); i=1; }
		fprintf(stderr, "%c (0x%02x) ", *c >= 32 ? *c : '.', *c);
	}
	fprintf(stderr, "\n  ---\n");
	fprintf(stderr, "  cmd_queue_size = %d\n", cmd_queue_size);
	fprintf(stderr, "  cmd_queue_len = %d\n", cmd_queue_len);
	fprintf(stderr, "  cmd_current = %d\n", cmd_current);
	fprintf(stderr, "  cmd_fds[0].fd = %d\n", cmd_fds[0].fd);
	return (CMDBUF_OK);
}

/* Print usage
 * */
void
usage(const char *cmd)
{
	printf("Usage: %s [-h] [-v <verb>] [-n <iter>]\n", cmd);
	printf("  -h             print this help and exit\n");
	printf("  -v <verb>      set verbosity to <verb> (from 0 to 2) (default = 1)\n");
	printf("  -n <iter>      perform <iter> iterations of the tests (default = 100)\n\n");
	return;
}

/* Get commands from the pipe and compare them with the expected ones
 * */
void
receive_commands(int rfd, int verbose)
{
	int res;
	int i = 0;
	char *command;

	if (cmd_buffer_init(rfd, 1024, 2) != CMDBUF_OK)
	{
		perror("cmd_buffer_init");
		exit(1);
	}

	do
	{
		if (verbose>0) printf("C: expecting \"%s\"\n", commands[i]);
		switch(res = cmd_buffer_get_command(&command))
		{
		case CMDBUF_OK:
			if (verbose>0) printf("C: %d received \"%s\"\n", time(0), command);
			if (verbose>1) cmd_buffer_dump_internals();
			assert(strcmp(command, commands[i]) == 0);
			free(command);
			if (++i == NUM_COMMANDS) i = 0;
			break;
		case CMDBUF_TIMEOUT:
			if (verbose>0) printf("C: %d ---timeout occurred---\n", time(0));
			if (verbose>1) cmd_buffer_dump_internals();
			break;
		case CMDBUF_ERROR_READ:
			/* End of File: the father closed the the pipe */
			break;
		default:
			fprintf(stderr, "cmd_buffer_get_command() returned %d: ", res);
			perror("");
			break;
		}
	} while (res == CMDBUF_OK || res == CMDBUF_TIMEOUT);

	cmd_buffer_free();
	return;
}

/* Send commands to the pipe (splitting them 1/3 of the times)
 * */
void
send_commands(int wfd, int iter, int verbose)
{
	size_t c = 0;
	ssize_t n = 0;
	int i;

	srand(time(0));
	for (i=0; i<iter*NUM_COMMANDS; i++) {
		if (rand() % 3 > 0)
		{
			n = write(wfd, commands[c], strlen(commands[c]));
			if (verbose>0) printf("F: %d sent %d bytes\n", time(0), n);
		}
		else
		{
			if (verbose>0) printf("F: splitting the command\n");
			n = write(wfd, commands[c], strlen(commands[c])/2);
			if (verbose>0) printf("F: %d sent %d bytes\n", time(0), n);
			sleep(1);
			n = write(wfd, commands[c]+n, strlen(commands[c])-n);
			if (verbose>0) printf("F: %d sent %d bytes\n", time(0), n);
		}
		write(wfd, "\r\n", 2);
		if (verbose>0) printf("F: %d sent '\\r\\n'\n", time(0));
		else printf(".");
		fflush(stdout);
		sleep(rand() % 3);
		if (++c == NUM_COMMANDS) c = 0;
	}
	printf("\n");
	return;
}


int
main(int argc, char **argv)
{
	int pfd[2];
	pid_t pid;
	int verb = 1, iter = 100;
	char opt;

	while((opt = getopt(argc, argv, "hn:v:")) != -1)
	{
		switch(opt) {
		case 'h':
			usage(argv[0]);
			exit(0);
		case 'v':
			verb = atoi(optarg);
			break;
		case 'n':
			iter = atoi(optarg);
			break;
		default:
			usage(argv[0]);
			exit(1);
		}
	}
		
	if (pipe(pfd) == -1)
	{
		perror("pipe"); 
		exit(1);
	}

	switch(pid = fork())
	{
	case -1: /* error */
		perror("fork()");
		exit(1);
	case 0: /* child */
		close(pfd[1]); /* close the 'write' end */
		receive_commands(pfd[0], verb);
		close(pfd[0]);
		_exit(0);
	default: /* father */
		close(pfd[0]); /* close the 'read' end */
		send_commands(pfd[1], iter, verb);
		close(pfd[1]);
		wait(NULL);
		exit(0);
	}
}


#endif
