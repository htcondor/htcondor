/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
