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
** Author:  Jim Basney ( and Mike Yoder )
**          University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "baseshadow.h"
#include "shadow.h"
#include "mpishadow.h"
#include "exit.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_qmgr.h"

//static CShadow ShadowObj;
//CShadow *Shadow = &ShadowObj;

BaseShadow *Shadow;

static void
usage()
{
	dprintf( D_ALWAYS, "Usage: condor_shadow schedd_addr host"
			 "capability cluster proc\n" );
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

	if (argv[1][0] != '<') {
		EXCEPT("schedd_addr not specified with sinful string.");
	}
	
	if (argv[2][0] != '<') {
		EXCEPT("host not specified with sinful string.");
	}

		// We have to figure out what sort of shadow to make.
		// That's why so much processing goes on here....

		// Get the jobAd from the schedd:
	ClassAd *jobAd;
	char schedd_addr[128];
	int cluster, proc;
	strncpy ( schedd_addr, argv[1], 128 );
	cluster = atoi(argv[4]);
	proc = atoi(argv[5]);

		// talk to the schedd to get job ad & set remote host:
	ConnectQ(schedd_addr);
	jobAd = ::GetJobAd(cluster, proc);
		// Move the following somewhere else for MPI/PVM?
	SetAttributeString(cluster,proc,ATTR_REMOTE_HOST,argv[2]);
	DisconnectQ(NULL);
	if (!jobAd) {
		EXCEPT("Failed to get job ad from schedd.");
	}

	int universe;
	if (jobAd->LookupInteger(ATTR_JOB_UNIVERSE, universe) < 0) {
			// change to standard when they work...
		universe = VANILLA;
	}
	
	switch ( universe ) {
	case VANILLA:
	case STANDARD:
		Shadow = new UniShadow();
		break;
	case MPI:
		EXCEPT( "MPI coming soon!" );
//		Shadow = new MPIShadow();
		break;
	case PVM:
		EXCEPT( "PVM...hopefully one day..." );
//		Shadow = new PVMShadow();
		break;
	default:
		EXCEPT( "Universe not supported" );
	}

	Shadow->init( jobAd, argv[1], argv[2], argv[3], argv[4], argv[5]);

	return 0;
}

int
main_config()
{
	Shadow->config();
	return 0;
}

int
main_shutdown_fast()
{
	Shadow->shutDown(JOB_NOT_CKPTED, 0);
	return 0;
}

int
main_shutdown_graceful()
{
	// should request a graceful shutdown from startd here
	exit(0);	// temporary
	return 0;
}
