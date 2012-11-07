#include "condor_common.h"
#include "condor_debug.h"
#include <iostream>
#include <fstream>
#include "MyString.h"

#include "constants2.h"
#include "typedefs2.h"
#include "alarm2.h"
#include "server2.h"
#include "protocol.h"

/* This doesn't nest, so be careful! */
extern Alarm rt_alarm;

/* This function is a debugging function and is only called when it is hand
	inserted into the code flow to see what the binary of a packet looks like.
*/
void dump_pkt(char *netpkt, size_t size);

/* This does a blocking read for size amount of bytes. However, there is
	a timeout associated with it. This function is closely allied in
	implementation with _condor_full_read, but due to the alarm signal
	hilarity, it gets its own implementation here. The logic of this
	is a bit screwed up, but I had to fit it within the mindset of the
	ckpt server codebase, which is odd. The reason the logic is
	screwed up is because it is problematic to discover if the function
	failed due to timeout, or a different reason.
*/
read_result_t net_read_with_timeout(int fd, char *ptr, size_t nbytes,
	size_t *numread, int timeout)
{
    int nleft, nread;
	MyString log_msg;
	int save_errno;

	/* we use signal to implement breaking out of a permanently blocked 
		or very slow read situation */
	rt_alarm.SetAlarm(timeout);

    nleft = nbytes;
    while (nleft > 0) {
        REISSUE_READ: 
        nread = read(fd, ptr, nleft);
        if (nread < 0) {
			save_errno = errno;
            if (errno == EINTR) {
				/*	If the alarm is expired (we'll know because the SIGALARM
					handler tells the global rt_alarm object it is expired),
					then we timed out on the connection, otherwise
					ignore whatever signal it was and reissue the read.
				*/
				if (rt_alarm.IsExpired() == true) {

					rt_alarm.ResetAlarm();

					/* of course, we really don't know exactly how much we
						read, but that's ok, since we're closing the connection
						very soon at any rate. */
    				*numread = (nbytes - nleft);     

					return NET_READ_TIMEOUT;
				} 

				/* However, if it was some other kind of signal instead of
					the alarm, we'll be generous and resubmit the read with
					the same timeout again. This does make it possible for
					the timeout to never fire of the right frequency of non
					alarm signals happens, but this checkpoint server code
					is horrible and probably going away very soon. So we'll
					soak that small chance. */
				rt_alarm.SetAlarm(timeout);
                goto REISSUE_READ;
            }

            /* The caller has no idea how much was actually read in this
                scenario, but we know we aren't going to be reading anymore.
			*/
			rt_alarm.ResetAlarm();
			
			errno = save_errno;
			/* This represents confirmed bytes read, more could have been
				read off of the socket, however, and lost by the kernel when
				the error occured. */
			*numread = (nbytes - nleft);
            return NET_READ_FAIL;

        } else if (nread == 0) {
            /* We've reached the end of file marker, so stop looping. */
            break;
        }

		/* update counters */
        nleft -= nread;
        ptr = ((char *)ptr) + nread;
    }

	rt_alarm.ResetAlarm();

    /* return how much was actually read, which could include 0 in an
        EOF situation */
    *numread = (nbytes - nleft);     

	return NET_READ_OK;
}

