/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
