#ifndef CKPT_PROTOCOL_H
#define CKPT_PROTOCOL_H

/* This code is part of a heinous backwards compatibity hack. */

enum {
	FDC_UNKNOWN,
	FDC_32, /* A 32 bit client */
	FDC_64 /* a 64 bit client */
};

/* sizes of various structures the client could send to me */
enum {
	/* Sizes in bytes of a service_req_pkt */
	SREQ_PKT_32 = 576,
	SREQ_PKT_64 = 592,
	SREQ_MINSIZE = SREQ_PKT_32,
	SREQ_MAXSIZE = SREQ_PKT_64,

	/* Sizes in bytes of a service_reply_pkt */
	SREP_PKT_32 = 32,
	SREP_PKT_64 = 40,
};

typedef struct FDContext_s
{
	int fd;				/* the fd associated with this connection. */
	int type;			/* is the other side 32 or 64 bit? */
	struct in_addr who;	/* Who is the ip address with the connection */
	int req_ID;			/* The reqid associated with this connection */
} FDContext;

enum {
	/* The protocol packet conversion went ok */
	PC_OK,
	/* The protocol packet conversion did not go ok! */
	PC_NOT_OK
};


enum read_result_t
{
	NET_READ_FAIL,
	NET_READ_OK
};

int recv_service_req_pkt(service_req_pkt *srp, FDContext *fdc);

read_result_t net_read_with_timeout(int fd, char *ptr, size_t nbytes,
	size_t *numread, int timeout);

#endif
