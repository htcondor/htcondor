#include "condor_common.h"
#include "dap_constants.h"
#include "dap_server_interface.h"
#include "dap_client_interface.h"
#include "daemon.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
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

  char * result = NULL;
  char * error_reason = NULL;

  int rc = stork_rm (dap_id,
				   global_opts.server,
				   result,
				   error_reason);

  if (rc) {
	  fprintf (stdout, "%s\n",result);
  } else {
	  fprintf (stderr, "ERROR removing job %s (%s)\n", dap_id, error_reason);
  }

  return (rc)?0:1;
}











