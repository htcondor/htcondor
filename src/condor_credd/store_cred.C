#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "X509credential.h"
#include "my_username.h"
#include "condor_config.h"
#include "credential.h"
#include "condor_distribution.h"
#include "sslutils.h"
#include "internet.h"
#include "client_common.h"

const char * MyName = "condor_store_cred";

int parseMyProxyArgument(const char*, char*&, char*&, int&);
char * prompt_password (const char *);

void
usage()
{
	fprintf( stderr, "Usage: %s [options] [cmdfile]\n", MyName );
	fprintf( stderr, "      Valid options:\n" );
	fprintf( stderr, "      -v\tverbose output\n\n" );
	fprintf( stderr, "      -s <host>\tsubmit to the specified credd\n" );
	fprintf( stderr, "      \t(e.g. \"-s myhost.cs.wisc.edu\")\n\n");
		//  fprintf( stderr, "      -t <x509|password> specify credential type );
	fprintf( stderr, "      -f <file>\tspecify where credential is stored\n\n");
	fprintf( stderr, "      -n <name>\tspecify credential name\n\n");
	fprintf( stderr, "      -m [user@]host[:port]\tspecify MyProxy user/server\n" );
	fprintf( stderr, "      \t(e.g. \"-m wright@myproxy.cs.wisc.edu:1234\")\n\n");
	fprintf( stderr, "      -D <DN>\tspecify myproxy server DN (if not standard)\n" );
	fprintf( stderr, "      \t(e.g. \"-D \'/CN=My/CN=Proxy/O=Host\'\")\n\n" );
	fprintf( stderr, "      -h\tprint this message\n\n");
	exit( 1 );
}


int main(int argc, char **argv)
{
	char ** ptr;


	int cred_type = 0;
	char * cred_name = NULL;
	char * cred_file_name = NULL;
	char * myproxy_user = NULL;

	char * myproxy_host = NULL;
	int myproxy_port = 0;

	char * myproxy_dn = NULL;

	char * server_address= NULL;

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
			case 't':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -t requires another argument\n",
							 MyName );
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
							 MyName );
					exit(1);
				}
				cred_file_name = strdup (*ptr);
				break;
			case 'n':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -n requires another argument\n",
							 MyName );
					exit(1);
				}
				cred_name = strdup (*ptr);
				break;

			case 'm':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -m requires another argument\n",
							 MyName );
					exit(1);
				}
	
				parseMyProxyArgument (*ptr, myproxy_user, myproxy_host, myproxy_port);
				break;
			case 'D':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -D requires another argument\n",
							 MyName );
					exit(1);
				}
				myproxy_dn = strdup (*ptr);
				break;
			default:
				fprintf( stderr, "%s: Unknown option %s\n",
						 MyName, *ptr);
				usage();
				exit(1);
			}
		} //fi
	} //rof

	config();

	if (( cred_file_name == NULL ) && (cred_type == 0)) {
		fprintf ( stderr, "Credential filename or type not specified\n");
		exit (1);

	}

	X509Credential * cred = new X509Credential();

	cred->SetStorageName(cred_file_name);
    
	int fd = open (cred_file_name, O_RDONLY);
	if (fd == -1) {
		fprintf (stderr, "Can't open %s\n", cred_file_name);
		exit (1);
	}
 
	char buff [100000];
	int data_size = read (fd, buff, 100000);
	buff[data_size]='\0';
	close (fd);

	cred->SetData (buff, data_size);

	if (cred_name !=NULL) {
		cred->SetName(cred_name);
	} else {
		cred->SetName("<default>");
	}

	char * username = my_username(0);
	cred->SetOwner (username);
  
	if (myproxy_host != NULL) {
		MyString str_host_port = myproxy_host;
		if (myproxy_port != 0) {
			str_host_port += ":";
			str_host_port += myproxy_port;
		}
		cred->SetMyProxyServerHost (str_host_port.Value());

		if (myproxy_user != NULL) {
			cred->SetMyProxyUser (myproxy_user);
		} else {
			cred->SetMyProxyUser (username);
		}

		if (myproxy_dn != NULL) {
			cred->SetMyProxyServerDN (myproxy_dn);
		}

		char * myproxy_password = prompt_password( "Please enter the MyProxy password:" );
		if (myproxy_password) {
			cred->SetRefreshPassword ( myproxy_password );
		}
	}

	int size = cred->GetDataSize();

	ReliSock * sock = NULL;
	if (!start_command_and_authenticate (server_address, CREDD_STORE_CRED, sock)) {
		return 1;
	}

	sock->encode();


	classad::ClassAd classad(*(cred->GetClassAd()));
	classad::ClassAdUnParser unparser;
	std::string adbuffer;
	unparser.Unparse(adbuffer,&classad);
	char * classad_str = strdup(adbuffer.c_str());

	void * data;
	cred->GetData (data, size);	

	if (!(sock->code (classad_str) && 
		  sock->code_bytes (data, size))) {
		fprintf (stderr, "Unable to submit credential (communication error)!\n");
		delete sock;
		return 1;
	}



	free (classad_str);
	free (data);

	sock->eom();
	sock->decode();
	int rc;
	sock->code(rc);
	if (rc == 0) {
		printf ("Credential submitted successfully\n");
	} else {
		fprintf (stderr, "Unable to submit credential (error code %d)!\n", rc);
	}

	sock->close();
	delete sock;
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
	char pass_buff[500];
	int rc = des_read_pw_string (pass_buff, 499, prompt, 1);
	if (rc) {
		return NULL;
	}

	return strdup(pass_buff);
}
