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
