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
