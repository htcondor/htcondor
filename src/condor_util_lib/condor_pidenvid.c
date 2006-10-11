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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_pidenvid.h"

extern char **environ;

/* set the structure to default values */
void pidenvid_init(PidEnvID *penvid)
{
	int i;

	/* all of these structures in Condor will be of this size */
	penvid->num = PIDENVID_MAX;

	for (i = 0; i < penvid->num; i++) {
		penvid->ancestors[i].active = FALSE;
		memset(penvid->ancestors[i].envid, '\0', PIDENVID_ENVID_SIZE);
	}
}

/* overwrite the pidenv with the family tree specific environment variables
	discovered in env */
int pidenvid_filter_and_insert(PidEnvID *penvid, char **env)
{
	int i;
	char **curr;

	i = 0;

	for (curr = env; *curr != NULL; curr++) {

		if (strncmp(*curr, PIDENVID_PREFIX, strlen(PIDENVID_PREFIX)) == 0) {

			if (i == PIDENVID_MAX) {
				return PIDENVID_NO_SPACE;
			}

			if ((strlen(*curr) + 1) >= PIDENVID_ENVID_SIZE) {
				return PIDENVID_OVERSIZED;
			}

			strcpy(penvid->ancestors[i].envid, *curr);
			penvid->ancestors[i].active = TRUE;

			i++;
		}
	}

	return PIDENVID_OK;
}

/* add a line created by pidenvid_format() into the pidenvid structure */
int pidenvid_append(PidEnvID *penvid, char *line)
{
	int i;

	/* find the first unused entry and append there */

	for (i = 0; i < penvid->num; i++)
	{
		if (penvid->ancestors[i].active == FALSE) {
			/* insert the name and val */

			/* check to make sure the name, val, =, and NUL all fit */
			if ((strlen(line) + 1) >= PIDENVID_ENVID_SIZE) {
				return PIDENVID_OVERSIZED;
			}

			strcpy(penvid->ancestors[i].envid, line);
			penvid->ancestors[i].active = TRUE;

			return PIDENVID_OK;
		}
	}
	
	return PIDENVID_NO_SPACE;
}

/* given a left and right hand side, put it into the buffer specified
	and ensure it fits correctly */
int pidenvid_format_to_envid(char *dest, int size, 
	pid_t forker_pid, pid_t forked_pid, time_t t, unsigned int mii)
{
	if (size > PIDENVID_ENVID_SIZE) {
		return PIDENVID_OVERSIZED;
	}

	/* Here is the representation which will end up in an environment 
		somewhere */
	sprintf(dest, "%s%d=%d%s%lu%s%u", 
		PIDENVID_PREFIX, forker_pid, 
		forked_pid, PIDENVID_SEP, t, PIDENVID_SEP, mii);

	return PIDENVID_OK;
}

/* given a pidenvid entry, convert it back to numbers */
int pidenvid_format_from_envid(char *src, pid_t *forker_pid, 
		pid_t *forked_pid, time_t *t, unsigned int *mii)
{
	int rval;

	/* rip out the values I care about and into the arguments */
	rval = sscanf(src, 
		PIDENVID_PREFIX "%d=%d" PIDENVID_SEP "%lu" PIDENVID_SEP "%u", 
		forker_pid, forked_pid, t, mii);

	/* There are 4 things I'm taking out of the string, make sure I get them */
	if (rval == 4) {
		return PIDENVID_OK;
	}

	return PIDENVID_BAD_FORMAT;

}

/* Determine if the left side is a perfect subset of the right side. 
	It must be the case that if two empty PidEnvID structures, a
	filled left and an empty right, or an empty left and a filled
	right are compared to each other, they will NOT match. This
	handles cases where OSes can't/don't read this information from
	the proc pid interface. */
int pidenvid_match(PidEnvID *left, PidEnvID *right)
{
	int l, r;
	int count;

	/* how many of the left hand side did I see in the right hand side? 
		It should be equal to the number of left hand side variables for a
		match to be considered. */
	count = 0;
	l = 0;

	for (l = 0; l < left->num; l++) {

		/* if I run out of the left hand side, abort whole operation */
		if (left->ancestors[l].active == FALSE) {
			break;
		} 

		for (r = 0; r < right->num; r++) {
			/* abort when I get to the end of the right side */
			if (right->ancestors[r].active == FALSE) {
				break;
			}
			
			/* this must be an active entry, so check it */
			if (strcmp(left->ancestors[l].envid, right->ancestors[r].envid) == 0) {
				count++;
			}
		}
	}

	/* If I saw every single lhs in the rhs, and there *was* something to match,
		then it is a match */
	if (count == l && l != 0) {
		return PIDENVID_MATCH;
	}

	/* else I found some of the left hand side in the right hand side which
		represents a sibling-like relationship and not direct parenthood */
	return PIDENVID_NO_MATCH;
}

/* combine the function calls of pidenvid_format_to_envid() and 
	pidenvid_append() into one easy to use insertion function */
int pidenvid_append_direct(PidEnvID *penvid,
	pid_t forker_pid, pid_t forked_pid, time_t t, unsigned int mii)
{
	int rval;
	char buf[PIDENVID_ENVID_SIZE];

	rval = pidenvid_format_to_envid(buf, PIDENVID_ENVID_SIZE, 
				forker_pid, forked_pid, t, mii);

	if (rval == PIDENVID_OVERSIZED) {
		return rval;
	}

	rval = pidenvid_append(penvid, buf);
	if (rval == PIDENVID_OVERSIZED) {
		return rval;
	}

	return PIDENVID_OK;
}

void pidenvid_copy(PidEnvID *to, PidEnvID *from)
{
	int i;

	pidenvid_init(to);

	to->num = from->num;

	for (i = 0; i < from->num; i++) {
		to->ancestors[i].active = from->ancestors[i].active;
		if (from->ancestors[i].active == TRUE) {
			strcpy(to->ancestors[i].envid, from->ancestors[i].envid);
		}
	}
}

/* emit the structure to the logs at the debug level specified */
void pidenvid_dump(PidEnvID *penvid, unsigned int dlvl)
{
	int i;

	dprintf(dlvl, "PidEnvID: There are %d entries total.\n", penvid->num);

	for (i = 0; i < penvid->num; i++) {
		/* only print out true ones */
		if (penvid->ancestors[i].active == TRUE) {
			dprintf(dlvl, "\t[%d]: active = %s\n", i,
				penvid->ancestors[i].active == TRUE ? "TRUE" : "FALSE");
			dprintf(dlvl, "\t\t%s\n", penvid->ancestors[i].envid);
		}
	}
}




