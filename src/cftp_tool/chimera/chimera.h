/*
** $Id: chimera.h,v 1.19 2006/06/07 09:21:28 krishnap Exp $
**
** Matthew Allen
** description: 
*/

#ifndef _CHIMERA_H_
#define _CHIMERA_H_

#include "host.h"
#include "key.h"
#include "log.h"
#include <pthread.h>
#include "message.h"
#include "semaphore.h"
#include "include.h"


typedef void (*chimera_forward_upcall_t) (Key **, Message **, ChimeraHost **);
typedef void (*chimera_deliver_upcall_t) (Key *, Message *);
typedef void (*chimera_update_upcall_t) (Key *, ChimeraHost *, int);

typedef struct
{
    ChimeraHost *me;
    ChimeraHost *bootstrap;
    void *join;			/* semaphore */
    pthread_mutex_t lock;
    chimera_forward_upcall_t forward;
    chimera_deliver_upcall_t deliver;
    chimera_update_upcall_t update;
    Sema globalSeqNum;		/* for future security enhancement */
} ChimeraGlobal;

/**
 ** chimera_init: port
 **  Initialize Chimera on port port and returns the ChimeraState * which 
 ** contains global state of different chimera modules.
 */
ChimeraState *chimera_init (int port);

/**
 ** chimera_join: 
 ** Join the network that the bootstrap host is a part of 
 */
void chimera_join (ChimeraState * state, ChimeraHost * bootstrap);

/**
 * chimera_route:
 * Send a message msg through the system to key. hint is currently
 * ignored, but it will one day be the next hop 
*/
void chimera_route (ChimeraState * state, Key * key, Message * msg,
		    ChimeraHost * hint);

/**
 ** chimera_forward:
 ** Set the chimera forward upcall to func. This handler will be called every
 ** time a message is routed to a key through the current node. The host argument
 ** is upsupported, but will allow the programmer to choose the next hop 
 */
void chimera_forward (ChimeraState * state, chimera_forward_upcall_t func);

/**
 ** chimera_deliver:
 ** Set the chimera deliver upcall to func. This handler will be called every
 ** time a message is delivered to the current node
 */
void chimera_deliver (ChimeraState * state, chimera_deliver_upcall_t func);

/** chimera_update: 
 ** Set the chimera update upcall to func. This handler will be called every
 ** time a host leaves or joins your neighbor set. The final integer is a 1 if
 ** the host joins and a 0 if the host leaves 
*/
void chimera_update (ChimeraState * state, chimera_update_upcall_t func);

/** chimera_setkey: 
 ** Manually sets the key for the current node 
 */
void chimera_setkey (ChimeraState * state, Key key);

/** chimera_register:
 ** register an integer message type to be routed by the chimera routing layer
 ** ack is the argument that defines wether this message type should be acked or not
 ** ack ==1 means message will be acknowledged, ack=2 means no acknowledge is necessary 
 ** for this type of message. 
 */
void chimera_register (ChimeraState * state, int type, int ack);

/** chimera_send: 
 ** Route a message of type to key containing size bytes of data. This will
 ** send data through the Chimera system and deliver it to the host closest to the
 ** key 
 */
void chimera_send (ChimeraState * state, Key key, int type, int len,
		   char *data);

/**
 ** chimera_ping: 
 ** sends a ping message to the host. the message is acknowledged in network layer
 */
int chimera_ping (ChimeraState * state, ChimeraHost * host);

#endif /* _CHIMERA_H_ */
