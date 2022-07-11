/***************************************************************
 *
 * Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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

//#include <windowsx.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <tchar.h>

#include "condor_auth_passwd.h"
#include "match_prefix.h"
#include "CondorError.h"

#include <Shlwapi.h>
#pragma comment(linker, "/defaultlib:Shlwapi.lib")

#include <bcrypt.h>
#pragma comment(linker, "/defaultlib:bcrypt.lib")

// build this as a GUI app rather than as a console app, but still use main() as the entry point.
// so when we run it under the installer we don't see a console window pop up
#pragma comment(linker, "/subsystem:WINDOWS")
#pragma comment(linker, "/entry:mainCRTStartup")

#include <sddl.h> // for ConvertSidtoStringSid

#include <secure_file.h>

// for win8 and later, there a pseudo handles for Impersonation Tokens
HANDLE getCurrentProcessToken () { return (HANDLE)(LONG_PTR) -4; }
HANDLE getCurrentThreadToken () { return (HANDLE)(LONG_PTR) -5; }
HANDLE getCurrentThreadEffectiveToken () { return (HANDLE)(LONG_PTR) -6; }

#define BUFFER_SIZE 256

/* get rid of some boring warnings */
#define UNUSED_VARIABLE(x) x;

// NULL returned if name lookup fails
// caller should free returned name using LocalFree()
PWCHAR get_sid_name(PSID sid, int full_name)
{
	WCHAR name[BUFFER_SIZE], domain[BUFFER_SIZE];
	DWORD cchdomain = BUFFER_SIZE, cchname = BUFFER_SIZE;
	PWCHAR ret = NULL;
	SID_NAME_USE sid_name_use;
	if ( ! LookupAccountSidW(NULL, sid, name, &cchname, domain, &cchdomain, &sid_name_use)) {
		return NULL;
	}
	cchname = lstrlenW(name);
	cchdomain = lstrlenW(domain);
	switch (full_name) {
	case 0:
		ret = (PWCHAR)LocalAlloc(LPTR, (cchname + 1)*sizeof(WCHAR));
		lstrcpyW(ret, name);
		break;
	case 1:
		ret = (PWCHAR)LocalAlloc(LPTR, (cchname + 1 + cchdomain + 1)*sizeof(WCHAR));
		lstrcpyW(ret, domain);
		ret[cchdomain] = '/';
		lstrcpyW(ret + cchdomain + 1, name);
		break;
	default:
		ret = (PWCHAR)LocalAlloc(LPTR, (cchname + 1 + cchdomain + 1)*sizeof(WCHAR));
		lstrcpyW(ret, name);
		ret[cchname] = '@';
		lstrcpyW(ret + cchname + 1, domain);
		break;
	}
	return ret;
}


// if there are at least 85 characters in the charset, we can do 5 output characters for each input character.
// this is used for turning a random signing key into a signing key that has approximately the same amount of
// entropy but is a bit longer - it is not required that this operation be reversible
void ConvertToPrintable(CHAR out[], size_t cout, const UCHAR buf[], size_t cbbuf)
{
	const CHAR charset[] = "0123456789" "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz" "!#$%&()*+-;<=>?@^_`{|}~";
	int cbchars = (int)sizeof(charset) - 1;

	CHAR * pout = out;
	CHAR * poutend = out + cout;
	const UCHAR * p = buf;
	const UCHAR * pend = buf + cbbuf;
	while (p < pend) {
		UINT32 nn = 0;
		for (int ii = 0; ii < 4; ++ii) {
			UCHAR ch = (p < pend) ? *p++ : 0;
			nn <<= 8; nn += ch;
		}
		for (int ii = 0; ii < 5; ++ii) {
			if (pout >= poutend) break;
			*pout++ = charset[nn % cbchars];
			nn /= cbchars;
		}
	}
}

