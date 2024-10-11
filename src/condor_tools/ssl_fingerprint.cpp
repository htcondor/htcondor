/***************************************************************
 *
 * Copyright (C) 2022, HTCondor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_distribution.h"

#include "ca_utils.h"
#include "CondorError.h"
#include "fcloser.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <iomanip>
#include <memory>
#include <string>
#include <unordered_set>

namespace {

void print_usage(FILE* fp, const char *argv0)
{
	fprintf(fp, "Usage: %s [FILE]\n"
		"Prints the fingerprint of the first X509 certificate found at FILE or\n"
		"the contents of a condor known_hosts file.\n"
		"\n"
		"If FILE is not provided, the prints the user's default known_hosts file.\n"
		"\n", argv0);
}

int print_known_hosts_file(std::string desired_fname = "")
{
	const auto &fname = desired_fname.empty() ? htcondor::get_known_hosts_filename() : desired_fname;

	std::unique_ptr<FILE, fcloser> fp(fopen(fname.c_str(), "r"));
	if (!fp) {
		fprintf(stderr, "Failed to open %s for reading: %s (errno=%d)\n",
			fname.c_str(), strerror(errno), errno);
		exit(2);
	}

	std::unordered_set<std::string> observed_hostnames;

	for (std::string line; readLine(line, fp.get(), false);)
	{
		trim(line);
		if (line.empty() || line[0] == '#') continue;

		std::vector<std::string> tokens = split(line, " ");
		if (tokens.size() < 3 || !tokens[0].size() || !tokens[1].size()) {
			fprintf(stderr, "No PEM-formatted certificate found; incorrect format for known_hosts file.\n");
			exit(5);
		}

		bool permitted = tokens[0][0] != '!';
		std::string hostname = permitted ? tokens[0] : tokens[0].substr(1);

		auto result = observed_hostnames.insert(hostname);
		// Only print out the first match for each condor name.
		if (!result.second) {continue;}

		std::string method_info = tokens[2];
		if (tokens[1] == "SSL" && method_info != "@trusted") {
			CondorError err;
			auto cert = htcondor::load_x509_from_b64(method_info, err);
			if (!cert) {
				fprintf(stderr, "Failed to parse the X.509 certificate for %s.\n",
					hostname.c_str());
				fprintf(stderr, "%s\n", err.getFullText(true).c_str());
				exit(6);
			}
			std::string fingerprint;
			if (!htcondor::generate_fingerprint(cert.get(), fingerprint, err)) {
				fprintf(stderr, "Failed to generate fingerprint for %s.\n", hostname.c_str());
				fprintf(stderr, "%s\n", err.getFullText(true).c_str());
				exit(7);
			}
			method_info = fingerprint;
		} else {
			continue;
		}
		printf("%c%s %s %s\n", permitted ? ' ' : '!', hostname.c_str(), tokens[1].c_str(),
			method_info.c_str());
	}
	return 0;
}

}


int main(int argc, const char *argv[]) {

	set_priv_initialize();
	config();

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
	ERR_load_crypto_strings();
	OpenSSL_add_all_digests();
#endif

	if (argc == 1) {
		return print_known_hosts_file();
	} else if (argc != 2) {
		print_usage(stdout, argv[0]);
		exit(1);
	}

	std::unique_ptr<FILE, fcloser> fp(fopen(argv[1], "r"));
	if (!fp) {
		fprintf(stderr, "Failed to open %s for reading: %s (errno=%d)\n",
			argv[1], strerror(errno), errno);
		exit(2);
	}

	std::unique_ptr<X509, decltype(&X509_free)> cert(PEM_read_X509(fp.get(), NULL, NULL, NULL), X509_free);
	if (!cert) {
		return print_known_hosts_file(argv[1]);
	}

	std::string fingerprint;
	CondorError err;
	if (!htcondor::generate_fingerprint(cert.get(), fingerprint, err)) {
		fprintf(stderr, "Failed to generate fingerprint from %s:\n", argv[1]);
		fprintf(stderr, "%s\n", err.getFullText(true).c_str());
		exit(err.code() ? err.code() : 4);
	}

	fprintf(stderr, "%s\n", fingerprint.c_str());

	return 0;
}