/* This function accepts the *first* packet on a ready service req socket.
	Its job is to figure out if the client side is 32 bits or 64 bits, 
	read the appropriate information off the socket, and set up the
	FDContext with the fd and the knowledge of what type of bit width
	the client side is. This function also translates between the host
	service_req_socket and whatever the client has.
*/
int recv_service_req_pkt(service_req_pkt *srq, FDContext *fdc)
{
	size_t bytes_recvd;
	char netpkt[SREQ_PKTSIZE_MAX];
	read_result_t ret;
	int ok;
	size_t diff;

	/* We better not know what the client bit width is when this function is
		called, since it figures it out! */
	ASSERT(fdc->type == FDC_UNKNOWN);

	memset(netpkt, 0, SREQ_PKTSIZE_MAX);

	/* Read the *smallest* of the two possible structure
		widths, this will ensure we don't deadlock reading more
		bytes that will never come.
	*/
	ret = net_read_with_timeout(fdc->fd, netpkt, SREQ_PKTSIZE_MIN, &bytes_recvd,
								REQUEST_TIMEOUT);
	switch(ret) {
		case NET_READ_FAIL:
			Server::Log("Failed to read initial service_req_pkt "
						"packet length!");
			return PC_NOT_OK;
			break;

		case NET_READ_TIMEOUT:
			Server::Log("Timed out while reading initial service_req_pkt "
						"packet length!");
			return PC_NOT_OK;
			break;

		case NET_READ_OK:
			/* ensure I read how much I meant to read */
			if (bytes_recvd != SREQ_PKTSIZE_MIN) {
				Server::Log("Short read while reading a possible 32-bit "
							"service_req_pkt.");
				return PC_NOT_OK;
			}

			/* all good, keep the control flow going */

			break;

		default:
			/* Normally, one would except, but why take down a whole server
				when we could just close this errant connection? */
			Server::Log("Programmer error: unhandled return code on "
						"net_read_with_timeout() with suspected 32 bit client "
						"while handling a service_req_pkt");
			return PC_NOT_OK;
			break;
	}

	/* Figure out what kind of packet it is */
	ok = sreq_is_32bit(netpkt);
	if (!ok) {
		/* try to read the rest of the pkt, assuming it is a 64 bit packet */
		diff = SREQ_PKTSIZE_64 - SREQ_PKTSIZE_32;

		ret = net_read_with_timeout(fdc->fd, netpkt + SREQ_PKTSIZE_32,
									diff, &bytes_recvd, REQUEST_TIMEOUT);
		switch(ret) {
			case NET_READ_FAIL:
				Server::Log("Failed to read 64 bit portion of pkt");
				return PC_NOT_OK;
				break;

			case NET_READ_TIMEOUT:
				Server::Log("Timed out while reading 64 bit portion of pkt");
				return PC_NOT_OK;
				break;

			case NET_READ_OK:
				/* ensure I read how much I meant to read */
				if (bytes_recvd != diff) {
					Server::Log("Short read while reading the remainder of "
								"a possible 64-bit service_req_pkt.");
					return PC_NOT_OK;
				}

				/* all good, keep the control flow going */

				break;

			default:
				/* Normally, one would except, but why take down a whole server
					when we could just close this errant connection? */
				Server::Log("Programmer error: unhandled return code on "
							"net_read_with_timeout() with suspected 64 "
							"bit client while handling a service_req_pkt");
				return PC_NOT_OK;
				break;
		}

		/* now, see if we can find it in the 64 bit version of the packet */
		ok = sreq_is_64bit(netpkt);
		if (!ok) {
			/* oops, we didn't find the hard coded ticket in either context
				of 32 bit or 64 bit, so apparently, we didn't read the
				expected kind of packet on the wire or it was garbage.
				fdc stays unknown.
			*/
			Server::Log("Could not determine if packet is a service_req_pkt! "
						"Aborting connection!");
			return PC_NOT_OK;
		}

		/* unpack the 64 bit case into the host structure. Don't forget
			to undo the network byte ordering. */

		srq->ticket = unpack_uint64_t(netpkt, SREQ64_ticket);
		srq->ticket =
			network_uint64_t_order_to_host_uint64_t_order(srq->ticket);

		srq->service = unpack_uint16_t(netpkt, SREQ64_service);
		srq->service =
			network_uint16_t_order_to_host_uint16_t_order(srq->service);

		srq->key = unpack_uint64_t(netpkt, SREQ64_key);
		srq->key =
			network_uint64_t_order_to_host_uint64_t_order(srq->key);

		memmove(srq->owner_name,
			unpack_char_array(netpkt, SREQ64_owner_name),
			MAX_NAME_LENGTH);

		memmove(srq->file_name,
			unpack_char_array(netpkt, SREQ64_file_name),
			MAX_CONDOR_FILENAME_LENGTH);

		memmove(srq->new_file_name,
			unpack_char_array(netpkt, SREQ64_new_file_name),
			MAX_CONDOR_FILENAME_LENGTH-4);

		srq->shadow_IP = unpack_in_addr(netpkt, SREQ64_shadow_IP);
		srq->shadow_IP.s_addr =
			network_uint32_t_order_to_host_uint32_t_order(srq->shadow_IP.s_addr);

		Server::Log("Client is using the 64 bit protocol.");

		fdc->type = FDC_64;
		return PC_OK;
	}

	/* unpack the 32 bit case into the host structure. Don't forget
		to undo the network byte ordering. */
	srq->ticket = unpack_uint32_t(netpkt, SREQ32_ticket);
	srq->ticket = network_uint32_t_order_to_host_uint32_t_order(srq->ticket);

	srq->service = unpack_uint16_t(netpkt, SREQ32_service);
	srq->service = network_uint16_t_order_to_host_uint16_t_order(srq->service);

	srq->key = unpack_uint32_t(netpkt, SREQ32_key);
	srq->key = network_uint32_t_order_to_host_uint32_t_order(srq->key);

	memmove(srq->owner_name,
		unpack_char_array(netpkt, SREQ32_owner_name),
		MAX_NAME_LENGTH);

	memmove(srq->file_name,
		unpack_char_array(netpkt, SREQ32_file_name),
		MAX_CONDOR_FILENAME_LENGTH);

	memmove(srq->new_file_name,
		unpack_char_array(netpkt, SREQ32_new_file_name),
		MAX_CONDOR_FILENAME_LENGTH-4);

	srq->shadow_IP = unpack_in_addr(netpkt, SREQ32_shadow_IP);
	srq->shadow_IP.s_addr =
		network_uint32_t_order_to_host_uint32_t_order(srq->shadow_IP.s_addr);

	Server::Log("Client is using the 32 bit protocol.");

	fdc->type = FDC_32;
	return PC_OK;
}

/* Does a pile of bits on the floor look like a 32 bit service_request_pkt
	when squinted at in the right light? */
bool sreq_is_32bit(char *pkt)
{
	uint32_t ticket;
	uint16_t service;

	/* first we sanity check the ticket and see if the ticket field is the
		correct authentication number */
	ticket = unpack_uint32_t(pkt, SREQ32_ticket);
	ticket = network_uint32_t_order_to_host_uint32_t_order(ticket);
	if (ticket != AUTHENTICATION_TCKT) {
		return false;
	}
	
	/* To check the validity of this packet, I extract the second field
		out of the packet, the "service" field. With a 64 bit packet, this
		should be zero (at the offset of the field in a 32 bit packet) because
		this is the high 32 bits of the 64 bit "ticket" field, which comes
		before it in memory. On a 32 bit machine, the service type should be
		within the set defined by the service_type enum. 
	*/

	/* ok, extract the service type, and see if it is in the right set */
	service = unpack_uint16_t(pkt, SREQ32_service);
	service = network_uint16_t_order_to_host_uint16_t_order(service);

	if (service < CKPT_SERVER_SERVICE_STATUS || 
		service > SERVICE_ABORT_REPLICATION)
	{
		/* oops, can't be a 32 bit packet */
		return false;
	}
	
	/* I guess it passed! Heuristics to the rescue! */
	return true;
}

