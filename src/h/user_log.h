/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
