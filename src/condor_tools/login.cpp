/***************************************************************
 *
 * Copyright (C) 2024, Center for High Throughput Computing
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
#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "daemon.h"

void usage()
{
	fprintf(stderr, "Usage: condor_login login [-authz <authz-list>] [-project <project>] <user name>\n");
	fprintf(stderr, "       condor_login query-users\n");
	fprintf(stderr, "       condor_login query-tokens\n");
	exit(1);
}

int main(int argc, char** argv)
{
	const char* placementd_name = nullptr;
	const char* user_name = nullptr;
	const char* authz = nullptr;
	const char* project = nullptr;
	int cmd_int = -1;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "add") == 0 || strcmp(argv[i], "login") == 0) {
			if (cmd_int != -1) { usage(); }
			cmd_int = PLACEMENT_USER_LOGIN;
		} else if (strcmp(argv[i], "query-users") == 0) {
			if (cmd_int != -1) { usage(); }
			cmd_int = PLACEMENT_QUERY_USERS;
		} else if (strcmp(argv[i], "query-tokens") == 0) {
			if (cmd_int != -1) { usage(); }
			cmd_int = PLACEMENT_QUERY_TOKENS;
		} else if (strcmp(argv[i], "-authz") == 0) {
			if (argc <= i+1) {
				usage();
			}
			authz = argv[i+1];
			i++;
		} else if (strcmp(argv[i], "-project") == 0) {
			if (argc <= i+1) {
				usage();
			}
			project = argv[i+1];
			i++;
		} else {
			if (user_name != nullptr) { usage(); }
			user_name = argv[i];
		}
	}

	set_priv_initialize(); // allow uid switching if root
	config();

	Daemon placementd(DT_PLACEMENTD, placementd_name);
	if (placementd.locate(Daemon::LOCATE_FOR_LOOKUP) == false) {
		fprintf(stderr, "Failed to locate PlacementD '%s'\n",
				placementd_name ? placementd_name : "default");
		exit(1);
	}

	Sock* sock;
	if (!(sock = placementd.startCommand(cmd_int, Stream::reli_sock, 0))) {
		fprintf(stderr, "Failed to contact PlacementD\n");
		exit(1);
	}

	if (cmd_int == PLACEMENT_USER_LOGIN) {
		ClassAd cmd_ad;
		cmd_ad.Assign("UserName", user_name);
		if (authz) {
			cmd_ad.Assign("Authorizations", authz);
		}
		if (project) {
			cmd_ad.Assign("Project", project);
		}

		if ( !putClassAd(sock, cmd_ad) || !sock->end_of_message()) {
			fprintf(stderr, "Failed to send request to PlacementD\n");
			exit(1);
		}

		ClassAd result_ad;
		std::string idtoken;
		if ( !getClassAd(sock, result_ad) || ! sock->end_of_message()) {
			fprintf(stderr, "Failed to receive reply from PlacementD\n");
			exit(1);
		}

		int error_code = 0;
		std::string error_string = "(unknown)";
		if (result_ad.LookupString(ATTR_ERROR_STRING, error_string)) {
			result_ad.LookupInteger(ATTR_ERROR_CODE, error_code);
			fprintf(stderr, "PlacementD returned error %d, %s\n", error_code, error_string.c_str());
			exit(1);
		}

		if (!result_ad.EvaluateAttrString(ATTR_SEC_TOKEN, idtoken)) {
			fprintf(stderr, "No token returned by PlacementD\n");
			exit(1);
		}

		printf("%s\n", idtoken.c_str());
	} else if (cmd_int == PLACEMENT_QUERY_USERS) {
		ClassAd cmd_ad;

		if ( !putClassAd(sock, cmd_ad) || !sock->end_of_message()) {
			fprintf(stderr, "Failed to send request to PlacementD\n");
			exit(1);
		}

		ClassAd result_ad;
		do {
			result_ad.Clear();
			if ( !getClassAd(sock, result_ad)) {
				fprintf(stderr, "Failed to receive reply from PlacementD\n");
				exit(1);
			}

			std::string str;
			if (result_ad.LookupString("MyType", str) && str == "Summary") {
				int error_code = 0;
				std::string error_string = "(unknown)";
				if (result_ad.LookupString(ATTR_ERROR_STRING, error_string)) {
					result_ad.LookupInteger(ATTR_ERROR_CODE, error_code);
					fprintf(stderr, "PlacementD returned error %d, %s\n", error_code, error_string.c_str());
					exit(1);
				}
				sock->end_of_message();
				break;
			}

			fPrintAd(stdout, result_ad);
			fprintf(stdout, "\n");
		} while (true);
	} else if (cmd_int == PLACEMENT_QUERY_TOKENS) {
		ClassAd cmd_ad;

		if ( !putClassAd(sock, cmd_ad) || !sock->end_of_message()) {
			fprintf(stderr, "Failed to send request to PlacementD\n");
			exit(1);
		}

		ClassAd result_ad;
		do {
			result_ad.Clear();
			if ( !getClassAd(sock, result_ad)) {
				fprintf(stderr, "Failed to receive reply from PlacementD\n");
				exit(1);
			}

			std::string str;
			if (result_ad.LookupString("MyType", str) && str == "Summary") {
				int error_code = 0;
				std::string error_string = "(unknown)";
				if (result_ad.LookupString(ATTR_ERROR_STRING, error_string)) {
					result_ad.LookupInteger(ATTR_ERROR_CODE, error_code);
					fprintf(stderr, "PlacementD returned error %d, %s\n", error_code, error_string.c_str());
					exit(1);
				}
				sock->end_of_message();
				break;
			}

			fPrintAd(stdout, result_ad);
			fprintf(stdout, "\n");
		} while (true);
	}

	return 0;
}
