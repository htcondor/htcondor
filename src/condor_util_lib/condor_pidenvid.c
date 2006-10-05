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

	/* initially, there is nothing in a penvid */
	penvid->empty = TRUE;

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
			penvid->empty = FALSE;

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
			penvid->empty = FALSE;

			return PIDENVID_OK;
		}
	}
	
	return PIDENVID_NO_SPACE;
}

/* given a left and right hand side, put it into the buffer specified
	and ensure it fits correctly */
int pidenvid_format_to_envid(char *dest, int size, 
	pid_t forker_pid, time_t ft, pid_t forked_pid, time_t t, unsigned int mii)
{
	if (size > PIDENVID_ENVID_SIZE) {
		return PIDENVID_OVERSIZED;
	}

	/* Here is the representation which will end up in an environment 
		somewhere */
	sprintf(dest, "%s%d%s%lu=%d%s%lu%s%u", 
		PIDENVID_PREFIX, forker_pid, PIDENVID_SEP, ft,
		forked_pid, PIDENVID_SEP, t, PIDENVID_SEP, mii);

	return PIDENVID_OK;
}

/* given a pidenvid entry, convert it back to numbers */
int pidenvid_format_from_envid(char *src, pid_t *forker_pid, time_t *ft,
		pid_t *forked_pid, time_t *t, unsigned int *mii)
{
	int rval;

	/* rip out the values I care about and into the arguments */
	rval = sscanf(src, 
		/* the prefix string */
		PIDENVID_PREFIX 

		/* the parent pid */
		"%d" 

		/* a separater character */
		PIDENVID_SEP 

		/* the parent birth day */
		"%lu"
		
		/* an equals sign */
		"="

		/* The child's pid */
		"%d" 

		/* a separator character */
		PIDENVID_SEP 

		/* the child (approximate) birth time */
		"%lu"

		/* A separator character */
		PIDENVID_SEP 
		
		/* the random number */
		"%u", 

		forker_pid, ft, forked_pid, t, mii);

	/* There are 5 things I'm taking out of the string, make sure I get them */
	if (rval == 5) {
		return PIDENVID_OK;
	}

	/* One reason why we might fail here is because over time, 
		this format string has changed and there could
		be two different versions of Condor on the same
		machine with the takesnapshot code seeing each other's
		processes. Unfortunately, no version information is kept
		inside of the environment variables. Maybe in the future
		I'll add something, but I don't think this'll have to
		change again for a very long time. */
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
	pid_t forker_pid, time_t ft, pid_t forked_pid, time_t t, unsigned int mii)
{
	int rval;
	char buf[PIDENVID_ENVID_SIZE];

	rval = pidenvid_format_to_envid(buf, PIDENVID_ENVID_SIZE, 
				forker_pid, ft, forked_pid, t, mii);

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

	to->empty = from->empty;

	to->num = from->num;

	for (i = 0; i < from->num; i++) {
		to->ancestors[i].active = from->ancestors[i].active;
		if (from->ancestors[i].active == TRUE) {
			strcpy(to->ancestors[i].envid, from->ancestors[i].envid);
		}
	}
}

/* Be aware that this function can break the guarantee that there are only
	contiguous TRUE entries starting at index 0 if the ancestor penvid.
	So if you use this function make sure when you're done it is left in 
	a valid format. */
void pidenvid_copy_env(PidEnvID *to, int idxto, PidEnvID *from, int idxfrom)
{
	to->ancestors[idxto].active = from->ancestors[idxfrom].active;
	if (from->ancestors[idxto].active == TRUE) {
		strcpy(to->ancestors[idxto].envid, from->ancestors[idxfrom].envid);
	}
}

/* emit the structure to the logs at the debug level specified */
void pidenvid_dump(PidEnvID *penvid, unsigned int dlvl)
{
	int i;

	dprintf(dlvl, "PidEnvID: There are %d entries total.\n", penvid->num);
	dprintf(dlvl, "The PidEnvID structure %s empty.\n", 
		penvid->empty==TRUE?"is":"is not");

	for (i = 0; i < penvid->num; i++) {
		/* only print out true ones */
		if (penvid->ancestors[i].active == TRUE) {
			dprintf(dlvl, "\t[%d]: active = %s\n", i,
				penvid->ancestors[i].active == TRUE ? "TRUE" : "FALSE");
			dprintf(dlvl, "\t\t%s\n", penvid->ancestors[i].envid);
		}
	}
}


int pidenvid_empty(PidEnvID *penvid)
{
	return penvid->empty == TRUE ? TRUE : FALSE;
}

/* search the ancestor variables until I find the oldest ancestor. This
	function will set pid to 0 if the PidEnvID is empty. While it
	always appears that the oldest ancestor is at index 0 in the
	PidEnvID given manual inspection, the ordering is undefined and
	probably OS dependant, so we do a little algorithm to find the
	oldest ancestor. This algorithm assumes that the ancestor chain
	is not broken in the environment, and this should be the state
	of affairs at all times. The loc variable is the location in the pidenv
	index array that one can use to copy an environment string to another
	penvid. */
void pidenvid_find_oldest_ancestor(PidEnvID *penvid, pid_t *pid, time_t *bdate,
	int *loc)
{
	int done = FALSE;
	pid_t forker_pid;
	time_t ft;
	pid_t forked_pid;
	time_t t;
	unsigned int mii;

	pid_t ancestor;
	time_t bd;
	int location;
	int i;

	if (penvid->empty == TRUE) {
		/* nothing to find, return pid 0, which nothing can ever be */
		*pid = 0;
		return;
	}

	/* pick the first one out of the environment and start with that */
	pidenvid_format_from_envid(penvid->ancestors[0].envid, 
		&ancestor, &bd, &forked_pid, &t, &mii);

	/* every time I find the ancestor on the right hand side, set the ancestor
		to the pid that forked it. */

	start_over:
	while (done == FALSE) {
		/* search for the ancestor on the rhs */
		for (i = 0; i < PIDENVID_MAX; i++) {
			if (penvid->ancestors[i].active == FALSE) {
				/* I'm at the end, so this means I didn't find the acenstor
					on the RHS. Think recursion base case... */
				break;
			}

			pidenvid_format_from_envid(penvid->ancestors[i].envid, 
				&forker_pid, &ft, &forked_pid, &t, &mii);
			
			/* if I find the ancestor on the rhs, it means that it also
				has an ancestor, so keep track of the new ancestor's
				information and restart the search for an older one. */
			if (ancestor == forked_pid) {
				location = i;
				ancestor = forker_pid;
				bd = ft;

				/* found an older ancestor, so start over the search with it */
				goto start_over;
			}
		}

		/* if I searched the whole of the PidEnvID and did not find 
			the ancestor on the RHS, then whatever ancestor is currently
			set to is the oldest ancestor. */
		done = TRUE;
	}

	/* the pid of the oldest ancestor */
	*pid = ancestor;
	/* The birthdate of the oldest ancestor */
	*bdate = bd;
	/* the location in this penvid of the environment variable which is the
		ancestor */
	*loc = location;
}