const char* _appname = "win_install_helper";
void print_usage(FILE * fp, int exit_code)
{
	fprintf(fp,
		"usage: %s [-help | [-log] [-sid | -identity] [-token]\n"
		"  -help\t\t\tPrint this message\n"
		"  -log <file>\t\tLog to <file>, - for stdout\n"
		"  -key <name>\t\tUse <name> as signing key name\n"
		"  -sid <sid>\t\tlookup name of <sid> for use as identity\n"
		"  -identity <username>\tuse this name for the token identity\n"
		"  -token <name>\t\tname of token file\n"
		"  -config <file>\t\tPath to root config file. ONLY_ENV if no file\n"
	, _appname);
	exit(exit_code);
}

bool echo_file(FILE *fp, const char *root_config, const char * pre="")
{
	MacroStreamFile rcf;
	MACRO_SET ms;
	std::string errmsg;
	if (rcf.open(root_config, false, ms, errmsg)) {
		const char * line = nullptr;
		do {
			line = rcf.getline(0);
			if (line) {
				fprintf_s(fp, "%s%s\n", pre, line);
			} else {
				rcf.close(ms, 0);
				return true;
			}
		} while (line);
	} else {
		fprintf_s(fp, "Could not not open %s : %s", root_config, errmsg.c_str());
	}
	return false;
}


void arg_needed(const char * arg)
{
	fprintf(stderr, "%s needs an argument\n", arg);
	print_usage(stderr, EXIT_FAILURE);
}

