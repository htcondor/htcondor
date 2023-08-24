/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

#ifndef CONDOR_PIDENVID
#define CONDOR_PIDENVID

/* used for the prefix of the identification environment variable for pid 
	tracking. This must be a string since sizeof() is used to get its length at
	compile time later. */
#define PIDENVID_PREFIX "_CONDOR_ANCESTOR_"

/* the separator character used in the RHS, must be a string and ONE
	character. */
#define PIDENVID_SEP ":"

/* the usual number in an array of PidEnvID structures, this is good enough
	for a 32 process ancestor chain through daemoncore Create_Process. */
#define PIDENVID_MAX 32

/* the valid size of the entire envid */
#define PIDENVID_ENVID_SIZE \
		/* compiler computed space to represent PIDENVID_PREFIX. \
			NOTE: This method includes the NUL character already */ \
		(sizeof(PIDENVID_PREFIX) + \
\
		/* LHS */ \
		/* space to represent the ascii forker pid number \
			(account for pos/neg sign too with the + \
			1 at the end since some architecture allow negative \
			pid numbers. */ \
		((sizeof(pid_t) * 2) + (sizeof(pid_t) / 2) + 1) + \
\
		/* equal sign */ \
		1 + \
\
		/* RHS */ \
\
		/* space to represent the ascii forked child pid number */ \
		((sizeof(pid_t) * 2) + (sizeof(pid_t) / 2) + 1) + \
\
		/* the separator character */ \
		1 + \
\
		/* space to represent the full possible ascii digits of a \
			time_t */ \
		((sizeof(time_t) * 2) + (sizeof(time_t) / 2)) + \
\
		/* the separator character */ \
		1 + \
\
		/* space to represent the full possible ascii digits of a \
			uint */ \
		((sizeof(unsigned int) * 2) + (sizeof(unsigned int) / 2)) + \
\
		/* the null character (which is already accounted for above) */ \
		0)


/* return codes */
enum {
	/* if everything went fine during an insert or append */
	PIDENVID_OK,
	/* if there isn't enough room to insert or append into the PidEnvID */
	PIDENVID_NO_SPACE,
	/* if the thing you are trying to insert into the PidEnvID is larger
		than the allotted space for the envid */
	PIDENVID_OVERSIZED,
	/* failed to perform a conversion from an envid back to component pieces */
	PIDENVID_BAD_FORMAT
};

/* did a pidentry match an environment? */
enum {
	PIDENVID_MATCH,
	PIDENVID_NO_MATCH
};

typedef struct PidEnvIDEntry_s {

	/* whether or not this pid entry is actively being used at this
		time. */
	bool active;

	/* The structure that this structure is being embedded in doesn't have
		any allocated memory in it, and therefore no code that deals it
		deallocates it in any meaninful way to get rid of any memory that
		this structure might dynamically allocate. So, I have determined the
		maximum size which could be contained in this variable and made
		a static size for it. This causes no code changes to try and 
		deallocate this space when dealing with the structure that this
		structure is embedded in.
	*/
	char envid[PIDENVID_ENVID_SIZE];

} PidEnvIDEntry;

typedef struct PidEnvID_s
{
	/* the initializaer function will set this field to PIDENVID_MAX */
	int num;
	PidEnvIDEntry ancestors[PIDENVID_MAX];

} PidEnvID;

/* initialize the structure above */
void pidenvid_init(PidEnvID *penvid);

/* Take an already specified environment array, and look through it
	while recording things prefixed with PIDENVID_PREFIX into the PidEnvID.
	This function assumes 'env' will have a NULL value for the last entry. */
int pidenvid_filter_and_insert(PidEnvID *penvid, char **env);

/* create an envid string */
int pidenvid_format_to_envid(char *dest, unsigned size, 
	pid_t forker_pid, pid_t forked_pid, time_t t, unsigned int mii);

/* expand an envid string into the component pieces */
int pidenvid_format_from_envid(char *src, 
		pid_t *forker_pid, pid_t *forked_pid, time_t *t, unsigned int *mii);

/* insert a specific environment variable into the penvid */
int pidenvid_append(PidEnvID *penvid, char *line);

/* wrapper around pidenvid_format_to_envid() and pidenvid_append() to combine
	those two into one easy to use function */
int pidenvid_append_direct(PidEnvID *penvid,
	pid_t forker_pid, pid_t forked_pid, time_t t, unsigned int mii);

/* check to see the left PidEnvID matches (is a subset of) the right PidEnvID */
int pidenvid_match(PidEnvID *left, PidEnvID *right);

/* copy the from into the to */
void pidenvid_copy(PidEnvID *to, PidEnvID *from);

/* dump out the structure to the debugging logs */
void pidenvid_dump(PidEnvID *penvid, int dlvl);

/* Move pidenvid entries near the front of the environment array,
   because linux only lets us see the first 4k of the environment in
   /proc/<pid>/environ.  Unfortunately, just because we order things
   this way in the process that we spawn doesn't mean it will order
   things this way in the children that it creates (for example,
   /bin/bash appears to completely reorder everything when it spawns
   children).
*/
void pidenvid_shuffle_to_front(char **env);

/* do optimizations of the environment array to increases chances of
   tracking by pidenvid entries working
   (e.g. this calls pidenvid_shuffle_to_front() under Linux)
*/
void pidenvid_optimize_final_env(char **env);

#endif


