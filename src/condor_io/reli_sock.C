/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_config.h"
#include "condor_io.h"
#include "authentication.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_rw.h"
#include "condor_socket_types.h"
#include "get_port_range.h"

#ifdef WIN32
#include <mswsock.h>    // For TransmitFile()
#endif


static double inline
s_pow (double base, unsigned exp)
{
	double result = 1.0;

	for (int i=0; i < exp; i++) {
		result *= base;
	}
	return result;
}


/**************************************************************/

/* 
   NOTE: All ReliSock constructors initialize with this, so you can
   put any shared initialization code here.  -Derek Wright 3/12/99
*/
void
ReliSock::init()
{
    ignore_next_encode_eom = FALSE;
    ignore_next_decode_eom = FALSE;
    _bytes_sent = 0.0;
    _bytes_recvd = 0.0;
    _special_state = relisock_none;
    is_client = 0;
    authob = NULL;
    hostAddr = NULL;
    _bandReg = false;
    _mngSock = 0;
    _maxBytes = 0;
    _greeted = false;
}


ReliSock::ReliSock()
    : Sock()
{
    init();
}


ReliSock::ReliSock(const ReliSock & orig) : Sock(orig)
{
    init();
    // now copy all cedar state info via the serialize() method
    char *buf = NULL;
    buf = orig.serialize();    // get state from orig sock
    assert(buf);
    serialize(buf);            // put the state into the new sock
    delete [] buf;
}


ReliSock::~ReliSock()
{
    close();
    if(_bandReg) {
        reportClose();
        ::close(_mngSock);
    }
    if ( authob ) {
        delete authob;
        authob = NULL;
    }
    if ( hostAddr ) {
        free( hostAddr );
        hostAddr = NULL;
    }
}


int
ReliSock::bandRegulate()
{
    _bandReg = true;
    
    // if this has already connected to the peer and
    // not greeted to the netMnger, do greet to netMnger now
    if(_state == sock_connect && !_greeted)
        return greetMnger();
    else
        return TRUE;
}


// Every parameter must be in network byte order
int
ReliSock::reportConnect(const int myIP, const short myPort,
                        const int peerIP, const short peerPort)
{
    dprintf(D_FULLDEBUG, "ReliSock reported to netMnger a new connection:\n");
    dprintf(D_FULLDEBUG, "\tmyIP: %d(in network bytes order)\n", myIP);
    dprintf(D_FULLDEBUG, "\tmyPort: %d(in network bytes order)\n", myPort);
    dprintf(D_FULLDEBUG, "\tpeerIP: %d(in network bytes order)\n", peerIP);
    dprintf(D_FULLDEBUG, "\tpeerPort: %d(in network bytes order)\n", peerPort);

    char buffer[BND_SZ_CONN];
    int index = 1;
    char pad = 0x00;

    // Make the header
    buffer[0] = BND_CONN_RPT;

    // Fill the content
    memcpy(&buffer[index], &myIP, 4);
    index += 4;

    memcpy(&buffer[index], &myPort, 2);
    index += 2;

    memcpy(&buffer[index], &peerIP, 4);
    index += 4;

    memcpy(&buffer[index], &peerPort, 2);

    // Send the report to netMnger
    if(condor_write(_mngSock, buffer, BND_SZ_CONN, _timeout) == BND_SZ_CONN)
        return TRUE;
    else
        return FALSE;
}


void
ReliSock::reportClose()
{
    dprintf(D_FULLDEBUG, "ReliSock reported close\n");

    char buffer;

    buffer = BND_CLOSE_RPT;

    (void)condor_write(_mngSock, &buffer, BND_SZ_CLOSE, _timeout);

    _bandReg = false;
    _maxBytes = 0;
}


int
ReliSock::reportBandwidth()
{
    char buffer[BND_SZ_BAND];
    unsigned long temp = 0;
    char pad = 0x00;

    // Make the header
    buffer[0] = BND_BAND_RPT;
    dprintf(D_FULLDEBUG, "ReliSock reported Bandwidth Usage upon request from NetMnger: \n");

    if(_bndCtl_state == normal) {
        // report as not being congested
        buffer[1] = (char) false;
        dprintf(D_FULLDEBUG, "\tCongest: false(in normal state)\n");
        if(!_beenBackedOff) {
            if(_totalSent > 0) {
                // report actual bytes sent during the period
                for(int i=0; i<BND_CTL_WINS; i++)
                    temp += _s[i];
            } else {
                temp = _maxBytes;
            }
            dprintf(D_FULLDEBUG, "\tTotal bytes sent: %ld\n", temp);
        } else {
            // report extrapolated value from recent 4 WIN
            int index = _curIdx;
            for(int i=0; i<4; i++) {
                temp += _s[index--];
                if(index < 0)
                    index = BND_CTL_WINS - 1;
            }
            temp *= 4;
            dprintf(D_FULLDEBUG, "\tTotal bytes sent: %ld(Xtrapolated value)\n", temp);
        }
    } else if(_bndCtl_state == backOff) {
        // report as being congested
        buffer[1] = (char) true;
        dprintf(D_FULLDEBUG, "\tCongest: true(in backOff state)\n");
        // report the capability of the current network
        temp = _capability;
        dprintf(D_FULLDEBUG, "\tTotal bytes sent: %ld(_capability)\n", temp);
    } else {
        // report as being not congested
        buffer[1] = (char) false;
        dprintf(D_FULLDEBUG, "\tCongest: false(in scouting state)\n");
        // report the capability of the current network
        temp = _maxBytes;
        dprintf(D_FULLDEBUG, "\tTotal bytes sent: %ld(_maxBytes)\n", temp);
    }

    // Fill the content
    temp = htonl((u_long) temp);
    int index = 2;
    for(int i=0; i<8-sizeof(long); i++)
        buffer[index++] = pad;
    memcpy(&buffer[index], &temp, sizeof(long));

    // Send the report to netMnger
    int result = condor_write(_mngSock, buffer, BND_SZ_BAND, _timeout);
    if (result == BND_SZ_BAND)
        return TRUE;
    else if (result == 0) {
        dprintf(D_ALWAYS, "ReliSock::reportBandwidth - \
                mngSock has been closed prematuraly\n");
        return FALSE;
    } else {
        reportClose();
        ::close(_mngSock);
        return FALSE;
    }
}


int 
ReliSock::listen()
{
    if (_state != sock_bound) {
        dprintf(D_ALWAYS, "ReliSock::listen - socket has not been bound\n");
        return FALSE;
    }
    if (::listen(_sock, 5) < 0) return FALSE;

    dprintf( D_NETWORK, "LISTEN %s\n", sock_to_string(_sock) );

    _state = sock_special;
    _special_state = relisock_listen;

    return TRUE;
}


