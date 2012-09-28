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

#undef DEBUG
#ifndef _STORE_H_
#define _STORE_H_

#ifdef DEBUG

#ifndef lint
static char *rcs_store_c =
	"$Header: /space/home/matt/CVS2GIT/CONDOR_SRC-src/src/condor_util_lib/malloc.c,v 1.7 2007-11-07 23:21:28 nleroy Exp $";
#endif




#include <stdio.h>
#include "condor_fix_assert.h"

char	*Fmt1 = "%x: %-10s %-35s %-4d %-4d\n";

int printall=1;

FILE *allocout;
#define MAGIC 12348

static struct chunk {
	int magic;
	char *fname;
	int line;
	char *ffname;
	int fline;
	int allocated;
#define ALLOCATED 1
#define FREED 2
#define FORGOTTEN 3
	int size;
	struct chunk *next, *prev;
	char buf[1];
} head;

#define OFFSET (head.buf - (char *)&head)

void exit(), abort(), memset(), perror();
char *sbrk();

char *
mymalloc(fname, line, size)
	char *fname;
	int line, size;
{
	struct chunk *p;
	int s = size + sizeof *p;
	extern int errno;

	/* round up to multiple of 4 */
	s = (s+3) & ~3;
	errno = 0;
#ifdef lint
	p = 0;
#else
	p = (struct chunk *)sbrk(s);
#endif
	if (errno) perror("mymalloc");
	p->magic = MAGIC;
	p->fname = fname;
	p->line = line;
	p->allocated = ALLOCATED;
	p->size = size;
	if (p->next = head.next) p->next->prev = p;
	p->prev = &head;
	head.next = p;

	if (printall && allocout) {
		fprintf(allocout, Fmt1, p->buf, "malloc @", fname, line, size);
		fflush(allocout);
	}

	return p->buf;
}

char *malloc(size)
	unsigned size;
{
	return mymalloc("--malloc--",-1,(int)size);
}

void
myfree(fname, line, ptr)
	char *fname;
	int line;
	char *ptr;
{
	struct chunk *p;

	if(!ptr) {
		if (allocout) {
			fprintf(allocout, ">>>NULL free(%s,%d,0x%x)\n",fname,line,ptr);
			fflush(allocout);
			abort();
		}
		return;
	}
#ifdef lint
	p = (struct chunk *)0;
#else
	p = (struct chunk *)(ptr - OFFSET);
#endif
	if (p->magic != MAGIC) {
		if (allocout) {
			fprintf(allocout, ">>>bad free(%s,%d,0x%x)\n",fname,line,ptr);
			fflush(allocout);
			abort();
		}
		return;
	}
	switch (p->allocated) {
		case ALLOCATED: 
			if (!printall) break;
			if (!allocout) return;
			fprintf(allocout, Fmt1, p->buf, "free @", fname, line, p->size);
			fflush(allocout);
			break;
		case FREED:
			if (!allocout) return;
			fprintf(allocout,
				">>>%s line %d (prev %s %d; alloc %s %d): free already free\n",
				fname,line, p->ffname,p->fline, p->fname, p->line);
			fflush(allocout);
			abort();
			return;
		case FORGOTTEN:
			if (!printall) break;
			if (!allocout) return;
			fprintf(allocout,">>>%s line %d freeing FORGOTTEN %x, size %d\n",
				fname, line, p->buf, p->size);
			fflush(allocout);
			return;
		default:
			if (allocout) {
				fprintf(allocout, ">>>bad allocated %d (%s,%d,0x%x)\n",
					p->allocated,fname,line,ptr);
				fflush(allocout);
				abort();
			}
			return;
	}
		
	p->allocated = FREED;
	p->ffname = fname;
	p->fline = line;
	if (p->prev)
		p->prev->next = p->next;
	if (p->next)
		p->next->prev = p->prev;
	p->next = p->prev = 0;
}

/* ARGSUSED */
void
free(p)
	char *p;
{
	myfree("--free--",-1,p);
}

char *
mycalloc(fname, line, count, size)
	char *fname;
	int line, count, size;
{
	int s = count*size;
	char *res = mymalloc(fname,line,s);

	memset(res,0,s);
	return res;
}

char *
calloc(a,b)
	unsigned a,b;
{
	return mycalloc("--calloc--",-1,(int)a,(int)b);
}


char *
myrealloc(fname, line, ptr, size)
	char *fname;
	int line, size;
	char *ptr;
{
	struct chunk *p;
	char *new;
#ifdef lint
	p = (struct chunk *)0;
#else
	p = (struct chunk *)(ptr - OFFSET);
#endif
	assert(p->buf == ptr);
	new = mymalloc(fname, line, size);
	(void) memcpy(new, ptr, p->size);
	myfree(fname, line, ptr);

	return new;
}

/* ARGSUSED */
char *
realloc(p,n)
	unsigned n;
	char *p;
{
	return myrealloc( "--realloc--", -1, p, n);
}

char *
myalloca(fname, line, size)
	char *fname;
	int line, size;
{
	fprintf(stderr,"alloca(%d) called, line %d file %d\n",
		size,line,fname);
	exit(1);
}

/* ARGSUSED */
char *
alloca(n)
	int n;
{
	abort();
}


showalloc(fname)
	char *fname;
{
	struct chunk *c;
	register int tot=0;
	static int num =0;

	if (!allocout) return;
	fprintf(allocout,"\nshowalloc(%s) %d\n",fname, ++num);
	for (c = head.next; c; c = c->next) {
		if (c->allocated == ALLOCATED)
			fprintf(allocout, Fmt1, c->buf, "", c->fname, c->line, c->size);
		else fprintf(allocout, ">>>bad alloc type %d: %x:\t%s\t%d\t%d\n",
			c->allocated, c, c->fname, c->line, c->size);
		tot += c->size;
	}
	fprintf(allocout,"showalloc %d TOTAL %d\n\n",num, tot);
	(void)fflush(allocout);
}

clearalloc(fname, print_all)
char *fname;
int print_all;
{
	static char buf[BUFSIZ];
	struct chunk *c,*next;

	printall = print_all;
	if (fname) {
		if (allocout) (void)fclose(allocout);
		allocout = fopen(fname,"w");
		if (!allocout) perror(fname);
		else setbuf(allocout, buf);
	}
	for (c = head.next; c; c = next) {
		next = c->next;
		c->allocated = FORGOTTEN;
		c->next = c->prev = 0;
	}

	head.next = 0;
}
#endif /* DEBUG */
#endif /* _STORE_H_ */
