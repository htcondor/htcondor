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
