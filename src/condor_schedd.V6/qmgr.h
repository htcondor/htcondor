#if !defined(_QMGR_H)
#define _QMGR_H

#if !defined(WIN32)
#include <netinet/in.h>
#endif

#define QMGR_PORT 9605

#include "condor_io.h"

#if defined(__cplusplus)
extern "C" {
#endif

class Service;
int handle_q(Service *, int, Stream *sock);

#if defined(__cplusplus)
}
#endif

#endif /* _QMGR_H */
