#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "X509credential.h"
#include "client_common.h"

int main(int argc, char **argv)
{
  char * credd_sin = argv[1];
  char * cred_name = argv[2];

  ReliSock * sock;
  if (!start_command_and_authenticate (credd_sin, CREDD_GET_CRED, sock)) {
	  return 1;
  }

  sock->encode();

  sock->code (cred_name);

  sock->eom();

  sock->decode();

  int size;
  sock->code (size);

  if (size > 0) {
    char * bytes = (char*)malloc (size);
    if (sock->code_bytes (bytes, size) )
      printf ("Received %d \n%s\n", size, bytes);
    else
      printf ("ERROR\n");
  } else {
    fprintf (stderr, "ERROR (code=%d)\n", size);
  }

  sock->close();
  delete sock;
  return 0;
}

  
