/*
#  File:     commands.c
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#   23 Apr 2004 - Origina release
#   07 Feb 2006 - Added correct recipe to unescape special characters
#                 in commands.
#   27 Mar 2006 - COMMANDS_NUM definition changed (no need to update
#                 it manually when adding/removing commands).
#
#  Description:
#   Parse client commands
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
#include <stdio.h>
#include <string.h>
#include "commands.h"
#include "blahpd.h"

/* Initialise commands array (strict alphabetical order)
 * handler functions prototypes are in commands.h
 * */    
command_t commands_array[] = {
	/* cmd string, # of pars, threaded (0=not threaded, 1=threaded, 2=treaded+proxy), handler */
	{ "ASYNC_MODE_OFF",               0, 0, cmd_async_off },
	{ "ASYNC_MODE_ON",                0, 0, cmd_async_on },
	{ "BLAH_GET_HOSTPORT",            1, 1, cmd_get_hostport },
	{ "BLAH_JOB_CANCEL",              2, 1, cmd_cancel_job },
	{ "BLAH_JOB_HOLD",                2, 1, cmd_hold_job },
	{ "BLAH_JOB_REFRESH_PROXY",       3, 2, cmd_renew_proxy },
	{ "BLAH_JOB_RESUME",              2, 1, cmd_resume_job },
	{ "BLAH_JOB_SEND_PROXY_TO_WORKER_NODE", 4, 2, cmd_send_proxy_to_worker_node },
	{ "BLAH_JOB_STATUS",              2, 1, cmd_status_job },
	{ "BLAH_JOB_STATUS_ALL",          1, 1, cmd_unknown },
	{ "BLAH_JOB_STATUS_SELECT",       2, 1, cmd_unknown },
	{ "BLAH_JOB_SUBMIT",              2, 2, cmd_submit_job },
	{ "BLAH_PING",                    2, 1, cmd_ping },
	{ "BLAH_SET_GLEXEC_DN",           3, 0, cmd_set_glexec_dn },
	{ "BLAH_SET_GLEXEC_OFF",          0, 0, cmd_unset_glexec_dn },	
	{ "BLAH_SET_SUDO_ID",             1, 0, cmd_set_sudo_id },
	{ "BLAH_SET_SUDO_OFF",            0, 0, cmd_set_sudo_off },	
	{ "CACHE_PROXY_FROM_FILE",        2, 0, cmd_unknown },
	{ "COMMANDS",                     0, 0, cmd_commands },
	{ "QUIT",                         0, 0, cmd_quit },
	{ "RESULTS",                      0, 0, cmd_results },
	{ "UNCACHE_PROXY",                1, 0, cmd_unknown },
	{ "USE_CACHED_PROXY",             1, 0, cmd_unknown },
	{ "VERSION",                      0, 0, cmd_version }
};
/* N.B.: KEEP STRICT ALPHABETICAL ORDER WHEN ADDING COMMANDS !!!*/

#define COMMANDS_NUM ( sizeof(commands_array) / sizeof(command_t) )

/* Key comparison function 
 * */
int
cmd_search_array(const void *key, const void *cmd_struct)
{
	return(strcasecmp((const char *)key, ((const command_t *)cmd_struct)->cmd_name));
}

/* Return a pointer to the command_t structure
 * whose cmd_name is cmd
 */
command_t *
find_command(const char *cmd)
{
	return(bsearch(cmd, commands_array, COMMANDS_NUM, sizeof(command_t), cmd_search_array));
}

/* Return a list of known commands
 * */
char *
known_commands(void)
{
	char *result;
	char *reallocated;
	size_t i;

	if ((result = strdup("")))
	{
		for (i=0; i<COMMANDS_NUM; i++)
		{
			/* Skip "unknown" commands */
			if (commands_array[i].cmd_handler == cmd_unknown) continue;
			reallocated = (char *) realloc (result, strlen(result) + strlen(commands_array[i].cmd_name) + 2);
			if (reallocated == NULL)
			{
				free (result);
				result = NULL;
				break;
			}
			result = reallocated;
			if (i) strcat(result, " ");
			strcat(result, commands_array[i].cmd_name);
		}
	}
	return (result);
}

/* Unescape special characters in command tokens.
 * "In the GAHP protocol, the following characters must be escaped with  
 *  the backslash if they appear within an argument:
 * - space (this is taken care elsewhere)
 * - backslash
 * - carriage return ('/r')
 * - newline ('/n')"
 * */
char *
unescape_special_chars(char *str)
{
	int i,j,slen;

	if (str == NULL) return str;

	slen = strlen(str);

	for (i = 0,j = 0; j < slen; i++,j++)
	{
		if( (str[j]=='\\' && str[j+1]=='\\') ||
		    (str[j]=='\\' && str[j+1]=='\r') ||
		    (str[j]=='\\' && str[j+1]=='\n') )
		{
			j++;
		}
		if (i!=j) str[i]=str[j];
	}
	str[i]='\000';
	return(str);
}

/* Split a command string into tokens
 * */
int
parse_command(const char *cmd, int *argc, char ***argv)
{
	char *pointer;
	char *parsed;
	char *next;
	char **retval;
	int my_argc, join_arg;

	if (strlen(cmd) == 0)
	{
		*argv = NULL;
		*argc = 0;
		return(1);
	}
	
	if((retval = (char **)malloc(sizeof(char *))) == NULL)
	{
		fprintf(stderr, "Out of memory.\n");
		exit(MALLOC_ERROR);
	}

	if((parsed = strdup(cmd)) == NULL)
	{
		fprintf(stderr, "Out of memory.\n");
		exit(MALLOC_ERROR);
	}

	my_argc = 0;
	
	next = strtok_r(parsed, " ", &pointer);
	if (next != NULL) retval[my_argc] = strdup(next);
	else              retval[my_argc] = NULL;

	while (next != NULL)
	{
		join_arg = (retval[my_argc][strlen(retval[my_argc]) - 1] != '\\') ? 0 : 1;
				
		next = strtok_r(NULL, " ", &pointer);
		if (next != NULL)
		{
			if (join_arg)
			{
				retval[my_argc][strlen(retval[my_argc]) - 1] = ' ';
				retval[my_argc] = (char *) realloc (retval[my_argc], strlen(retval[my_argc]) + strlen(next) + 1);
				strcat(retval[my_argc], next);
			}
			else
			{
				unescape_special_chars(retval[my_argc]);
				my_argc++;
				if ((retval = (char **) realloc (retval, (my_argc+1) * sizeof(char *))) == NULL)
				{
					fprintf(stderr, "Out of memory.\n");
					exit(MALLOC_ERROR);
				}
				retval[my_argc] = strdup(next);
			}
		}
	}

	unescape_special_chars(retval[my_argc]);
	my_argc++;
	if ((retval = (char **) realloc (retval, (my_argc+1) * sizeof(char *))) == NULL)
	{
		fprintf(stderr, "Out of memory.\n");
		exit(2);
	}
	retval[my_argc] = NULL;

	*argv = retval;
	*argc = my_argc;

	free(parsed);

	return(0);
}
