
#include "condor_auth_passwd.h"
#include "classad/classad.h"
#include "classad/sink.h"
#include "match_prefix.h"
#include "CondorError.h"
#include "condor_config.h"
#include "daemon.h"
#include "dc_collector.h"

namespace {

void print_usage(const char *argv0) {
	fprintf(stderr, "Usage: %s -identity USER@UID_DOMAIN [-key KEYID] [-authz AUTHZ] [-lifetime VAL]\n"
		"Alternate usage: %s ([-type TYPE]|[-name NAME]|[-pool POOL]) [-authz AUTHZ] [-lifetime VAL]\n\n"
		"Generates a derived key from the local master password or remote session and prints its contents to stdout.\n", argv0, argv0);
	exit(1);
}

int
generate_remote_token(const std::string &pool, const std::string &name, daemon_t dtype,
	const std::vector<std::string> &authz_list, long lifetime)
{
	std::unique_ptr<Daemon> daemon;
	if (!pool.empty()) {
		DCCollector col(pool.c_str());
		if (!col.addr()) {
			fprintf(stderr, "ERROR: %s\n", col.error());
			exit(1);
		}
		daemon.reset(new Daemon( dtype, name.c_str(), col.addr() ));
	} else {
		daemon.reset(new Daemon( dtype, name.c_str() ));
	}

	if (!(daemon->locate(Daemon::LOCATE_FOR_LOOKUP))) {
		if (!name.empty()) {
			fprintf(stderr, "ERROR: couldn't locate daemon %s!\n", name.c_str());
		} else {
			fprintf(stderr, "ERROR: couldn't locate default daemon type.\n");
		}
		exit(1);
	}

	CondorError err;
	std::string token;
	if (!daemon->getSessionToken(authz_list, lifetime, token, &err)) {
		fprintf(stderr, "Failed to request a session token: %s\n", err.getFullText().c_str());
		exit(1);
	}
	printf("%s\n", token.c_str());
	return 0;
}

}


int main(int argc, char *argv[]) {

	if (argc < 3) {
		print_usage(argv[0]);
	}

	daemon_t dtype = DT_NONE;
	std::string pool;
	std::string name;
	std::string identity;
	std::string key = "POOL";
	std::vector<std::string> authz_list;
	long lifetime = -1;
	for (int i = 1; i < argc; i++) {
		if (is_dash_arg_prefix(argv[i], "identity", 2)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -identity requires a condor identity as an argument\n", argv[0]);
				exit(1);
			}
			identity = argv[i];
		} else if (is_dash_arg_prefix(argv[i], "key", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -key requires a key ID as an argument\n", argv[0]);
				exit(1);
			}
			key = argv[i];
		} else if (is_dash_arg_prefix(argv[i], "authz", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -authz requires an authorization name argument\n", argv[0]);
				exit(1);
			}
			authz_list.push_back(argv[i]);
		} else if (is_dash_arg_prefix(argv[i], "lifetime", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -lifetime requires an integer in seconds.\n", argv[0]);
				exit(1);
			}
			try {
				lifetime = std::stol(argv[i]);
			} catch (...) {
				fprintf(stderr, "%s: Invalid argument for -lifetime: %s.\n", argv[0], argv[i]);
				exit(1);
			}
		} else if (is_dash_arg_prefix(argv[i], "pool", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -pool requires a pool name argument.\n", argv[0]);
				exit(1);
			}
			pool = argv[i];
		} else if (is_dash_arg_prefix(argv[i], "name", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -name requires a daemon name argument.\n", argv[0]);
				exit(1);
			}
			name = argv[i];
		} else if (is_dash_arg_prefix(argv[i], "type", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -type requires a daemon type argument.\n", argv[0]);
				exit(1);
			}
			dtype = stringToDaemonType(argv[i]);
			if( dtype == DT_NONE) {
				fprintf(stderr, "ERROR: unrecognized daemon type: %s\n", argv[i]);
				print_usage(argv[0]);
				exit(1);
			}
		} else if(!strcmp(argv[i],"-debug")) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", 0);
		} else if (is_dash_arg_prefix(argv[i], "help", 1)) {
			print_usage(argv[0]);
			exit(1);
		} else {
			fprintf(stderr, "%s: Invalid command line argument: %s\n", argv[0], argv[i]);
			print_usage(argv[0]);
			exit(1);
		}
	}

	config();

	if ((dtype != DT_NONE) || (!name.empty())) {
		if (dtype == DT_NONE) { dtype = DT_SCHEDD; }
		return generate_remote_token(pool, name, dtype, authz_list, lifetime);
	}

	CondorError err;
	std::string token;
	if (!Condor_Auth_Passwd::generate_derived_key(identity, key, authz_list, lifetime, token, &err)) {
		fprintf(stderr, "Failed to generate a derived key.\n");
		fprintf(stderr, "%s\n", err.getFullText(true).c_str());
		exit(2);
	}

	printf("%s\n", token.c_str());
	return 0;
}
