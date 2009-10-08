/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/



#include "condor_common.h"
#include "condor_config.h"
#include "print_wrapped_text.h"


void
printNoCollectorContact( FILE* fp, const char* addr, bool verbose )
{
	char error_message[1000];
	const char *collector_host = NULL;
	char *free_param = NULL;

	if (addr != NULL) {
		collector_host = addr; 
	} else {
		free_param = param("COLLECTOR_HOST");
		if( free_param ) { 
			collector_host = free_param;
		}
	}
	snprintf( error_message, 1000,
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
		snprintf( error_message, 1000,
				 "If you are the system administrator, check that the "
				 "condor_collector is running on %s, check the ALLOW/DENY "
				 "configuration in your condor_config, and check the "
				 "MasterLog and CollectorLog files in your log directory "
				 "for possible clues as to why the condor_collector "
				 "is not responding. Also see the Troubleshooting "
				 "section of the manual.", 
				 collector_host ? collector_host : "your central manager" );
		print_wrapped_text( error_message, fp );
	}

	if( free_param ) {
		free( free_param );
	}
}
