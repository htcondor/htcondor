#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "X509credential.h"
#include "dc_credd.h"

int main(int argc, char **argv)
{
  char * credd_sin = argv[1];
  char * cred_name = argv[2];

  CondorError errorstack;
  char * cred_data = NULL;
  int cred_size = 0;

  DCCredd credd(credd_sin);
  if (credd.getCredentialData (cred_name, (void*)cred_data, cred_size, errorstack)) {
      printf ("Received %d \n%s\n", cred_size, cred_data);
	  return 0;
  } else {
	  fprintf (stderr, "ERROR (%d : %s)\n", 
			   errorstack.code(),
			   errorstack.message());
	  return 1;
  }
}

  