/* Does a pile of bits on the floor look like a 64 bit service_request_pkt
	when squinted at in the right light? */
bool sreq_is_64bit(char *pkt)
{
	uint64_t ticket;
	uint16_t service;

	/* first we sanity check the ticket and see if the ticket field is the
		correct authentication number */
	ticket = unpack_uint64_t(pkt, SREQ64_ticket);
	ticket = network_uint64_t_order_to_host_uint64_t_order(ticket);

	if (ticket != AUTHENTICATION_TCKT) {
		return false;
	}
	
	/* To check the validity of this packet, I extract the second field
		out of the packet, the "service" field. With a 64 bit packet, this
		should be in range (at the offset of the 64 bit packet). If
		not, then I guess it isn't a 64 bit packet. 
	*/

	/* ok, extract the service type, and see if it is in the right set */
	service = unpack_uint16_t(pkt, SREQ64_service);
	service = network_uint16_t_order_to_host_uint16_t_order(service);

	if (service < CKPT_SERVER_SERVICE_STATUS || 
		service > SERVICE_ABORT_REPLICATION)
	{
		/* oops, can't be a 64 bit packet */
		return false;
	}
	
	/* I guess it passed! Heuristics to the rescue! */
	return true;
}

/* depending upon the connection type, assemble a service_reply_pkt and
	send it to the other side */
int send_service_reply_pkt(service_reply_pkt *srp, FDContext *fdc)
{
	/* This will be the service_reply_pkt */
	char netpkt[SREP_PKTSIZE_MAX];
	/* The fields of the packet */
	uint16_t			req_status;
	struct in_addr		server_addr;
	uint16_t			port;
	uint32_t			num_files_32 = 0;
	uint64_t			num_files_64 = 0;
	char				capacity_free_ACD[MAX_ASCII_CODED_DECIMAL_LENGTH];
	int					ret = -1;

	ASSERT(fdc->type != FDC_UNKNOWN);

	memset(netpkt, 0, SREP_PKTSIZE_MAX);

	/* get the right sized quantities I need depending upon what the client
		needs.  Ensure to convert it to network byte order here too.
	*/
	req_status = srp->req_status;
	req_status = host_uint16_t_order_to_network_uint16_t_order(req_status);

	server_addr = srp->server_addr;
	server_addr.s_addr =
		host_uint32_t_order_to_network_uint32_t_order(server_addr.s_addr);
	
	port = srp->port;
	port = host_uint16_t_order_to_network_uint16_t_order(port);

	switch(fdc->type)
	{
		case FDC_32:
			num_files_32 = srp->num_files;
			num_files_32 =
				host_uint32_t_order_to_network_uint32_t_order(num_files_32);
			break;

		case FDC_64:
			num_files_64 = srp->num_files;
			num_files_64 =
				host_uint64_t_order_to_network_uint64_t_order(num_files_64);
			break;

		default:
			Server::Log("service_reply_pkt type conversion error!");
			return NET_WRITE_FAIL;
			break;
	}

	memmove(capacity_free_ACD,
			srp->capacity_free_ACD,
			MAX_ASCII_CODED_DECIMAL_LENGTH);
	
	/* Assemble the packet according to what type of connection it is. 
		Then send it. */
	switch(fdc->type)
	{
		case FDC_32:
			pack_uint16_t(netpkt, SREP32_req_status, req_status);
			pack_in_addr(netpkt, SREP32_server_addr, server_addr);
			pack_uint16_t(netpkt, SREP32_port, port);
			pack_uint32_t(netpkt, SREP32_num_files, num_files_32);
			pack_char_array(netpkt, SREP32_capacity_free_ACD,
				capacity_free_ACD, MAX_ASCII_CODED_DECIMAL_LENGTH);

			ret = net_write(fdc->fd, netpkt, SREP_PKTSIZE_32);
			break;

		case FDC_64:
			pack_uint16_t(netpkt, SREP64_req_status, req_status);
			pack_in_addr(netpkt, SREP64_server_addr, server_addr);
			pack_uint16_t(netpkt, SREP64_port, port);
			pack_uint64_t(netpkt, SREP64_num_files, num_files_64);
			pack_char_array(netpkt, SREP64_capacity_free_ACD,
				capacity_free_ACD, MAX_ASCII_CODED_DECIMAL_LENGTH);

			ret = net_write(fdc->fd, netpkt, SREP_PKTSIZE_64);
			break;

		default:
			Server::Log("service_reply_pkt type packing error!");
			return NET_WRITE_FAIL;
			break;
	}

	if (ret < 0) {
		return NET_WRITE_FAIL;
	}

	return NET_WRITE_OK;
}

