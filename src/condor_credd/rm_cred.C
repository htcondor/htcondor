#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "X509credential.h"
#include "condor_config.h"
#include "condor_distribution.h"

const char * MyName = "condor_rm_cred";

void
usage()
{
  fprintf( stderr, "Usage: %s [options] [cmdfile]\n", MyName );
  fprintf( stderr, "      Valid options:\n" );
  fprintf( stderr, "      -n <name>\tname of the credential to remove\n\n");
  fprintf( stderr, "      -s <host>\tsubmit to the specified credd\n" );
  fprintf( stderr, "      \t(e.g. \"-s myhost.cs.wisc.edu\")\n\n");
  fprintf( stderr, "      -v\tverbose output\n\n" );
  fprintf( stderr, "      -h\tprint this message\n\n");

}


int main(int argc, char **argv)
{
    char * server_address = NULL;
    char * cred_name = NULL;
    char ** ptr;

  myDistro->Init (argc, argv);

  for (ptr=argv+1,argc--; argc > 0; argc--,ptr++) {
    if ( ptr[0][0] == '-' ) {
      switch ( ptr[0][1] ) {
      case 'h':
	usage();
	exit(0);
	break;
      case 'v':
	if (ptr[0][2] == 'e') {
	  // dprintf to console
	  Termlog = 1;
	  dprintf_config ("TOOL", 2 );
	}
	break;
      case 's':
	if( !(--argc) || !(*(++ptr)) ) {
          fprintf( stderr, "%s: -s requires another argument\n",
                   MyName );
          exit(1);
        }
	
	server_address = strdup (*ptr);

	break;
      case 'n':
        if( !(--argc) || !(*(++ptr)) ) {
          fprintf( stderr, "%s: -n requires another argument\n",
                   MyName );
          exit(1);
        }
        cred_name = strdup (*ptr);
        break;

      default:
	fprintf( stderr, "%s: Unknown option %s\n",
		 MyName, *ptr);
	usage();
	exit(1);
      }
    } //fi
  } //rof

  config ();


  // Start command
  Daemon my_credd(DT_CREDD, server_address, NULL);
  Sock * sock = my_credd.startCommand (CREDD_REMOVE_CRED, Stream::reli_sock, 0);

  if (!sock) {
    fprintf (stderr, "Unable to start CREDD_REMOVE_CRED command to host %s\n", server_address);
    return 1;
  }
  ReliSock * reli_sock = (ReliSock*)sock;

  if (!reli_sock->isAuthenticated()) { 
    char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "CLIENT");
    MyString methods;
    if (p) {
      methods = p;
      free (p);
    } else {
     methods = SecMan::getDefaultAuthenticationMethods();
    }
    CondorError errstack;
    if( ! reli_sock->authenticate(methods.Value(), &errstack) ) {
      dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
      delete reli_sock;
      return 1;
    }
  }


  sock->encode();

  sock->code (cred_name);

  sock->eom();

  sock->decode();

  int rc=0;
  sock->code (rc);
  
  if (rc) {
    fprintf (stderr, "Unable to remove credential %s\n", cred_name);
  }

  sock->close();
  delete sock;
  return rc;
}

  
