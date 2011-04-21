#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

//sendall - taken from the Beej guide to socket programming
//http://beej.us/guide/bgnet/output/html/multipage/advanced.html#sendall

int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n = 0;

	if( buf == NULL )
		return -1;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
} 


