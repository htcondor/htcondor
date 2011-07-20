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

/* This file sets up some definitions in order to create a private malloc
	implementation suitable for use by the checkpointing library to make a
	private heap.  This is *not* the malloc called by ordinary user code.
	Its use is primarily by zlib when it compressed/uncompresses checkpoints.
	The privacy is enforced at the symbol namespace level via the
	MALLOC_SYMBOL #define that is part of the malloc.c interface.
*/


/* First we check to make sure we're being compiled properly.
	Traditionally, you can use the values:
	-DMORECORE=condor_morecore
	-DHAVE_MMAP=0
	-Dmalloc_getpagesize=8192

	The first states to use the function condor_morecore() instead of sbrk().
	The second states that MORECORE should *always* be called instead of mmap().
	The third dictates how big the pages are wrt to the allocator.
*/
#if !defined(MORECORE) || !defined(HAVE_MMAP) || !defined(malloc_getpagesize)
#error You should be defining MORECORE, HAVE_MMAP, and malloc_getpagesize on the compilation line for this file!
#endif

/* give a prototype of the morecore function for the allocator */
extern void* MORECORE(int);

/* We use this function to figure out whatever the pagesize was that we told
	the allocator to use. This allows up to later compute the correct size
	of our alternate heap. The prototype is in image.cpp.
*/
int condor_malloc_getpagesize(void)
{
	return malloc_getpagesize;
}

/* Change all usual malloc calls to be prefixed with condor_. The ckpt
	library uses this new interface when performing compressed checkpoints.
*/
#define MALLOC_SYMBOL(x) condor_##x

#include "malloc.c"



