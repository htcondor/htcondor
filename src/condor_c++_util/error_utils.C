/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
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
#include "condor_config.h"
#include "print_wrapped_text.h"


void
printNoCollectorContact( FILE* fp, const char* addr, bool verbose )
{
	char error_message[1000];
	char *collector_host;
	bool needs_free = false;

	if (addr != NULL) {
		collector_host = (char*)addr; 
	} else {
		collector_host = param("COLLECTOR_HOST");
		if( collector_host ) { 
			needs_free = true;
		}
	}
	sprintf( error_message, 
			 "Error: Couldn't contact the condor_collector on %s.",
			 collector_host ? collector_host : "your central manager");

	print_wrapped_text( error_message, fp );

	if( verbose ) {
		fprintf(fp, "\n");
		print_wrapped_text("Extra Info: the condor_collector is a process "
						   "that runs on the central manager of your Condor "
						   "pool and collects the status of all the machines "
						   "and jobs in the Condor pool. "
						   "The condor_collector might not be running, "
						   "it might be refusing to communicate with you, "
						   "there might be a network problem, or there may be "
						   "some other problem. Check with your system "
						   "administrator to fix this problem.", fp);
		fprintf( fp, "\n" );
		sprintf( error_message, 
				 "If you are the system administrator, check that the "
				 "condor_collector is running on %s, check the HOSTALLOW "
				 "configuration in your condor_config, and check the "
				 "MasterLog and CollectorLog files in your log directory "
				 "for possible clues as to why the condor_collector "
				 "is not responding. Also see the Troubleshooting "
				 "section of the manual.", 
				 collector_host ? collector_host : "your central manager" );
		print_wrapped_text( error_message, fp );
	}

	if( needs_free ) {
		free( collector_host );
	}
}
