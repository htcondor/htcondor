/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#define STAT_SIZE sizeof(struct stat)
#define STATFS_SIZE sizeof(struct statfs)
#define GID_T_SIZE sizeof(gid_t)
#define INT_SIZE sizeof(int)
#define LONG_SIZE sizeof(long)
#define FD_SET_SIZE sizeof(fd_set)
#define TIMEVAL_SIZE sizeof(struct timeval)
#define TIMEVAL_ARRAY_SIZE (sizeof(struct timeval) * 2)
#define TIMEZONE_SIZE sizeof(struct timezone)
#define FILEDES_SIZE sizeof(struct filedes)
#define RLIMIT_SIZE sizeof(struct rlimit)
#define UTSNAME_SIZE sizeof(struct utsname)
#define POLLFD_SIZE sizeof(struct pollfd)
#define RUSAGE_SIZE sizeof(struct rusage)
#define MAX_STRING 1024
#define EIGHT 8
#define STATFS_ARRAY_SIZE (rval * sizeof(struct statfs))
#define PROC_SIZE sizeof(PROC)
#define STARTUP_INFO_SIZE sizeof(STARTUP_INFO)
#define SIZE_T_SIZE sizeof(size_t)
#define U_SHORT_SIZE sizeof(u_short)
#define U_INT_SIZE sizeof(u_int)
#define NEG_ONE -1
