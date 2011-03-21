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
#include "dap_constants.h"
#include "dap_server_interface.h"
#include "dap_client_interface.h"
#include "condor_daemon_core.h"
#include "condor_config.h"

#define USAGE \
"[stork_server] job_id\n\
stork_server\t\t\tspecify explicit stork server (deprecated)\n\
job_id\t\t\t\tstork job id"

int main(int argc, char **argv)
{
	char * dap_id = NULL;
	struct stork_global_opts global_opts;

 	config();	// read config file

	// Parse out stork global options.
	stork_parse_global_opts(argc, argv, USAGE, &global_opts, true);

	//check number of arguments
	switch (argc) {
		case 2:
			if (argv[1][0] == '-') {
				// an invalid option argument
				fprintf(stderr, "unknown option: '%s'\n", argv[1]);
				stork_print_usage(stderr, argv[0], USAGE, true);
				exit(1);
			}
			dap_id = argv[1];
			break;
		case 3:
			if (argv[1][0] == '-') {
				// an invalid option argument
				fprintf(stderr, "unknown option: '%s'\n", argv[1]);
				stork_print_usage(stderr, argv[0], USAGE, true);
				exit(1);
			}
			// heritage positional parameter to specify remote server.
			global_opts.server = argv[1];
			dap_id = argv[2];
			break;
		default:
			stork_print_usage(stderr, argv[0], USAGE, true);
			exit(1);
	}

  classad::ClassAd * result = NULL;
  char * error_reason = NULL;

  int rc =
	  stork_status ( dap_id,
				   global_opts.server,
				   result,
				   error_reason);

  
  if (!rc) {
	  fprintf (stderr, "ERROR getting status for job %s (%s)\n", 
			   dap_id, 
			   error_reason);
  } else {
	  classad::PrettyPrint unparser;
	  std::string result_str;
	  unparser.Unparse (result_str, result);
	  fprintf(stdout, "===============\n");
	  fprintf(stdout, "status history:\n");
	  fprintf(stdout, "===============\n");
	  fprintf(stdout, "\n%s\n", result_str.c_str());
	  fprintf(stdout, "\n===============\n");
  }
  
  return (rc)?0:1;

}











