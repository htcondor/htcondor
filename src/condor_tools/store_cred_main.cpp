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

#include "daemon.h"
#include "condor_config.h"
#include "store_cred.h"
#include "my_username.h"
#include "condor_distribution.h"
#include "basename.h"
#include "dynuser.h"
#include "get_daemon_name.h"
#include "condor_string.h"

#if defined(WIN32)
#include "lsa_mgr.h"  // for CONFIG_MODE
#endif

struct StoreCredOptions {
	int mode;
	char pw[MAX_PASSWORD_LENGTH + 1];
	char username[MAX_PASSWORD_LENGTH + 1];
	char *daemonname;
	char *password_file;
	bool help;
};

const char *MyName;
void usage(void);
bool parseCommandLine(StoreCredOptions *opts, int argc, char *argv[]);
void badOption(const char* option);
void badCommand(const char* command);
void optionNeedsArg(const char* option);
bool goAheadAnyways();

int main(int argc, char *argv[]) {
	
	MyString my_full_name;
	const char *full_name;
	char* pw = NULL;
	struct StoreCredOptions options;
	int result = FAILURE_ABORTED;
	bool pool_password_arg = false;
	bool pool_password_delete = false;
	Daemon *daemon = NULL;
	char *credd_host;

	MyName = condor_basename(argv[0]);
	
	// load up configuration file
	myDistro->Init( argc, argv );
	config();

	if (!parseCommandLine(&options, argc, argv)) {
		goto cleanup;
	}

	// if -h was given, just print usage
	if (options.help || (options.mode == 0)) {
		usage();
		goto cleanup;
	}

#if !defined(WIN32)
	// if the -f argument was given, we just want to prompt for a password
	// and write it to a file (in our weirdo XORed format)
	//
	if (options.password_file != NULL) {
		if (options.pw[0] == '\0') {
			pw = get_password();
			printf("\n");
		}
		else {
			pw = strnewp(options.pw);
			SecureZeroMemory(options.pw, MAX_PASSWORD_LENGTH + 1);
		}
		result = write_password_file(options.password_file, pw);
		SecureZeroMemory(pw, strlen(pw));
		if (result != SUCCESS) {
			fprintf(stderr,
			        "error writing password file: %s\n",
			        options.password_file);
		}
		goto cleanup;
	}
#endif

	// determine the username to use
	if ( strcmp(options.username, "") == 0 ) {
			// default to current user and domain
		char* my_name = my_username();	
		char* my_domain = my_domainname();

		my_full_name.formatstr("%s@%s", my_name, my_domain);
		if ( my_name) { free(my_name); }
		if ( my_domain) { free(my_domain); }
		my_name = my_domain = NULL;
	} else if (strcmp(options.username, POOL_PASSWORD_USERNAME) == 0) {
#if !defined(WIN32)
		// we don't support querying the pool password on UNIX
		// (since it is not possible to do so through the
		//  STORE_POOL_CRED command)
		if (options.mode == QUERY_MODE) {
			fprintf(stderr, "Querying the pool password is not supported.\n");
			goto cleanup;
		}
#endif
			// append the correct domain name if we're using the pool pass
			// we first try POOL_PASSWORD_DOMAIN, then UID_DOMAIN
		pool_password_arg = true;
		char *domain;
		if ((domain = param("SEC_PASSWORD_DOMAIN")) == NULL) {
			if ((domain = param("UID_DOMAIN")) == NULL) {
				fprintf(stderr, "ERROR: could not domain for pool password\n");
				goto cleanup;
			}
		}
		my_full_name.formatstr(POOL_PASSWORD_USERNAME "@%s", domain);
		free(domain);
	} else {
			// username was specified on the command line
		my_full_name += options.username;
	}
	full_name = my_full_name.Value();
	printf("Account: %s\n\n", full_name);

	// determine where to direct our command
	daemon = NULL;
	credd_host = NULL;
	if (options.daemonname != NULL) {
		if (pool_password_arg && (options.mode != QUERY_MODE)) {
			// daemon named on command line; go to master for pool password
			//printf("sending command to master: %s\n", options.daemonname);
			daemon = new Daemon(DT_MASTER, options.daemonname);
		}
		else {
			// daemon named on command line; go to schedd for user password
			//printf("sending command to schedd: %s\n", options.daemonname);
			daemon = new Daemon(DT_SCHEDD, options.daemonname);
		}
	}
	else if (!pool_password_arg && ((credd_host = param("CREDD_HOST")) != NULL)) {
		// no daemon given, use credd for user passwords if CREDD_HOST is defined
		// (otherwise, we use the local schedd)
		//printf("sending command to CREDD: %s\n", credd_host);
		daemon = new Daemon(DT_CREDD);
	}
	else {
		//printf("sending command to local daemon\n");
	}

	// flag the case where we're deleting the pool password
	if (pool_password_arg && (options.mode == DELETE_MODE)) {
		pool_password_delete = true;
	}

	switch (options.mode) {
		case ADD_MODE:
		case DELETE_MODE:
			if (!pool_password_delete) {
				if ( strcmp(options.pw, "") == 0 ) {
					// get password from the user.
					pw = get_password();
					printf("\n\n");
				} else {
					// got the passwd from the command line.
					pw = strnewp(options.pw);
					SecureZeroMemory(options.pw, MAX_PASSWORD_LENGTH);
				}
			}
			if ( pw || pool_password_delete) {
				result = store_cred(full_name, pw, options.mode, daemon);
				if ((result == FAILURE_NOT_SECURE) && goAheadAnyways()) {
						// if user is ok with it, send the password in the clear
					result = store_cred(full_name, pw, options.mode, daemon, true);
				}
				if (pw) {
					SecureZeroMemory(pw, strlen(pw));
					delete[] pw;
				}
			}
			break;
		case QUERY_MODE:
			result = queryCredential(full_name, daemon);
			break;
#if defined(WIN32)
		case CONFIG_MODE:
			return interactive();
			break;
#endif
		default:
			fprintf(stderr, "Internal error\n");
			goto cleanup;
	}

	// output result of operation
	switch (result) {
		case SUCCESS:
			if (options.mode == QUERY_MODE) {
				printf("A credential is stored and is valid.\n");
			}
			else {
				printf("Operation succeeded.\n");
			}
			break;

		case FAILURE:
			printf("Operation failed.\n");
			if (pool_password_arg && (options.mode != QUERY_MODE)) {
				printf("    Make sure you have CONFIG access to the target Master.\n");
			}
			else {
				printf("    Make sure your ALLOW_WRITE setting includes this host.\n");
			}
			break;

		case FAILURE_BAD_PASSWORD:
			if (options.mode == QUERY_MODE) {
				printf("A credential is stored, but it is invalid. "
				       "Run 'condor_store_cred add' again.\n");
			}
			else {
				printf("Operation failed: bad password.\n");
			}
			break;

		case FAILURE_NOT_FOUND:
			if (options.mode == QUERY_MODE) {
				printf("No credential is stored.\n");
			}
			else {
				printf("Operation failed: username not found.\n");
			}
			break;

		case FAILURE_NOT_SECURE:
		case FAILURE_ABORTED:
			printf("Operation aborted.\n");
			break;

		case FAILURE_NOT_SUPPORTED:
			printf("Operation failed.\n"
			       "    The target daemon is not running as SYSTEM.\n");
			break;

		default:
			fprintf(stderr, "Operation failed: unknown error code\n");
	}

cleanup:			
	if (options.daemonname) {
		delete[] options.daemonname;
	}
	if (daemon) {
		delete daemon;
	}
	
	if ( result == SUCCESS ) {
	   	return 0;
	} else {
		return 1;
	}
}

