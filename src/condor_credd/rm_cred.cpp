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
#include "condor_daemon_core.h"
#include "daemon.h"
#include "X509credential.h"
#include "condor_config.h"
#include "condor_distribution.h"
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
  fprintf( stderr, "      -N <name>\tname of the credential to remove\n\n");
  fprintf( stderr, "      -n <host:port>\tsubmit to the specified credd\n" );
  fprintf( stderr, "      \t(e.g. \"-N myhost.cs.wisc.edu:1234\")\n\n");
  fprintf( stderr, "      -v\tprint version\n\n" );
  fprintf( stderr, "      -h\tprint this message\n\n");

}


int main(int argc, char **argv)
{
    char * server_address = NULL;
    char * cred_name = NULL;
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
  config();

  for (ptr=argv+1,argc--; argc > 0; argc--,ptr++) {
    if ( ptr[0][0] == '-' ) {
      switch ( ptr[0][1] ) {
      case 'h':
	usage(myName);
	exit(0);
	break;
      case 'd':
	  // dprintf to console
	  dprintf_set_tool_debug("TOOL", 0);
	break;
      case 'n':
	if( !(--argc) || !(*(++ptr)) ) {
          fprintf( stderr, "%s: -n requires another argument\n",
                   myName );
          exit(1);
        }
	
	server_address = strdup (*ptr);

	break;
      case 'N':
        if( !(--argc) || !(*(++ptr)) ) {
          fprintf( stderr, "%s: -N requires another argument\n",
                   myName );
          exit(1);
        }
        cred_name = strdup (*ptr);
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
    else {
      fprintf( stderr, "%s: Unknown option %s\n",
	       myName, *ptr);
      usage(myName);
      exit(1);
    }
  } //rof

  if (cred_name == NULL) {
	  cred_name = DEFAULT_CREDENTIAL_NAME;
  }

	CondorError errstack;
	DCCredd dc_credd (server_address);

	// resolve server address
	if ( ! dc_credd.locate() ) {
		fprintf (stderr, "%s\n", dc_credd.error() );
		return 1;
	}

	if (dc_credd.removeCredential(cred_name, errstack)) {
		fprintf (stderr, "Unable to remove credential '%s'\n%s\n",
				cred_name, errstack.getFullText(true).c_str());
		return 1;
	}

	return 0;
}

