/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
