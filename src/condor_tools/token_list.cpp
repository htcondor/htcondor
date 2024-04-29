/***************************************************************
 *
 * Copyright (C) 2019, HTCondor Team, Computer Sciences Department,
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

#include "match_prefix.h"
#include "CondorError.h"
#include "condor_regex.h"
#include "directory.h"

// The GCC_DIAG_OFF() disables warnings so that we can build on our
// -Werror platforms.
//
// Older Clang compilers on macOS define __cpp_attributes but not
//   __has_cpp_attribute.
// LibreSSL advertises itself as OpenSSL 2.0.0
//   (OPENSSL_VERSION_NUMBER 0x20000000L), but doesn't have some
//   functions introduced in OpenSSL 1.1.0.
// OpenSSL 0.9.8 (used on older macOS versions) doesn't have
//    RSA_verify_PKCS1_PSS_mgf1() or RSA_padding_add_PKCS1_PSS_mgf1().
//    But since jwt calls them using the same value for the Hash and
//    mgf1Hash arguments, we can use the non-mgf1 versions of these
//    functions, which are available.

#include <openssl/opensslv.h>

#if defined(LIBRESSL_VERSION_NUMBER)
#define OPENSSL10
#endif

#if defined(__cpp_attributes) && !defined(__has_cpp_attribute) && !defined(WIN32)
#undef __cpp_attributes
#endif

#if OPENSSL_VERSION_NUMBER < 0x10000000L
#include <openssl/rsa.h>
static int RSA_verify_PKCS1_PSS_mgf1(RSA *rsa, const unsigned char *mHash,
        const EVP_MD *Hash, const EVP_MD *mgf1Hash,
        const unsigned char *EM, int sLen)
{ return RSA_verify_PKCS1_PSS(rsa, mHash, Hash, EM, sLen); }
static int RSA_padding_add_PKCS1_PSS_mgf1(RSA *rsa, unsigned char *EM,
        const unsigned char *mHash,
        const EVP_MD *Hash, const EVP_MD *mgf1Hash, int sLen)
{ return RSA_padding_add_PKCS1_PSS(rsa, EM, mHash, Hash, sLen); }
#endif

GCC_DIAG_OFF(float-equal)
GCC_DIAG_OFF(cast-qual)
#include "jwt-cpp/jwt.h"
GCC_DIAG_ON(float-equal)
GCC_DIAG_ON(cast-qual)

#include <fstream>
#include <stdio.h>

namespace {

void print_usage(FILE* fp, const char *argv0) {
	fprintf(fp, "Usage: %s [-dir PATH]\n"
		"Lists the tokens available to the current user.\n"
		"\noptions:\n"
		"    -dir  <path>  Look in the given directory for tokens\n"
		"    -file  <filename>  Look in the given file for a token\n"
		"    -help         print this message\n"
		"\nIf no -dir agument is supplied, tokens are located by using the condor config files.\n"
		"Ordinary users will use SEC_TOKEN_DIRECTORY or ~/tokens.d, The root user will use SEC_TOKEN_SYSTEM_DIRECTORY."
		"\n", argv0);
}

bool printToken(const std::string &tokenfilename) {

	dprintf(D_SECURITY|D_FULLDEBUG, "TOKEN: Will use examine tokens found in %s.\n",
		tokenfilename.c_str());
	
	FILE * f = safe_fopen_no_create( tokenfilename.c_str(), "r" );
	if( f == NULL ) {
		dprintf(D_ALWAYS, "Failed to open token file '%s': %d (%s)\n",
			tokenfilename.c_str(), errno, strerror(errno));
		return false;
	}

	int valid_tokens = 0;
	int invalid_lines = 0;
	for( std::string line; readLine( line, f, false ); ) {
		trim(line);  // remove leading and trailing whitespace incl ending /r and /n
		if (line.empty() || line[0] == '#') {
			continue;
		}
		try {
			auto decoded_jwt = jwt::decode(line);
			printf("Header: %s Payload: %s File: %s\n", decoded_jwt.get_header().c_str(),
				decoded_jwt.get_payload().c_str(),
				tokenfilename.c_str());
			++valid_tokens;
		} catch (...) {
			++invalid_lines;
		}
	}
	if (invalid_lines) {
		if (valid_tokens) {
			dprintf(D_ALWAYS, "Failed to decode %d JWT lines in keyfile '%s'\n", invalid_lines, tokenfilename.c_str());
		} else {
			dprintf(D_FULLDEBUG, "file '%s' has no tokens\n", tokenfilename.c_str());
		}
	}
	return true;
}

bool
printAllTokens(const char * token_dir) {
	std::string dirpath(token_dir?token_dir:"");
	if (dirpath.empty()) {
        if (!param(dirpath, "SEC_TOKEN_DIRECTORY")) {
            std::string file_location;
            if (!find_user_file(file_location, "tokens.d", false, false)) {
                param(dirpath, "SEC_TOKEN_SYSTEM_DIRECTORY");
            } else {
                dirpath = file_location;
			}
		}
	}
	dprintf(D_FULLDEBUG, "Looking for tokens in directory %s\n", dirpath.c_str());

	int _errcode, _erroffset;
	std::string excludeRegex;
		// We simply fail invalid regex as the config subsys should have EXCEPT'd
		// in this case.
	if (!param(excludeRegex, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP")) {
		dprintf(D_FULLDEBUG, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP is unset");
		return false;
	}
	Regex excludeFilesRegex;   
	if (!excludeFilesRegex.compile(excludeRegex, &_errcode, &_erroffset)) {
		dprintf(D_FULLDEBUG, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP "
			"config parameter is not a valid "
			"regular expression.  Value: %s,  Error Code: %d",
			excludeRegex.c_str(), _errcode);
		return false;
	}
	if(!excludeFilesRegex.isInitialized() ) {
		dprintf(D_FULLDEBUG, "Failed to initialize exclude files regex.");
		return false;
	}

	Directory dir(dirpath.c_str());
	if (!dir.Rewind()) {
		dprintf(D_SECURITY, "Cannot open %s: %s (errno=%d)",
			dirpath.c_str(), strerror(errno), errno);
		return false;
	}

	const char *file;
	while ( (file = dir.Next()) ) {
		if (dir.IsDirectory()) {
			continue;
		}
		if(!excludeFilesRegex.match(file)) {
			printToken(dir.GetFullPath());
		} else {
			dprintf(D_FULLDEBUG|D_SECURITY, "Ignoring token file "
				"based on LOCAL_CONFIG_DIR_EXCLUDE_REGEXP: "
				"'%s'\n", dir.GetFullPath());
		}
	}
	return true;
}

}


int main(int argc, const char *argv[]) {

	const char * token_dir = nullptr;
	const char * pcolon;

	set_priv_initialize();
	config();

	for (int i = 1; i < argc; i++) {
		if (is_dash_arg_colon_prefix(argv[i], "debug", &pcolon, 1)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		} else if (is_dash_arg_prefix(argv[i], "help", 1)) {
			print_usage(stdout, argv[0]);
			exit(0);
		} else if (is_dash_arg_prefix(argv[i], "dir", 1)) {
			if (!argv[i+1]) {
				fprintf(stderr, "%s: -dir requires path argument.\n", argv[0]);
				print_usage(stderr, argv[0]);
				exit(1);
			}
			token_dir = argv[++i];
		} else if (is_dash_arg_prefix(argv[i], "file", 1)) {
			if (!argv[i+1]) {
				fprintf(stderr, "%s: -file requires file argument.\n", argv[0]);
				print_usage(stderr, argv[0]);
				exit(1);
			}
			const char *token_file = argv[++i];
			printToken(token_file);
			exit(0);

		} else {
			fprintf(stderr, "%s: Invalid command line argument: %s\n", argv[0], argv[i]);
			print_usage(stderr, argv[0]);
			exit(1);
		}
	}

	printAllTokens(token_dir);
	return 0;
}
