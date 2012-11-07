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
#include "my_username.h"
#include "condor_config.h"
#include "credential.h"
#include "condor_distribution.h"
#include "sslutils.h"
#include "internet.h"
#include <unistd.h>
#include "dc_credd.h"
#include "credential.h"
#include "condor_version.h"
#include "openssl/ui_compat.h"

char Myproxy_pw[512];	// pasaword for credential access from MyProxy
// Read MyProxy password from terminal, or stdin.
bool Read_Myproxy_pw_terminal = true;

int parseMyProxyArgument(const char*, char*&, char*&, int&);
char * prompt_password (const char *);
char * stdin_password (const char *);
bool read_file (const char * filename, char *& data, int & size);

static int Termlog = 0;

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
	fprintf( stderr, "      -t x509\tspecify credential type\n");
	fprintf( stderr, "      \tonly the x509 type is currently available\n");
	fprintf( stderr, "      \thowever this option is now REQUIRED\n\n");
	fprintf( stderr, "      -f <file>\tspecify where credential is stored\n\n");
	fprintf( stderr, "      -N <name>\tspecify credential name\n\n");
	fprintf( stderr, "      -m [user@]host[:port]\tspecify MyProxy user/server\n" );
	fprintf( stderr, "      \t(e.g. \"-m wright@myproxy.cs.wisc.edu:1234\")\n\n");
	fprintf( stderr, "      -D <DN>\tspecify myproxy server DN (if not standard)\n" );
	fprintf( stderr, "      \t(e.g. \"-D \'/CN=My/CN=Proxy/O=Host\'\")\n\n" );
	fprintf( stderr, "      -S\tread MyProxy password from standard input\n\n");
	fprintf( stderr, "      -v\tprint version\n\n" );
	fprintf( stderr, "      -h\tprint this message\n\n");
	exit( 1 );
}


int main(int argc, char **argv)
{
	char ** ptr;
	const char * myName;

	// find our name
	myName = strrchr( argv[0], DIR_DELIM_CHAR );
	if( !myName ) {
		myName = argv[0];
	} else {
		myName++;
	}

	int cred_type = 0;
	char * cred_name = NULL;
	char * cred_file_name = NULL;
	char * myproxy_user = NULL;

	char * myproxy_host = NULL;
	int myproxy_port = 0;

	char * myproxy_dn = NULL;

	char * server_address= NULL;

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
				Termlog = 1;

				break;
			case 'S':

					// dprintf to console
				Termlog = 1;
				Read_Myproxy_pw_terminal = false;

				break;
			case 'n':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -n requires another argument\n",
							 myName );
					exit(1);
				}
	
				server_address = strdup (*ptr);

				break;
			case 't':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -t requires another argument\n",
							 myName );
					exit(1);
				}

				if (strcmp (*ptr, "x509") == 0) {
					cred_type = X509_CREDENTIAL_TYPE;
				} else {
					fprintf( stderr, "Invalid credential type %s\n",
							 *ptr );
					exit(1);
				}
				break;
			case 'f':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -f requires another argument\n",
							 myName );
					exit(1);
				}
				cred_file_name = strdup (*ptr);
				break;
			case 'N':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -N requires another argument\n",
							 myName );
					exit(1);
				}
				cred_name = strdup (*ptr);
				break;

			case 'm':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -m requires another argument\n",
							 myName );
					exit(1);
				}
	
				parseMyProxyArgument (*ptr, myproxy_user, myproxy_host, myproxy_port);
				break;
			case 'D':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -D requires another argument\n",
							 myName );
					exit(1);
				}
				myproxy_dn = strdup (*ptr);
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

	if(Termlog) dprintf_set_tool_debug("TOOL", 0);

	if (( cred_file_name == NULL ) || (cred_type == 0)) {
		fprintf ( stderr, "Credential filename or type not specified\n");
		exit (1);

	}

    Credential * cred = NULL;
	if (cred_type == X509_CREDENTIAL_TYPE) {
		cred = new X509Credential();
	} else {
		fprintf ( stderr, "Invalid credential type\n");
		exit (1);
	}

    
	char * data = NULL;
	int data_size;
	if (!read_file (cred_file_name, data, data_size)) {
		fprintf (stderr, "Can't open %s\n", cred_file_name);
		exit (1);
	}

	cred->SetData (data, data_size);

	if (cred_name !=NULL) {
		cred->SetName(cred_name);
	} else {
		cred->SetName(DEFAULT_CREDENTIAL_NAME);
	}

	char * username = my_username(0);
	cred->SetOwner (username);
  
	if (cred_type == X509_CREDENTIAL_TYPE && myproxy_host != NULL) {
		X509Credential * x509cred = (X509Credential*)cred;

		MyString str_host_port = myproxy_host;
		if (myproxy_port != 0) {
			str_host_port += ":";
			str_host_port += myproxy_port;
		}
		x509cred->SetMyProxyServerHost (str_host_port.Value());

		if (myproxy_user != NULL) {
			x509cred->SetMyProxyUser (myproxy_user);
		} else {
			x509cred->SetMyProxyUser (username);
		}

		if (myproxy_dn != NULL) {
			x509cred->SetMyProxyServerDN (myproxy_dn);
		}

		char * myproxy_password;
		if ( Read_Myproxy_pw_terminal ) {
			myproxy_password = 
				prompt_password(
					"Please enter the MyProxy password:" );
		} else {
			myproxy_password = 
				stdin_password(
				"Please enter the MyProxy password from the standard input\n");
		}
		if (myproxy_password) {
			x509cred->SetRefreshPassword ( myproxy_password );
		}

		x509cred->display( D_FULLDEBUG );
	}

	CondorError errstack;
	DCCredd dc_credd (server_address);

	// resolve server address
	if ( ! dc_credd.locate() ) {
		fprintf (stderr, "%s\n", dc_credd.error() );
		return 1;
	}

	if (dc_credd.storeCredential(cred, errstack)) {
		printf ("Credential submitted successfully\n");
	} else {
		fprintf (stderr, "Unable to submit credential\n%s\n",
				 errstack.getFullText(true).c_str());
		return 1;
	}

	return 0;
}

