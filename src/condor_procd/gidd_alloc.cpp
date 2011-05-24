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

// this program allocates a GID for tracking purposes using an "ethernet"-
// style approach. it:
//   1) scans the system for unsed GIDs
//   2) selects one and adds it to its own supplementary group list
//   3) scans the system again expecting to see only itself using the GID
//   4) if no collision has occurred, it is done
//   5) if a collision has occurred, it retries

#include "condor_common.h"

/* Apparently, in condor_sys_linux.h, we can't include <grp.h> as per the
	comment that it messes about with standard universe. So instead
	of trying to fix the larger problem, I'm just going to slam it in right
	here. This also forces the compilation of this program to only happen
	on Linux platforms. This is because setgroups() requires priviledge and
	therefore isn't in POSIX.1-2001 so I can't rely on it being in all OSes
	at this time. When this feature is desired on other platforms, it can be
	fixed then. I suspect all platforms actually support it, but may have
	differing prototypes and be defined in different header files.
		-psilord 12/15/2010
*/
#include <grp.h>

#define ERR_STRLEN 255

char err_str[ERR_STRLEN + 1];

void
err_sprintf(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(err_str, ERR_STRLEN + 1, fmt, ap);
	va_end(ap);
}


// helper function to create a path of the form: /proc/<pid>/status. returns
// NULL if a failure occurs, otherwise a malloc-allocated buffer with the
// path is returned
//
static char*
statuspath(char* pid)
{
	char proc[] = "/proc/";
	char status[] = "/status";
	int proclen = sizeof(proc) - 1;
	int pidlen = strlen(pid);
	int statuslen = sizeof(status) - 1;

	char* path = (char*)malloc(proclen + pidlen + statuslen + 1);
	if (path == NULL) {
		err_sprintf("malloc failure: %s", strerror(errno));
		return NULL;
	}

	memcpy(path, proc, proclen);
	memcpy(path + proclen, pid, pidlen);
	memcpy(path + proclen + pidlen, status, statuslen);
	path[proclen + pidlen + statuslen] = '\0';

	return path;
}

// helper type for a node in a list of GIDs
//
typedef struct gidnode {
	gid_t gid;
	struct gidnode* next;
} gidnode_t;

// helper type to build up a list of GIDs incrementally
//
typedef struct {
	gidnode_t* head;
	int count;
} gidlist_t;

// initialize a gidlist_t
//
static void
listinit(gidlist_t* list) {
	list->head = NULL;
	list->count = 0;
}

// add a GID to a gidlist_t. returns 0 on success, -1 on failure
//
static int
listadd(gidlist_t* list, gid_t gid)
{
	gidnode_t* node = (gidnode_t*)malloc(sizeof(gidnode_t));
	if (node == NULL) {
		err_sprintf("malloc failure: %s", strerror(errno));
		return -1;
	}
	node->gid = gid;
	node->next = list->head;
	list->head = node;
	list->count++;
	return 0;
}

// free up any memory being used by a gidlist_t
//
static void
listfree(gidlist_t* list)
{
	gidnode_t* node = list->head;
	while (node != NULL) {
		gidnode_t* next = node->next;
		free(node);
		node = next;
	}
}