bool
parseCommandLine(StoreCredOptions *opts, int argc, char *argv[]) {

	int i;
	opts->mode = 0;
	opts->pw[0] = opts->pw[MAX_PASSWORD_LENGTH] = '\0';
	opts->username[0] = opts->username[MAX_PASSWORD_LENGTH] = '\0';
	opts->daemonname = NULL;
	opts->password_file = NULL;;
	opts->help = false;

	bool err = false;
	for (i=1; i<argc && !err; i++) {
		switch(argv[i][0]) {
		case 'a':
		case 'A':	// Add
			if (strcasecmp(argv[i], ADD_CREDENTIAL) == 0) {
				if (!opts->mode) {
					opts->mode = ADD_MODE;
				}
				else if (opts->mode != ADD_MODE) {
					fprintf(stderr,
					        "ERROR: exactly one command must be provided\n");
					usage();
					err = true;
				}
			} else {
				err = true;
				badCommand(argv[i]);
			}	
			break;
		case 'd':	
		case 'D':	// Delete
			if (strcasecmp(argv[i], DELETE_CREDENTIAL) == 0) {
				if (!opts->mode) {
					opts->mode = DELETE_MODE;
				}
				else if (opts->mode != DELETE_MODE) {
					fprintf(stderr,
					        "ERROR: exactly one command must be provided\n");
					usage();
					err = true;
				}
			} else {
				err = true;
				badCommand(argv[i]);
			}	
			break;
		case 'q':	
		case 'Q':	// tell me if I have anything stored
			if (strcasecmp(argv[i], QUERY_CREDENTIAL) == 0) {
				if (!opts->mode) {
					opts->mode = QUERY_MODE;
				}
				else if (opts->mode != QUERY_MODE) {
					fprintf(stderr,
					        "ERROR: exactly one command must be provided\n");
					usage();
					err = true;
				}
			} else {
				err = true;
				badCommand(argv[i]);
			}	
			break;
#if defined(WIN32)
		case 'c':	
		case 'C':	// Config
			if (strcasecmp(argv[i], CONFIG_CREDENTIAL) == 0) {
				if (!opts->mode) {
					opts->mode = CONFIG_MODE;
				}
				else {
					fprintf(stderr, "ERROR: exactly one command must be provided\n");
					usage();
					err = true;
				}
			} else {
				err = true;
				badCommand(argv[i]);
			}	
			break;
#endif
		case '-':
			// various switches
			switch (argv[i][1]) {
				case 'n':
					if (i+1 < argc) {
						if (opts->daemonname != NULL) {
							fprintf(stderr, "ERROR: only one '-n' arg my be provided\n");
							usage();
							err = true;
						}
						else {
							opts->daemonname = get_daemon_name(argv[i+1]);
							if (opts->daemonname == NULL) {
								fprintf(stderr, "ERROR: %s is not a valid daemon name\n",
									argv[i+1]);
								err = true;
							}
							i++;
						}
					} else {
						err = true;
						optionNeedsArg(argv[i]);
					}
					break;
				case 'p':
					if (i+1 < argc) {
						if (opts->pw[0] != '\0') {
							fprintf(stderr, "ERROR: only one '-p' args may be provided\n");
							usage();
							err = true;
						}
						else {
							strncpy(opts->pw, argv[i+1], MAX_PASSWORD_LENGTH);
							i++;
						}
					} else {
						err = true;
						optionNeedsArg(argv[i]);
					}
					break;
				case 'c':
					if (opts->username[0] != '\0') {
						fprintf(stderr, "ERROR: only one '-c' or '-u' arg may be provided\n");
						usage();
						err = true;
					}
					else {
						strcpy(opts->username, POOL_PASSWORD_USERNAME);
					}
					break;
				case 'u':
					if (i+1 < argc) {
						if (opts->username[0] != '\0') {
							fprintf(stderr, "ERROR: only one of '-s' or '-u' may be provided\n");
							usage();
							err = true;
						}
						else {
							strncpy(opts->username, argv[i+1], MAX_PASSWORD_LENGTH);
							i++;
							char* at_ptr = strchr(opts->username, '@');
							// '@' must be in the string, but not the beginning
							// or end of the string.
							if (at_ptr == NULL || 
								at_ptr == opts->username ||
						   		at_ptr == opts->username+strlen(opts->username)-1) {
								fprintf(stderr, "ERROR: Username '%s' is not of "
									   "the form: account@domain\n", opts->username);
								usage();
							}
						}
					} else {
						err = true;
						optionNeedsArg(argv[i]);
					}
					break;
#if !defined(WIN32)
				case 'f':
					if (i+1 >= argc) {
						err = true;
						optionNeedsArg(argv[i]);
					}
					opts->password_file = argv[i+1];
					i++;
					opts->mode = ADD_MODE;
					break;
#endif
				case 'd':
					dprintf_set_tool_debug("TOOL", 0);
					break;
				case 'h':
					opts->help = true;
					break;
				default:
					err = true;
					badOption(argv[i]);
			}
			break;	// break for case '-'
		default:
			err = true;
			badCommand(argv[i]);
			break;
		}
	}

	return !err;
}