/* This function accepts the *first* packet on a ready store req socket.
	Its job is to figure out if the client side is 32 bits or 64 bits, 
	read the appropriate information off the socket, and set up the
	FDContext with the fd and the knowledge of what type of bit width
	the client side is. This function also translates between the host
	store_req_socket and whatever the client has.
*/
int recv_store_req_pkt(store_req_pkt *strq, FDContext *fdc)
{
	size_t bytes_recvd;
	char netpkt[STREQ_PKTSIZE_MAX];
	read_result_t ret;
	int ok;
	size_t diff;
	uint32_t tmp64;

	/* We better not know what the client bit width is when this function is
		called, since it figures it out! */
	ASSERT(fdc->type == FDC_UNKNOWN);

	memset(netpkt, 0, STREQ_PKTSIZE_MAX);

	/* Read the *smallest* of the two possible structure
		widths, this will ensure we don't deadlock reading more
		bytes that will never come.
	*/
	ret = net_read_with_timeout(fdc->fd, netpkt, STREQ_PKTSIZE_MIN, 
								&bytes_recvd, REQUEST_TIMEOUT);
	switch(ret) {
		case NET_READ_FAIL:
			Server::Log("Failed to read initial store_req_pkt packet length!");
			return PC_NOT_OK;
			break;

		case NET_READ_TIMEOUT:
			Server::Log("Timed out while reading initial store_req_pkt "
						"packet length!");
			return PC_NOT_OK;
			break;

		case NET_READ_OK:
			/* ensure I read how much I meant to read */
			if (bytes_recvd != STREQ_PKTSIZE_MIN) {
				Server::Log("Short read while reading a possible 32-bit "
							"store_req_pkt.");
				return PC_NOT_OK;
			}

			/* all good, keep the control flow going */
			break;

		default:
			/* Normally, one would except, but why take down a whole server
				when we could just close this errant connection? */
			Server::Log("Programmer error: unhandled return code on "
						"net_read_with_timeout() with suspected 32 bit client "
						"while handling a store_req_pkt");
			return PC_NOT_OK;
			break;
	}

	/* Figure out what kind of packet it is */
	ok = streq_is_32bit(netpkt);
	if (!ok) {
		/* try to read the rest of the pkt, assuming it is a 64 bit packet */
		diff = STREQ_PKTSIZE_64 - STREQ_PKTSIZE_32;

		ret = net_read_with_timeout(fdc->fd, netpkt + STREQ_PKTSIZE_32,
									diff, &bytes_recvd, REQUEST_TIMEOUT);
		switch(ret) {
			case NET_READ_FAIL:
				Server::Log("Failed to read 64 bit portion of store_req_pkt");
				return PC_NOT_OK;
				break;

			case NET_READ_TIMEOUT:
				Server::Log("Timed out while reading 64 bit portion of "
							"store_req_pkt");
				return PC_NOT_OK;
				break;

			case NET_READ_OK:
				/* ensure I read how much I meant to read */
				if (bytes_recvd != diff) {
					Server::Log("Short read while reading the remainder of "
								"a possible 64-bit store_req_pkt.");
					return PC_NOT_OK;
				}

				/* all good, keep the control flow going */
				break;

			default:
				/* Normally, one would except, but why take down a whole server
					when we could just close this errant connection? */
				Server::Log("Programmer error: unhandled return code on "
							"net_read_with_timeout() with suspected 64 "
							"bit client while handling a store_req_pkt");
				return PC_NOT_OK;
				break;
		}

		/* now, see if we can find it in the 64 bit version of the packet */
		ok = streq_is_64bit(netpkt);
		if (!ok) {
			/* oops, we didn't find the hard coded ticket in either context
				of 32 bit or 64 bit, so apparently, we didn't read the
				expected kind of packet on the wire or it was garbage.
				fdc stays unknown.
			*/
			Server::Log("Could not determine if packet is a store_req_pkt! "
						"Aborting connection!");
			return PC_NOT_OK;
		}

		/* unpack the 64 bit case into the host structure. Don't forget
			to undo the network byte ordering. */

		tmp64 = unpack_uint64_t(netpkt, STREQ64_file_size);
		tmp64 = network_uint64_t_order_to_host_uint64_t_order(tmp64);
		/* This could be type narrowing if compiled on a 32 bit machine. */
		strq->file_size = tmp64;

		/* ok, if the file_size field we're putting this into can't support the 
			extent of the number, we'll bail on the connection with a
			warning to upgrade to a 64 bit checkpoint server. */
		if ((uint64_t)strq->file_size != (uint64_t)tmp64) {
			Server::Log("This checkpoint server cannot represent the file "
				"size for this checkpoint STORE request. Please upgrade this "
				"checkpoint server to a 64-bit executable.");
			return PC_NOT_OK;
		}

		strq->ticket = unpack_uint64_t(netpkt, STREQ64_ticket);
		strq->ticket =
			network_uint64_t_order_to_host_uint64_t_order(strq->ticket);

		strq->priority = unpack_uint64_t(netpkt, STREQ64_priority);
		strq->priority =
			network_uint64_t_order_to_host_uint64_t_order(strq->priority);

		strq->time_consumed = unpack_uint64_t(netpkt, STREQ64_time_consumed);
		strq->time_consumed =
			network_uint64_t_order_to_host_uint64_t_order(strq->time_consumed);

		strq->key = unpack_uint64_t(netpkt, STREQ64_key);
		strq->key =
			network_uint64_t_order_to_host_uint64_t_order(strq->key);

		memmove(strq->filename,
			unpack_char_array(netpkt, STREQ64_filename),
			MAX_CONDOR_FILENAME_LENGTH);

		memmove(strq->owner,
			unpack_char_array(netpkt, STREQ64_owner),
			MAX_NAME_LENGTH);

		Server::Log("Client is using the 64 bit protocol.");

		fdc->type = FDC_64;
		return PC_OK;
	}

	/* unpack the 32 bit case into the host structure. Don't forget
		to undo the network byte ordering. */

	strq->file_size = unpack_uint32_t(netpkt, STREQ32_file_size);
	strq->file_size =
		network_uint32_t_order_to_host_uint32_t_order(strq->file_size);

	strq->ticket = unpack_uint32_t(netpkt, STREQ32_ticket);
	strq->ticket =
		network_uint32_t_order_to_host_uint32_t_order(strq->ticket);

	strq->priority = unpack_uint32_t(netpkt, STREQ32_priority);
	strq->priority =
		network_uint32_t_order_to_host_uint32_t_order(strq->priority);

	strq->time_consumed = unpack_uint32_t(netpkt, STREQ32_time_consumed);
	strq->time_consumed =
		network_uint32_t_order_to_host_uint32_t_order(strq->time_consumed);

	strq->key = unpack_uint32_t(netpkt, STREQ32_key);
	strq->key =
		network_uint32_t_order_to_host_uint32_t_order(strq->key);

	memmove(strq->filename,
		unpack_char_array(netpkt, STREQ32_filename),
		MAX_CONDOR_FILENAME_LENGTH);

	memmove(strq->owner,
		unpack_char_array(netpkt, STREQ32_owner),
		MAX_NAME_LENGTH);

	Server::Log("Client is using the 32 bit protocol.");

	fdc->type = FDC_32;
	return PC_OK;
}

