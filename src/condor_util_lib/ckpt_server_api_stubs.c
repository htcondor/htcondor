/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1995 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Jim Pruyne
** 	        University of Wisconsin, Computer Sciences Dept.
** 
*/ 


/* 
   These are stubs for the functions provided in ckpt_server_api.a.
   This allows us to use there functions anywhere, and not require
   everyone to link against the ckpt_server_api.a from the checkpoint
   server.
*/

#include <sys/types.h>
#include <sys/stat.h>

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


int RenameRemoteFile(const char* owner,
					 const char* filename,
					 const char* new_filename)
{ return -1; }
