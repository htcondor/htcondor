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

