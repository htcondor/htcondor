#include "condor_common.h"
#include "dap_constants.h"
#include "dap_server_interface.h"
#include "dap_client_interface.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_config.h"

#ifndef WANT_NAMESPACES
#define WANT_NAMESPACES
#endif
#include "classad_distribution.h"

int main(int argc, char **argv)
{
  char * hostname = NULL;

  //check number of arguments
  if (argc < 2){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <host_name>\n", argv[0]);
    fprintf(stderr, "==============================================================\n");
    exit( 1 );
  }

  hostname = strdup (argv[1]);


  config();

  char * stork_host = get_stork_sinful_string (hostname);
  printf ("Sending request to server %s\n", stork_host);
  
  char * error_reason = NULL;
  int result_size = 0;
  classad::ClassAd ** result = NULL;

  int rc =
	  stork_list ( stork_host,
				 result_size,
				   result,
				   error_reason);

  
  if (!rc) {
	  fprintf (stderr, "ERROR getting list of jobs (%s)\n", 
			   error_reason);
  } else {
	  fprintf(stdout, "===============\n");
	  fprintf(stdout, "job queue:\n");
	  fprintf(stdout, "===============\n");

	  classad::PrettyPrint unparser;
	  std::string buffer;

	  int i;
	  for (i=0; i<result_size; i++) {
		  unparser.Unparse (buffer, result[i]);
		  fprintf (stdout, "\n%s\n", buffer.c_str());
	  }
	  fprintf(stdout, "===============\n");
	  delete [] result;
  }
  
  return (rc)?0:1;

}











