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
    void Lock(struct in_addr machine_IP,
	      const char*    owner_name,
	      const char*    file_name,
	      file_state     state);
    void Unlock(struct in_addr machine_IP,
		const char*    owner_name,
		const char*    file_name);
    int LockStatus(struct in_addr machine_IP,
		   const char*    owner_name,
		   const char*    file_name);
    u_lint GetNumFiles();
    void TransferFileInfo(int socket_desc);
};


#endif
