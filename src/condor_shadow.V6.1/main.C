/* 
** Copyright 1998 by the Condor Team
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
** Author:  Jim Basney
**          University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "shadow.h"
#include "exit.h"
#include "condor_debug.h"

static CShadow ShadowObj;
CShadow *Shadow = &ShadowObj;

static void
usage()
{
	dprintf( D_ALWAYS, "Usage: condor_shadow schedd_addr host capability cluster proc\n" );
	exit( JOB_SHADOW_USAGE );
}


/* DaemonCore interface implementation */

char *mySubSystem = "SHADOW";

int
main_init(int argc, char *argv[])
{
	if (argc != 6) {
		dprintf(D_ALWAYS, "argc = %d\n", argc);
		for (int i=0; i < argc; i++) {
			dprintf(D_ALWAYS, "argv[%d] = %s\n", i, argv[i]);
		}
		usage();
	}

	Shadow->Init(argv[1], argv[2], argv[3], argv[4], argv[5]);

	return 0;
}

int
main_config()
{
	Shadow->Config();
	return 0;
}

int
main_shutdown_fast()
{
	Shadow->Shutdown(JOB_NOT_CKPTED);
	return 0;
}

int
main_shutdown_graceful()
{
	// should request a graceful shutdown from startd here
	exit(0);	// temporary
	return 0;
}
