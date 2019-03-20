
#include "condor_auth_passwd.h"
#include "match_prefix.h"
#include "CondorError.h"
#include "condor_config.h"
#include "daemon.h"
#include "dc_collector.h"
#include "directory.h"

namespace {

void print_usage(const char *argv0) {
	fprintf(stderr, "Usage: %s -identity USER@DOMAIN [-key KEYID] [-authz AUTHZ] [-lifetime VAL] [-token NAME]\n"
		"Generates a token from the local password.\n"
		"\nToken options:\n"
		"    -authz    <authz>                Whitelist one or more authorization\n"
		"    -lifetime <val>                  Max token lifetime, in seconds\n"
		"    -identity <USER@DOMAIN>          User identity\n"
		"    -key      <KEYID>                Key to use for signing\n"
		"\nOther options:\n"
		"    -token    <NAME>                 Name of token file\n", argv0);
	exit(1);
}

int
write_out_token(const std::string &token_name, const std::string &token)
{
	if (token_name.empty()) {
		printf("%s\n", token.c_str());
		return 0;
	}
	std::string dirpath;
	if (!param(dirpath, "SEC_TOKEN_DIRECTORY")) {
		MyString file_location;
		if (!find_user_file(file_location, "tokens.d", false)) {
			param(dirpath, "SEC_TOKEN_SYSTEM_DIRECTORY");
		} else {
			dirpath = file_location;
		}
	}
	mkdir_and_parents_if_needed(dirpath.c_str(), 0700);

	std::string token_file = dirpath + DIR_DELIM_CHAR + token_name;
	int fd = open(token_file.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0600);
	if (-1 == fd) {
		fprintf(stderr, "Cannot write token to %s: %s (errno=%d)\n", token_file.c_str(),
			strerror(errno), errno);
		return 1;
	}
	auto result = _condor_full_write(fd, token.c_str(), token.size());
	if (result != static_cast<ssize_t>(token.size())) {
		fprintf(stderr, "Failed to write token to %s: %s (errno=%d)\n", token_file.c_str(),
			strerror(errno), errno);
		return 1;
	}
	std::string newline = "\n";
	_condor_full_write(fd, newline.c_str(), 1);
	return 0;
}

}


int main(int argc, char *argv[]) {

	if (argc < 3) {
		print_usage(argv[0]);
	}

	std::string pool;
	std::string name;
	std::string identity;
	std::string key = "POOL";
	std::string token_name;
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
		} else if (is_dash_arg_prefix(argv[i], "token", 2)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -token requires a file name argument.\n", argv[0]);
				exit(1);
			}
			token_name = argv[i];
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

	CondorError err;
	std::string token;
	if (!Condor_Auth_Passwd::generate_token(identity, key, authz_list, lifetime, token, &err)) {
		fprintf(stderr, "Failed to generate a token.\n");
		fprintf(stderr, "%s\n", err.getFullText(true).c_str());
		exit(2);
	}

	return write_out_token(token_name, token);
}