void
badCommand(const char* command) {
	fprintf(stderr, "ERROR: Unrecognized command - '%s'\n\n", command);
	usage();
}

void
badOption(const char* option) {
	fprintf(stderr, "ERROR: Unrecognized option - '%s'\n\n", option);
	usage();
}

void
optionNeedsArg(const char* option)
{
	fprintf(stderr, "ERROR: Option '%s' requires an argument\n\n", option);
	usage();
}

void
usage()
{
	fprintf( stderr, "Usage: %s [options] action\n", MyName );
	fprintf( stderr, "  where action is one of:\n" );
	fprintf( stderr, "    add               (Add credential to secure storage)\n" );
	fprintf( stderr, "    delete            (Remove credential from secure storage)\n" );
	fprintf( stderr, "    query             (Check if a credential has been stored)\n" );
	fprintf( stderr, "  and where [options] is one or more of:\n" );
	fprintf( stderr, "    -u username       (use the specified username)\n" );
	fprintf( stderr, "    -c                (update/query the condor pool password)\n");
	fprintf( stderr, "    -p password       (use the specified password rather than prompting)\n" );
	fprintf( stderr, "    -n name           (update/query to the named machine)\n" );
#if !defined(WIN32)
	fprintf( stderr, "    -f filename       (generate a pool password file)\n" );
#endif
	fprintf( stderr, "    -d                (display debugging messages)\n" );
	fprintf( stderr, "    -h                (display this message)\n" );
	fprintf( stderr, "\n" );

	exit( 1 );
};

bool
goAheadAnyways()
{
	printf("WARNING: Continuing will result in your password "
		   "being sent in the clear!\n"
		   "  Do you want to continue? [y/N] ");
	fflush(stdout);

	const int BUFSIZE = 10;
	char buf[BUFSIZE];
	bool result = read_from_keyboard(buf, BUFSIZE);
	printf("\n\n");
	if (!result) {
		return false;
	}
	if ((buf[0] == 'y') || (buf[0] == 'Y')) {
		return true;
	}
	return false;
}
