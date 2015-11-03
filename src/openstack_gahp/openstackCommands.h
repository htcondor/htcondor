/***************************************************************
 *
 * Copyright (C) 1990-2015, Condor Team, Computer Sciences Department,
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

#ifndef OPENSTACK_COMMANDS_H
#define OPENSTACK_COMMANDS_H

#include "condor_common.h"

// Keystone commands
#define KEYSTONE_COMMAND_SERVICE		"KEYSTONE_SERVICE"

// Nova commands
#define NOVA_COMMAND_PING				"NOVA_PING"
#define NOVA_COMMAND_SERVER_CREATE		"NOVA_SERVER_CREATE"
#define NOVA_COMMAND_SERVER_DELETE		"NOVA_SERVER_DELETE"
#define NOVA_COMMAND_SERVER_LIST		"NOVA_SERVER_LIST"
#define NOVA_COMMAND_SERVER_DETAIL		"NOVA_SERVER_LIST_DETAIL"

#define GENERAL_GAHP_ERROR_CODE			"GAHPERROR"
#define GENERAL_GAHP_ERROR_MSG			"GAHP_ERROR"

class NovaRequest {
  public:
	NovaRequest();
	virtual ~NovaRequest();

	virtual bool sendRequest();

	// Each request maintains its own copy of the Keystone data it was
	// born with, because the KEYSTONE_SERVICE request could be issued
	// at any time and is defined to only affect subsequent requests;
	// prior queued requests must not use the new information.
	//
	// (On the other hand, my understanding of the threading model is such
	// that this copy is unnecessary unless any given request makes more
	// than one i/o request, and would therefore use the global Keystone
	// data after unlocking the mutex.)
	//
	// This could, of course, be replaced by a reference-counted pointer
	// should memory usage become a concern.
	bool getAuthToken();

	// We also need to know (and cache) region endpoint URL lookups.
	bool getNovaEndpointForRegion( const std::string & region );

	std::string serviceURL;
	std::string contentType;
	std::string requestBody;
	std::string requestMethod;

	unsigned long responseCode;
	std::string responseString;

	std::string errorCode;
	std::string errorMessage;

	bool includeResponseHeader;

	protected:
		std::string authToken;
};

// Nova commands

class KeystoneService : public NovaRequest {
 public:
	KeystoneService();
	virtual ~KeystoneService();

	static bool ioCheck( char **argv, int argc );
	static bool workerFunction( char **argv, int argc, std::string &result_string );
};

class NovaPing : public NovaRequest {
 public:
	NovaPing();
	virtual ~NovaPing();

	static bool ioCheck( char **argv, int argc );
	static bool workerFunction( char **argv, int argc, std::string &result_string );
};

class NovaServerCreate : public NovaRequest {
 public:
	NovaServerCreate();
	virtual ~NovaServerCreate();

	static bool ioCheck( char **argv, int argc );
	static bool workerFunction( char **argv, int argc, std::string &result_string );
};

class NovaServerDelete : public NovaRequest {
 public:
	NovaServerDelete();
	virtual ~NovaServerDelete();

	static bool ioCheck( char **argv, int argc );
	static bool workerFunction( char **argv, int argc, std::string &result_string );
};

class NovaServerList : public NovaRequest {
 public:
	NovaServerList();
	virtual ~NovaServerList();

	static bool ioCheck( char **argv, int argc );
	static bool workerFunction( char **argv, int argc, std::string &result_string );
};

class NovaServerListDetail : public NovaRequest {
 public:
	NovaServerListDetail();
	virtual ~NovaServerListDetail();

	static bool ioCheck( char **argv, int argc );
	static bool workerFunction( char **argv, int argc, std::string &result_string );
};

#endif /* OPENSTACK_COMMANDS_H */
