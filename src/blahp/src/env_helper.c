/*
#  File:     env_helper.c
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#    10 Mar 2009 - Original release
#
#  Description:
#    Helper functions to manage an execution environment.
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

#include "env_helper.h"
#include "blahpd.h"

int
push_env(env_t *my_env, const char *new_entry)
{
	int my_env_size = 1;
	env_t envcopy, new_envcopy;
	char *my_new_entry;

	if (new_entry == NULL)
	{
		BLAHDBG("push_env: %s\n", "trying to add a null string");
		errno = EINVAL;
		return(-1);
	}

	/* Duplicate the new entry */
	my_new_entry = strdup(new_entry);
	if (my_new_entry == NULL)
	{
		BLAHDBG("push_env: %s\n", "out of memory while duplicating the new entry");
		errno = ENOMEM;
		return(-1);
	}
	
	/* Enlarge or create the environment */
	if (my_env == NULL)
	{
		my_env_size = 1;
		new_envcopy = (env_t)malloc(sizeof(char *) * (my_env_size + 1));
	} else {
		envcopy = *my_env;
		if (envcopy != NULL)
			while (envcopy[my_env_size - 1] != NULL) my_env_size++;
		new_envcopy = (env_t)realloc(envcopy, sizeof(char *) * (my_env_size + 1));
	}

	if (new_envcopy == NULL)
	{
		BLAHDBG("push_env: %s\n", "out of memory while extending the environment");
		free(my_new_entry);
		errno = ENOMEM;
		return(-1);
	}

	/* Set the new value */
	new_envcopy[my_env_size - 1] = my_new_entry;
	new_envcopy[my_env_size] = (char *)NULL;

	*my_env = new_envcopy;
	return(0);
}

int
append_env(env_t *env_dest, const env_t env_src)
{
	int env_size;
	env_t envcopy;

	if (env_src == NULL)
	{
		errno = EINVAL;
		return(-1);
	}

	for (env_size = 0; env_src[env_size] != NULL; env_size++)
	{
		if (push_env(env_dest, env_src[env_size]) < 0)
		{
			if (env_dest != NULL) free_env(env_dest);
			return(-1);
		}
	}
	return(0);
}

int
copy_env(env_t *env_dest, const env_t env_src)
{
	if ((env_dest != NULL) && (*env_dest != NULL)) free_env(env_dest);
	return(append_env(env_dest, env_src));
}

void
free_env(env_t *my_env)
{
	int env_size;

	if (my_env == NULL) return;

	env_t envcopy = *my_env;

	if (*my_env == NULL) return;

	for (env_size = 0; envcopy[env_size] != NULL; env_size++)
		free(envcopy[env_size]);
	free(envcopy);
	*my_env = NULL;
}

