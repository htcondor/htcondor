#include "condor_common.h"
#include "dap_constants.h"
#include "dap_server_interface.h"
#include "dap_client_interface.h"
#include "daemon.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_config.h"

int main(int argc, char **argv)
{
	char * hostname = NULL;
	char * dap_id = NULL;

  //check number of arguments
  if (argc < 3){
	  fprintf(stderr, "==============================================================\n");
	  fprintf(stderr, "USAGE: %s <host_name> <dap_id>\n", argv[0]);
	  fprintf(stderr, "==============================================================\n");
	  exit( 1 );
  }

  hostname = strdup(argv[1]);
  dap_id = strdup (argv[2]);

  config();

  char * stork_host = get_stork_sinful_string (hostname);
  dprintf (D_FULLDEBUG, "Sending request to server %s\n", stork_host);

  char * result = NULL;
  char * error_reason = NULL;

  int rc = stork_rm (dap_id,
				   stork_host,
				   result,
				   error_reason);

  if (rc) {
	  fprintf (stdout, "%s\n",result);
  } else {
	  fprintf (stderr, "ERROR removing job %s (%s)\n", dap_id, error_reason);
  }

  return (rc)?0:1;
}











