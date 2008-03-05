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


#ifndef MAPS_H
#define MAPS_H

#define MAX_SEGS 2048
#define MAX_TAB_SIZE (128 * 1024)

enum
{
	NOT_FOUND = -1,
	SUCCESS = 0,
	CANT_MPROTECT,
	NO_VDSO_SEGMENT
};

/* The reasons why there are no pointers to heap in here for things like the
	path field is that heap might not exist when we'd have to be looking
	at this structure. This, of course, is only valid for a stduniv job which
	is utilizing this information.
*/
typedef struct Segment_s {
	unsigned long long	mem_start;
	unsigned long long	mem_end;
	unsigned long long	offset;
	char			r, w, x, t;
	unsigned long	flags;
	unsigned long	major;
	unsigned long	minor;
	unsigned long long	inode;
	char			path[PATH_MAX];
} Segment;

typedef struct SegmentTable_s
{
	int num_avail_segs;
	Segment segs[MAX_SEGS];
} SegmentTable;

void segment_parse(Segment *seg, char *line);

int segment_table_init(SegmentTable *st, pid_t pid);
void segment_table_stdout(SegmentTable *st);
/* return an index to the vdso segment or -1 if not found */
int segment_table_find_vdso(SegmentTable *st);

int segment_table_fix_vsyscall_page_perms(SegmentTable *st);

#endif
