/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

/*
This file sets up some definitions and then builds
a malloc suitable for use by user code.  This object
appears as a member of libc, but blocks checkpointing
signals around memory management calls.
*/

/*
Manually prototype these instead of pulling in the header
file, because the Condor header file madness will confuse
malloc.c badly.
*/

extern void _condor_ckpt_disable();
extern void _condor_ckpt_enable();

static int malloc_preaction()
{
	_condor_ckpt_disable();
	return 0;
}

static int malloc_postaction()
{
	_condor_ckpt_enable();
	return 0;
}

#define MALLOC_PREACTION malloc_preaction()
#define MALLOC_POSTACTION malloc_postaction()

#define USE_PUBLIC_MALLOC_WRAPPERS

/*
Use sbrk exclusively instead of mmap().  This simplifies
management of dynamically-linked executables.
*/

#define HAVE_MMAP 0

/*
On Digital UNIX, the C startup code calls this function to
initialize the mutexes in the malloc code.  With our new
malloc, there is nothing to initialize, so provide a dummy.
*/

#ifdef OSF1
void __malloc_locks_reinit() {}
#endif

#include "malloc.c"
