/* 
** Copyright 1998 by the Condor Design Team
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
** Authors:  Mike Litzkow, Jim Pruyne, and Jim Basney
**           University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "afs.h"
#include "shadow.h"
#include "pseudo_ops.h"
#include "exit.h"

static char *_FileName_ = __FILE__;

extern ReliSock *syscall_sock;
extern BaseShadow *Shadow;
extern RemoteResource *thisRemoteResource;

int
pseudo_register_machine_info(char *uiddomain, char *fsdomain)
{
	return 0;
}

int
pseudo_get_job_info(ClassAd *&ad)
{
	ad = Shadow->getJobAd();
	return 0;
}

int
pseudo_get_executable(char *source)
{
	sprintf(source, "%s/cluster%d.ickpt.subproc0", Shadow->getSpool(), 
			Shadow->getCluster());
	return 0;
}

int
pseudo_job_exit(int status, int reason)
{
	dprintf(D_SYSCALLS,"in pseudo_job_exit: status=%d,reason=%d\n",status,
		reason);
	thisRemoteResource->setExitStatus(status);
	thisRemoteResource->setExitReason(reason);
	return 0;
}



