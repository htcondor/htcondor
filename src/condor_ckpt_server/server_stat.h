#include <sys/types.h>
#include "typedefs2.h"
#include "constants2.h"
#include "network2.h"


class ServerStat
{
  private:
    file_state_info* ptr;
    int              num_files;
    int              socket_desc;
    struct in_addr   server_addr;
    double           free_capacity;
    service_reply_pkt InitHandshake();

  public:
    ServerStat();
    ~ServerStat();
    void SortInfo();
    void PrintInfo();
    void LoadInfo();
    void ConvertInfo();
};


int compare(const void* A_ptr,
	    const void* B_ptr);
