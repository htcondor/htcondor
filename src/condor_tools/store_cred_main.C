/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"

#ifdef WIN32

#include "condor_config.h"
#include "store_cred.h"
#include "lsa_mgr.h"
#include "my_username.h"
#include "condor_distribution.h"
#include "basename.h"
#include "dynuser.h"

struct StoreCredOptions {
	int mode;
	char pw[MAX_PASSWORD_LENGTH];
	char username[MAX_PASSWORD_LENGTH];
};

char *MyName;
void usage(void);
void parseCommandLine(StoreCredOptions *opts, int argc, char *argv[]);
void badOption(const char* option);


int main(int argc, char *argv[]) {
	
	char* pw = NULL;
	char my_full_name[_POSIX_PATH_MAX];
	struct StoreCredOptions options;


	int result;
	
	MyName = basename(argv[0]);
	
	// load up configuration file
	myDistro->Init( argc, argv );
	config();

	parseCommandLine(&options, argc, argv);

	if ( strcmp(options.username, "") == 0 ) {
		char* my_name = my_username();	
		char* my_domain = my_domainname();

		sprintf(my_full_name, "%s@%s", my_name, my_domain);
		if ( my_name) { free(my_name); }
		if ( my_domain) { free(my_domain); }
		my_name = my_domain = NULL;
	} else {
		strcpy(my_full_name, options.username);
	}
	printf("Account: %s\n\n", my_full_name);

	switch (options.mode) {
		case ADD_MODE:
			if ( strcmp(options.pw, "") == 0 ) {
				// get password from the user.
				pw = get_password();
			} else { 
				// got the passwd from the command line.
				pw = new char[MAX_PASSWORD_LENGTH];
				strncpy(pw, options.pw, MAX_PASSWORD_LENGTH);
				ZeroMemory(options.pw, MAX_PASSWORD_LENGTH);
			}
			if ( pw ) {
				result = addCredential( my_full_name, pw );			
				ZeroMemory(pw, MAX_PASSWORD_LENGTH);
				delete[] pw;
				if ( result == FAILURE_BAD_PASSWORD ) {
					fprintf(stdout, "\n\nPassword is invalid for this account"
							".\n\n");
				} else if ( result == SUCCESS ) {
					fprintf(stdout, "\n\nPassword has been stored.\n");
				} else {
					fprintf(stdout, "\n\nFailure occured.\n");
				}
			} else {
				fprintf(stderr, "\nAborted.\n");
			}
			break;
		case DELETE_MODE:
			result = deleteCredential(my_full_name);
			if ( result == SUCCESS ) {
				fprintf(stdout, "Delete Successful.\n");
			} else {
				fprintf(stdout, "Delete failed.\n");
			}
			break;
		case QUERY_MODE:
			result = queryCredential(my_full_name);
			if ( result == SUCCESS ) {
				fprintf(stdout, "A credential is stored and is valid.\n");
			} else if ( result == FAILURE_BAD_PASSWORD ){
				fprintf(stdout, "A credential is stored, but it is invalid. "
						"Run 'condor_store_cred add' again.\n");
			} else {
				fprintf(stdout, "No credential is stored.\n");
			}
			break;
		case CONFIG_MODE:
			return interactive();
			break;
		case 0:
			usage();
			break;

	
	}


	
	if ( result == SUCCESS ) {
	   	return 0;
	} else {
		return 1;
	}
}

void
parseCommandLine(StoreCredOptions *opts, int argc, char *argv[]) {

	int i;
	opts->mode = 0;
	opts->pw[0] = '\0';
	opts->username[0] = '\0';

	for (i=1; i<argc; i++) {
		switch(argv[i][0]) {
		case 'a':
		case 'A':	// Add
			if (stricmp(argv[i], ADD_CREDENTIAL) == 0) {
				opts->mode = ADD_MODE;
			} else {
				badOption(argv[i]);
			}	
			break;
		case 'd':	
		case 'D':	// Delete
			if (stricmp(argv[i], DELETE_CREDENTIAL) == 0) {
				opts->mode = DELETE_MODE;
			} else {
				badOption(argv[i]);
			}	
			break;
		case 'q':	
		case 'Q':	// tell me if I have anything stored
			if (stricmp(argv[i], QUERY_CREDENTIAL) == 0) {
				opts->mode = QUERY_MODE;
			} else {
				badOption(argv[i]);
			}	
			break;
		case 'c':	
		case 'C':	// Config
			if (stricmp(argv[i], CONFIG_CREDENTIAL) == 0) {
				opts->mode = CONFIG_MODE;
			} else {
				badOption(argv[i]);
			}	
			break;
		case '-':
			// various switches
			switch (argv[i][1]) {
				case 'p':
					if (i+1 < argc) {
						strncpy(opts->pw, argv[i+1], MAX_PASSWORD_LENGTH);
						i++;
					} else {
						badOption(argv[i]);
					}
					break;
				case 'u':
					if (i+1 < argc) {
						strncpy(opts->username, argv[i+1], MAX_PASSWORD_LENGTH);
						i++;
						char* at_ptr = strchr(opts->username, '@');
						// '@' must be in the string, but not the beginning
						// or end of the string.
						if (at_ptr == NULL || 
							at_ptr == opts->username ||
						   	at_ptr == opts->username+strlen(opts->username)-1) {
							fprintf(stderr, "ERROR: Username '%s' is not of "
								   "the form account@domain\n", opts->username);
							badOption(argv[i]);
						}

					} else {
						badOption(argv[i]);
					}
					break;
				case 'h':
					usage();
					
					break;
			}
			break;
			// fail through
		default:
			badOption(argv[i]);
			break;
		}
	}
}

void
badOption(const char* option) {
	fprintf(stderr, "Unrecognized option - '%s'\n\n", option);
	usage();
}

void
usage()
{
	fprintf( stderr, "Usage: %s [options] action\n", MyName );
	fprintf( stderr, "  where action is one of:\n" );
	fprintf( stderr, "    add               (Add your credential to secure storage)\n" );
	fprintf( stderr, "    delete            (Remove your credential from secure storage)\n" );
	fprintf( stderr, "    query             (Check if your credential has been stored)\n" );
	fprintf( stderr, "  and where [options] is one or more of:\n" );
	fprintf( stderr, "    -u username       (use the specified username)\n" );
	fprintf( stderr, "    -p password       (use the specified password rather than prompting)\n" );
	fprintf( stderr, "    -h                (display this message)\n" );
	fprintf( stderr, "\n" );

	exit( 1 );
};

#endif // WIN32