int  
parseMyProxyArgument (const char * arg,
					  char * & user, 
					  char * & host, 
					  int & port) {

	MyString strArg (arg);
	int at_idx = strArg.FindChar ((int)'@');
	int colon_idx = strArg.FindChar ((int)':', at_idx+1);

	if (at_idx != -1) {
		MyString _user = strArg.Substr (0, at_idx-1);
		user = strdup(_user.Value());
	}
  
  
	if (colon_idx == -1) {
		MyString _host = strArg.Substr (at_idx+1, strArg.Length()-1);
		host = strdup(_host.Value());
	} else {
		MyString _host = strArg.Substr (at_idx+1, colon_idx-1);
		host = strdup(_host.Value());

		MyString _port = strArg.Substr (colon_idx+1, strArg.Length()-1);
		port = atoi(_port.Value());

	}

	return TRUE;
}

char *
prompt_password(const char * prompt) {
	int rc =
		// Read password from terminal.  Disable terminal echo.
		des_read_pw_string (
			Myproxy_pw,					// buffer
			sizeof ( Myproxy_pw ) - 1,	// length
			prompt,						// prompt
			true						// verify
		);
	if (rc) {
		return NULL;
	}

	return Myproxy_pw;
}

char *
stdin_password(const char * prompt) {
	int nbytes;
	printf("%s", prompt);
	nbytes =
		// Read password from stdin.
		read (
			0,							// file descriptor = stdin
			Myproxy_pw,					// buffer
			sizeof ( Myproxy_pw ) - 1	// length
		);
	if ( nbytes <= 0 ) {
		return NULL;
	}

	return Myproxy_pw;
}

bool
read_file (const char * filename, char *& data, int & size) {

	int fd = safe_open_wrapper_follow(filename, O_RDONLY);
	if (fd == -1) {
		return false;
	}

	struct stat my_stat;
	if (fstat (fd, &my_stat) != 0) {
		close (fd);
		return false;
	}

	size = (int)my_stat.st_size;
	data = (char*)malloc(size+1);
	data[size]='\0';
	if (!data) 
		return false;
 	
	if (!read (fd, data, size)) {
		free (data);
		return false;
	}

	close (fd);
	return true;
}
