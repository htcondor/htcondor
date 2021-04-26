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
#include "secure_file.h"
#include "match_prefix.h"
#include "iso_dates.h"

#if defined(WIN32)
#include "lsa_mgr.h"  // for CONFIG_MODE
#endif

const int MODE_MASK = 3;
const int CRED_TYPE_MASK = 0x2c;

const char * cred_type_name(int mode) {
	switch (mode & CRED_TYPE_MASK) {
	case 0:
	case STORE_CRED_USER_PWD: return "password";
	case STORE_CRED_USER_KRB: return "kerberos";
	case STORE_CRED_USER_OAUTH: return "oauth";
	}
	return "unknown";
}

struct StoreCredOptions {
	int mode;
	char username[MAX_PASSWORD_LENGTH + 1];
	char *daemonname;
	const char *pw; // password if supplied on the command line
	const char *service; // service name from the -s argument
	const char *handle; // handle name from the -H argument
	const char *scopes;  // scopes from the -S argument
	const char *audience; // audience from the -A argument
	const char *credential_file;
	const char *pool_password_file;
	bool help;
};

const char *MyName;
void usage(void);
bool parseCommandLine(StoreCredOptions *opts, int argc, const char *argv[]);
void badOption(const char* option);
void badCommand(const char* command);
void optionNeedsArg(const char* option, const char * type);
bool goAheadAnyways();