int 
ReliSock::accept( ReliSock  &c )
{
    int c_sock;
    SOCKET_LENGTH_TYPE addr_sz;

    if (_state != sock_special ||
        _special_state != relisock_listen ||
        c._state != sock_virgin)
    {
        return FALSE;
    }

    if (_timeout > 0) {
        struct timeval  timer;
        fd_set          readfds;
        int             nfds=0, nfound;
        timer.tv_sec = _timeout;
        timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
        nfds = _sock + 1;
#endif
        FD_ZERO( &readfds );
        FD_SET( _sock, &readfds );

        nfound = select( nfds, &readfds, 0, 0, &timer );

        switch(nfound) {
        case 0:
            return FALSE;
            break;
        case 1:
            break;
        default:
            dprintf( D_ALWAYS, "select returns %d, connect failed\n",
                nfound );
            return FALSE;
            break;
        }
    }
    
    addr_sz = sizeof(c._who);
#ifndef WIN32 /* Unix */
    errno = 0;
#endif
    if ((c_sock = ::accept(_sock, (sockaddr *)&c._who, &addr_sz)) < 0) {
#ifndef WIN32 /* Unix */
        if ( errno == EMFILE ) {
            _condor_fd_panic ( __LINE__, __FILE__ ); /* This calls dprintf_exit! */
        }
#endif
        return FALSE;

    }

    c._sock = c_sock;
    c._state = sock_connect;
    c.decode();

    int on = 1;
    c.setsockopt(SOL_SOCKET, SO_KEEPALIVE, (char*)&on, sizeof(on));
    if(_bandReg) {
        if( c.greetMnger() != TRUE ) {
            dprintf(D_ALWAYS, "ReliSock::accept - failed to do greetMnger");
            ::close(c_sock);
            return FALSE;
        }
        if( c.bandRegulate() != TRUE ) {
            dprintf(D_ALWAYS, "ReliSock::accept - failed to do bandRegulate");
            ::close(c_sock);
            return FALSE;
        }
    }

    dprintf( D_NETWORK, "ACCEPT %s ", sock_to_string(_sock) );
    dprintf( D_NETWORK|D_NOHEADER, "%s\n", sin_to_string(c.endpoint()) );

    return TRUE;
}


int 
ReliSock::accept( ReliSock    *c)
{
    if (!c) 
    {
        return FALSE;
    }

    return accept(*c);
}


ReliSock *
ReliSock::accept()
{
    ReliSock    *c_rs;
    int c_sock;

    if (!(c_rs = new ReliSock())) {
        return (ReliSock *)0;
    }

    if ((c_sock = accept(*c_rs)) < 0) {
        delete c_rs;
        return (ReliSock *)0;
    }

    return c_rs;
}


int
ReliSock::connect( char *host, int port, bool non_blocking_flag )
{
    is_client = 1;
    if(!host) {
        dprintf(D_ALWAYS, "ReliSock::connect - 'host' is NULL\n");
        return FALSE;
    }

    /* Connect to peer ReliSock */
    hostAddr = strdup( host );
    if(do_connect( host, port, non_blocking_flag) != TRUE) {
        if(_bandReg && _greeted) {
            reportClose();
            ::close(_mngSock);
        }
        dprintf(D_ALWAYS, "ReliSock::connect - do_connect failed\n");
        return FALSE;
    }

    if(_bandReg) {
        if (greetMnger() != TRUE) {
            ::close(_sock);
            return FALSE;
        } else
            return TRUE;
    }
    else return TRUE;
}


