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

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_qmgr.h"
#include "match_prefix.h"
#include "sig_install.h"
#include "get_daemon_addr.h"
#include "condor_attributes.h"
#include "condor_distribution.h"

void
usage(char name[])
{
	fprintf(stderr, "Usage: %s [-n schedd-name] { cluster | cluster.proc | owner | -constraint constraint } attribute-name attribute-value ...\n", name);
	exit(1);
}

bool
ProtectedAttribute(char attr[])
{
	return (strcmp(attr, ATTR_OWNER) == 0) ||
		(strcmp(attr, ATTR_CLUSTER_ID) == 0) ||
		(strcmp(attr, ATTR_PROC_ID) == 0) ||
		(strcmp(attr, ATTR_MY_TYPE) == 0) ||
		(strcmp(attr, ATTR_TARGET_TYPE) == 0) ||
		(strcmp(attr, ATTR_JOB_STATUS) == 0);
}

int
main(int argc, char *argv[])
{
	char *DaemonName = NULL, constraint[ATTRLIST_MAX_EXPRESSION];
	Qmgr_connection *q;
	int nextarg = 1, cluster, proc;
	bool UseConstraint = false;

	myDistro->Init( argc, argv );
	config();

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	if (argc < 2) {
		usage(argv[0]);
	}

	if (argv[1][0] == '-' && argv[1][1] == 'n') {
		// use the given name as the schedd name to connect to
		if (argc < 3) {
			fprintf(stderr, "%s: -n requires another argument\n", 
					argv[0]);
			exit(1);
		}				
		if (!(DaemonName = get_daemon_name(argv[2]))) { 
			fprintf(stderr, "%s: unknown host %s\n", 
					argv[0], get_host_part(argv[2]) );
			exit(1);
		}
		nextarg = 3;
	}

		// Open job queue 
	q = ConnectQ(DaemonName);
	if( !q ) {
		if( DaemonName ) {
			fprintf( stderr, "Failed to connect to queue manager %s\n", 
					 DaemonName );
		} else {
			fprintf( stderr, "Failed to connect to local queue manager\n" );
		}
		exit(1);
	}

	if (argc <= nextarg) {
		usage(argv[0]);
	}

	if (match_prefix(argv[nextarg], "-constraint")) {
		nextarg++;
		if (argc <= nextarg) {
			usage(argv[0]);
		}
		sprintf(constraint, "%s", argv[nextarg]);
		nextarg++;
		UseConstraint = true;
	} else if (isdigit(argv[nextarg][0])) {
		char *tmp;
		cluster = strtol(argv[nextarg], &tmp, 10);
		if (cluster <= 0) {
			fprintf( stderr, "Invalid cluster # from %s.\n", argv[nextarg]);
			exit(1);
		}
		if (*tmp == '.') {
			proc = strtol(tmp + 1, &tmp, 10);
			if (cluster <= 0) {
				fprintf( stderr, "Invalid proc # from %s.\n", argv[nextarg]);
				exit(1);
			}
			UseConstraint = false;
		} else {
			sprintf(constraint, "(%s == %d)", ATTR_CLUSTER_ID, cluster);
			UseConstraint = true;
		}
		nextarg++;
	} else {
		sprintf(constraint, "(%s == \"%s\")", ATTR_OWNER, argv[nextarg]);
		nextarg++;
		UseConstraint = true;
	}

	if (argc <= nextarg) {
		usage(argv[0]);
	}

	for (; nextarg < argc; nextarg += 2) {
		if (argc <= nextarg+1) {
			usage(argv[0]);
		}
		if (ProtectedAttribute(argv[nextarg])) {
			fprintf(stderr, "Update of attribute \"%s\" is not allowed.\n",
					argv[nextarg]);
			exit(1);
		}
		if (UseConstraint) {
			if (SetAttributeByConstraint(constraint, argv[nextarg],
										 argv[nextarg+1]) < 0) {
				fprintf(stderr,
						"Failed to set attribute \"%s\" by constraint: %s\n",
						argv[nextarg], constraint);
				exit(1);
			}
		} else {
			if (SetAttribute(cluster, proc, argv[nextarg],
							 argv[nextarg+1]) < 0) {
				fprintf(stderr,
						"Failed to set attribute \"%s\" for job %d.%d.\n",
						argv[nextarg], cluster, proc);
				exit(1);
			}
		}
		printf("Set attribute \"%s\".\n", argv[nextarg]);
	}

	if (!DisconnectQ(q)) {
		fprintf(stderr,
				"Queue transaction failed.  No attributes were set.\n");
		exit(1);
	}

	return 0;
}

