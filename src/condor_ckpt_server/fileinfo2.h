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

