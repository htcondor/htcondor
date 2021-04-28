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

#ifndef ARC_COMMANDS_H
#define ARC_COMMANDS_H

#include "condor_common.h"
#include <curl/curl.h>

// ARC Commands
#define ARC_COMMAND_PING              "ARC_PING"
#define ARC_COMMAND_JOB_NEW           "ARC_JOB_NEW"
#define ARC_COMMAND_JOB_STATUS        "ARC_JOB_STATUS"
#define ARC_COMMAND_JOB_STATUS_ALL    "ARC_JOB_STATUS_ALL"
#define ARC_COMMAND_JOB_INFO          "ARC_JOB_INFO"
#define ARC_COMMAND_JOB_STAGE_IN      "ARC_JOB_STAGE_IN"
#define ARC_COMMAND_JOB_STAGE_OUT     "ARC_JOB_STAGE_OUT"
#define ARC_COMMAND_JOB_KILL          "ARC_JOB_KILL"
#define ARC_COMMAND_JOB_CLEAN         "ARC_JOB_CLEAN"
#define ARC_COMMAND_DELEGATION_NEW    "ARC_DELEGATION_NEW"
#define ARC_COMMAND_DELEGATION_RENEW  "ARC_DELEGATION_RENEW"

#define GENERAL_GAHP_ERROR_CODE             "GAHPERROR"
#define GENERAL_GAHP_ERROR_MSG              "GAHP_ERROR"

class GahpRequest;

class HttpRequest {
 public:
	HttpRequest();
	virtual ~HttpRequest();

	virtual bool SendRequest();

	static curl_version_info_data *curlVerInfo;
	static bool curlUsesNss;

	std::string serviceURL;

	std::string errorCode;
	std::string errorMessage;

	std::string proxyFile;
	std::string requestMethod;
	std::string requestBody;
	std::string requestBodyFilename;
	std::string contentType;
	size_t requestBodyReadPos;

	std::string responseBody;
	std::string responseBodyFilename;
	std::map<std::string, std::string> responseHeaders;

	bool includeResponseHeader;

	static size_t CurlReadCb(char *buffer, size_t size, size_t nitems, void *userdata);
};

// ARC Commands

bool ArcPingArgsCheck( char **argv, int argc );
bool ArcPingWorkerFunction( GahpRequest *gahp_request );

bool ArcJobNewArgsCheck( char **argv, int argc );
bool ArcJobNewWorkerFunction( GahpRequest *gahp_request );

bool ArcJobStatusArgsCheck( char **argv, int argc );
bool ArcJobStatusWorkerFunction( GahpRequest *gahp_request );

bool ArcJobStatusAllArgsCheck( char **argv, int argc );
bool ArcJobStatusAllWorkerFunction( GahpRequest *gahp_request );

bool ArcJobInfoArgsCheck( char **argv, int argc );
bool ArcJobInfoWorkerFunction( GahpRequest *gahp_request );

bool ArcJobStageInArgsCheck( char **argv, int argc );
bool ArcJobStageInWorkerFunction( GahpRequest *gahp_request );

bool ArcJobStageOutArgsCheck( char **argv, int argc );
bool ArcJobStageOutWorkerFunction( GahpRequest *gahp_request );

bool ArcJobKillArgsCheck( char **argv, int argc );
bool ArcJobKillWorkerFunction( GahpRequest *gahp_request );

bool ArcJobCleanArgsCheck( char **argv, int argc );
bool ArcJobCleanWorkerFunction( GahpRequest *gahp_request );

bool ArcDelegationNewArgsCheck( char **argv, int argc );
bool ArcDelegationNewWorkerFunction( GahpRequest *gahp_request );

bool ArcDelegationRenewArgsCheck( char **argv, int argc );
bool ArcDelegationRenewWorkerFunction( GahpRequest *gahp_request );

#endif
