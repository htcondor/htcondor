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

typedef struct {
	int		num;
	char	*fmt;
} USR_MSG;

enum {
	THIS_RUN,
	TOTAL
};

enum {
	SUBMIT,
	EXECUTE,
	RUN_REMOTE_USAGE,
	RUN_LOCAL_USAGE,
	TOT_REMOTE_USAGE,
	TOT_LOCAL_USAGE,
	IMAGE_SIZE,
	NOT_EXECUTABLE,
	BAD_LINK,
	NORMAL_EXIT,
	ABNORMAL_EXIT,
	CHECKPOINTED,
	NOT_CHECKPOINTED,
	CORE_NAME,
};

static char *MSG_HDR = "%3d (%d.%d.%d) %d/%d %02d:%02d:%02d ";

static
USR_MSG MsgCatalog[] = {
 SUBMIT,			"Submitted from host \"%s\" GMT %+03d%02d",
 EXECUTE,			"Executing on host \"%s\"",
 RUN_REMOTE_USAGE,	"Run Rem: Usr %d %02d:%02d:%02d, Sys %d %02d:%02d:%02d",
 RUN_LOCAL_USAGE,	"Run Loc: Usr %d %02d:%02d:%02d, Sys %d %02d:%02d:%02d",
 TOT_REMOTE_USAGE,	"Tot Rem: Usr %d %02d:%02d:%02d, Sys %d %02d:%02d:%02d",
 TOT_LOCAL_USAGE,	"Tot Loc: Usr %d %02d:%02d:%02d, Sys %d %02d:%02d:%02d",
 IMAGE_SIZE,		"Set Image Size to %d kilobytes",
 NOT_EXECUTABLE,	"Job file not executable",
 BAD_LINK,			"Not properly linked for V5 execution",
 NORMAL_EXIT,		"Exited normally with status %d",
 ABNORMAL_EXIT,		"Killed by signal %d",
 CHECKPOINTED,		"Job was checkpointed",
 NOT_CHECKPOINTED,	"Kicked off without a checkpoint",
 CORE_NAME,			"Core file is %s/core.%d.%d",
 -1,				""
};

#include "condor_constants.h"

typedef struct {
	char		*path;
#if !defined(WIN32)
	uid_t		user_uid, saved_uid;
	gid_t		user_gid, saved_gid;
#endif
	int			cluster;
	int			proc;
	int			subproc;
	int			fd;
	FILE		*fp;
	int			in_block;
	int			locked;
} USER_LOG;

#if defined(__cplusplus)
extern "C" {
#endif

USER_LOG *
	OpenUserLog( const char *own, const char *file, int c, int p, int s );
void CloseUserLog( USER_LOG * lp );
void PutUserLog( USER_LOG *lp,  int, ... );
void BeginUserLogBlock( USER_LOG *lp );
void EndUserLogBlock( USER_LOG *lp );

#if defined(__cplusplus)
}
#endif
