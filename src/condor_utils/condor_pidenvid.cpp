/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_pidenvid.h"

DLL_IMPORT_MAGIC extern char **environ;

/* set the structure to default values */
void pidenvid_init(PidEnvID *penvid)
{
	/* zero out the whole struct */
	memset(penvid, 0, sizeof(PidEnvID));

	/* all of these structures in Condor will be of this size */
	penvid->num = PIDENVID_MAX;
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

			strncpy(penvid->ancestors[i].envid, *curr, PIDENVID_ENVID_SIZE);
			penvid->ancestors[i].envid[PIDENVID_ENVID_SIZE-1] = '\0';
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

			strncpy(penvid->ancestors[i].envid, line, PIDENVID_ENVID_SIZE);
			penvid->ancestors[i].envid[PIDENVID_ENVID_SIZE-1] = '\0';
			penvid->ancestors[i].active = TRUE;

			return PIDENVID_OK;
		}
	}
	
	return PIDENVID_NO_SPACE;
}

/* given a left and right hand side, put it into the buffer specified
	and ensure it fits correctly */
int
pidenvid_format_to_envid( char *dest, unsigned size,
						  pid_t forker_pid, pid_t forked_pid,
						  time_t t, unsigned int mii )
{
	if ( size > PIDENVID_ENVID_SIZE) {
		return PIDENVID_OVERSIZED;
	}

	/* Here is the representation which will end up in an environment 
		somewhere. time_t doesn't always match %lu, so coerce it*/
	sprintf(dest, "%s%d=%d%s%lu%s%u", 
			PIDENVID_PREFIX, forker_pid, 
			forked_pid, PIDENVID_SEP, (long)t, PIDENVID_SEP, mii);

	return PIDENVID_OK;
}

/* given a pidenvid entry, convert it back to numbers */
int pidenvid_format_from_envid(char *src, pid_t *forker_pid, 
		pid_t *forked_pid, time_t *t, unsigned int *mii)
{
	int rval;
	long tmpt = (long)(*t); // because time_t doesn't always match %lu

	/* rip out the values I care about and into the arguments */
	rval = sscanf(src, 
		PIDENVID_PREFIX "%d=%d" PIDENVID_SEP "%lu" PIDENVID_SEP "%u", 
		forker_pid, forked_pid, &tmpt, mii);
	*t = tmpt;
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
			if (strncmp(left->ancestors[l].envid, right->ancestors[r].envid,
						PIDENVID_ENVID_SIZE) == 0) {
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
			strncpy(to->ancestors[i].envid, from->ancestors[i].envid, PIDENVID_ENVID_SIZE);
			to->ancestors[i].envid[PIDENVID_ENVID_SIZE-1] = '\0';
		}
	}
}

/* emit the structure to the logs at the debug level specified */
void pidenvid_dump(PidEnvID *penvid, int dlvl)
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

void
pidenvid_shuffle_to_front(char **env)
{
	size_t count;
	size_t prefix_len = strlen( PIDENVID_PREFIX );
	int changed;

	for( count=0; env[count]; count++ ) { }

	if( count == 0 ) {
		return; // no environment
	}

		// March through backwards and bubble up any pidenvid entries.
		// This preserves the order of other stuff in the environment,
		// in case it matters.

	do {
		size_t i;

		changed = 0;
		for( i=count-1; i; i-- ) {
			if( !strncmp(env[i],PIDENVID_PREFIX,prefix_len) ) {
				size_t j;
					// We found a pidenv entry.  Bubble it up until we
					// hit the beginning of the array or another
					// pidenvid entry.
				for( j=i; j--; ) {
					char *prev = env[j];
					if( !strncmp(prev,PIDENVID_PREFIX,prefix_len) ) {
						break; // we hit another pidenvid entry
					}
						// swap positions
					changed = 1;
					env[j] = env[i];
					env[i] = prev;
					i = j;
				}
				if( i==0 ) {
					break;
				}
			}
		}

	} while( changed );
}

void
pidenvid_optimize_final_env(char **env)
{
#if defined(LINUX)
	pidenvid_shuffle_to_front(env);
#else
	if (env) {}
#endif
}