int __cdecl main(int argc, const char * argv[])
{
	_appname = argv[0];

	const char * key_id = "LOCAL";
	const char * ssid = nullptr;
	const char * log = "-";
	const char * root_config = nullptr;
	const char * token_file;
	bool close_log_file = true;
	bool log_only = false;	 // when true, we don't want to return non-zero exit when token create fails (because we want to see the log)
	std::string identity;

	for (int ix = 1; ix < argc; ++ix) {
		if (is_dash_arg_prefix(argv[ix], "help")) {
			print_usage(stdout, EXIT_SUCCESS);
		} else if (is_dash_arg_prefix(argv[ix], "log")) {
			if ( ! argv[ix + 1]) {
				arg_needed("log");
			} else {
				log = argv[++ix];
			}
		} else if (is_dash_arg_prefix(argv[ix], "key")) {
			if (! argv[ix + 1]) {
				arg_needed("key");
			} else {
				key_id = argv[++ix];
			}
		} else if (is_dash_arg_prefix(argv[ix], "sid")) {
			if (! argv[ix + 1]) {
				arg_needed("sid");
			} else {
				ssid = argv[++ix];
				log_only = true;
			}
		} else if (is_dash_arg_prefix(argv[ix], "identity")) {
			if (! argv[ix + 1]) {
				arg_needed("identity");
			} else {
				identity = argv[++ix];
			}
		} else if (is_dash_arg_prefix(argv[ix], "token")) {
			if (! argv[ix + 1]) {
				arg_needed("token");
			} else {
				token_file = argv[++ix];
			}
		} else if (is_dash_arg_prefix(argv[ix], "config")) {
			if (! argv[ix + 1]) {
				arg_needed("config");
			} else {
				root_config = argv[++ix];
			}
		} else {
		#if 0 // for now, do default token generation
			print_usage(stderr, EXIT_FAILURE);
		#endif
		}
	}

	FILE * fp = stdout;
	if (log && MATCH != strcmp(log, "-")) {
		fp = fopen(log, "wb");
		close_log_file = fp != NULL;
	}

	config_continue_if_no_config(true);
	set_priv_initialize();
	if (root_config) {
		fprintf_s(fp, "\nUsing this root config: %s\n", root_config);
		echo_file(fp, root_config, "  ");
		config_host(NULL, CONFIG_OPT_NO_EXIT | CONFIG_OPT_USE_THIS_ROOT_CONFIG, root_config);
	} else {
		config_ex(CONFIG_OPT_NO_EXIT);
	}

	auto_free_ptr key_path;
	if (MATCH == strcasecmp(key_id, "POOL")) {
		key_id = "POOL"; // make sure to use all uppercase POOL for Linux compat
		key_path.set(param("SEC_TOKEN_POOL_SIGNING_KEY_FILE"));
	} else {
		std::string keyfn = std::string("$(SEC_PASSWORD_DIRECTORY:.)\\") + key_id;
		key_path.set(expand_param(keyfn.c_str()));
	}

	// check to see if signing key file exists already if not, then create it.
	void * exist_key = nullptr;
	size_t exist_key_size = 0;
	if (read_secure_file(key_path, &exist_key, &exist_key_size, false, 0)) {
		fprintf_s(fp, "\nSigning key %s exists (%d bytes)", key_path.ptr(), (int)exist_key_size);
	} else {

		UCHAR signing[32];
		ZeroMemory(signing, sizeof(signing));
		NTSTATUS ntr = BCryptGenRandom(NULL, signing, sizeof(signing), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
		if (ntr == STATUS_INVALID_HANDLE) {
			fprintf_s(fp, "BCryptGenRandom - invalid handle\n");
		} else if (ntr == STATUS_INVALID_PARAMETER) {
			fprintf_s(fp, "BCryptGenRandom - invalid parameter\n");
		}

		const int cbpwd = (sizeof(signing) / 4) * 5;
		CHAR pwd[cbpwd + 1];
		ZeroMemory(pwd, sizeof(pwd));
		ConvertToPrintable(pwd, cbpwd, signing, sizeof(signing));
		//fprintf_s(fp, "\nGeneratedSecret=\"%s\"\n", pwd);

		fprintf_s(fp, "Saving signing key to \"%s\"\n", key_path.ptr());
		write_binary_password_file(key_path, pwd, cbpwd);
	}

	if (ssid) {
		PSID sid = nullptr;
		if ( ! ConvertStringSidToSidA(ssid, &sid)) {
			fprintf_s(fp, "ConvertStringSidToSid(%s) error %u\n", ssid, GetLastError());
			exit(EXIT_FAILURE);
		}
		PWCHAR wname = get_sid_name(sid, 2);
		if ( ! wname) {
			fprintf_s(fp, "get_sid_name(%s) error %u\n", ssid, GetLastError());
			exit(EXIT_FAILURE);
		}
		formatstr(identity, "%ls", wname);
		fprintf_s(fp, "%s resolves to identity \"%s\"\n", ssid, identity.c_str());
	}

	std::vector<std::string> authz;
	authz.push_back("ADMINISTRATOR");
	authz.push_back("CONFIG");
	authz.push_back("READ");

	// Token generation uses $(TRUST_DOMAIN) as the Issuer
	// the default for TRUST_DOMAIN is $(COLLECTOR_HOST) which is wrong for
	// the token we are creating here.
	set_live_param_value("TRUST_DOMAIN", "$(FULL_HOSTNAME)");

	if ( ! identity.empty()) {
		CondorError err;
		std::string token;
		if (!Condor_Auth_Passwd::generate_token(identity, key_id, authz, -1, token, 0, &err)) {
			fprintf_s(fp, "Failed to generate a token.\n");
			fprintf_s(fp, "%s\n", err.getFullText(true).c_str());
			if ( ! log_only) { exit(2); }
		} else {
			//fprintf_s(fp, "AdminToken: {%s}\n", token.c_str());
			if (token_file) {
				fprintf_s(fp, "Saving token to \"%s\"\n", token_file);
				write_secure_file(token_file, token.c_str(), token.size(), false, false);
			}
		}
	}

#if 0
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	WCHAR cmd_buf[1024];

	// get the names for "Everyone" (S-1-1-0) and "Administrators" (S-1-5-32-544)
	get_names();

	wprintf_s(cmd_buf, sizeof(cmd_buf) / sizeof(cmd_buf[0]),
		L"cmd.exe /c echo y|cacls \"%S\" /t /g \"%s:F\" \"%s:R\"",
		argv[1], administrators_buf, everyone_buf);

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	/* Start the child process. Since we are using the cmd command, we
	   use CREATE_NO_WINDOW to hide the ugly black window from the user
	*/
	if (!CreateProcessW(NULL, cmd_buf, NULL, NULL, FALSE,
		CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
		/* silently die ... */
		return EXIT_FAILURE;
	}

	// Wait until child process exits.
	WaitForSingleObject(pi.hProcess, INFINITE);

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
#endif

	if (close_log_file) {
		fclose(fp);
	}

	return EXIT_SUCCESS;
}