int main(int argc, const char *argv[]) {
	
	std::string my_full_name;
	const char *full_name;
	struct StoreCredOptions options;
	int result = FAILURE_ABORTED;
	bool pool_password_arg = false;
	bool is_query = false;
	Daemon *daemon = NULL;
	auto_free_ptr credd_host;
	ClassAd cred_info; // additional info passed in to do_store_cred
	int cred_type = 0;
	time_t cred_timestamp = 0;
	ClassAd return_ad;
	const char * errret = NULL;
	char cred_time[ISO8601_DateAndTimeBufferMax];

	MyName = condor_basename(argv[0]);
	
	// load up configuration file
	myDistro->Init( argc, argv );
	set_priv_initialize(); // allow uid switching if root
	config();

	if (!parseCommandLine(&options, argc, argv)) {
		goto cleanup;
	}
	cred_type = (options.mode & CRED_TYPE_MASK);

	// if -h was given, just print usage
	if (options.help || (options.mode < 0 && options.pool_password_file == NULL)) {
		usage();
		goto cleanup;
	}

#if !defined(WIN32)
	// if the -f argument was given, we just want to prompt for a password
	// and write it to a file (in our weirdo XORed format)
	//
	if (options.pool_password_file != NULL) {
		auto_free_ptr pw;
		if (!options.pw || options.pw[0] == '\0') {
			pw.set(get_password());
			printf("\n");
		}
		else {
			pw.set(strdup(options.pw));
			//SecureZeroMemory(options.pw, MAX_PASSWORD_LENGTH + 1);
		}
		result = write_password_file(options.pool_password_file, pw);
		SecureZeroMemory(pw.ptr(), strlen(pw.ptr()));
		if (result != SUCCESS) {
			fprintf(stderr,
			        "error writing password file: %s\n",
			        options.pool_password_file);
		}
		goto cleanup;
	}
#endif

	// determine the username to use
	if ( strcmp(options.username, "") == 0 ) {
		if (cred_type == STORE_CRED_USER_KRB || cred_type == STORE_CRED_USER_OAUTH) {
			// for KRB and OAuth pass an empty username to indicate that we want
			// the Credd to use the mapped authenticated username
			my_full_name = "";
		} else {
			// default to current user and domain
			auto_free_ptr my_name(my_username());
			auto_free_ptr my_domain(my_domainname());
			formatstr(my_full_name, "%s@%s", my_name.ptr(), my_domain.ptr());
		}
	} else if (strcmp(options.username, POOL_PASSWORD_USERNAME) == 0) {
#if !defined(WIN32)
		// we don't support querying the pool password on UNIX
		// (since it is not possible to do so through the
		//  STORE_POOL_CRED command)
		if ((options.mode&MODE_MASK) == GENERIC_QUERY) {
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
		formatstr(my_full_name, POOL_PASSWORD_USERNAME "@%s", domain);
		free(domain);
	} else {
			// username was specified on the command line
		my_full_name = options.username;
	}
	// print the account, if we haven't looked up the account because we want the credd
	// to figure it out print the name of the current user just as a double check
	full_name = my_full_name.c_str();
	if (my_full_name.empty()) {
		auto_free_ptr user(my_username());
		printf("Account: <current> (%s)\n", user.ptr());
	} else {
		printf("Account: %s\n", full_name);
	}
	printf("CredType: %s\n\n", cred_type_name(options.mode));

	// setup cred_info classad
	if (options.service) {
		cred_info.Assign("service", options.service);
	}
	if (options.handle) {
		cred_info.Assign("handle", options.handle);
	}
	if (options.scopes) {
		cred_info.Assign("scopes", options.scopes);
	}
	if (options.audience) {
		cred_info.Assign("audience", options.audience);
	}

	// determine where to direct our command
	daemon = NULL;
	credd_host.set(param("CREDD_HOST"));
	is_query = (options.mode & MODE_MASK) == GENERIC_QUERY;
	if (options.daemonname != NULL) {
		if (pool_password_arg && ! is_query) {
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
	else if (!pool_password_arg && credd_host) {
		// no daemon given, use credd for user passwords if CREDD_HOST is defined
		// (otherwise, we use the local schedd)
		//printf("sending command to CREDD: %s\n", credd_host);
		daemon = new Daemon(DT_CREDD);
	}
	else if (cred_type == STORE_CRED_USER_KRB || cred_type == STORE_CRED_USER_OAUTH) {
		// only a CREDD can process these types of creds (for now?), use a local CREDD
		daemon = new Daemon(DT_CREDD);
	}
	else {
		//printf("sending command to local daemon\n");
	}

	switch (options.mode & MODE_MASK) {
		case GENERIC_ADD: {
			auto_free_ptr cred;
			size_t credlen = 0;
			if (options.credential_file) {
				if (MATCH == strcmp(options.credential_file, "-")) {
					int max_len = 0x1000 * 0x1000; // max read from stdin is 1 Mb
					cred.set((char*)malloc(max_len));
					#ifdef _WIN32
					// disable CR+LF munging of the stdin stream, we want to treat it as binary data
					_setmode(_fileno(stdin), _O_BINARY);
					#endif
					credlen = _condor_full_read(fileno(stdin), cred.ptr(), max_len);
					if (((ssize_t)credlen) < 0) {
						fprintf(stderr, "ERROR: could read from stdin: %s\n", strerror(errno));
						goto cleanup;
					}
				} else {
					char *buf = NULL;
					bool read_as_root = false;
					int verify_opts = 0;
					if ( ! read_secure_file(options.credential_file, (void**)&buf, &credlen, read_as_root, verify_opts)) {
						fprintf(stderr, "Could not read credential from %s\n", options.credential_file);
						result = FAILURE;
					} else {
						cred.set(buf);
					}
				}
			} else if (options.pw && options.pw[0]) {
				cred.set(strdup(options.pw));
				credlen = strlen(options.pw);
			} else if (cred_type == STORE_CRED_USER_KRB) {
				//TODO: run the SEC_CREDENTIAL_PRODUCER (if not root!) here?
				result = FAILURE;
			} else if (cred_type == STORE_CRED_USER_OAUTH) {
				//TODO: run the SEC_CREDENTIAL_PRODUCER (if not root!) here?
				result = FAILURE;
			} else {
				cred.set(get_password());
				credlen = strlen(cred);
			}

			if (cred.ptr() && credlen) {
				result = do_store_cred(full_name, options.mode, (const unsigned char*)cred.ptr(), (int)credlen, return_ad, &cred_info, daemon);
				if ((result == FAILURE_NOT_SECURE) && goAheadAnyways()) {
						// if user is ok with it, send the password in the clear
					result = do_store_cred(full_name, options.mode, (const unsigned char*)cred.ptr(), (int)credlen, return_ad, &cred_info, daemon);
				}
				SecureZeroMemory(cred.ptr(), credlen);
			}
		} break;

		case GENERIC_DELETE:
		case GENERIC_QUERY:
			result = do_store_cred(full_name, options.mode, NULL, 0, return_ad, &cred_info, daemon);
			break;
#if defined(WIN32)
		case GENERIC_CONFIG:
			return interactive();
			break;
#endif
		default:
			fprintf(stderr, "Internal error\n");
			goto cleanup;
	}

	// the result is a bit overloaded,  it could be an error code, or it might be a timestamp
	// unpack that here.
	cred_time[0] = 0;
	cred_timestamp = 0;
	if ( ! store_cred_failed(result, options.mode, &errret) && result > 100) {
		cred_timestamp = result;
		time_to_iso8601(cred_time, *localtime(&cred_timestamp), ISO8601_ExtendedFormat, ISO8601_DateAndTime, false);
		cred_time[10] = ' ';
		result = SUCCESS;
	}

	// output result of operation
	switch (result) {
		case SUCCESS_PENDING:
		case SUCCESS:
			if (is_query) {
				const char * pending = (result == SUCCESS_PENDING) ? " and is waiting for processing" : " and is valid";
				if (cred_timestamp) {
					printf("A credential was stored on %s%s.\n", cred_time, pending);
				} else {
					printf("A credential was stored%s.\n", pending);
				}
			} else {
				const char * pending = (result == SUCCESS_PENDING) ? " and is waiting for processing" : "";
				if (((options.mode & MODE_MASK) == GENERIC_ADD) && cred_timestamp) {
					printf("Operation succeeded%s. Credential was stored on %s\n", pending, cred_time);
				} else {
					printf("Operation succeeded%s.\n", pending);
				}
			}
			if (return_ad.size()) {
				printf("\nCredential info:\n");
				fPrintAd(stdout, return_ad);
				printf("\n");
			}
			break;

		case FAILURE:
			printf("Operation failed.\n");
			if (pool_password_arg && ! is_query) {
				printf("    Make sure you have CONFIG access to the target Master.\n");
			}
			else {
				printf("    Make sure your ALLOW_WRITE setting includes this host.\n");
			}
			break;

		case FAILURE_BAD_PASSWORD:
			if (is_query) {
				printf("A credential is stored, but it is invalid. Run 'condor_store_cred add' again.\n");
			}
			else {
				printf("Operation failed: bad password.\n");
			}
			break;

		case FAILURE_NOT_FOUND:
			if (is_query) {
				printf("No credential is stored.\n");
			}
			else {
				printf("Operation failed: username not found.\n");
			}
			break;

		case FAILURE_NO_IMPERSONATE:
			printf("Operation failed.\n"
			       "    The target daemon is not running as SYSTEM and cannot store credentials securely.\n");
			break;

		default:
			if (errret) {
				fprintf(stderr, "%s\n", errret);
			} else {
				fprintf(stderr, "Operation failed: unknown error code %d\n", (int)result);
			}
			break;
	}

cleanup:
	if (options.daemonname) {
		free(options.daemonname);
	}
	if (daemon) {
		delete daemon;
	}
	
	if ( result == SUCCESS || result == SUCCESS_PENDING ) {
		return 0;
	} else if ( result == FAILURE_CRED_MISMATCH ) {
		return 2;
	} else {
		return 1;
	}
}

static bool qualify_mode(int & mode, const char * tag)
{
	// it's ok for there to be no qualifier
	if ( ! tag) {
		return true;
	}

	std::string utag(tag);
	lower_case(utag);
	tag = utag.c_str();

	if (is_dash_arg_prefix(tag, "pwd") || is_dash_arg_prefix(tag, "password", 2)) {
		mode |= STORE_CRED_USER_PWD;
	} else if (is_dash_arg_prefix(tag, "krb", 2) || is_dash_arg_prefix(tag, "kerberos", 2)) {
		mode |= STORE_CRED_USER_KRB;
	} else if (is_dash_arg_prefix(tag, "oauth", 2) || is_dash_arg_prefix(tag, "oath", 2) || is_dash_arg_prefix(tag, "scitokens", 3)) {
		mode |= STORE_CRED_USER_OAUTH;
	} else {
		return false;
	}
	return true;
}

bool
parseCommandLine(StoreCredOptions *opts, int argc, const char *argv[])
{
	bool no_wait = false;
	char arg_prefix[16];
	opts->mode = -1;
	opts->daemonname = NULL;
	opts->credential_file = NULL;
	opts->pool_password_file = NULL;;
	opts->pw = NULL;
	opts->service = NULL;
	opts->handle = NULL;
	opts->scopes = NULL;
	opts->audience = NULL;
	opts->help = false;
	SecureZeroMemory(opts->username, sizeof(opts->username));

	bool err = false;
	for (int ix = 1; ix < argc && ! err; ++ix) {
		// argument may be something like "add-krb", in which case we want to split it into "add" and "-krb"
		const char * arg = argv[ix];
		const char * dasharg = strchr(arg, '-');
		if (dasharg && (dasharg != arg)) {
			strncpy(arg_prefix, arg, sizeof(arg_prefix)-1);
			arg_prefix[dasharg - arg] = 0;
			arg = arg_prefix;
		}
		switch(*arg) {
		case 'a':
		case 'A':	// Add
			if (strcasecmp(arg, ADD_CREDENTIAL) == MATCH) {
				if (opts->mode < 0) {
					opts->mode = GENERIC_ADD;
					if ( ! qualify_mode(opts->mode, dasharg)) {
						fprintf(stderr, "ERROR: %s is not a known credential type\n", dasharg);
						usage();
						err = true;
					}
				}
				else if ((opts->mode&MODE_MASK) != GENERIC_ADD) {
					fprintf(stderr, "ERROR: exactly one command must be provided\n");
					usage();
					err = true;
				}
			} else {
				err = true;
				badCommand(arg);
			}	
			break;
		case 'd':	
		case 'D':	// Delete
			if (strcasecmp(arg, DELETE_CREDENTIAL) == MATCH) {
				if (opts->mode < 0) {
					opts->mode =  GENERIC_DELETE;
					if ( ! qualify_mode(opts->mode, dasharg)) {
						fprintf(stderr, "ERROR: %s is not a known credential type\n", dasharg);
						usage();
						err = true;
					}
				}
				else if ((opts->mode&MODE_MASK) != GENERIC_DELETE) {
					fprintf(stderr,
					        "ERROR: exactly one command must be provided\n");
					usage();
					err = true;
				}
			} else {
				err = true;
				badCommand(arg);
			}	
			break;
		case 'q':	
		case 'Q':	// tell me if I have anything stored
			if (strcasecmp(arg, QUERY_CREDENTIAL) == MATCH) {
				if (opts->mode < 0) {
					opts->mode = GENERIC_QUERY;
					if ( ! qualify_mode(opts->mode, dasharg)) {
						fprintf(stderr, "ERROR: %s is not a known credential type\n", dasharg);
						usage();
						err = true;
					}
				}
				else if ((opts->mode&MODE_MASK) != GENERIC_QUERY) {
					fprintf(stderr,
					        "ERROR: exactly one command must be provided\n");
					usage();
					err = true;
				}
			} else {
				err = true;
				badCommand(arg);
			}	
			break;
#if defined(WIN32)
		case 'c':	
		case 'C':	// Config
			if (strcasecmp(arg, CONFIG_CREDENTIAL) == MATCH) {
				if (opts->mode < 0) {
					opts->mode = GENERIC_CONFIG;
					if ( ! qualify_mode(opts->mode, dasharg)) {
						fprintf(stderr, "ERROR: %s is not a known credential type\n", dasharg);
						usage();
						err = true;
					}
				}
				else {
					fprintf(stderr, "ERROR: exactly one command must be provided\n");
					usage();
					err = true;
				}
			} else {
				err = true;
				badCommand(arg);
			}	
			break;
#endif
		case '-':
			// various switches
			switch (arg[1]) {
				case 'n':
					if (ix+1 < argc) {
						if (opts->daemonname != NULL) {
							fprintf(stderr, "ERROR: only one '-n' arg may be provided\n");
							usage();
							err = true;
						}
						else {
							opts->daemonname = get_daemon_name(argv[ix+1]);
							if (opts->daemonname == NULL) {
								fprintf(stderr, "ERROR: %s is not a valid daemon name\n",
									argv[ix+1]);
								err = true;
							}
							++ix;
						}
					} else {
						err = true;
						optionNeedsArg(arg, "name");
					}
					break;

				case 'p':
					if (ix+1 < argc) {
						if (opts->pw) {
							fprintf(stderr, "ERROR: only one '-p' arg may be provided\n");
							usage();
							err = true;
						}
						else {
							opts->pw = argv[ix+1];
							++ix;
						}
					} else {
						err = true;
						optionNeedsArg(arg, "password");
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
					if (ix+1 < argc) {
						if (opts->username[0] != '\0') {
							fprintf(stderr, "ERROR: only one of '-s' or '-u' may be provided\n");
							usage();
							err = true;
						}
						else {
							strcpy_len(opts->username, argv[ix+1], COUNTOF(opts->username));
							++ix;
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
						optionNeedsArg(arg, "username");
					}
					break;

				case 'i':
					if (ix+1 < argc) {
						if (opts->credential_file) {
							fprintf(stderr, "ERROR: credential filename already specified\n");
							usage();
							err = true;
						}
						else {
							opts->credential_file = argv[ix + 1];
							++ix;
						}
					} else {
						err = true;
						optionNeedsArg(arg, "filename");
					}
					break;

				case 's':
					if (ix+1 < argc) {
						if (opts->service) {
							fprintf(stderr, "ERROR: OAuth service already specified\n");
							usage();
							err = true;
						}
						else {
							opts->service = argv[ix + 1];
							++ix;
			}
					} else {
						err = true;
						optionNeedsArg(arg, "service");
					}
					break;

				case 'H':
					if (ix+1 < argc) {
						if (opts->handle) {
							fprintf(stderr, "ERROR: OAuth handle already specified\n");
							usage();
							err = true;
						}
						else {
							opts->handle = argv[ix + 1];
							++ix;
			}
					} else {
						err = true;
						optionNeedsArg(arg, "handle");
					}
					break;

				case 'S':
					if (ix+1 < argc) {
						if (opts->scopes) {
							fprintf(stderr, "ERROR: OAuth scopes already specified\n");
							usage();
							err = true;
						}
						else {
							opts->scopes = argv[ix + 1];
							++ix;
			}
					} else {
						err = true;
						optionNeedsArg(arg, "scopes");
					}
					break;

				case 'A':
					if (ix+1 < argc) {
						if (opts->audience) {
							fprintf(stderr, "ERROR: OAuth audience already specified\n");
							usage();
							err = true;
						}
						else {
							opts->audience = argv[ix + 1];
							++ix;
			}
					} else {
						err = true;
						optionNeedsArg(arg, "audience");
					}
					break;


#if !defined(WIN32)
				case 'f':
					if (ix+1 >= argc) {
						err = true;
						optionNeedsArg(arg, "filename");
					}
					opts->pool_password_file = argv[ix+1];
					++ix;
					opts->mode = GENERIC_ADD;
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
					badOption(arg);
			}
			break;	// break for case '-'
		default:
			err = true;
			badCommand(argv[ix]);
			break;
		}
	}

	// do inter-argument validation
	if ( ! err) {
		int cred_type = opts->mode & CRED_TYPE_MASK;
		int op_type = opts->mode & MODE_MASK;
		//int legacy = opts->mode & STORE_CRED_LEGACY;
		if ( ! cred_type) {
			opts->mode |= STORE_CRED_LEGACY_PWD;
		}

		if (op_type == GENERIC_ADD) {
			// Krb and OAuth require a credential file
			if (cred_type == STORE_CRED_USER_KRB || cred_type == STORE_CRED_USER_OAUTH) {
				if (! opts->credential_file) {
					fprintf(stderr, "ERROR: add-krb and add-oauth commands require a credential filename argument\n");
					err = true;
				}
			}
			// when storing Krb credentials, we want to wait for the credmon to process
			if ((cred_type == STORE_CRED_USER_KRB) && ! no_wait) {
				opts->mode |= STORE_CRED_WAIT_FOR_CREDMON;
			}
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
optionNeedsArg(const char* option, const char * type)
{
	fprintf(stderr, "ERROR: Option '%s' requires a %s argument\n\n", option, type);
	usage();
}

void
usage()
{
	fprintf( stderr, "Usage: %s action [options]\n", MyName );
	fprintf( stderr, "  where action is one of:\n" );
	fprintf( stderr, "    add[-type]        Add credential to secure storage\n" );
	fprintf( stderr, "    delete[-type]     Remove credential from secure storage\n" );
	fprintf( stderr, "    query[-type]      Check if a credential has been stored\n" );
	fprintf( stderr, "  and -type is an optional credential type. it must be one of:\n" );
	fprintf( stderr, "    -pwd              Credential is a password (default)\n" );
	fprintf( stderr, "    -krb              Credential is Kerberos/AFS token\n" );
	fprintf( stderr, "    -oauth            Credential is Scitoken or OAuth2 token\n" );
	fprintf( stderr, "  and where [options] is zero or more of:\n" );
	fprintf( stderr, "    -u username       Use the specified username\n" );
	fprintf( stderr, "    -c                Manage the condor pool password\n");
	fprintf( stderr, "    -p <password>     Use the specified password rather than prompting\n" );
	fprintf( stderr, "    -i <filename>     Read the credential from <filename>\n"
	                 "                         If <filename> is -, read from stdin\n"
	);
	fprintf( stderr, "    -s <service>      Add/Remove/Query for the given OAuth2 service\n" );
	fprintf( stderr, "    -H <handle>       Specify a handle for the given OAuth2 service\n" );
	fprintf( stderr, "    -S <scopes>       Add the given OAuth2 comma-separated scopes, or make sure\n" );
	fprintf( stderr, "                         a Query matches\n" );
	fprintf( stderr, "    -A <audience>     Add the given OAuth2 audience, or make sure a Query matches\n" );
	fprintf( stderr, "    -n <name>         Manage credentials on the named machine\n" );
#if !defined(WIN32)
	fprintf( stderr, "    -f <filename>     Write password to a pool password file\n" );
#endif
	fprintf( stderr, "    -d                Display debugging messages\n" );
	fprintf( stderr, "    -h                Display this message\n" );
	fprintf( stderr, "\nIf no -u argument is supplied, the name of the current user will be used.\n"
	                 "The add-krb and add-oauth action must be used with the -i argument to specify\n"
	                 "a file to read the credential from. The add or add-pwd action will prompt for\n"
	                 "a password unless the -p argument is used.\n"
	                 "Using -A or -S with oauth requires the credential to be in JSON format.\n"
	);
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
