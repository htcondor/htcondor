/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