/* Does a pile of bits on the floor look like a 32 bit store_request_pkt
	when squinted at in the right light? */
bool streq_is_32bit(char *pkt)
{
	uint32_t ticket;

	/* we sanity check the ticket and see if the ticket field is the
		correct authentication number. This is in a different place between
		the 32 and 64 bit packets, so it is good enough to separate them. */
	ticket = unpack_uint32_t(pkt, STREQ32_ticket);
	ticket = network_uint32_t_order_to_host_uint32_t_order(ticket);
	if (ticket != AUTHENTICATION_TCKT) {
		return false;
	}
	
	/* I guess it passed! Heuristics to the rescue! */
	return true;
}

/* Does a pile of bits on the floor look like a 64 bit store_request_pkt
	when squinted at in the right light? */
bool streq_is_64bit(char *pkt)
{
	uint64_t ticket;

	/* we sanity check the ticket and see if the ticket field is the
		correct authentication number. This is in a different place between
		the 32 and 64 bit packets, so it is good enough to separate them. */
	ticket = unpack_uint64_t(pkt, STREQ64_ticket);
	ticket = network_uint64_t_order_to_host_uint64_t_order(ticket);

	if (ticket != AUTHENTICATION_TCKT) {
		return false;
	}
	
	/* I guess it passed! Heuristics to the rescue! */
	return true;
}

/* depending upon the connection type, assemble a store_reply_pkt and
	send it to the other side */
int send_store_reply_pkt(store_reply_pkt *strp, FDContext *fdc)
{
	/* This will be the service_reply_pkt */
	char netpkt[STREP_PKTSIZE_MAX];
	/* The fields of the packet */
	struct in_addr		server_name;
	uint16_t			port;
	uint16_t			req_status;
	int					ret = -1;

	ASSERT(fdc->type != FDC_UNKNOWN);

	memset(netpkt, 0, STREP_PKTSIZE_MAX);

	/* get the right sized quantities I need depending upon what the client
		needs.  Ensure to convert it to network byte order here too.
	*/
	req_status = strp->req_status;
	req_status = host_uint16_t_order_to_network_uint16_t_order(req_status);

	server_name = strp->server_name;
	server_name.s_addr =
		host_uint32_t_order_to_network_uint32_t_order(server_name.s_addr);
	
	port = strp->port;
	port = host_uint16_t_order_to_network_uint16_t_order(port);

	/* Assemble the packet according to what type of connection it is. 
		Then send it. */
	switch(fdc->type)
	{
		case FDC_32:
			pack_in_addr(netpkt, STREP32_server_name, server_name);
			pack_uint16_t(netpkt, STREP32_req_status, req_status);
			pack_uint16_t(netpkt, STREP32_port, port);

			ret = net_write(fdc->fd, netpkt, STREP_PKTSIZE_32);
			break;

		case FDC_64:
			pack_in_addr(netpkt, STREP64_server_name, server_name);
			pack_uint16_t(netpkt, STREP64_req_status, req_status);
			pack_uint16_t(netpkt, STREP64_port, port);

			ret = net_write(fdc->fd, netpkt, STREP_PKTSIZE_64);
			break;

		default:
			Server::Log("store_reply_pkt type packing error!");
			return NET_WRITE_FAIL;
			break;
	}

	if (ret < 0) {
		return NET_WRITE_FAIL;
	}

	return NET_WRITE_OK;
}

