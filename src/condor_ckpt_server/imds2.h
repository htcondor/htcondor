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
