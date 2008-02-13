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


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_pers.h"

#include "../condor_ckpt/maps.h"

/* must be global instead of an object so I do as little memory 
	accesses in the heap as possible. This stuff is in bss. */
SegmentTable g_segment_table;
SegmentTable *g_st = &g_segment_table;

void usage(char *argv0);
int matches(char *arg, char *thing);

void usage(char *argv0)
{
	dprintf(D_ALWAYS, "%s <options>\n", argv0);

	dprintf(D_ALWAYS,

" --vdso-addr        Emit the vsyscall page location if applicable\n"
" --segments         Emit a map of all loaded segments\n"
" --no-pers-change   [Linux Only] Don't change the personality of the process\n"

	);

	exit(EXIT_FAILURE);
}

/* return 1 if the two exactly match, 0 if not */
int matches(char *arg, char *thing)
{
	if (strncmp(arg, thing, strlen(thing)) == 0) {
		return 1;
	}

	return 0;
}

int main(int argc, char *argv[], char *envp[])
{
	unsigned long long vdso_idx;
	int i;
	int pers_change = 1; /* assume yes */
	char **new_args;
	char my_exec[PATH_MAX];

	Termlog = 1;
	dprintf_config("TOOL");
	set_debug_flags("D_ALWAYS");
	set_debug_flags("D_NOHEADER");

	/* must supply at least some options */
	if (argc < 2) {
		usage(argv[0]);
	}
	

#if defined(LINUX)
	/* see if I should rexec myself: if --no-pers-change isn't there, then
		perform the rexec process */
	for (i = 1; i < argc; i++) {
		if (matches(argv[i], "--no-pers-change")) {
			pers_change = 0;
			break;
		}
	}

	if (pers_change) {
		/* add one for the new argument, and one for NULL */
		new_args = (char**)malloc(sizeof(char*) * (argc + 2));
		if (new_args == NULL) {
			dprintf(D_ALWAYS, "Out of memory! Exiting.\n");
			exit(EXIT_FAILURE);
		}

		/* rebuild a new arguments list and add --no-pers-change into it */
		new_args[0] = strdup(argv[0]);
		new_args[1] = strdup("--no-pers-change");
		for (i = 1; i < argc; i++) {
			new_args[1+i] = strdup(argv[i]);
		}
		new_args[1+i] = NULL;

		patch_personality();

		execve("/proc/self/exe", new_args, envp);

		dprintf(D_ALWAYS, "execve failed: errno %d(%s)\n",
			errno, strerror(errno));

		exit(EXIT_FAILURE);
	}
#endif

	segment_table_init(g_st, getpid());

	for (i = 1; i < argc; i++) {
		if (matches(argv[i], "--vdso-addr")) {
			vdso_idx = segment_table_find_vdso(g_st);

			if (vdso_idx == NOT_FOUND) {
				dprintf(D_ALWAYS, "VDSO: N/A\n");
				return EXIT_FAILURE;
			}
			dprintf(D_ALWAYS, "VDSO: 0x%llx\n", g_st->segs[vdso_idx].mem_start);

		} else if (matches(argv[i], "--segments")) {
			segment_table_stdout(g_st);
		} else if (matches(argv[i], "--no-pers-change")) {
			/* ignore */
		} else {
			usage(argv[0]);
		}
	}

	return EXIT_SUCCESS;
}
