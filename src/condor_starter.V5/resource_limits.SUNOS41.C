
/* 
** Copyright 1992 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include "proto.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

#define MEG (1024 * 1024)


void
set_resource_limits()
{
	limit( RLIMIT_CPU, RLIM_INFINITY );
	limit( RLIMIT_FSIZE, RLIM_INFINITY );
	limit( RLIMIT_DATA, RLIM_INFINITY );
	limit( RLIMIT_RSS, RLIM_INFINITY );

/*
	The stack segment in the sun core file always has a virtual size which
	is the same as the stack size limit in force at the time of the dump.
	This is implemented as a file "hole" so that the actual amount of space
	taken up on disk is only that required to hold the valid part of the
	stack.  Thus the virtual size of the core file is generally much larger
	than the actual size.  This has some unfortunate interactions with
	process limits, see comments below regarding specific limits.
*/

/*
	We would like to unlimit the stack size so that the process can have
	complete flexibility in how it uses its virtual memory resources.
	Unfortunately if we do that, the virtual size of the stack segment
	in the core file will be 2 gigabytes.  For some reason the operating
	system won't produce such a core, even though there is plenty of disk
	to accomodate the actual size of the file.  We therefore set the
	stack limit to a default of 8 megabytes.
*/
	limit( RLIMIT_STACK, 8 * MEG );

/*
	We would like to limit the size of the coredump to something less than
	the actual free disk in the filesystem.  That way we could ensure
	that the system would not try to produce a core that won't fit, overflow
	the file system, and produce ugly messages on the console.  Unfortunately
	if seems that the coredump size limit controls the virtual rather
	than the actual size of the core.  That means if there is 6 Meg
	of free disk and we set the limit accordingly, we would never get
	a core at all, even if the actual size of the core would only be
	2 meg!!  For this reason, we don't limit the core file size on
	Suns.
*/

	limit( RLIMIT_CORE, RLIM_INFINITY );
}