/* Make a connection to netMnger and report connection */
int
ReliSock::greetMnger()
{
    struct sockaddr_in sockAddr;
    unsigned int addrLen = sizeof(sockAddr);

    // Create management socket
    _mngSock = socket(AF_INET, SOCK_STREAM, 0);
    if(_mngSock <= 0) {
        dprintf(D_ALWAYS, "ReliSock::greetMnger - socket creation failed\n");
        return FALSE;
    }

    // Bind _mngSock
    int lowPort, highPort;
    if ( get_port_range(&lowPort, &highPort) == TRUE ) {
        if ( bindWithin(_mngSock, lowPort, highPort) == TRUE ) {
            return TRUE;
        } else return FALSE;
    } else {
        bzero(&sockAddr, sizeof(sockAddr));
        sockAddr.sin_family = AF_INET;
        sockAddr.sin_addr.s_addr = htonl(my_ip_addr());
        sockAddr.sin_port = htons(0);
        if(::bind(_mngSock, (sockaddr *)&sockAddr, addrLen)) {
            dprintf(D_ALWAYS, "ReliSock::greetMnger -\
                               Failed to bind _mngSock to a local port\n");
            return FALSE;
        }
    }

    // get (ip-addr, port) of netMnger
    char *mngerHost = getenv("CONDOR_NET_MNGER");
        mngerHost = NET_MNGER_ADDR;
    char *mngerPort = getenv("CONDOR_NET_MNGER_PORT");
    int portNum = (mngerPort) ? atoi(mngerPort) : NET_MNGER_PORT;

    // Connect to netMnger
    bzero(&sockAddr, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    unsigned long mngerIP;
    if((mngerIP = inet_addr(mngerHost)) != (unsigned)-1) {
        sockAddr.sin_addr.s_addr = mngerIP;
        memcpy(&sockAddr.sin_addr, &mngerIP, sizeof(mngerIP));
    } else {
        struct hostent *mngerEnt = gethostbyname(mngerHost);
        if(!mngerEnt) {
            dprintf(D_ALWAYS, "ReliSock::greetMnger - could not get hostent using %s\n", mngerHost);
            return FALSE;
        }
        memcpy(&sockAddr.sin_addr, mngerEnt->h_addr_list[0], mngerEnt->h_length);
    }
    sockAddr.sin_port = htons((u_short)portNum);
    if(::connect(_mngSock, (sockaddr *)&sockAddr, addrLen)) {
        dprintf(D_ALWAYS, "ReliSock::greetMnger -\
                           Failed to connect to netMnger\n");
        return FALSE;
    }

    // Get the bind point, (IP-addr, port#), of _sock
    bzero(&sockAddr, sizeof(sockAddr));
    if(getsockname(_sock, (sockaddr *)&sockAddr, &addrLen)) {
        dprintf(D_ALWAYS, "Sock::greetMnger - Failed to get socket name\n");
        return FALSE;
    }

    // Report connection
    if(reportConnect(sockAddr.sin_addr.s_addr, sockAddr.sin_port,
                     _who.sin_addr.s_addr, _who.sin_port) == TRUE) {
        _greeted = true;
        return TRUE;
    } else {
        dprintf(D_ALWAYS, "Sock::greetMnger - Failed to reportConnection\n");
        return FALSE;
    }
}


int
ReliSock::put_bytes_nobuffer( char *buf, int length , int send_size)
{

    int i, result;
    int pagesize = 4096;  // Optimize large writes to be page sized.
    char * cur = buf;
    struct timeval ctime;
    bool congested, mngReady;

    // Tell peer how big the transfer is going to be, if requested.
    // Note: send_size param is 1 (true) by default.
    this->encode();
    if ( send_size ) {
        ASSERT( this->code(length) != FALSE );
        ASSERT( this->end_of_message() != FALSE );
    }

    // Optimize transfer by writing in pagesized chunks.
    for(i = 0; i < length;)
    {
        // If there is less then a page left.
        if( (length - i) < pagesize ) {
            if(_bandReg) { // if in bandwidth regulation
                mngReady = false;
                if(_maxBytes > 0) {
                    (void)gettimeofday(&ctime, NULL);
                    _deltaT = (ctime.tv_sec - _T.tv_sec)*1000 +
                              (ctime.tv_usec - _T.tv_usec)/1000;
                    // if the smallest window amount of time has been elapsed
                    if(_deltaT >= _w[BND_CTL_LEVELS-1])
                        calculateAllowance();
                    beNice(pagesize);

                    congested = false;
                    result = condor_mux_write(_sock, _mngSock, cur, (length - i), _timeout,
                                              40000, congested, mngReady);
                    if(result <0) {
                        dprintf(D_ALWAYS, "ReliSock::put_bytes_nobuffer: Send failed.\n");
                        return -1;
                    }
                    _deltaS += result;
                    _numSends++;
                    if(congested) {
                        //cout << "congested: _shortReached = " << _shortReached + 1 << endl;
                        _shortReached++;
                    }
                } else {
                    result = condor_mux_write(_sock, _mngSock, cur, (length - i), _timeout,
                                              0, congested, mngReady);
                    if(result <0) {
                        dprintf(D_ALWAYS, "ReliSock::put_bytes_nobuffer: Send failed.\n");
                        return -1;
                    }
                }

                if(mngReady) { // if something has been arrived at the _mngSock
                    //cout << "put_bytes_nobuf: mngSock(= " << _mngSock << ") is ready\n";
                    result = handleMngPacket();
                    if(result <0) {
                        dprintf(D_ALWAYS, "ReliSock::put_bytes_nobuffer: handleMngPacket failed.\n");
                        return -1;
                    }
                }
            } else { // if not in bandwidth regulation
                result = condor_write(_sock, cur, (length - i), _timeout);
                if( result < 0 ) {
                    dprintf(D_ALWAYS, "ReliSock::put_bytes_nobuffer: Send failed.\n");
                    return -1;
                }
            }

            cur += (length - i);
            i += (length - i);

        } else {  
            if(_bandReg) { // if in bandwidth regulation
                mngReady = false;
                if(_maxBytes > 0) {
                    (void)gettimeofday(&ctime, NULL);
                    _deltaT = (ctime.tv_sec - _T.tv_sec)*1000 +
                              (ctime.tv_usec - _T.tv_usec)/1000;
                    // if the smallest window amount of time has been elapsed
                    if(_deltaT >= _w[BND_CTL_LEVELS-1])
                        calculateAllowance();
                    beNice(pagesize);

                    congested = false;
                    result = condor_mux_write(_sock, _mngSock, cur, pagesize, _timeout,
                                              40000, congested, mngReady);
                    if(result < 0) {
                        dprintf(D_ALWAYS, "ReliSock::put_bytes_nobuffer: Send failed.\n");
                        return -1;
                    }
                    _deltaS += result;
                    _numSends++;
                    if(congested) {
                        //cout << "congested: _shortReached = " << _shortReached + 1 << endl;
                        _shortReached++;
                    }
                } else {
                    result = condor_mux_write(_sock, _mngSock, cur, pagesize, _timeout,
                                              0, congested, mngReady);
                    if(result < 0) {
                        dprintf(D_ALWAYS, "ReliSock::put_bytes_nobuffer: Send failed.\n");
                        return -1;
                    }
                }

                if(mngReady) { // if something has been arrived at the _mngSock
                    result = handleMngPacket();
                    if(result < 0) {
                        dprintf(D_ALWAYS, "ReliSock::put_bytes_nobuffer: handleMngPacket failed.\n");
                        return -1;
                    }
                }
            } else { // if not in bandwidth regulation
                result = condor_write(_sock, cur, pagesize, _timeout);

                if( result < 0 ) {
                    dprintf(D_ALWAYS, "ReliSock::put_bytes_nobuffer: Send failed.\n");
                    return -1;
                }
            }
            cur += pagesize;
            i += pagesize;
        }
    }
    if (i > 0) {
        _bytes_sent += i;
    }
    return i;
}

int 
ReliSock::get_bytes_nobuffer(char *buffer, int max_length, int receive_size)
{
    int result;
    int length;
    bool mngReady = false;

    ASSERT(buffer != NULL);
    ASSERT(max_length > 0);

    // Find out how big the file is going to be, if requested.
    // No receive_size means read max_length bytes.
    this->decode();
    if ( receive_size ) {
        ASSERT( this->code(length) != FALSE );
        ASSERT( this->end_of_message() != FALSE );
    } else {
        length = max_length;
    }

    // First drain incoming buffers
    if ( !prepare_for_nobuffering(stream_decode) ) {
        // error draining buffers; error message already printed
        return -1;
    }


    if( length > max_length ) {
        dprintf(D_ALWAYS, 
            "ReliSock::get_bytes_nobuffer: data too large for buffer.\n");
        return -1;
    }

    if(_bandReg)
        result = condor_mux_read(_sock, _mngSock, buffer, length, _timeout, mngReady);
    else
        result = condor_read(_sock, buffer, length, _timeout);
    
    if(mngReady) {
        //cout << "get_bytes_nobuf: mngSock(= " << _mngSock << ") is ready\n";
        result = handleMngPacket();
    }

    if( result < 0 ) {
        dprintf(D_ALWAYS, 
            "ReliSock::get_bytes_nobuffer: Failed to receive file.\n");
        return -1;
    } else {
        _bytes_recvd += result;
        return result;
    }
}


int
ReliSock::get_file(const char *destination, bool flush_buffers)
{
    int nbytes, written, fd;
    char buf[4096];
    unsigned int filesize;
    unsigned int eom_num;
    unsigned int total = 0;

    if ( !get(filesize) || !end_of_message() ) {
        dprintf(D_ALWAYS, 
            "Failed to receive filesize in ReliSock::get_file\n");
        return -1;
    }
    // dprintf(D_FULLDEBUG,"get_file(): filesize=%d\n",filesize);

#if defined(WIN32)
    if ((fd = ::open(destination, O_WRONLY | O_CREAT | O_TRUNC | 
         _O_BINARY | _O_SEQUENTIAL, 0644)) < 0)
#else /* Unix */
    errno = 0;
    if ((fd = ::open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
#endif
    {
#ifndef WIN32 /* Unix */
        if ( errno == EMFILE ) {
            _condor_fd_panic( __LINE__, __FILE__ ); /* This calls dprintf_exit! */
        }
#endif
        dprintf(D_ALWAYS, "get_file(): Failed to open file %s, errno = %d.\n",
                destination, errno);
        return -1;
    }

    while ((filesize == -1 || total < (int)filesize) &&
            (nbytes = get_bytes_nobuffer(buf, MIN(sizeof(buf),filesize-total),0)) > 0) {
        // dprintf(D_FULLDEBUG, "read %d bytes\n", nbytes);
        if ((written = ::write(fd, buf, nbytes)) < nbytes) {
            dprintf(D_ALWAYS, "failed to write %d bytes in ReliSock::get_file "
                    "(only wrote %d, errno=%d)\n", nbytes, written, errno);
            ::close(fd);
            unlink(destination);
            return -1;
        }
        // dprintf(D_FULLDEBUG, "wrote %d bytes\n", written);
        total += written;
    }

    if ( filesize == 0 ) {
        get(eom_num);
        if ( eom_num != 666 ) {
            dprintf(D_ALWAYS,"get_file: Zero-length file check failed!\n");
            ::close(fd);
            unlink(destination);
            return -1;
        }            
    }

    if (flush_buffers) {
        fsync(fd);
    }
    
    dprintf(D_FULLDEBUG, "wrote %d bytes to file %s\n",
            total, destination);

    if (::close(fd) < 0) {
        dprintf(D_ALWAYS, "close failed in ReliSock::get_file\n");
        return -1;
    }

    if ( total < (int)filesize && filesize != -1 ) {
        dprintf(D_ALWAYS,"get_file(): ERROR: received %d bytes, expected %d!\n",
            total, filesize);
        unlink(destination);
        return -1;
    }

    return total;
}


int
ReliSock::put_file(const char *source)
{
    int fd;
    char buf[4097];
    struct stat filestat;
    unsigned int filesize;
    unsigned int eom_num = 666;
    unsigned int total = 0;

#if defined(WIN32)
    if ((fd = ::open(source, O_RDONLY | _O_BINARY | _O_SEQUENTIAL, 0)) < 0)
#else
    int nbytes, nrd;
    if ((fd = ::open(source, O_RDONLY, 0)) < 0)
#endif
    {
        dprintf(D_ALWAYS, "Failed to open file %s, errno = %d.\n", source, errno);
        return -1;
    }

    if (::fstat(fd, &filestat) < 0) {
        dprintf(D_ALWAYS, "fstat of %s failed\n", source);
        ::close(fd);
        return -1;
    }

    filesize = filestat.st_size;
    if ( !put(filesize) || !end_of_message() ) {
        dprintf(D_ALWAYS, "Failed to send filesize in ReliSock::put_file\n");
        ::close(fd);
        return -1;
    }

    if ( filesize > 0 ) {
        /* Here we used to use different sequence of codes to send a file for
          * windows NT and Unix. The reason that we use the same code for both
          * of them is that we need to control the file transfer in a more fine
          * granuality for network bandwidth regulation.
          */
        while (total < filestat.st_size &&
            (nrd = ::read(fd, buf, sizeof(buf))) > 0) {
            // dprintf(D_FULLDEBUG, "read %d bytes\n", nrd);
            if ((nbytes = put_bytes_nobuffer(buf, nrd, 0)) < nrd) {
                dprintf(D_ALWAYS, "failed to put %d bytes in ReliSock::put_file "
                    "(only wrote %d)\n", nrd, nbytes);
                ::close(fd);
                return -1;
            }
            // dprintf(D_FULLDEBUG, "wrote %d bytes\n", nbytes);
            total += nbytes;
        }
        dprintf(D_FULLDEBUG, "done with transfer (errno = %d)\n", errno);
    } // end of if filesize > 0

    if ( filesize == 0 ) {
        put(eom_num);
    }

    dprintf(D_FULLDEBUG, "sent file %s (%d bytes)\n", source, total);

    if (::close(fd) < 0) {
        dprintf(D_ALWAYS, "close failed in ReliSock::put_file, errno = %d\n", errno);
        return -1;
    }

    if (total < (int)filesize) {
        dprintf(D_ALWAYS,"put_file(): ERROR, only sent %d bytes out of %d\n",
            total, filesize);
        return -1;
    }

    return total;
}


int 
ReliSock::handle_incoming_packet()
{
    bool mngReady = false;

    /* if socket is listening, and packet is there, it is ready for accept */
    if (_state == sock_special && _special_state == relisock_listen) {
        return TRUE;
    }

    /* do not queue up more than one message at a time on reliable sockets */
    /* but return 1, because old message can still be read. */
    if (rcv_msg.ready) {
        return TRUE;
    }

    if(!_bandReg) _mngSock = 0;
    if (!rcv_msg.rcv_packet(_sock, _mngSock, _timeout, mngReady)) {
        return FALSE;
    }

    if(mngReady) {
        //cout << "handle_incoming_packet: mngSock(= " << _mngSock << ") is ready\n";
        if (!handleMngPacket()) {
			return FALSE;
		}
    }

    return TRUE;
}


int 
ReliSock::end_of_message()
{
    int ret_val = FALSE;
    struct timeval ctime;
    int nbytes;
    bool mngReady, congested;

    switch(_coding){
        case stream_encode:
            if ( ignore_next_encode_eom == TRUE ) {
                ignore_next_encode_eom = FALSE;
                return TRUE;
            }
            if (!snd_msg.buf.empty()) {
                // follow the bandwidth regulation rule
                if(_bandReg) {
                    mngReady = false;
                    // we won't be nice here, because the packet we are sending
                    // is too small to regulate
                    if(_maxBytes > 0) {
                        congested = false;
                        nbytes = snd_msg.snd_packet(_sock, _mngSock, TRUE, _timeout, 40000, congested, mngReady);
                        if(nbytes < 0) return FALSE;
                        _deltaS += nbytes;
                        _numSends++;
                        if(congested) _shortReached++;
                    } else {
                        nbytes = snd_msg.snd_packet(_sock, _mngSock, TRUE, _timeout, 0, congested, mngReady);
                    }

                    // check mngSock
                    if(mngReady) {
                        ret_val = handleMngPacket();
                    }
                } else { // not regulating bandwidth
                    _mngSock = 0;
                    if(snd_msg.snd_packet(_sock, _mngSock, TRUE, _timeout, 0, congested, mngReady) < 0)
                        return FALSE;
					ret_val = TRUE;
                }
                return ret_val;
            } // if buf is NOT empty
            break;

        case stream_decode:
            if ( ignore_next_decode_eom == TRUE ) {
                ignore_next_decode_eom = FALSE;
                return TRUE;
            }
            if ( rcv_msg.ready ) {
                if ( rcv_msg.buf.consumed() )
                    ret_val = TRUE;
                rcv_msg.ready = FALSE;
                rcv_msg.buf.reset();
            }
            break;

        default:
            assert(0);
    }

    return ret_val;
}



int 
ReliSock::put_bytes(const void *dta, int sz)
{
    int tw, nw, ret_val = TRUE;
    int nbytes;
    struct timeval ctime;
    bool mngReady, congested;

    ignore_next_encode_eom = FALSE;

    for(nw=0;;) {
        if (snd_msg.buf.full()) {
            // follow the bandwidth regulation rule
            if(_bandReg) { // if in bandwidth regulation
                mngReady = false;
                if(_maxBytes > 0) {
                    (void)gettimeofday(&ctime, NULL);
                    _deltaT = (ctime.tv_sec - _T.tv_sec)*1000 +
                              (ctime.tv_usec - _T.tv_usec)/1000;
                    // if the smallest window amount of time has been elapsed
                    if(_deltaT >= _w[BND_CTL_LEVELS-1])
                        calculateAllowance();
                    beNice(CONDOR_IO_BUF_SIZE);
                
                    congested = false;
                    nbytes = snd_msg.snd_packet(_sock, _mngSock, FALSE, _timeout, 40000, congested, mngReady);
                    if(nbytes < 0) return FALSE;

                    _deltaS += nbytes;
                    _numSends++;
                    if(congested) _shortReached++;
                } else {
                    nbytes = snd_msg.snd_packet(_sock, _mngSock, FALSE, _timeout, 0, congested, mngReady);
                    if(nbytes < 0) return FALSE;
                }

                if(mngReady) { // if something has been arrived at the _mngSock
                    ret_val = handleMngPacket();
                }
            } else { // if not in bandwidth regulation
                _mngSock = 0;
                if(snd_msg.snd_packet(_sock, _mngSock, FALSE, _timeout, 0, congested, mngReady) < 0)
                    return FALSE;
            }

            if(ret_val != TRUE) return FALSE;
        } // if buf full
        
        if (snd_msg.buf.empty()) {
            snd_msg.buf.seek(5);
        }
        
        if ((tw = snd_msg.buf.put_max(&((char *)dta)[nw], sz-nw)) < 0) {
            return -1;
        }
        
        nw += tw;
        if (nw == sz) {
            break;
        }
    }
    if (nw > 0) {
        _bytes_sent += nw;
    }
    return nw;
}


int 
ReliSock::get_bytes(void *dta, int max_sz)
{
    int        bytes;

    ignore_next_decode_eom = FALSE;

    while (!rcv_msg.ready) {
        if (!handle_incoming_packet()){
            return FALSE;
        }
    }

    bytes = rcv_msg.buf.get(dta, max_sz);
    if (bytes > 0) {
        _bytes_recvd += bytes;
    }
    return bytes;
}


int ReliSock::get_ptr( void *&ptr, char delim)
{
    while (!rcv_msg.ready){
        if (!handle_incoming_packet()) {
            return FALSE;
        }
    }

    return rcv_msg.buf.get_tmp(ptr, delim);
}


int ReliSock::peek( char &c)
{
    while (!rcv_msg.ready) {
        if (!handle_incoming_packet()) {
            return FALSE;
        }
    }

    return rcv_msg.buf.peek(c);
}


int ReliSock::RcvMsg::rcv_packet(SOCKET _sock,
                                 SOCKET _mngSock,
                                 int _timeout,
                                 bool &mngReady)
{
    Buf        *tmp;
    char    hdr[5];
    int        end;
    int        len, len_t;
    int        tmp_len;
    struct    timeval *timer = NULL;
    fd_set    rdfds;
    int        nfound, maxfd;

#if !defined(WIN32) // nfds is ignored on WIN32
    maxfd = (_sock > _mngSock) ? _sock + 1 : _mngSock + 1;
#endif
    if(_timeout > 0) {
        timer = (struct timeval *)malloc(sizeof(struct timeval));
        timer->tv_sec = _timeout;
        timer->tv_usec = 0;
    }

    len = 0;
    while (len < 5) {
        FD_ZERO(&rdfds);
        FD_SET(_sock, &rdfds);
        if(_mngSock > 0)
            FD_SET(_mngSock, &rdfds);
                
        nfound = select(maxfd, &rdfds, 0, 0, timer);
        if(nfound < 0) {
            if(errno == EINTR) continue;
            dprintf(D_ALWAYS, "ReliSock::RcvMsg::rcv_packet -\
                               select failed: %s\n", strerror(errno));
            return FALSE;
        }
        if(nfound == 0) {
            dprintf(D_ALWAYS, "ReliSock::RcvMsg::rcv_packet -\
                               No socket descriptor is ready for read: timeout\n");
            if(timer) free(timer);
            return FALSE;
        }
        if(_mngSock > 0 && FD_ISSET(_mngSock, &rdfds)) {
            mngReady = true;
        }
        if(FD_ISSET(_sock, &rdfds)) {
            tmp_len = recv(_sock, hdr+len, 5-len, 0);
            if (tmp_len == 0) {
                if(timer) free(timer);
                dprintf(D_ALWAYS, "ReliSock::rcv_packet: peer closed prematurely\n");
                return FALSE;
            } else if (tmp_len <= 0) {
                if(timer) free(timer);
                dprintf(D_ALWAYS, "ReliSock::rcv_packet: recv failed %s\n", strerror(errno));
                return FALSE;
            }
            len += tmp_len;
        }
    }
    if(timer) free(timer);

    end = (int) ((char *)hdr)[0];
    memcpy(&len_t,  &hdr[1], 4);
    len = (int) ntohl(len_t);
        
    if (!(tmp = new Buf)){
        dprintf(D_ALWAYS, "IO: Out of memory\n");
        return FALSE;
    }
    if (len > tmp->max_size()){
        delete tmp;
        dprintf(D_ALWAYS, "IO: Incoming packet is too big\n");
        return FALSE;
    }
    if ((tmp_len = tmp->read(_sock, len, _timeout)) != len){
        delete tmp;
        dprintf(D_ALWAYS, "IO: Packet read failed: read %d of %d\n",
                tmp_len, len);
        return FALSE;
    }
    if (!buf.put(tmp)) {
        delete tmp;
        dprintf(D_ALWAYS, "IO: Packet storing failed\n");
        return FALSE;
    }
        
    if (end) {
        ready = TRUE;
    }
    return TRUE;
}


/* Handle a packet ready at _mngSock
 * This method receives a packet from _mngSock and calls appropriate functions.
 * Note that the readiness of _mngSock for readability has been checked.
 */
int ReliSock::handleMngPacket()
{
    char opCode;
    int result;

    // Read op-code
    result = condor_read(_mngSock, &opCode, 1, 0);
    if (result == 0) {
        dprintf(D_ALWAYS, "ReliSock::handleMngPacket - \
                fail to read op-code: socket closed prematuraly\n");
        return FALSE;
    } else if (result != 1) {
        dprintf(D_ALWAYS, "ReliSock::handleMngPacket - \
                           fail to read op-code from the managing socket\n");
        return FALSE;
    }

    switch(opCode) {
        case BND_POLLING:
            dprintf(D_FULLDEBUG, "ReliSock::handleMngPacket - bandwidth report req. arrived\n");
            return reportBandwidth();
        case BND_ALLOC_BAND:
            dprintf(D_FULLDEBUG, "ReliSock::handleMngPacket - bandwidth allocation arrived\n");
            return handleAlloc();
        default:
            dprintf(D_ALWAYS, "ReliSock::handleMngPacket - \
                               invalid op-code arrived to managing socket\n");
            return FALSE;
    }
}


int ReliSock::handleAlloc()
{
    char buffer[BND_SZ_ALLOC-1];
    long long_param = 0;
    short short_param = 0;
    long sec, bytes;
    float percent = 0.0;
    int result;

    // Read a packet after op-code
    result = condor_read(_mngSock, buffer, BND_SZ_ALLOC-1, 0);
    if (result == 0) {
        dprintf(D_ALWAYS, "ReliSock::handleMngPacket - \
                mngSock has been closed prematuraly\n");
        return FALSE;
    } else if (result != BND_SZ_ALLOC-1) {
        dprintf(D_ALWAYS, "ReliSock::handleMngPacket - \
                fail to read a packet from the managing socket\n");
        reportClose();
        return FALSE;
    }

    // Stripe parameters
    memcpy(&long_param, &buffer[8-sizeof(long)], sizeof(long));
    bytes = ntohl((u_long)long_param);
    long_param = 0;
    memcpy(&long_param, &buffer[8+8-sizeof(long)], sizeof(long));
    sec = ntohl((u_long)long_param);
    memcpy(&short_param, &buffer[16+4-sizeof(short)], sizeof(short));
    percent = (float)ntohs((u_short)short_param) / 100.0;

    dprintf(D_FULLDEBUG, "\tmax bytes allowed: %ld\n", bytes);
    dprintf(D_FULLDEBUG, "\twindow: %d(sec)\n", sec);
    dprintf(D_FULLDEBUG, "\tbackOff rate: %f(%)\n", percent);
    // Call setLimit
    return setLimit(sec, bytes, percent);
}


/* Set the maximum allowable to send during 'sec' seconds to kbytes
 * @param: sec
 * @param: kbytes
 * @param: percent
 */
int ReliSock::setLimit(const int sec, const int bytes, const float percent)
{
    // check parameters
    if(sec <= 0 ||
       bytes <= CONDOR_IO_BUF_SIZE * s_pow(9.0/5.0, BND_CTL_LEVELS-1) ||
       percent <= 0.0 || percent > 1.0)
    {
        dprintf(D_NETWORK, "ReliSock::setLimit - Bad Parameter\n");
        _maxBytes = 0;
        _totalSent = 0;
        return FALSE;
    }
    
    _maxBytes = _capability = bytes;
    _maxPercent = percent;

    // define initial windows _w[i] and limits _l[i]
    _w[0] = sec * 1000;
    _l[0] = _maxBytes;
    dprintf(D_FULLDEBUG, "=================================================================\n");
    dprintf(D_FULLDEBUG, "New windows and corresponding limits defined:\n");
    dprintf(D_FULLDEBUG, "[%d msec, %d bytes]", _w[0], _l[0]);
    for(int i=1; i<BND_CTL_LEVELS; i++) {
        _w[i] = _w[i-1] / 2;
        _l[i] = _l[i-1] / 9.0 * 5.0;
        dprintf(D_FULLDEBUG, "  [%d msec, %d bytes]", _w[i], _l[i]);
    }
    dprintf(D_FULLDEBUG, "\n");
    dprintf(D_FULLDEBUG, "=================================================================\n");

    // initialize _s[i]'s and _allowance
    int initVal = _maxBytes / BND_CTL_WINS;
    for(int i=0; i<BND_CTL_WINS; i++)
        _s[i] = initVal;

    _allowance = initVal;
    
    // start from index 0
    _curIdx = 0;

    // set _setTime and _T as the current time
    _setTime = time(NULL);
    (void)gettimeofday(&_T, NULL);

    // initialize status variables
    _deltaS = _totalSent = _totalRcvd = 0;
    _congested = _resolved = _shortReached = _numSends = 0;
    _beenBackedOff = false;
    _bndCtl_state = normal;

    return TRUE;
}


int ReliSock::getLimit(int sec[], int bytes[])
{
    // check parameters
    if(!sec || !bytes) return FALSE;

    for(int i=0; i<BND_CTL_LEVELS; i++) {
        sec[i] = _w[i]/1000;
        bytes[i] = _l[i];
    }

    return TRUE;
}


int ReliSock::getBandwidth(long &shortBand, long &longBand)
{
    int current = time(NULL);

    shortBand = _s[_curIdx] * 1024;
    longBand = _totalSent/(current - _setTime);

    return TRUE;
}


void ReliSock::retreatWall()
{
    // Get the total number of bytes sent during the last 2 windows
    int index = _curIdx;
    unsigned long total = 0;
    for(int i=0; i<2; i++) {
        total += _s[index];
        if(--index < 0) index = BND_CTL_WINS-1;
    }

    // Extrapolate '_capability' based on the last 2 windows
    _capability = (unsigned long)(total * BND_CTL_WINS / 2.0);

    // check if _capability is too small
    if(_capability > BND_CTL_WINS * 65500) {
        // Adjust limits, _l[i], based on the _capability calculated above
        // and _maxPercent given by user
        _l[0] = (unsigned long)(_capability * _maxPercent);
        dprintf(D_FULLDEBUG, "\tBacked off to:\n");
        dprintf(D_FULLDEBUG, "\t\t[%d]\n", _l[0]);
        for(int i = 1; i < BND_CTL_LEVELS; i++) {
            _l[i] = (unsigned long) (_l[i-1] * 5.0 / 9.0);
            dprintf(D_FULLDEBUG, "\t\t[%d]\n", _l[i]);
        }
    }

    // Initialize the array of actual-sent so as not to back off further
    // based on the past sending
    int initVal = _l[0] / BND_CTL_WINS;
    for(int i=0; i<BND_CTL_WINS; i++)
        _s[i] = initVal;

    _allowance = initVal;

    return;
}


void ReliSock::advanceWall()
{
    _capability = _l[0];
    
    // Adjust _l[i]
    _l[0] = (unsigned long)(_l[0] * 1.3);
    if(_l[0] > _maxBytes) _l[0] = _maxBytes;
    dprintf(D_FULLDEBUG, "\tMoved forward to:\n");
    dprintf(D_FULLDEBUG, "\t\t[%d]\n", _l[0]);
    for(int i = 1; i < BND_CTL_LEVELS; i++) {
        _l[i] = (unsigned long) (_l[i-1] * 5.0 / 9.0);
        dprintf(D_FULLDEBUG, "\t\t[%d]\n", _l[i]);
        //cout << "\t\t[" << _l[i] << "]\n";
    }

    // Reset s[i]
    int initVal = _l[0] / BND_CTL_WINS;
    for(int i=0; i<BND_CTL_WINS; i++)
        _s[i] = initVal;

    _allowance = initVal;

    //cout << "\tCapability: " << _capability << endl;
    //cout << "\tAllowance: " << _allowance << endl;

    return;
}


/* Later, this function must be called inside of timeout handler */
void ReliSock::calculateAllowance()
{
    int index;
    unsigned long sent = 0;
    struct timeval temp;

    dprintf(D_FULLDEBUG, "Allowed: %ld :: Sent: %ld\n", _allowance, _deltaS);
    //cout << "Allowed: " << _allowance << "::";
    //cout << " Sent: " << _deltaS << endl;

    /*
     * Adjust _s[i], _T, and _deltaT, _totalSent
     */
    // Put _deltaS into the slot next to _curIdx. _curIdx be also advanced
    if(++_curIdx > BND_CTL_WINS-1)
        _curIdx = 0;
    _s[_curIdx] = _deltaS;
    _totalSent += _deltaS;

    // advance _T and deduct _deltaT
    _T.tv_sec += _w[BND_CTL_LEVELS-1]/1000;
    _deltaT -= _w[BND_CTL_LEVELS-1];

    // For the case that _deltaT is bigger than 2*_w[BND_CTL_LEVELS-1], which means
    // there was no network traffic for _deltaT - _w[4] sec, advance _curIdx
    // appropriatly while filling _s[i] with 0
    while(_deltaT >= _w[BND_CTL_LEVELS-1]) {
        if(++_curIdx > BND_CTL_WINS - 1)
            _curIdx = 0;
        _s[_curIdx] = 0;
        _T.tv_sec += _w[BND_CTL_LEVELS-1]/1000;
        _deltaT -= _w[BND_CTL_LEVELS-1];
    }

    
    /*
    printf("\n==>");
    cout << "\t_s[i](bytes): " << _s[_curIdx];
    int tidx = _curIdx;
    for(int i=0; i<BND_CTL_WINS-1; i++) {
        if(--tidx < 0) tidx = BND_CTL_WINS-1;
        cout << ", " << _s[tidx];
    }
    printf("\n");
    */

    /* Adjust _l[i]'s
     *
     * If real bandwidths of last 4 consecutive windows have been smaller than
     * that allowed, we must check if there has been a network congestion
     * and back, if there has been, off ourselves.
     *
     * Also, if we have backed off and have been using full bandwidth for
     * the last 16 consecutive windows, we try to use more bandwidth, taking
     * it as the network congestion has been resolved.
     */
    dprintf(D_FULLDEBUG, "# of packets sent: %ld :: blocked: %ld\n", _numSends, _shortReached);
    //cout << "short: " << _shortReached << "  numSends: " <<  _numSends << endl;
    if (_allowance > _deltaS + 65536 &&
		_shortReached * 200 > _numSends /* 0.5% */ ||
		_shortReached * 50 > _numSends /* 2% */ ) { // congested during the last WIN
        dprintf(D_FULLDEBUG, "_congested: %d\n", _congested+1);
        _resolved = -10;
        if(++_congested >= 2) {
            _bndCtl_state = backOff;
            _beenBackedOff = true;
            retreatWall();
            _shortReached = _numSends = _congested = 0;
            _deltaS = 0;
            return;
        } else if (_bndCtl_state == scouting) {
            _bndCtl_state = backOff;
		}

    }
    else {
        _congested = 0;
        if(_bndCtl_state != normal) { // if not congested and in backOff or scouting state
            dprintf(D_FULLDEBUG, "_resolved: %d\n", _resolved+1);
            //cout << "_resolved: " << _resolved+1 << endl;
            if(++_resolved >= 2) {
                dprintf(D_FULLDEBUG, "\tadvance wall by 1.3 times\n");
                //cout << "\tadvance wall by 1.3\n";
                advanceWall();
                _resolved = 0;
                _shortReached = _numSends = 0;
                _deltaS = 0;

                if(_bndCtl_state == backOff) {
                    _bndCtl_state = scouting;
                    return;
                } 
                else {  // in scouting state
                    if(_l[0] == _maxBytes) {
                        _capability = _maxBytes;
                        dprintf(D_FULLDEBUG, "\tgo back to normal state\n");
                        _bndCtl_state = normal;
                    }
                    return;
                }
            }
        }
    }

    // Get maximum bandwidth allowed for the next _w[4] sec
    // This value must be the minimum value of
    // { _l[i] - bytes sent during the last (_w[i] - _w[4]) sec }
    _allowance = _l[BND_CTL_LEVELS - 1];
    index = _curIdx;
    for(int i=0; i<BND_CTL_LEVELS-1; i++) {
        for(int j=0; j<(int)s_pow(2,i); j++) {
            sent += _s[index];
            if(--index < 0) index = BND_CTL_WINS - 1;
        }
        int differ = _l[BND_CTL_LEVELS - 2 - i] - sent;
        if (_allowance > differ) _allowance = differ;
    }

    // Adjust _deltaS, _shortReached, and _numSends
    _shortReached = _numSends = 0;
    _deltaS = 0;

    return;
}


/*
 * param: size - the number of bytes of the packet being sent
 */
void ReliSock::beNice(int size)
{
    struct timeval ctime, timer;
    
    (void)gettimeofday(&ctime, NULL);
    _deltaT = (ctime.tv_sec - _T.tv_sec)*1000 +
              (ctime.tv_usec - _T.tv_usec)/1000;
    while ( _deltaT > 10 &&
            (_deltaS + size) > (_allowance * ((float)_deltaT / _w[BND_CTL_LEVELS-1])) )
    {
        /*
        cout << "\nCurrent:\n";
        cout << "\t_deltaS = " << _deltaS;
        cout << ", size = " << size;
        cout << ", _allowance = " << _allowance;
        cout << ", _deltaT = " << _deltaT;
        cout << ", _w[" << BND_CTL_LEVELS-1 << "] = " << _w[BND_CTL_LEVELS-1] << endl;
        cout << "\t\t_deltaS+size = " << _deltaS + size << endl;
        cout << "\t\tthe other = " << (unsigned long)(_allowance*((float)_deltaT/_w[BND_CTL_LEVELS-1])) << endl;
        */
        // Sleep for few micro seconds
        timer.tv_sec = 0;
        timer.tv_usec = 100;
        (void) select( 5, NULL, NULL, NULL, &timer );

        // Get new _deltaT and calculate _allowance if necessary
        (void)gettimeofday(&ctime, NULL);
        _deltaT = (ctime.tv_sec - _T.tv_sec)*1000 +
                 (ctime.tv_usec - _T.tv_usec)/1000;
        //printf("Now (_deltaT = %d, _deltaS = %d)\n", _deltaT, _deltaS);
        if(_deltaT >= _w[BND_CTL_LEVELS-1]) {
            calculateAllowance();
        }
    }

    return;
}


int ReliSock::SndMsg::snd_packet(int _sock,
                                 int _mngSock,
                                 int end,
                                 int _timeout,
                                 int threshold,
                                 bool &congested,
                                 bool &ready)
{
    char hdr[5];
    int    len, ns;

    // prepare header of the packet
    hdr[0] = (char) end;
    ns = buf.num_used()-5;
    len = (int) htonl(ns);
    memcpy(&hdr[1], &len, 4);

    // send the packet
    if(buf.flush(_sock, _mngSock, hdr, 5, _timeout, threshold, congested, ready) != (ns+5)){
        return -1;
    }

    return ns+5;
}


#ifndef WIN32
    // interface no longer supported
int 
ReliSock::attach_to_file_desc( int fd )
{
    if (_state != sock_virgin) {
        return FALSE;
    }

    _sock = fd;
    _state = sock_connect;
    timeout(0); // make certain in blocking mode
    return TRUE;
}
#endif

Stream::stream_type 
ReliSock::type() 
{ 
    return Stream::reli_sock; 
}

char * 
ReliSock::serialize() const
{
    // here we want to save our state into a buffer

    // first, get the state from our parent class
    char * parent_state = Sock::serialize();
    // now concatenate our state
    char * outbuf = new char[50];
    sprintf(outbuf,"*%d*%s",_special_state,sin_to_string(&_who));
    strcat(parent_state,outbuf);
    delete []outbuf;
    return( parent_state );
}

char * 
ReliSock::serialize(char *buf)
{
    char sinful_string[28];
    char *ptmp;
    
    assert(buf);

    // here we want to restore our state from the incoming buffer

    // first, let our parent class restore its state
    ptmp = Sock::serialize(buf);
    assert( ptmp );
    sscanf(ptmp,"%d*%s",&_special_state,sinful_string);
    string_to_sin(sinful_string, &_who);

    return NULL;
}

int 
ReliSock::prepare_for_nobuffering(stream_coding direction)
{
    int ret_val = TRUE;
    struct timeval ctime;
    int nbytes;
    bool congested, mngReady;

    if ( direction == stream_unknown ) {
        direction = _coding;
    }

    switch(direction){
        case stream_encode:
            if ( ignore_next_encode_eom == TRUE ) {
                // optimization: if we already prepared for nobuffering,
                // just return true.
                return TRUE;
            }
            if (!snd_msg.buf.empty()) {
                if(_bandReg) { // if in bandwidth regulation
                    mngReady = false;
                    // we won't be nice here for the same reason that we
                    // weren't at end_of_message
                    // epilogue book keeping
                    if(_maxBytes > 0) {
                        congested = false;
                        nbytes = snd_msg.snd_packet(_sock, _mngSock, TRUE, _timeout, 70, congested, mngReady);
                        if(nbytes < 0) return FALSE;
                        _deltaS += nbytes;
                        _numSends++;
                        if(congested) _shortReached++;
                        //cout << "prepare_for_nobu: " << _deltaS << endl;
                    } else {
                        if(snd_msg.snd_packet(_sock, _mngSock, TRUE, _timeout, 0, congested, mngReady) < 0)
                            return FALSE;
                    }

                    if ( mngReady ) {
                        //cout << "prepare_for_nobuf: mngSock(= " << _mngSock << ") is ready\n";
                        ret_val = handleMngPacket();
                    }
                } // if(_bandReg)
                else {
                    _mngSock = 0;
                    if(snd_msg.snd_packet(_sock, _mngSock, TRUE, _timeout, 0, congested, mngReady) < 0)
                        return FALSE;
                }
            }

            if ( ret_val ) {
                ignore_next_encode_eom = TRUE;
            }
            break;

        case stream_decode:
            if ( ignore_next_decode_eom == TRUE ) {
                // optimization: if we already prepared for nobuffering,
                // just return true.
                return TRUE;
            }
            if ( rcv_msg.ready ) {
                if ( !rcv_msg.buf.consumed() )
                    ret_val = FALSE;
                rcv_msg.ready = FALSE;
                rcv_msg.buf.reset();
            }
            if ( ret_val ) {
                ignore_next_decode_eom = TRUE;
            }
            break;

        default:
            assert(0);
    }

    return ret_val;
}

int 
ReliSock::authenticate() {
    if ( !authob ) {
        authob = new Authentication( this );
    }
    if ( authob ) {
        return( authob->authenticate( hostAddr ) );
    }
    return( 0 );
}

void
ReliSock::setOwner( const char *newOwner ) {
    if ( authob ) {
        authob->setOwner( newOwner );
    }
}

const char *
ReliSock::getOwner() {
    if ( authob ) {
        return( authob->getOwner() );
    }
    return NULL;
}

int
ReliSock::isAuthenticated()
{
    if ( !authob ) {
        dprintf(D_FULLDEBUG, "authentication not called prev, auth'ing TRUE\n" );
        return 1;
    }
    return( authob->isAuthenticated() );
    return 0;
}

void
ReliSock::unAuthenticate()
{
    if ( authob ) {
        authob->unAuthenticate();
    }
}
