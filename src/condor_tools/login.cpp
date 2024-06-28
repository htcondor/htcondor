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

int main(int argc, char** argv)
{
	const char* placementd_name = nullptr;
	const char* cmd = nullptr;
	const char* user_name = nullptr;

	if (argc < 3) {
		fprintf(stderr, "Usage: condor_login add <user name>\n");
		exit(1);
	}
	// For now, only 'add' is supported
	cmd = argv[1];
	user_name = argv[2];

	set_priv_initialize(); // allow uid switching if root
	config();
	dprintf_set_tool_debug("TOOL", nullptr);

	Daemon placementd(DT_PLACEMENTD, placementd_name);
	if (placementd.locate(Daemon::LOCATE_FOR_LOOKUP) == false) {
		fprintf(stderr, "Failed to locate PlacementD\n");
		exit(1);
	}

	Sock* sock;
	if (!(sock = placementd.startCommand(USER_LOGIN, Stream::reli_sock, 0))) {
		fprintf(stderr, "Failed to contact PlacementD\n");
		exit(1);
	}

	ClassAd cmd_ad;
	cmd_ad.Assign("UserName", user_name);

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

	return 0;
}
