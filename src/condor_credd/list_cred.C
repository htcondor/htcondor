#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "X509credential.h"
#include "condor_config.h"
#include "condor_distribution.h"
#include "classadUtil.h"
#include "dc_credd.h"
#include "condor_version.h"


void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
	exit( 0 );
}

void
usage(const char *myName)
{
	fprintf( stderr, "Usage: %s [options] [cmdfile]\n", myName );
	fprintf( stderr, "      Valid options:\n" );
	fprintf( stderr, "      -d\tdebug output\n\n" );
	fprintf( stderr, "      -n <host:port>\tsubmit to the specified credd\n" );
	fprintf( stderr, "      \t(e.g. \"-s myhost.cs.wisc.edu:1234\")\n\n");
	fprintf( stderr, "      -h\tprint this message\n\n");
	fprintf( stderr, "      -v\tprint version\n\n" );
}

int main(int argc, char **argv)
{
	char * server_address = NULL;
	char ** ptr;
	const char * myName;

	// find our name
	myName = strrchr( argv[0], DIR_DELIM_CHAR );
	if( !myName ) {
		myName = argv[0];
	} else {
		myName++;
	}

	// read config file
	myDistro->Init (argc, argv);
	config ();

	for (ptr=argv+1,argc--; argc > 0; argc--,ptr++) {
		if ( ptr[0][0] == '-' ) {
			switch ( ptr[0][1] ) {
			case 'h':
				usage(myName);
				exit(0);
				break;
			case 'd':
					// dprintf to console
				Termlog = 1;
				dprintf_config ("TOOL", 2 );
				break;
			case 'n':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -n requires another argument\n",
							 myName );
					exit(1);
				}
	
				server_address = strdup (*ptr);

				break;
			case 'v':
				version();	// this function calls exit(0)
				break;
			default:
				fprintf( stderr, "%s: Unknown option %s\n",
						 myName, *ptr);
				usage(myName);
				exit(1);
			}
		} //fi
	} //rof


	CondorError errorstack;
	int number = 0;
	SimpleList <Credential*> result;

	DCCredd credd(server_address);

	// resolve server address
	if ( ! credd.locate() ) {
		fprintf (stderr, "%s\n", credd.error() );
		return 1;
	}

	if (!credd.listCredentials (result,
								number,
								errorstack)) {
		fprintf (stderr, "Unable to retrieve credentials (%s)\n",
				 errorstack.getFullText(true));
		return 1;
	}
	 


	if (number > 0) {
		Credential * cred;
		result.Rewind();
		printf ("Name\tType\n-----\t-----\n");
		while (result.Next (cred)) {
			
			printf ("%s\t%s\n", cred->GetName(), cred->GetTypeString());
		}

		printf ("\nTotal %d\n", number);
	} else if (number == 0) {
		printf ("No credentials currently stored on this server\n");
	} else {
		fprintf (stderr, "ERROR\n");
		return 1;
	}

	return 0;
}

  
