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


	/* --------------------------------------------------------*/
	/* These describe offsets into the 64 bit structures from x86_64 */

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
		char[MAX_ASCII_CODED_DECIMAL_LENGTH] capacity_free_ACD [off: 24,len: 15]
	*/
	SREP64_req_status = 0,
	SREP64_server_addr = 4,
	SREP64_port = 8,
	SREP64_num_files = 16,
	SREP64_capacity_free_ACD = 24,

	/* offsets into a 64-bit store_req_pkt on x86_64 for members desired */
	/*
	store_req_pkt on x86_64
		u_lint file_size [off: 0, len: 8]
		u_lint ticket [off: 8, len: 8]
		u_lint priority [off: 16, len: 8]
		u_lint time_consumed [off: 24, len: 8]
		u_lint key [off: 32, len: 8]
		char[MAX_CONDOR_FILENAME_LENGTH] filename [off: 40, len: 256]
		char[MAX_NAME_LENGTH] owner [off: 296, len: 50]
	*/
	STREQ64_file_size = 0,
	STREQ64_ticket = 8,
	STREQ64_priority = 16,
	STREQ64_time_consumed = 24,
	STREQ64_key = 32,
	STREQ64_filename = 40,
	STREQ64_owner = 296,

	/* offsets into a 64-bit store_reply_pkt on x86_64 for members desired */
	STREP64_server_name = 0,
	STREP64_port = 4,
	STREP64_req_status = 6,

	/* offsets into a 64-bit restore_req_pkt on x86_64 for members desired */
	/*
	restore_req_pkt on x86_64
		u_lint ticket [off: 0, len: 8]
		u_lint priority [off: 8, len: 8]
		u_lint key [off: 16, len: 8]
		char[MAX_CONDOR_FILENAME_LENGTH] filename [off: 24, len: 256]
		char[MAX_NAME_LENGTH] owner [off: 280, len: 50]
	*/
	RSTREQ64_ticket = 0,
	RSTREQ64_priority = 8,
	RSTREQ64_key = 16,
	RSTREQ64_filename = 24,
	RSTREQ64_owner = 280,

	/* offsets into a 64-bit restore_req_pkt on x86_64 for members desired */
	/*
	restore_reply_pkt on x86_64
		struct in_addr server_name [off: 0, len: 4]
		u_short port [off: 4, len: 2]
		u_lint file_size [off: 8, len: 8]
		u_short req_status [off: 16, len: 2]
	*/
	RSTREP64_server_name = 0,
	RSTREP64_port = 4,
	RSTREP64_file_size = 8,
	RSTREP64_req_status = 16,

	/* --------------------------------------------------------*/
	/* These describe offsets into the 32 bit structures from x86 */

	/* offsets into a 32-bit service_req_pkt on x86 for members desired */
	/*
	service_req_pkt on x86
		u_lint ticket [off: 0, len: 4]
		u_short service [off: 4, len: 2]
		u_lint key [off: 8, len: 4]
		char[MAX_NAME_LENGTH] owner_name [off: 12, len: 50]
		char[MAX_CONDOR_FILENAME_LENGTH] file_name [off: 62, len: 256]
		char[MAX_CONDOR_FILENAME_LENGTH-4] new_file_name [off: 318, len: 252]
		struct in_addr shadow_IP [off: 572, len: 4]
	*/
	SREQ32_ticket = 0,
	SREQ32_service = 4,
	SREQ32_key = 8,
	SREQ32_owner_name = 12,
	SREQ32_file_name = 62,
	SREQ32_new_file_name = 318,
	SREQ32_shadow_IP = 572,

	/* offsets into a 32-bit service_reply_pkt on x86 for members desired */
	/*
	service_reply_pkt on x86
		u_short req_status [off: 0, len: 2]
		struct in_addr server_addr [off: 4, len: 4]
		u_short port [off: 8, len: 2]
		u_lint num_files [off: 12, len: 4]
		char[MAX_ASCII_CODED_DECIMAL_LENGTH] capacity_free_ACD [off: 16,len: 15]
	*/
	SREP32_req_status = 0,
	SREP32_server_addr = 4,
	SREP32_port = 8,
	SREP32_num_files = 12,
	SREP32_capacity_free_ACD = 16,

	/* offsets into a 32-bit store_req_pkt on x86 for members desired */
	/*
	store_req_pkt on x86
		u_lint file_size [off: 0, len: 4]
		u_lint ticket [off: 4, len: 4]
		u_lint priority [off: 8, len: 4]
		u_lint time_consumed [off: 12, len: 4]
		u_lint key [off: 16, len: 4]
		char[MAX_CONDOR_FILENAME_LENGTH] filename [off: 20, len: 256]
		char[MAX_NAME_LENGTH] owner [off: 276, len: 50]
	*/
	STREQ32_file_size = 0,
	STREQ32_ticket = 4,
	STREQ32_priority = 8,
	STREQ32_time_consumed = 12,
	STREQ32_key = 16,
	STREQ32_filename = 20,
	STREQ32_owner = 276,

	/* offsets into a 32-bit store_reply_pkt on x86 for members desired */
	/*
	store_reply_pkt on x86
		struct in_addr server_name [off: 0, len: 4]
		u_short port [off: 4, len: 2]
		u_short req_status [off: 6, len: 2]
	*/
	STREP32_server_name = 0,
	STREP32_port = 4,
	STREP32_req_status = 6,

	/* offsets into a 32-bit restore_req_pkt on x86 for members desired */
	/*
	restore_req_pkt on x86
		u_lint ticket [off: 0, len: 4]
		u_lint priority [off: 4, len: 4]
		u_lint key [off: 8, len: 4]
		char[MAX_CONDOR_FILENAME_LENGTH] filename [off: 12, len: 256]
		char[MAX_NAME_LENGTH] owner [off: 268, len: 50]
	*/
	RSTREQ32_ticket = 0,
	RSTREQ32_priority = 4,
	RSTREQ32_key = 8,
	RSTREQ32_filename = 12,
	RSTREQ32_owner = 268,

	/* offsets into a 32-bit restore_reply_pkt on x86 for members desired */
	/*
	restore_reply_pkt on x86
		struct in_addr server_name [off: 0, len: 4]
		u_short port [off: 4, len: 2]
		u_lint file_size [off: 8, len: 4]
		u_short req_status [off: 12, len: 2]
	*/
	RSTREP32_server_name = 0,
	RSTREP32_port = 4,
	RSTREP32_file_size = 8,
	RSTREP32_req_status = 12,
	
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
