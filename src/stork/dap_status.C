#include "condor_common.h"
#include "dap_constants.h"
#include "dap_server_interface.h"
#include "dap_client_interface.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_config.h"

int main(int argc, char **argv)
{
  char * dap_id = NULL;
  char * hostname = NULL;

  //check number of arguments
  if (argc < 3){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <host_name> <dap_id>\n", argv[0]);
    fprintf(stderr, "==============================================================\n");
    exit( 1 );
  }

  hostname = strdup (argv[1]);
  dap_id = strdup(argv[2]);


  config();

  char * stork_host = get_stork_sinful_string (hostname);
  printf ("Sending request to server %s\n", stork_host);
  
  classad::ClassAd * result = NULL;
  char * error_reason = NULL;

  int rc =
	  stork_status ( dap_id,
				   stork_host,
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











