#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "X509credential.h"
#include "condor_config.h"
#include "condor_distribution.h"
#include "client_common.h"
#include "classadUtil.h"

const char * MyName = "condor_list_cred";

void
usage()
{
	fprintf( stderr, "Usage: %s [options] [cmdfile]\n", MyName );
	fprintf( stderr, "      Valid options:\n" );
	fprintf( stderr, "      -s <host>\tsubmit to the specified credd\n" );
	fprintf( stderr, "      \t(e.g. \"-s myhost.cs.wisc.edu\")\n\n");
	fprintf( stderr, "      -h\tprint this message\n\n");
	fprintf( stderr, "      -v\tverbose output\n\n" );
}

int main(int argc, char **argv)
{
	char * server_address = NULL;
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
					// dprintf to console
				Termlog = 1;
				dprintf_config ("TOOL", 2 );
				break;
			case 's':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -s requires another argument\n",
							 MyName );
					exit(1);
				}
	
				server_address = strdup (*ptr);

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

	ReliSock * sock = NULL;
	if (!start_command_and_authenticate (server_address, CREDD_QUERY_CRED, sock)) {
		return 1;
	}


		// Send request
	sock->encode();
	char * request = "_";
	sock->code (request);
	sock->eom();

		// Receive response
	sock->decode();

	int number = -1;
	sock->code (number); 
  
	if (number > 0) {
		printf ("Name\tType\n-----\t-----\n");
		for (int i=0; i<number; i++) {
			classad::ClassAd classad;
			//classad.initFromStream (*sock);
			getOldClassAd(sock, classad);
      
			X509Credential cred (classad);

			printf ("%s\t%s\n", cred.GetName(), cred.GetTypeString());
		}

		printf ("\nTotal %d\n", number);
	} else if (number == 0) {
		printf ("No credentials currently stored on this server\n");
	} else {
		fprintf (stderr, "ERROR\n");
	}

	sock->close();
	delete sock;
	return 0;
}

  
