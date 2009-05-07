#ifndef CKPT_PROTOCOL_H
#define CKPT_PROTOCOL_H

/* This code is part of a heinous backwards compatibity hack. */

enum {
	FDC_UNKNOWN,
	FDC_32, /* A 32 bit client */
	FDC_64 /* a 64 bit client */
};

/* This enum represents details about packet sizes and offsets of member
	fields in the network protocol between the schedd, shadow, and checkpoint
	server. */
enum {
	/* Sizes in bytes of a 32 or 64 bit service_req_pkt */
	SREQ_PKT_32 = 576,
	SREQ_PKT_64 = 592,
	SREQ_MINSIZE = SREQ_PKT_32,
	SREQ_MAXSIZE = SREQ_PKT_64,

	/* offsets into a 64-bit service_req_pkt for members desired */
	/*
	service_req_pkt on x86_64
		u_lint ticket [off: 0, len: 8]
		u_short service [off: 8, len: 2]
		u_lint key [off: 16, len: 8]
		char[MAX_NAME_LENGTH] owner_name [off: 24, len: 50]
		char[MAX_CONDOR_FILENAME_LENGTH] file_name [off: 74, len: 256]
		char[MAX_CONDOR_FILENAME_LENGTH-4] new_file_name [off: 330, len: 252]
		struct in_addr shadow_IP [off: 584, len: 4]
	*/
	SREQ64_ticket = 0,
	SREQ64_service = 8,
	SREQ64_key = 16,
	SREQ64_owner_name = 24,
	SREQ64_file_name = 74,
	SREQ64_new_file_name = 330,
	SREQ64_shadow_IP = 584,

	/* offsets into a 64-bit service_reply_pkt for members desired */
	/*
	service_reply_pkt on x86_64
		u_short req_status [off: 0, len: 2]
		struct in_addr server_addr [off: 4, len: 4]
		u_short port [off: 8, len: 2]
		u_lint num_files [off: 16, len: 8]
		char[MAX_ASCII_CODED_DECIMAL_LENGTH] capacity_free_ACD [off: 24, len: 15]
	*/
	SREP64_req_status = 0,
	SREP64_server_addr = 4,
	SREP64_port = 8,
	SREP64_num_files = 16,
	SREP64_capacity_free_ACD = 24,

	
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