/* This function accepts the *first* packet on a ready restore req socket.
	Its job is to figure out if the client side is 32 bits or 64 bits, 
	read the appropriate information off the socket, and set up the
	FDContext with the fd and the knowledge of what type of bit width
	the client side is. This function also translates between the host
	restore_req_socket and whatever the client has.
*/
int recv_restore_req_pkt(restore_req_pkt *rstrq, FDContext *fdc)
{
	size_t bytes_recvd;
	char netpkt[RSTREQ_PKTSIZE_MAX];
	read_result_t ret;
	int ok;
	size_t diff;

	/* We better not know what the client bit width is when this function is
		called, since it figures it out! */
	ASSERT(fdc->type == FDC_UNKNOWN);

	memset(netpkt, 0, RSTREQ_PKTSIZE_MAX);

	/* Read the *smallest* of the two possible structure
		widths, this will ensure we don't deadlock reading more
		bytes that will never come.
	*/
	ret = net_read_with_timeout(fdc->fd, netpkt, RSTREQ_PKTSIZE_MIN, 
								&bytes_recvd, REQUEST_TIMEOUT);
	switch(ret) {
		case NET_READ_FAIL:
			Server::Log("Failed to read initial restore_req_pkt packet length!");
			return PC_NOT_OK;
			break;

		case NET_READ_TIMEOUT:
			Server::Log("Timed out while reading initial restore_req_pkt "
						"packet length!");
			return PC_NOT_OK;
			break;

		case NET_READ_OK:
			/* ensure I read how much I meant to read */
			if (bytes_recvd != RSTREQ_PKTSIZE_MIN) {
				Server::Log("Short read while reading a possible 32-bit "
							"restore_req_pkt.");
				return PC_NOT_OK;
			}

			/* all good, keep the control flow going */
			break;

		default:
			/* Normally, one would except, but why take down a whole server
				when we could just close this errant connection? */
			Server::Log("Programmer error: unhandled return code on "
						"net_read_with_timeout() with suspected 32 bit client "
						"while handling a restore_req_pkt");
			return PC_NOT_OK;
			break;
	}

	/* Figure out what kind of packet it is */
	ok = rstreq_is_32bit(netpkt);
	if (!ok) {
		/* try to read the rest of the pkt, assuming it is a 64 bit packet */
		diff = RSTREQ_PKTSIZE_64 - RSTREQ_PKTSIZE_32;

		ret = net_read_with_timeout(fdc->fd, netpkt + RSTREQ_PKTSIZE_32,
									diff, &bytes_recvd, REQUEST_TIMEOUT);
		switch(ret) {
			case NET_READ_FAIL:
				Server::Log("Failed to read 64 bit portion of restore_req_pkt");
				return PC_NOT_OK;
				break;

			case NET_READ_TIMEOUT:
				Server::Log("Timed out while reading 64 bit portion of "
							"restore_req_pkt");
				return PC_NOT_OK;
				break;

			case NET_READ_OK:
				/* ensure I read how much I meant to read */
				if (bytes_recvd != diff) {
					Server::Log("Short read while reading the remainder of "
								"a possible 64-bit restore_req_pkt.");
					return PC_NOT_OK;
				}

				/* all good, keep the control flow going */
				break;

			default:
				/* Normally, one would except, but why take down a whole server
					when we could just close this errant connection? */
				Server::Log("Programmer error: unhandled return code on "
							"net_read_with_timeout() with suspected 64 "
							"bit client while handling a restore_req_pkt");
				return PC_NOT_OK;
				break;
		}

		/* now, see if we can find it in the 64 bit version of the packet */
		ok = rstreq_is_64bit(netpkt);
		if (!ok) {
			/* oops, we didn't find the hard coded ticket in either context
				of 32 bit or 64 bit, so apparently, we didn't read the
				expected kind of packet on the wire or it was garbage.
				fdc stays unknown.
			*/
			Server::Log("Could not determine if packet is a restore_req_pkt! "
						"Aborting connection!");
			return PC_NOT_OK;
		}

		/* unpack the 64 bit case into the host structure. Don't forget
			to undo the network byte ordering. */

		rstrq->ticket = unpack_uint64_t(netpkt, RSTREQ64_ticket);
		rstrq->ticket =
			network_uint64_t_order_to_host_uint64_t_order(rstrq->ticket);

		rstrq->priority = unpack_uint64_t(netpkt, RSTREQ64_priority);
		rstrq->priority =
			network_uint64_t_order_to_host_uint64_t_order(rstrq->priority);

		rstrq->key = unpack_uint64_t(netpkt, RSTREQ64_key);
		rstrq->key =
			network_uint64_t_order_to_host_uint64_t_order(rstrq->key);

		memmove(rstrq->filename,
			unpack_char_array(netpkt, RSTREQ64_filename),
			MAX_CONDOR_FILENAME_LENGTH);

		memmove(rstrq->owner,
			unpack_char_array(netpkt, RSTREQ64_owner),
			MAX_NAME_LENGTH);

		Server::Log("Client is using the 64 bit protocol.");

		fdc->type = FDC_64;
		return PC_OK;
	}

	/* unpack the 32 bit case into the host structure. Don't forget
		to undo the network byte ordering. */

	rstrq->ticket = unpack_uint32_t(netpkt, RSTREQ32_ticket);
	rstrq->ticket =
		network_uint32_t_order_to_host_uint32_t_order(rstrq->ticket);

	rstrq->priority = unpack_uint32_t(netpkt, RSTREQ32_priority);
	rstrq->priority =
		network_uint32_t_order_to_host_uint32_t_order(rstrq->priority);

	rstrq->key = unpack_uint32_t(netpkt, RSTREQ32_key);
	rstrq->key =
		network_uint32_t_order_to_host_uint32_t_order(rstrq->key);

	memmove(rstrq->filename,
		unpack_char_array(netpkt, RSTREQ32_filename),
		MAX_CONDOR_FILENAME_LENGTH);

	memmove(rstrq->owner,
		unpack_char_array(netpkt, RSTREQ32_owner),
		MAX_NAME_LENGTH);

	Server::Log("Client is using the 32 bit protocol.");

	fdc->type = FDC_32;
	return PC_OK;
}

/* checking the restore_req_pkt for 32 or 64 bit width is difficult due to the
	layout of the packet in memory:

t = ticket
p = priority (known to be hardcoded to zero)
k = key (a pid of a process)
f = filename
o = owner

32 bit Layout (in bytes): ttttppppkkkkf...fo...o
64 bit Layout (in bytes): ttttttttppppppppkkkkkkkkf...fo...o

*/

/* Does a pile of bits on the floor look like a 32 bit restore_request_pkt
	when squinted at in the right light? */
