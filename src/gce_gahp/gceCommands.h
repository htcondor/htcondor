/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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

#ifndef GCE_COMMANDS_H
#define GCE_COMMANDS_H

#include "condor_common.h"

// GCE Commands
#define GCE_COMMAND_PING				"GCE_PING"
#define GCE_COMMAND_INSTANCE_INSERT		"GCE_INSTANCE_INSERT"
#define GCE_COMMAND_INSTANCE_DELETE		"GCE_INSTANCE_DELETE"
#define GCE_COMMAND_INSTANCE_LIST		"GCE_INSTANCE_LIST"

#define GENERAL_GAHP_ERROR_CODE             "GAHPERROR"
#define GENERAL_GAHP_ERROR_MSG              "GAHP_ERROR"

class GceRequest {
 public:
	GceRequest();
	virtual ~GceRequest();

	virtual bool SendRequest();

	std::string serviceURL;

	std::string errorMessage;
	std::string errorCode;

	std::string accessToken;
	std::string requestMethod;
	std::string requestBody;
	std::string contentType;

	std::string resultString;

	bool includeResponseHeader;
};

// GCE Commands

class GcePing : public GceRequest {
 public:
	GcePing();
	virtual ~GcePing();

	static bool ioCheck( char **argv, int argc );
	static bool workerFunction( char **argv, int argc, std::string &result_string );
};

class GceInstanceInsert : public GceRequest {
 public:
	GceInstanceInsert();
	virtual ~GceInstanceInsert();

	static bool ioCheck( char **argv, int argc );
	static bool workerFunction( char **argv, int argc, std::string &result_string );
};

class GceInstanceDelete : public GceRequest {
 public:
	GceInstanceDelete();
	virtual ~GceInstanceDelete();

	static bool ioCheck( char **argv, int argc );
	static bool workerFunction( char **argv, int argc, std::string &result_string );
};

class GceInstanceList : public GceRequest {
 public:
	GceInstanceList();
	virtual ~GceInstanceList();

	static bool ioCheck( char **argv, int argc );
	static bool workerFunction( char **argv, int argc, std::string &result_string );
};

#endif
