#if !defined(_QMGR_H)
#define _QMGR_H

#include <netinet/in.h>
#include "condor_xdr.h"

#define QMGR_PORT 9605


#if defined(__cplusplus)
extern "C" {
#endif

int handle_q(XDR *, struct sockaddr_in*);

#if defined(__cplusplus)
}
#endif

#endif /* _QMGR_H */
