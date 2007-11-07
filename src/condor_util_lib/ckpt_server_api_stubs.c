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


 


/* 
   These are stubs for the functions provided in ckpt_server_api.a.
   This allows us to use there functions anywhere, and not require
   everyone to link against the ckpt_server_api.a from the checkpoint
   server.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef unsigned long int u_lint;
typedef int service_type;

int FileExists(const char *filename, const char *owner)
{
	struct stat file_stat;
	
	if (stat(filename, &file_stat) == 0)
		return TRUE;
	else
		return FALSE;
}


int RequestRestore(const char*     owner,
				   const char*     filename,
				   u_lint*         len,
				   struct in_addr* server_IP,
				   u_short*        port)
{ return -1; }


int RequestStore(const char*     owner,
				 const char*     filename,
				 u_lint	         len,
				 struct in_addr* server_IP,
				 u_short*        port)
{ return -1; }


int RequestService(const char*     owner,
				   const char*     filename,
				   const char*     new_filename,
				   service_type    type,
				   struct in_addr* server_IP,
				   u_short*        port,
				   u_lint*         num_files,
				   char*           cap_free)
{ return -1; }


int FileOnServer(const char* owner,
				 const char* filename)
{ return -1; }


int RemoveRemoteFile(const char* owner,
					 const char* filename)
{ return -1; }


int RemoveLocalOrRemoteFile(const char* owner,
							const char* filename)
{ return unlink(filename); }


int RenameRemoteFile(const char* owner,
					 const char* filename,
					 const char* new_filename)
{ return -1; }


int get_ckpt_server_count() { return 0; }


int set_ckpt_server_number(int i) {}

int SetCkptServerHost(const char *host) {}