// helper function to get the set of supplementary group IDs for a given
// PID. returns 0 on success, -1 on failure. on success, the list parameter
// will return a gidlist_t with all the GIDs
//
static int
getgids(char* pid, gidlist_t* list)
{
	char* path = statuspath(pid);
	if (path == NULL) {
		return -1;
	}

	FILE* fp = fopen(path, "r");
	if (fp == NULL) {
		err_sprintf("fopen failure on %s: %s", path, strerror(errno));
		free(path);
		return -1;
	}

	int found = 0;
	char grps[] = "Groups:\t";
	int grpslen = sizeof(grps) - 1;
	char line[1024];
	while (fgets(line, sizeof(line), fp)) {
		if (strncmp(line, grps, grpslen) == 0) {
			found = 1;
			break;
		}
	}

	if (!found) {
		if (feof(fp)) {
			err_sprintf("groups not found in %s", path);
		}
		else {
			err_sprintf("fgets error on %s: %s",
			            path,
			            strerror(errno));
		}
		free(path);
		fclose(fp);
		return -1;
	}
	fclose(fp);

	int linelen = strlen(line);
	if (line[linelen - 1] != '\n') {
		err_sprintf("partial read of groups line in %s", path);
		free(path);
		return -1;
	}

	listinit(list);
	char* ptr = line + grpslen;
	while (*ptr != '\n') {
		char* end;
		gid_t gid = (gid_t)strtoul(ptr, &end, 10);
		if ((end == ptr) || (*end != ' ')) {
			err_sprintf("unexpected format for groups in %s",
			            path);
			free(path);
			listfree(list);
			return -1;
		}
		if (listadd(list, gid) == -1) {
			free(path);
			listfree(list);
			return -1;
		}
		ptr = end + 1;
	}
	free(path);

	return 0;
}

int
giduse_probe(gid_t first, int count, int* used)
{
	int i;

	DIR* dir = opendir("/proc");
	if (dir == NULL) {
		err_sprintf("opendir failure on /proc: %s", strerror(errno));
		return -1;
	}

	for (i = 0; i < count; i++) {
		used[i] = 0;
	}

	while (1) {

		struct dirent* de = readdir(dir);
		if (de == NULL) {
			break;
		}

		if (!isdigit(de->d_name[0])) {
			continue;
		}

		gidlist_t list;
		if (getgids(de->d_name, &list) == -1) {
			continue;
		}

		gidnode_t* node = list.head;
		while (node != NULL) {
			i = node->gid - first;
			if ((i >= 0) && (i < count)) {
				used[i]++;
			}
			node = node->next;
		}

		listfree(&list);
	}

	closedir(dir);

	return 0;
}


static gid_t
parsegid(char* str)
{
	if (!isdigit(str[0])) {
		return 0;
	}
	char* end;
	gid_t gid = (gid_t)strtoul(str, &end, 10);
	if (*end != '\0') {
		return 0;
	}
	return gid;
}

int
main(int argc, char* argv[])
{
	int i;

	if (argc != 3) {
		fprintf(stderr, "usage: gidd_alloc <min_gid> <max_gid>\n");
		return 1;
	}
	gid_t min = parsegid(argv[1]);
	if (min == 0) {
		fprintf(stderr,
		        "invalid value for minimum GID: %s\n",
		        argv[1]);
		return 1;
	}
	gid_t max = parsegid(argv[2]);
	if (max == 0) {
		fprintf(stderr,
		        "invalid value for maximum GID: %s\n",
		        argv[2]);
		return 1;
	}
	if (min > max) {
		fprintf(stderr,
		        "invalid GID range given: %u - %u\n",
		        (unsigned)min,
		        (unsigned)max);
		return 1;
	}

	int count = max - min + 1;
	int used[count];
	if (giduse_probe(min, count, used) == 1) {
		fprintf(stderr, "giduse_probe error: %s", err_str);
		return 1;
	}

	gid_t gid = 0;
	for (i = 0; i < count; i++) {
		if (used[i] == 0) {
			gid = min + i;
			break;
		}
	}
	if (gid == 0) {
		fprintf(stderr, "no GIDs available\n");
		return 1;
	}

	if (setgroups(1, &gid) == -1) {
		fprintf(stderr, "setgroups failure: %s\n", strerror(errno));
		return 1;
	}

	int n;
	if (giduse_probe(gid, 1, &n) == -1) {
		fprintf(stderr,
		        "giduse_probe error checking GID %u: %s\n",
		        (unsigned)gid,
		        err_str);
		return 1;
	}
	if (n != 1) {
		fprintf(stderr, "collision on GID %u\n", (unsigned)gid);
		return 1;
	}

	printf("%u\n", (unsigned)gid);
	fclose(stdout);

	pause();

	return 0;
}
