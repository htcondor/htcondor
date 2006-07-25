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
#ifndef FILEINFO_H
#define FILEINFO_H


#include "constants2.h"
#include "typedefs2.h"


class FileInformation
{
  private:
    file_info_node* head;
    file_info_node* tail;
    u_lint          num_files;

  public:
    FileInformation();
    ~FileInformation();
    void DeleteFileInfo();
    int RemoveFileInfo(file_info_node* d_ptr);
    file_info_node* AddFileInfo(struct in_addr machine_IP,
				const char*    owner_name,
				const char*    file_name,
				u_lint         file_size,
				file_state     state,
				time_t*        last_modified_ptr);
    int RenameFileInfo(file_info_node* r_ptr,
                       const char*     new_file_name);
    u_lint GetNumFiles() { return num_files; }
    void PrintFileInfo();
    void TransferFileInfo(int socket_desc);
    void WriteFileInfoToDisk();
    void LoadFileInfoFromDisk();
};


#endif

