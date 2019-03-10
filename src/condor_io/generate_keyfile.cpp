
#include "condor_auth_passwd.h"
#include "classad/classad.h"
#include "classad/sink.h"
#include "match_prefix.h"
#include "CondorError.h"
#include "condor_config.h"

void print_usage(const char *argv0) {
	fprintf(stderr, "Usage: %s -identity USER@UID_DOMAIN [-key KEYID]\n\n"
		"Generates a derived key from the local master password and prints its contents to stdout.\n", argv0);
	exit(1);
}

int main(int argc, char *argv[]) {

	if (argc < 3) {
		print_usage(argv[0]);
	}

	std::string identity;
	std::string key = "POOL";
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
	if (!Condor_Auth_Passwd::generate_derived_key(identity, key, token, &err)) {
		fprintf(stderr, "Failed to generate a derived key.\n");
		fprintf(stderr, "%s\n", err.getFullText(true).c_str());
		exit(2);
	}

	printf("%s\n", token.c_str());
	return 0;
}
