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
#ifndef IMDS2_H
#define IMDS2_H


#include "constants2.h"
#include "typedefs2.h"
#include "network2.h"
#include "fileinfo2.h"
#include "fileindex2.h"


class IMDS
{
  private:
    FileInformation FileStats;
    FileIndex       Index;

  public:
    int AddFile(struct in_addr machine_IP,
		const char*    owner_name,
		const char*    file_name,
		u_lint         file_size,
		file_state     state);
    int RenameFile(struct in_addr machine_IP,
		   const char*    owner_name,
		   const char*    file_name,
		   const char*    new_file_name);
    u_short RemoveFile(struct in_addr machine_IP,
		       const char*    owner_name,
		       const char*    file_name);
    void DumpIndex() { Index.IndexDump(); }
    void DumpInfo() { FileStats.PrintFileInfo(); }
    u_lint GetNumFiles();
    void TransferFileInfo(int socket_desc);
};


#endif
