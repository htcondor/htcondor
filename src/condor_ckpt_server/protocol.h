#ifndef CKPT_PROTOCOL_H
#define CKPT_PROTOCOL_H

/* This code is part of a heinous backwards compatibity hack. */

/* The reason for this hack is thus:

	It came to pass that the "typedef unsigned long u_lint;"
	in typedefs.h began to have a different size between x86 and
	x86_64 machines. The original architecture of the checkpoint
	server was such that the packets being sent from the clients
	to the servers were in memory structures written in a raw form
	straight to the socket (incuding all of the compiler padding
	bytes and whatnot). These packets had typedefed quantities like
	u_lint in them which caused size mismatches on the server side of
	the client (in this case the schedd (to remove the checkpoints)
	and the shadow (to schedule,store,restore them) was compiled 32
	bits or 64 bits.

	To continue, in flocking situations where the starter chooses
	the checkpoint server, the flockers may be of arbitrary 32/64
	bit width and so the set opposite the bit width of the ckpt
	server at the execution site would fail.

	Fixing the code with the obvious answer (which could have been
	done by altering the typedefs to be known sized values for all
	architectures, like uint64_t) would have fixed the code forever
	forward, but not helped flockers and federations of pools since
	it would have required all *flockers* to upgrade comming into the
	various federated pools). Since the presenting customers wanting
	the checkpoint server to handle both 32 and 64 bit clients are
	exactly in this flocked/federated position, we must implement
	backwards compatibility.

	I had to "fix" three network protocols (SERVICE, STORE, RESTORE),
	each one on a different well-known socket. The fourth protocol
	REPLICATE is vestigal junk and should be removed--I did not
	fix that protocol, and actively broke it, in fact, since it
	is just yet another security problem waiting to happen. Each
	protocol had two packets types which could be sent upon the wire,
	a request packet, and a reply packet. The protcol at the time of
	the writing of this comment was the client sent one request packet
	(for the appropriate protocol on the appropriate socket), and the
	server responded with one appriopriate reply packet to the client and
	closed the connection.

	The method I chose for fixing was the explicit offset method
	since it is the most clear and manipulable if we have to touch
	it again. I read the smaller of the two possible packet sizes
	into a char buffer, determine if it is a 32 or 64 bit packet and
	read the rest if 64, then unpack the information from the packet
	(which was encoded in network byte order), put it into a real
	structure for the higher level layer. The connection to the
	client is abstracted via an FDContext structure which knows if
	the client is 32/64 bit and later when I send the reply packet
	it allows the sending functions to pack the results properly.

	The packing/unpacking method reads and writes the fields necessary
	from the binary blob using unaligned pointer reads/writes at
	the specified offsets. I learned the sizes of the structures and
	offsets of all fields by writing a piece of code which presented
	them to me.

	I didn't chose the overlay method of solving (which was defining
	a known structure size and using a type cast on the binary blob)
	because I looked around at gcc, and apparently some structure
	padding changed recently, so I didn't want to mess with that
	since it'd be almost impossible to make work if it broke.

	If I were to continue this hack, I would go into
	server_interface.cpp and manually pack a binary blob sent to
	the server instead of writing the raw structure. This would
	completely divorce the types in typedefs2.h from what is sent
	on the wire and the packets just become bits with an specific
	ordering and padding layout not related to what the compiler
	deemed necessary. At this point, the network protocol could be
	fixated to the 64 bit model and the request/reply *_pkt types
	could all be easily updated/changed.

	As this hack stands, the send_* / recv_* functions abstract
	the network protocol so it is *MUCH* easier at the higher level
	layer to talk to the client. Before this hack went in, it was
	just a bloody mess of explicit writes of in-place modified
	structures. At least now there is a semblance of maintainability.

	If someone were to ever stop this backwards compatibility,
	then keep the send_* / recv_* API, the FDContext, but dump the
	implementation of them and do something sane. Also, redo the
	server_interface.cpp codes.

	06/01/2009
	-pete
*/

