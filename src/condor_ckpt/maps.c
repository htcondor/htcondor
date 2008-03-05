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
#include "condor_mmap.h"
#include "condor_debug.h"
#include "condor_open.h"

#include "maps.h"

void segment_parse(Segment *seg, char *line)
{
	char tmp[8192];	/* this number is hardcoded due to the sscanf fmt string */
	char *wspc = NULL;

	memset(tmp, '\0', 8192);

	/* seg->t is shared memory or private (copy on write) */

#if defined(LINUX)
	/* the sscanf() might screw with what libraries are loaded or not.
		I have yet to determine if this is true--since I don't want to do this
		parsing of the line by hand. */
	seg->path[0] = '\0';

	sscanf(line, "%llx-%llx %c%c%c%c %llx %lx:%lx %llu%8192[^\n]",
		&seg->mem_start, &seg->mem_end,
		&seg->r, &seg->w, &seg->x, &seg->t,
		&seg->offset, 
		&seg->major, &seg->minor,
		&seg->inode,
		tmp);
	
	/* null terminate, just in case I read the full amount */
	tmp[8191] = '\0';
	
	seg->flags = 
		(seg->r=='r'?PROT_READ:0) |
		(seg->w=='w'?PROT_WRITE:0) |
		(seg->x=='x'?PROT_EXEC:0);
	
	/* remove any whitespace infront of the path */
	wspc = tmp;
	while(*wspc != '\0' && isspace(*wspc)) {
		wspc++;
	}

	/* sanity check */
	if (strlen(wspc) >= PATH_MAX) {
		EXCEPT("segment_parse(): path is too large!\n");
	}

	strcpy(seg->path, wspc);

#endif

}

int segment_table_init(SegmentTable *st, pid_t pid)
{
	char buf[128];
	int fd;
	char table[MAX_TAB_SIZE];
	int num_read, total_read;
	int i;
	char *ptr;

	st->num_avail_segs = 0;

#if	defined(LINUX)

	/* determine where my mapfile is */
	sprintf(buf, "/proc/%u/maps", pid);
	fd = safe_open_wrapper(buf, O_RDONLY, 0600);
	if (fd == -1)
	{
		dprintf(D_ALWAYS, "Could not open maps file! %d(%s)\n",
			errno, strerror(errno));
		return -1;
	}
	
	/* read the table, avoid using stdio to process stuff so it doesn't
		allocate more segments that might not have existed when I read the
		map table */
	memset(table, '\0', MAX_TAB_SIZE);
	num_read = 0;
	ptr = table;
	total_read = 0;

	num_read = read(fd, ptr, MAX_TAB_SIZE);
	total_read = num_read;
	while(num_read != 0 && num_read != -1)
	{
		num_read = read(fd, ptr += num_read, MAX_TAB_SIZE - total_read);
		total_read += num_read;
	}

	close(fd);
	if (num_read == -1)
	{
		dprintf(D_ALWAYS, "Error while reading: %d(%s)\n", 
			errno, strerror(errno));
		return -1;
	}

	/* count the number of newlines, which represents the number of fully
		parseable segments found. If the segment map is _much_ larger than
		the MAX_TAB_SIZE, then the partial segment line, plus anything after
		it will be lost in the segment map... 
		*/
	for (i = 0; i < total_read; i++) {
		if (table[i] == '\n') {
			st->num_avail_segs++;
		}
	}

	/* bail if there are more parseable segments than table space to 
		represent it.
	*/
	if (st->num_avail_segs > MAX_SEGS) {
		return -1;
	}

	/* ok, now parse each line out knowing how many lines I'm going to use */
	ptr = table;
	for (i = 0; i < st->num_avail_segs; i++)
	{
		segment_parse(&st->segs[i], ptr);

		/* move to the next map line */
		while (*ptr++ != '\n');
	}

#endif

	return 0;
}

void segment_table_stdout(SegmentTable *st)
{
	int i;

	dprintf(D_ALWAYS, "Printing out the segment table:\n");
	if (st == NULL) {
		dprintf(D_ALWAYS, "NULL Table\n");
		return;
	}

	for (i = 0; i < st->num_avail_segs; i++)
	{
		dprintf(D_ALWAYS, "Segment: %d\n", 
			i);
		dprintf(D_ALWAYS, "\tStart:   0x%08llx\n",
			st->segs[i].mem_start);
		dprintf(D_ALWAYS, "\tEnd:     0x%08llx\n",
			st->segs[i].mem_end);
		dprintf(D_ALWAYS, "\tSize:    %llu\n",
			st->segs[i].mem_end - st->segs[i].mem_start);
		dprintf(D_ALWAYS, "\tPerms:   %c%c%c%c\n", 
			st->segs[i].r, st->segs[i].w, st->segs[i].x, st->segs[i].t);
		dprintf(D_ALWAYS, "\tOffset:  0x%08llx\n",
			st->segs[i].offset);
		dprintf(D_ALWAYS, "\tMaj/Min: %02lx:%02lx\n", 
			st->segs[i].major, st->segs[i].minor);
		dprintf(D_ALWAYS, "\tInode:   %llu\n",
			st->segs[i].inode);
		dprintf(D_ALWAYS, "\tPath:    %s\n",
			st->segs[i].path[0]=='\0'?"<none>":st->segs[i].path);
	}
}

int segment_table_find_vdso(SegmentTable *st)
{
	int i;

#if	defined(LINUX)
	for (i = 0; i < st->num_avail_segs; i++) {
		/* Check for something with exactly [vdso] in it */
		if (strncmp(st->segs[i].path, "[vdso]", 6) == 0 && 
			strlen(st->segs[i].path) == 6) 
		{
			return i;
		}

#if defined(I386)
		/* Check for the old kind at a known location */
		if (st->segs[i].mem_start == 0x00000000ffffe000ULL) 
		{
			return i;
		}
#endif

	}

#endif

	return NOT_FOUND;
}

int segment_table_fix_vsyscall_page_perms(SegmentTable *st)
{
	int vdso_idx;

	vdso_idx = segment_table_find_vdso(st);
	if (vdso_idx == NOT_FOUND) {
		return NO_VDSO_SEGMENT;
	}

	if (mprotect((char*)st->segs[vdso_idx].mem_start, 
		st->segs[vdso_idx].mem_end - st->segs[vdso_idx].mem_start, 
		PROT_EXEC|PROT_READ|PROT_WRITE) < 0) 
	{
		return CANT_MPROTECT;
	}

	/* now update our view of the table */

	st->segs[vdso_idx].r = 'r';
	st->segs[vdso_idx].w = 'w';
	st->segs[vdso_idx].x = 'x';

	st->segs[vdso_idx].flags = PROT_READ | PROT_WRITE | PROT_EXEC;

	return SUCCESS;
}


