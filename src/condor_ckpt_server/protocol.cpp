#include "condor_common.h"
#include "condor_debug.h"
#include "condor_fix_iostream.h"
#include "condor_fix_fstream.h"
#include "MyString.h"

#include "constants2.h"
#include "typedefs2.h"
#include "alarm2.h"
#include "server2.h"
#include "protocol.h"

/*

int recv_store_req();
int recv_restore_req();
int recv_replicate_req();

int send_service_reply();
int send_store_reply();
int send_restore_reply();
int send_replicate_reply();

*/

/* This function accepts the *first* packet on a ready service req socket.
	Its job is to figure out if the client side is 32 bits or 64 bits, 
	read the appropriate information off the socket, and set up the
	FDContext with the fd and the knowledge of what type of bit width
	the client side is. This function also translates between the host
	service_req_socket and whatever the client has.
*/
int recv_service_req_pkt(service_req_pkt *srp, FDContext *fdc)
{
	size_t bytes_recvd;
	int req_len, temp_len;
	char netpkt[SREQ_MAXSIZE];
	read_result_t ret;
	int found;
	size_t diff;

	/* We better not know what the client bit width is when this function is
		called, since it figures it out! */
	ASSERT(fdc->type == FDC_UNKNOWN);

	/* Read the *smallest* of the two possible structure
		widths, this will ensure we don't deadlock reading more
		bytes that will never come.
	*/
	ret = net_read_with_timeout(fdc->fd, netpkt, SREQ_MINSIZE, &bytes_recvd,
								REQUEST_TIMEOUT);
	if (ret == NET_READ_FAIL) {
		return -1;
	}

	/* ok, see if I can pick out the magic number in the "ticket" member
		as described by the 32 bit packet */
/*	found = extract_ticket_from_32bit_service_req_pkt(netpkt);*/
	if (!found) {
		/* try to read the rest of the pkt, assuming it is a 64 bit packet */
		diff = SREQ_PKT_64 - SREQ_PKT_32;
		ret = net_read_with_timeout(fdc->fd, netpkt + diff, diff, &bytes_recvd,
								REQUEST_TIMEOUT);
		if (ret == NET_READ_FAIL) {
			return -1;
		}
		
		/* now, see if we can find it in the 64 bit version of the packet */
/*		found = extract_ticket_from_64bit_service_req_pkt(netpkt);*/
		if (!found) {
			/* oops, we didn't find the hard coded ticket in either context
				of 32 bit or 64 bit, so apparently, we didn't read the
				expected kind of packet on the wire.
			*/
			return -1;
		}

		/* TODO 64 bit case*/
		return 0;
	}

	/* TODO 32 bit case */

	return 0;
}

/* This does a blocking read for size amount of bytes. However, there is
	a timeout associated with it. This function is closely allied in
	implementation with _condor_full_read, but due to the alarm signal
	hilarity, it gets its own implementation here. The logic of this
	is a bit screwed up, but I had to fit it within the mindset of the
	ckpt server codebase, which is odd.
*/
read_result_t net_read_with_timeout(int fd, char *ptr, size_t nbytes,
	size_t *numread, int timeout)
{
    int nleft, nread;
	MyString log_msg;
	Alarm rt_alarm;
	int save_errno;

	/* we use signal to implement breaking out of a permanently blocked 
		or very slow read situation */
	rt_alarm.SetAlarm(fd, timeout);

    nleft = nbytes;
    while (nleft > 0) {
        REISSUE_READ: 
        nread = read(fd, ptr, nleft);
        if (nread < 0) {
			save_errno = errno;
            if (errno == EINTR) {
				/* ignore the einter, which, if it happened due to timeout
					the signal handler will have closed the fd associated with
					this call, then the next read issued on the closed fd will
					result in a failure of a badfd, which means the read was
					incomplete. This logic is screwy, but it is better than
					what I originally found. :(
				*/
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


/* ------------------------------------------------------------------------- */
/* These are the unpack/pack methods to get stuff out of the network packet */
/* This assumes the same endianess between client and server */

uint64_t unpack_uint64_t(char *pkt, size_t off)
{
	return *(uint64_t*)&pkt[off];
}

uint32_t unpack_uint32_t(char *pkt, size_t off)
{
	return *(uint32_t*)&pkt[off];
}

unsigned short unpack_uint16_t(char *pkt, size_t off)
{
	return *(unsigned short*)&pkt[off];
}

struct in_addr unpack_in_addr(char *pkt, size_t off)
{
	return *(struct in_addr*)&pkt[off];
}

/* Do not free this memory, caller doesn't own it */
char* unpack_char_array(char *pkt, size_t off)
{
	return &pkt[off];
}