/* The allowable types in a FDContext for the client fd. */
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
	SREQ_PKTSIZE_32 = 576,
	SREQ_PKTSIZE_64 = 592,
	SREQ_PKTSIZE_MIN = SREQ_PKTSIZE_32,
	SREQ_PKTSIZE_MAX = SREQ_PKTSIZE_64,

	/* Sizes in bytes of a 32 or 64 bit service_reply_pkt */
	SREP_PKTSIZE_32 = 32,
	SREP_PKTSIZE_64 = 40,
	SREP_PKTSIZE_MAX = SREP_PKTSIZE_64,

	/* Sizes in bytes of a 32 or 64 bit store_req_pkt */
	STREQ_PKTSIZE_32 = 328,
	STREQ_PKTSIZE_64 = 352,
	STREQ_PKTSIZE_MIN = STREQ_PKTSIZE_32,
	STREQ_PKTSIZE_MAX = STREQ_PKTSIZE_64,

	/* Sizes in bytes of a 32 or 64 bit store_reply_pkt */
	STREP_PKTSIZE_32 = 8,
	STREP_PKTSIZE_64 = 8,
	STREP_PKTSIZE_MAX = STREP_PKTSIZE_64,

	/* Sizes in bytes of a 32 or 64 bit restore_req_pkt */
	RSTREQ_PKTSIZE_32 = 320,
	RSTREQ_PKTSIZE_64 = 336,
	RSTREQ_PKTSIZE_MIN = RSTREQ_PKTSIZE_32,
	RSTREQ_PKTSIZE_MAX = RSTREQ_PKTSIZE_64,

	/* Sizes in bytes of a 32 or 64 bit restore_reply_pkt */
	RSTREP_PKTSIZE_32 = 16,
	RSTREP_PKTSIZE_64 = 24,
	RSTREP_PKTSIZE_MAX = RSTREP_PKTSIZE_64,

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

/* Instead of passing around an fd to the client, we set this up and
	pass this around instead. It makes it easier to determine what to do
	given the type of client to which we are talking. */
typedef struct FDContext_s
{
	int fd;				/* the fd associated with this connection. */
	int type;			/* is the other side 32 or 64 bit? */
	struct in_addr who;	/* Who is the ip address with the connection */
	int req_ID;			/* The reqid associated with this connection */
} FDContext;

/* When we try to convert a packet, this is what is can return. */
enum {
	/* The protocol packet conversion went ok */
	PC_OK,
	/* The protocol packet conversion did not go ok! */
	PC_NOT_OK
};

enum read_result_t
{
	NET_READ_FAIL,
	NET_READ_TIMEOUT,
	NET_READ_OK
};

enum write_result_t
{
	NET_WRITE_FAIL,
	NET_WRITE_OK
};

/* The packet reception/transmission API */

/* Dealing with the SERVICE protocol */
int recv_service_req_pkt(service_req_pkt *srq, FDContext *fdc);
bool sreq_is_32bit(char *pkt);
bool sreq_is_64bit(char *pkt);
int send_service_reply_pkt(service_reply_pkt *srp, FDContext *fdc);

/* Dealing with the STORE protocol */
int recv_store_req_pkt(store_req_pkt *strq, FDContext *fdc);
bool streq_is_32bit(char *pkt);
bool streq_is_64bit(char *pkt);
int send_store_reply_pkt(store_reply_pkt *strp, FDContext *fdc);

/* Dealing with the RESTORE protocol */
int recv_restore_req_pkt(restore_req_pkt *rstrq, FDContext *fdc);
bool rstreq_is_32bit(char *pkt);
bool rstreq_is_64bit(char *pkt);
int send_restore_reply_pkt(restore_reply_pkt *rstrp, FDContext *fdc);

/* read some bytes with a timeout on a socket */
read_result_t net_read_with_timeout(int fd, char *ptr, size_t nbytes,
	size_t *numread, int timeout);

/* byte unpacking interface */
uint64_t unpack_uint64_t(char *pkt, size_t off);
uint32_t unpack_uint32_t(char *pkt, size_t off);
unsigned short unpack_uint16_t(char *pkt, size_t off);
struct in_addr unpack_in_addr(char *pkt, size_t off);
char* unpack_char_array(char *pkt, size_t off);

/* byte packing interface */
void pack_uint64_t(char *pkt, size_t off, uint64_t val);
void pack_uint32_t(char *pkt, size_t off, uint32_t val);
void pack_uint16_t(char *pkt, size_t off, uint16_t val);
void pack_in_addr(char *pkt, size_t off, struct in_addr inaddr);
void pack_char_array(char *pkt, size_t off, char *str, size_t len);

/* endian garbage */
/* The 64 bit ones are notable since they don't exactly work the way
	one would think. Please see their comments. */
uint16_t network_uint16_t_order_to_host_uint16_t_order(uint16_t val);
uint32_t network_uint32_t_order_to_host_uint32_t_order(uint32_t val);
uint64_t network_uint64_t_order_to_host_uint64_t_order(uint64_t val);
uint16_t host_uint16_t_order_to_network_uint16_t_order(uint16_t val);
uint32_t host_uint32_t_order_to_network_uint32_t_order(uint32_t val);
uint64_t host_uint64_t_order_to_network_uint64_t_order(uint64_t val);

#endif