bool rstreq_is_32bit(char *pkt)
{
	uint32_t ticket;
	uint32_t priority;

	/* sanity check */
	ticket = unpack_uint32_t(pkt, RSTREQ32_ticket);
	ticket = network_uint32_t_order_to_host_uint32_t_order(ticket);
	if (ticket != AUTHENTICATION_TCKT) {
		return false;
	}

	/* priority must be zero */
	priority = unpack_uint32_t(pkt, RSTREQ32_priority);
	priority = network_uint32_t_order_to_host_uint32_t_order(priority);
	if (priority != 0) {
		return false;
	}

	/* if the first character of the filename portion is not a valid character,
		then this is a 64 bit packet. I know this because the subsequent byte
		in a 64 bit packet will *always* be zero */
	if (!isprint(pkt[RSTREQ32_filename])) {
		return false;
	}

	/* probably should check the first character of owner to ensure it is
		a printable character as well, if not, then this definitely can't be
		a 32 bit packet.
	*/
	if (!isprint(pkt[RSTREQ32_owner])) {
		return false;
	}
	
	/* I guess it passed! Heuristics to the rescue! */
	return true;
}

/* Does a pile of bits on the floor look like a 64 bit restore_request_pkt
	when squinted at in the right light? */
bool rstreq_is_64bit(char *pkt)
{
	uint64_t ticket;
	uint64_t priority;

	/* sanity check */
	ticket = unpack_uint64_t(pkt, RSTREQ64_ticket);
	ticket = network_uint64_t_order_to_host_uint64_t_order(ticket);
	if (ticket != AUTHENTICATION_TCKT) {
		return false;
	}

	/* priority must be zero */
	priority = unpack_uint64_t(pkt, RSTREQ64_priority);
	priority = network_uint64_t_order_to_host_uint64_t_order(priority);
	if (priority != 0) {
		return false;
	}

	/* I suppose I can check the first characters of the strings in this
		packet, and if either of them are not printable, then I would know
		for sure this _isn't_ a 64 bit packet. But it does not confirm that
		it is. :)
	*/
	if (!isprint(pkt[RSTREQ64_filename])) {
		return false;
	}

	if (!isprint(pkt[RSTREQ64_owner])) {
		return false;
	}

	/* I guess it passed! Heuristics to the rescue! */
	return true;
}

/* depending upon the connection type, assemble a store_reply_pkt and
	send it to the other side */
int send_restore_reply_pkt(restore_reply_pkt *rstrp, FDContext *fdc)
{
	/* This will be the service_reply_pkt */
	char netpkt[RSTREP_PKTSIZE_MAX];
	/* The fields of the packet */
	struct in_addr		server_name;
	uint16_t			port;
	uint32_t			file_size_32 = 0 ;
	uint64_t			file_size_64 = 0 ;
	uint16_t			req_status;
	int					ret = -1;

	ASSERT(fdc->type != FDC_UNKNOWN);

	memset(netpkt, 0, RSTREP_PKTSIZE_MAX);

	/* get the right sized quantities I need depending upon what the client
		needs.  Ensure to convert it to network byte order here too.
	*/
	server_name = rstrp->server_name;
	server_name.s_addr =
		host_uint32_t_order_to_network_uint32_t_order(server_name.s_addr);
	
	port = rstrp->port;
	port = host_uint16_t_order_to_network_uint16_t_order(port);

	switch(fdc->type)
	{
		case FDC_32:
			file_size_32 = rstrp->file_size;
			
			/* If there is the possibility of a type narrowing, then check
				to see if bits were actually lost. */
			if ((sizeof(file_size_32) < sizeof(rstrp->file_size)) &&
				(uint64_t)file_size_32 != (uint64_t)rstrp->file_size)
			{
				Server::Log("restore_reply_pkt type narrowed from 64 to 32 "
					"bits with known loss of data and the ckpt restore will "
					"likely fail.");
				return NET_WRITE_FAIL;
			}

			file_size_32 = 
				host_uint32_t_order_to_network_uint32_t_order(file_size_32);
			break;

		case FDC_64:
			file_size_64 = rstrp->file_size;
			file_size_64 = 
				host_uint64_t_order_to_network_uint64_t_order(file_size_64);
			break;

		default:
			Server::Log("restore_reply_pkt type conversion error!");
			return NET_WRITE_FAIL;
			break;
	}

	req_status = rstrp->req_status;
	req_status = host_uint16_t_order_to_network_uint16_t_order(req_status);

	/* Assemble the packet according to what type of connection it is. 
		Then send it. */
	switch(fdc->type)
	{
		case FDC_32:
			pack_in_addr(netpkt, RSTREP32_server_name, server_name);
			pack_uint16_t(netpkt, RSTREP32_port, port);
			pack_uint32_t(netpkt, RSTREP32_file_size, file_size_32);
			pack_uint16_t(netpkt, RSTREP32_req_status, req_status);

			ret = net_write(fdc->fd, netpkt, RSTREP_PKTSIZE_32);
			break;

		case FDC_64:
			pack_in_addr(netpkt, RSTREP64_server_name, server_name);
			pack_uint16_t(netpkt, RSTREP64_port, port);
			pack_uint64_t(netpkt, RSTREP64_file_size, file_size_64);
			pack_uint16_t(netpkt, RSTREP64_req_status, req_status);

			ret = net_write(fdc->fd, netpkt, RSTREP_PKTSIZE_64);
			break;

		default:
			Server::Log("restore_reply_pkt type packing error!");
			return NET_WRITE_FAIL;
			break;
	}

	if (ret < 0) {
		return NET_WRITE_FAIL;
	}

	return NET_WRITE_OK;
}




/* ------------------------------------------------------------------------- */
/* These are size and type specific functions to handle endianess wrt host and
	network ordering. These functions are named what they are named to make
	it perfectly clear what is going on. Also the 64bit ones are explicitly
	handled to dampen the extra crazy deep in the raw bits off the wire.
*/

