#ifndef _DGRAM_IO_HANDLE_H
#define _DGRAM_IO_HANDLE_H 

/* this structure is used to compact together all the information
   needed to 'send' datagram sockets from schedd and startd to the
   collector.  -- Raghu 2/28/95
*/

#include <netinet/in.h>

typedef struct {
        struct  sockaddr_in addr;
        int     sock;
} DGRAM_IO_HANDLE;

#endif
