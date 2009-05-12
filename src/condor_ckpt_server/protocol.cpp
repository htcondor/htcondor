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
	char netpkt[SREQ_PKTSIZE_MAX];
	read_result_t ret;
	int ok;
	size_t diff;

	/* We better not know what the client bit width is when this function is
		called, since it figures it out! */
	ASSERT(fdc->type == FDC_UNKNOWN);

	/* Read the *smallest* of the two possible structure
		widths, this will ensure we don't deadlock reading more
		bytes that will never come.
	*/
	ret = net_read_with_timeout(fdc->fd, netpkt, SREQ_PKTSIZE_MIN, &bytes_recvd,
								REQUEST_TIMEOUT);
	if (ret == NET_READ_FAIL) {
		return PC_NOT_OK;
	}

	/* ok, see if I can pick out the magic number in the "ticket" member
		as described by the 32 bit packet */
	ok = sreq_is_32bit(netpkt);
	if (!ok) {
		/* try to read the rest of the pkt, assuming it is a 64 bit packet */
		diff = SREQ_PKTSIZE_64 - SREQ_PKTSIZE_32;
		ret = net_read_with_timeout(fdc->fd, netpkt + diff, diff, &bytes_recvd,
								REQUEST_TIMEOUT);
		if (ret == NET_READ_FAIL) {
			/* Hrm, timed out or failed */
			return PC_NOT_OK;
		}
		
		/* now, see if we can find it in the 64 bit version of the packet */
		ok = sreq_is_64bit(netpkt);
		if (!ok) {
			/* oops, we didn't find the hard coded ticket in either context
				of 32 bit or 64 bit, so apparently, we didn't read the
				expected kind of packet on the wire or it was garbage.
			*/
			return PC_NOT_OK;
		}

		/* unpack the 64 bit case into the host structure */
		srp->ticket = unpack_uint64_t(netpkt, SREQ64_ticket);
		srp->service = unpack_uint16_t(netpkt, SREQ64_service);
		srp->key = unpack_uint64_t(netpkt, SREQ64_key);
		memmove(srp->owner_name,
			unpack_char_array(netpkt, SREQ64_owner_name),
			MAX_NAME_LENGTH);
		memmove(srp->file_name,
			unpack_char_array(netpkt, SREQ64_file_name),
			MAX_CONDOR_FILENAME_LENGTH);
		memmove(srp->new_file_name,
			unpack_char_array(netpkt, SREQ64_new_file_name),
			MAX_CONDOR_FILENAME_LENGTH-4);
		srp->shadow_IP = unpack_in_addr(netpkt, SREQ64_shadow_IP);
		return PC_OK;
	}

	/* TODO 32 bit case */

	return PC_OK;
}

/* Does a pile of bits on the floor look like a 32 bit service_request_pkt
	when squinted at in the right light? */
bool sreq_is_32bit(char *pkt)
{
	uint32_t ticket;

	/* Get what I think to be the ticket field out of the packet */
	ticket = unpack_uint32_t(pkt, SREQ32_ticket);

	/* If this constant ever changes from 1637102411L, it means a new "version"
		of the protocol. */
	if (ticket != AUTHENTICATION_TCKT) {
		return false;
	}

	/* if this equals the magic number, we're done! */
	return true;
}

/* Does a pile of bits on the floor look like a 64 bit service_request_pkt
	when squinted at in the right light? */
bool sreq_is_64bit(char *pkt)
{
	uint64_t ticket;

	/* Get what I think to be the ticket field out of the packet */
	ticket = unpack_uint64_t(pkt, SREQ64_ticket);

	/* If this constant ever changes from 1637102411L, it means a new "version"
		of the protocol. */
	if (ticket != AUTHENTICATION_TCKT) {
		return false;
	}

	/* if this equals the magic number, we're done! */
	return true;
}

/* ------------------------------------------------------------------------- */
/* These are the unpack/pack methods to get stuff out of the network packet */
/* This assumes the same endianess between client and server */

/* unpacking interface */

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


/* packing interface */

void pack_uint64_t(char *pkt, size_t off, uint64_t val)
{
	*(uint64_t*)&pkt[off] = val;
}

void pack_uint32_t(char *pkt, size_t off, uint32_t val)
{
	*(uint32_t*)&pkt[off] = val;
}

void pack_uint16_t(char *pkt, size_t off, uint16_t val)
{
	*(unsigned short*)&pkt[off] = val;
}

void pack_in_addr(char *pkt, size_t off, struct in_addr inaddr)
{
	*(struct in_addr*)&pkt[off] = inaddr;
}

void pack_char_array(char *pkt, size_t off, char *str, size_t len)
{
	memmove(&pkt[off], str, len);
}