uint16_t network_uint16_t_order_to_host_uint16_t_order(uint16_t val)
{
	/* This function is defined to take a uint16_t */
	return ntohs(val);
}

uint32_t network_uint32_t_order_to_host_uint32_t_order(uint32_t val)
{
	/* This function is defined to take a uint32_t */
	return ntohl(val);
}

uint64_t network_uint64_t_order_to_host_uint64_t_order(uint64_t val)
{
	union {
		uint64_t val;
		char bytes[sizeof(uint64_t)];
	} sex, xes;

	sex.val = val;

	if (42 != htonl(42)) { /* am I little endian? */
		/* This partial swapping is a result of the original author
			calling ntohl() on an unsigned long int, and then shoving it across
			the wire via write(). This was before ntohl() became *defined* to
			only work on 32 bits, and machines got powerful enough that  the
			unsigned long int became an 8 byte quantity instead of a 4 byte
			quantity.

			This means that this function, and its sister, does logically what
			it is meant to do, but not functionally.

			Don't weep for mankind. It isn't worth it.
		*/

		xes.bytes[0] = sex.bytes[3];
		xes.bytes[1] = sex.bytes[2];
		xes.bytes[2] = sex.bytes[1];
		xes.bytes[3] = sex.bytes[0];

		xes.bytes[4] = sex.bytes[4];
		xes.bytes[5] = sex.bytes[5];
		xes.bytes[6] = sex.bytes[6];
		xes.bytes[7] = sex.bytes[7];

	} else {
		/* no, so already done */
		xes = sex;
	}

	return xes.val;
}

uint16_t host_uint16_t_order_to_network_uint16_t_order(uint16_t val)
{
	/* This function is defined to take a uint16_t */
	return htons(val);
}

uint32_t host_uint32_t_order_to_network_uint32_t_order(uint32_t val)
{
	/* This function is defined to take a uint32_t */
	return htonl(val);
}

uint64_t host_uint64_t_order_to_network_uint64_t_order(uint64_t val)
{
	union {
		uint64_t val;
		char bytes[sizeof(uint64_t)];
	} sex, xes;
	
	sex.val = val;

	if (42 != htonl(42)) { /* am I little endian? */
		/* see the sister function to this one for an explanation */
		xes.bytes[0] = sex.bytes[3];
		xes.bytes[1] = sex.bytes[2];
		xes.bytes[2] = sex.bytes[1];
		xes.bytes[3] = sex.bytes[0];

		xes.bytes[4] = sex.bytes[4];
		xes.bytes[5] = sex.bytes[5];
		xes.bytes[6] = sex.bytes[6];
		xes.bytes[7] = sex.bytes[7];
	} else {
		/* no, so already done */
		xes = sex;
	}

	return xes.val;
}


/* ------------------------------------------------------------------------- */
/* These are the unpack/pack methods to get stuff out of the network packet */
/* This assumes the same endianess between client and server */

/* unpacking interface */

uint64_t unpack_uint64_t(char *pkt, size_t off)
{
	return *(uint64_t*)(&pkt[off]);
}

uint32_t unpack_uint32_t(char *pkt, size_t off)
{
	return *(uint32_t*)(&pkt[off]);
}

unsigned short unpack_uint16_t(char *pkt, size_t off)
{
	return *(uint16_t*)(&pkt[off]);
}

struct in_addr unpack_in_addr(char *pkt, size_t off)
{
	return *(struct in_addr*)(&pkt[off]);
}

/* Do not free this memory, caller doesn't own it */
char* unpack_char_array(char *pkt, size_t off)
{
	return &pkt[off];
}

/* packing interface */

void pack_uint64_t(char *pkt, size_t off, uint64_t val)
{
	*(uint64_t*)(&pkt[off]) = val;
}

void pack_uint32_t(char *pkt, size_t off, uint32_t val)
{
	*(uint32_t*)(&pkt[off]) = val;
}

void pack_uint16_t(char *pkt, size_t off, uint16_t val)
{
	*(uint16_t*)(&pkt[off]) = val;
}

void pack_in_addr(char *pkt, size_t off, struct in_addr inaddr)
{
	*(struct in_addr*)(&pkt[off]) = inaddr;
}

void pack_char_array(char *pkt, size_t off, char *str, size_t len)
{
	memmove(&pkt[off], str, len);
}


/* ------------------------------------------------------------------------- */
/* These are some utility functions */

/* This function is a debugging function and is only called when it is hand
	inserted into the code flow to see what the binary of a packet looks like.
*/
void dump_pkt(char *netpkt, size_t size)
{
	size_t r;
	MyString str, val = "", tmp;
	char c;
	const char *lookup[16] = 
		{	"0", "1", "2", "3", "4", "5", "6", "7",
			"8", "9", "A", "B", "C", "D", "E", "F" };

	str.formatstr("Netpkt dump: %u bytes\n", (unsigned int)size);
	Server::Log(str.Value());
	for (r = 0; r < size; r++) {
		val += lookup[((unsigned char)netpkt[r]) >> 4];
		val += lookup[((unsigned char)netpkt[r]) & 0x0F];
	}

	str.formatstr("%s\n",val.Value());
	Server::Log(str.Value());

	val = "";
	for (r = 0; r < size; r++) {
		tmp.formatstr("[%u: ", (unsigned int)r);
		c = netpkt[r];
		if (isprint(c)) {
			tmp += c;
		} else {
			tmp += '.';
		}
		tmp += "]\n";
		val += tmp;
	}

	str.formatstr("%s\n",val.Value());
	Server::Log(str.Value());
}
