/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

#ifndef _SERVER_INTERFACE_H
#define _SERVER_INTERFACE_H

#include <sys/types.h>
#include "typedefs2.h"
#include "network2.h"

#if defined(__cplusplus)
extern "C" {
#endif

int set_ckpt_server_number(int new_server);

int get_ckpt_server_count();

int SetCkptServerHost(const char *host);

void StripPrefix(const char* pathname,
		 char        filename[MAX_CONDOR_FILENAME_LENGTH]);


int ConnectToServer(request_type type);


int IsLocal(const char* path);

int FileExists(const char *filename, const char *owner, const char *schedd);

int RequestStore(const char*     owner,
				 const char*     schedd,
				 const char*     filename,
				 size_t          len,
				 struct in_addr* server_IP,
				 u_short*        port);


int RequestRestore(const char*     owner,
				   const char*     schedd,
				   const char*     filename,
				   size_t*         len,
				   struct in_addr* server_IP,
				   u_short*        port);


int RequestService(const char*     owner,
				   const char*     schedd,
				   const char*     filename,
				   const char*     new_filename,
				   service_type    type,
				   struct in_addr* server_IP,
				   u_short*        port,
				   u_lint*         num_files,
				   char*           cap_free);


int FileOnServer(const char* owner,
				 const char* schedd,
				 const char* filename);


int RemoveLocalOrRemoteFile(const char* owner,
							const char* schedd,
							const char* filename);

int RemoveRemoteFile(const char* owner,
					 const char* schedd,
					 const char* filename);


int RenameRemoteFile(const char* owner,
					 const char* schedd,
					 const char* filename,
					 const char* new_filename);

#if defined(__cplusplus)
}
#endif


#endif
