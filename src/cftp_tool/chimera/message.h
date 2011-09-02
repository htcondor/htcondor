/*
** $Id: message.h,v 1.20 2007/04/04 00:04:49 krishnap Exp $
**
** Matthew Allen
** description: 
*/

#ifndef _CHIMERA_MESSAGE_H_
#define _CHIMERA_MESSAGE_H_

#include "key.h"
#include "host.h"
#include "jrb.h"
#include "include.h"

#define DEFAULT_SEQNUM 0
#define RETRANSMIT_THREAD_SLEEP 1
#define RETRANSMIT_INTERVAL 1
#define MAX_RETRY 3

typedef struct
{
    Key dest;
    int type;			/* message type */
    int size;
    char *payload;
    Key source;			/* for future security enhancement */
    unsigned long seqNum;	/* for future security enhancement */
} Message;


typedef void (*messagehandler_t) (ChimeraState *, Message *);

/**
 ** message_init: chstate, port
 ** Initialize messaging subsystem on port and returns the MessageGlobal * which 
 ** contains global state of message subsystem.
 ** message_init also initiate the network subsystem
 */
void *message_init (void *chstate, int port);

/** 
 ** message_received:
 ** is called by network_activate and will be passed received data and size from socket
 **
 */
void message_received (void *chstate, char *data, int size);

/**
 ** registers the handler function #func# with the message type #type#,
 ** it also defines the acknowledgment requirement for this type 
 */
void message_handler (void *chstate, int type, messagehandler_t func,
		      int ack);

/**
 ** message_send:
 ** sendt the message to destination #host# the retry arg indicates to the network
 ** layer if this message should be ackd or not
 */
int message_send (void *chstate, ChimeraHost * host, Message * message,
		  Bool retry);

/** 
 ** message_create: 
 ** creates the message to the destination #dest# the message format would be like:
 **  [ type ] [ size ] [ key ] [ data ]. It return the created message structure.
 ** 
 */
Message *message_create (Key dest, int type, int size, char *payload);


/** 
 ** message_free:
 ** free the message and the payload
 */
void message_free (Message * msg);

#endif /* _CHIMERA_MESSAGE_H_ */
