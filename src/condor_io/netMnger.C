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
#include "condor_io.h"
#include "authentication.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_rw.h"
#include "netMnger.h"

#ifdef WIN32
#include <mswsock.h>    // For TransmitFile()
#endif

int pollingRate = 0;

NetMnger *mnger = NULL;

void alarmHandler(int signo)
{
    cout << "timer goes off\n";
    mnger->sendPolling();
}

void usage()
{
    cerr << "Usage: netMnger [option value]\n";
    cerr << "\toptions:\n";
    cerr << "\t\t-i: initial bandwidth(in MB) between two hosts\n";
    cerr << "\t\t    The initial bandwidth will be sharded among\n";
    cerr << "\t\t    connections between the same pair of hosts\n";
    cerr << "\t\t-w: the size of window(in sec) of bandwidth control\n";
    cerr << "\t\t-b: back-off rate(0 - 99)\n";
    cerr << "\t\t-p: the port to use\n";
}

int main(int argc, char *argv[])
{
    long initBand = 0;
    int window = 0;
    int backOffRate = -1;
    int port = 0;
    char c;

    while((c=getopt(argc,argv,"i:w:b:p:"))!=(char)-1) {
        switch(c) {
            case 'i':
                initBand = atoi(optarg);
                if(initBand <= 0) {
                    usage();
                    cerr << endl << "initial bandwidth should be greater than 0\n";
                    exit(-1);
                } else
                    initBand = initBand * 1048576;
                break;
            case 'w':
                window = atoi(optarg);
				pollingRate = window * 100;
                if(window <= 0) {
                    usage();
                    cerr << endl << "window should be greater than 0\n";
                    exit(-1);
                }
                break;
            case 'b':
                backOffRate = atoi(optarg);
                if(backOffRate < 0 || backOffRate >= 100) {
                    usage();
                    cerr << endl << "backOffRate should be within (0 ~ 99)\n";
                    exit(-1);
                }
                break;
            case 'p':
                port = atoi(optarg);
                if(port <= 1024) {
                    usage();
                    cerr << endl << "port should be greater than 1024\n";
                    exit(-1);
                }
                break;
            default:
                usage();
                return 1;
                break;
        }
    }

    mnger = new NetMnger(port, initBand, window, backOffRate);
    if(!mnger) {
        cerr << "Failed to create a netMnger\n";
        exit(-1);
    }

    /* Register timeout handler */
    struct sigaction act, oact;

    act.sa_handler = alarmHandler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    // Make interrupted system calls restart automatically
    act.sa_flags |= SA_RESTART;

    // Register signal handler using sigaction:
    //  - signal handler remains installed after catching a signal
    //  - the same signal will be blocked while the handler is executing
    if ( sigaction(SIGALRM, &act, &oact) < 0 ) {
        dprintf(D_ALWAYS, "sigaction failed: %s\n", strerror(errno));
        printf("sigaction failed: %s\n", strerror(errno));
        exit(-1);
    }
    dprintf(D_NETWORK, "NetMnger::doMainLoop - alarm handler registered\n");
    printf("NetMnger::doMainLoop - alarm handler registered\n");
    /* end of signal handler registration */

    mnger->mainLoop();
}


NetMnger::NetMnger(int port, long initBand, int window, int backOffRate)
{
    (port != 0) ? _port = port : _port = NET_MNGER_PORT;

    (initBand != 0) ? _initBandWidth = initBand : _initBandWidth = INIT_BANDWIDTH;
    // check overflow
    if(_initBandWidth < 0) {
        cerr << "_initBandWidth overflowed: " << _initBandWidth << endl;
        exit(-1);
    }

    (window != 0) ? _window = window : _window = HALFCONN_WINDOW;

    (backOffRate >= 0) ? _backOffRate = backOffRate : _backOffRate = HALFCONN_PERCENT; 

    // make _listenSock;
    _listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if(_listenSock <= 0) {
        dprintf(D_ALWAYS, "NetMnger::NetMnger - socket creation failed\n");
        printf("NetMnger::NetMnger - socket creation failed\n");
        exit(-1);
    }

    // initialize _mngSockTbl
    for(int i=0; i<MAX_MNG_SOCK; i++) {
        _mngSockTbl[i].bandInfo = NULL;
        _mngSockTbl[i].halfConn = NULL;
    }
    dprintf(D_NETWORK, "NetMnger created\n");
    printf("NetMnger created\n");
}


void NetMnger::mainLoop()
{
    struct sockaddr_in myAddr, peerAddr;
    unsigned int addrLen = sizeof(myAddr);
    fd_set rdfds;
    int maxfd = _listenSock+1;

    // Bind the listen socket to a local port
    bzero(&myAddr, addrLen);
    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = htonl(my_ip_addr());
    myAddr.sin_port = htons((u_short)_port);
    if(bind(_listenSock, (struct sockaddr *)&myAddr, addrLen)) {
        dprintf(D_ALWAYS, "NetMnger::doMainLoop - bind failed, errno = %d\n", errno);
        printf("NetMnger::doMainLoop - bind failed, errno = %d\n", errno);
        return;
    }

    // Change the listen socket to a passive one
    if(::listen(_listenSock, 5)) {
        dprintf(D_ALWAYS, "NetMnger::doMainLoop - listen failed, errno = %d\n", errno);
        printf("NetMnger::doMainLoop - listen failed, errno = %d\n", errno);
        return;
    }

    // Start timer
    alarm(pollingRate);

    while(true) {
        int result = -1;
        while(result < 0) {
            FD_ZERO(&rdfds);
            FD_SET(_listenSock, &rdfds);
            for(int i=0; i<MAX_MNG_SOCK; i++) {
                if(_mngSockTbl[i].halfConn != NULL) {
                    FD_SET(i, &rdfds);
                    if(maxfd <= i) maxfd = i+1;
                }
            }

            //printf("selecting...\n");
            result = select(maxfd, &rdfds, 0, 0, 0);
            if(result < 0) {
                if(errno == EINTR)
                continue;
                else {
                    dprintf(D_ALWAYS, "NetMnger::mainLoop - select failed: %s\n", strerror(errno));
                    printf("NetMnger::mainLoop - select failed: %s\n", strerror(errno));
                    exit(-1);
                }
            }
        }

        if(FD_ISSET(_listenSock, &rdfds)) {
            int sock;
            sock = accept(_listenSock, NULL, 0);

            /* tag as registered in _mngSockTbl but not connected */
            _mngSockTbl[sock].bandInfo = (BandInfo *)-1;
            _mngSockTbl[sock].halfConn = (HalfConn *)-1;

            dprintf(D_FULLDEBUG, "a connection accepted through sock = %d\n", sock);
            dprintf(D_FULLDEBUG, "register in _mngSockTbl[%d]\n", sock);
            printf("a connection accepted through sock = %d\n", sock);
            printf("register in _mngSockTbl[%d]\n", sock);
        }

        for(int i=0; i<MAX_MNG_SOCK; i++) {
            if(_mngSockTbl[i].bandInfo != NULL && FD_ISSET(i, &rdfds)) {
                handleMngPacket(i);
            }
        }
    }
}


void NetMnger::handleMngPacket(const int fd)
{
    char buffer[BND_MAX_PACKET];
    char opCode;
    long long_param;
    int m_param;
    short s_param;
    long ipAddrA, ipAddrB, portA, portB;
    bool backOff;
    long bandwidth;
    int index;

    if(condor_read(fd, &opCode, 1, 0) != 1) {
        dprintf(D_ALWAYS, "NetMnger::handleMngPacket -\
                           condor_read fail: errono = %d\n", errno);
        printf("NetMnger::handleMngPacket -\
                           condor_read fail: errono = %d\n", errno);
        return;
    }

    switch(opCode) {
        case BND_CONN_RPT:
            printf("Connection: (fd = %d)\n", fd);
            if(condor_read(fd, buffer, BND_SZ_CONN - 1, 0) != BND_SZ_CONN-1) {
                dprintf(D_ALWAYS, "NetMnger::handleMngPacket -\
                                   connRpt read fail: errono = %d\n", errno);
                printf("NetMnger::handleMngPacket -\
                                   connRpt read fail: errono = %d\n", errno);
                return;
            }

            index = 0;
            memcpy(&m_param, &buffer[index], 4);
            ipAddrA = ntohl((u_long)m_param);
            index += 4;

            memcpy(&s_param, &buffer[index], 2);
            portA = ntohs((u_short)s_param);
            index += 2;

            memcpy(&m_param, &buffer[index], 4);
            ipAddrB = ntohl((u_long)m_param);
            index += 4;

            memcpy(&s_param, &buffer[index], 2);
            portB = ntohs((u_short)s_param);

            handleConnRpt(fd, ipAddrA, portA, ipAddrB, portB);
            return;

        case BND_CLOSE_RPT:
            printf("Close: (fd = %d)\n", fd);
            handleClose(fd);
            return;

        case BND_BAND_RPT:
            if(condor_read(fd, buffer, BND_SZ_BAND - 1, 0) != BND_SZ_BAND-1) {
                dprintf(D_ALWAYS, "NetMnger::handleMngPacket -\
                                   bandRpt read fail: errono = %d\n", errno);
                printf("NetMnger::handleMngPacket -\
                                   bandRpt read fail: errono = %d\n", errno);
                return;
            }

            backOff = (bool) buffer[0];
            memcpy(&long_param, &buffer[9-sizeof(long)], sizeof(long));
            bandwidth = ntohl((u_long)long_param);

            handleBandRpt(fd, backOff, bandwidth);
            return;

        default:
            dprintf(D_ALWAYS, "NetMnger::handleMngPacket -\
                               invalid op-code\n");
            printf("NetMnger::handleMngPacket -\
                               invalid op-code\n");
            return;
    }
}


void NetMnger::handleConnRpt(const int fd,
                             const int myIpAddr,
                             const int myPort,
                             const int peerIpAddr,
                             const int peerPort)
{
    printf("Connection Report[fd=%d]:\n", fd);
    printf("\tmyIP: %d\n", myIpAddr);
    printf("\tmyPort: %d\n", myPort);
    printf("\tpeerIP: %d\n", peerIpAddr);
    printf("\tpeerPort: %d\n", peerPort);
    /* Check the status of the corresponding mngSock */
    if(_mngSockTbl[fd].bandInfo != (BandInfo *)-1 ||
       _mngSockTbl[fd].halfConn != (HalfConn *)-1)
    { // staled somehow
        dprintf(D_ALWAYS, "NetMnger::handleConnRpt -\
                           _mngSockTbl[%d] is not in the correct status\n\
                           mngSock %d will be closed\n", fd, fd);
        printf("NetMnger::handleConnRpt -\
                           _mngSockTbl[%d] is not in the correct status\n\
                           mngSock %d will be closed\n", fd, fd);
        close(fd);
        return;
    }

    /* Add a HalfConn to the corresponding BandInfo's HalfConn List */
    // hash into _bandInfoTbl[]
    int index = abs(myIpAddr + peerIpAddr) * 137 % SZ_BAND_HASH;

    // find the bandInfo with the same pair of (ipA, ipB)
    BandInfo *bandPtr = _bandInfoTbl[index];
    bool found = false;
    while(bandPtr && !found) {
        if(bandPtr->ipA == myIpAddr && bandPtr->ipB == peerIpAddr ||
           bandPtr->ipA == peerIpAddr && bandPtr->ipB == myIpAddr)
            found = true;
        else bandPtr = bandPtr->next;
    }

    // if not found, make a new BandInfo
    // Later we have to call some method to get the bandwidth between 2 hosts
    if(!found) {
        bandPtr = new BandInfo(myIpAddr, peerIpAddr, _initBandWidth);
        bandPtr->next = _bandInfoTbl[index];
        _bandInfoTbl[index] = bandPtr;
    }


    // make a new HalfConn with capacity/(noConn+1)
    HalfConn *connPtr = new HalfConn(bandPtr->capacity/(bandPtr->noConn+1),
                                     _window,
                                     _backOffRate,
                                     fd); // mngSock

    // register into _mngSockTbl
    _mngSockTbl[fd].bandInfo = bandPtr;
    _mngSockTbl[fd].halfConn = connPtr;
    
    // reallocate bandwidths of other connections
    // between the same pair of hosts, if necessary
    long chipOff = 0;
    if(bandPtr->noConn > 0)
        chipOff = connPtr->maxBytes / bandPtr->noConn;
    HalfConn *ptr = bandPtr->halfConn;
    while(ptr) {
        ptr->maxBytes -= chipOff;
        if(ptr->curUse > ptr->maxBytes) {
			ptr->sendAlloc();
            //ptr->realloc = false;
            //printf("allocation(fd=%d) is updated from %ld to %ld\n", ptr->mngSock, ptr->curUse, ptr->maxBytes);
        }
        ptr = ptr->next;
    }

    // add the new connection to the list
    connPtr->next = bandPtr->halfConn;
    bandPtr->halfConn = connPtr;
    
    
    // update bandInfo
    bandPtr->noConn++;
    bandPtr->allocated = 0;
    ptr = bandPtr->halfConn;
    while(ptr) {
        bandPtr->allocated += ptr->maxBytes;
        ptr = ptr->next;
    }

    // send allocation to the new connection
    connPtr->sendAlloc();

    return;
}


void NetMnger::handleClose(int fd)
{
    /* Check the status of the corresponding mngSock */
    if(_mngSockTbl[fd].bandInfo <= (BandInfo *)0 ||
       _mngSockTbl[fd].halfConn <= (HalfConn *)0)
    { // staled somehow
        dprintf(D_ALWAYS, "NetMnger::handleClose -\n\
                \t_mngSockTbl[%d] is not in the correct status\n\
                \t_mngSockTbl[%d] will be reset and\n\
                \t_mngSock %d will be closed\n", fd, fd, fd);
        printf("NetMnger::handleClose -\n\
                \t_mngSockTbl[%d] is not in the correct status\n\
                \t_mngSockTbl[%d] will be reset and\n\
                \t_mngSock %d will be closed\n", fd, fd, fd);
        _mngSockTbl[fd].bandInfo = NULL;
        _mngSockTbl[fd].halfConn = NULL;
        close(fd);
        return;
    }
    
    BandInfo *thisBand = _mngSockTbl[fd].bandInfo;
    HalfConn *thisConn = _mngSockTbl[fd].halfConn;

    // update BandInfo
    thisBand->noConn--;
    long aportion = 0;
    if (thisBand->noConn > 0) {
        aportion = thisConn->maxBytes / thisBand->noConn;
        thisBand->allocated += aportion * thisBand->noConn - thisConn->maxBytes;
    } else
        thisBand->allocated = 0;

    // Distribute freed bandwidth among other connections
    // Delete the connection information
    HalfConn *ptr = thisBand->halfConn;
    HalfConn *prev = NULL;
    while(ptr) {
        if(ptr == thisConn) { // delete the connection information
            if(prev == NULL) // if ptr is the 1st one
                thisBand->halfConn = thisConn->next;
            else
                prev->next = thisConn->next;
            // advance ptr but not prev
            ptr = ptr->next;
        } else {
            ptr->maxBytes += aportion;
			ptr->sendAlloc();
            // advance both of ptr and prev
            prev = ptr;
            ptr = ptr->next;
        }
    }
    delete thisConn;
    
    // nullify _mngSockTbl and close mngSock
    _mngSockTbl[fd].bandInfo = NULL;
    _mngSockTbl[fd].halfConn = NULL;
    close(fd);

    return;
}


void NetMnger::handleBandRpt(const int fd,
                             const bool congested,
                             const long curUsage)
{
    BandInfo *bandPtr = _mngSockTbl[fd].bandInfo;
    HalfConn *connPtr = _mngSockTbl[fd].halfConn;

    /* for testing */
    printf("Band report: (fd = %d)\n", fd);
    if(congested)
        printf("\t(congested, ");
    else
        printf("\t(NOT congested, ");
    printf("\tusage = %ld)\n", curUsage);
    printf("\tallocated: %ld\n", connPtr->maxBytes);
    /* end testing */


    if(connPtr->reported) {
        //printf("Bandwidth report received more than once\n");
        return;
    }

    connPtr->curUse = curUsage;
    connPtr->backOff = congested;
    connPtr->reported = true;

    bandPtr->curUse += curUsage;
    bandPtr->reported++;

    if (connPtr->maxBytes + EPSILONE < curUsage) {
        bandPtr->room = bandPtr->room - curUsage + connPtr->maxBytes;
        connPtr->realloc = true;
    }

    if(congested) {
        // cumulate spilling out
        bandPtr->spillOut += connPtr->maxBytes - curUsage;
    } else if (connPtr->maxBytes > curUsage + EPSILONE) {
        // cumulate room
        bandPtr->room += connPtr->maxBytes - curUsage;
        // reduce bandwidth allocation
        connPtr->maxBytes = curUsage + EPSILONE;
    }

    // reallocate and send out reallocation
    //cout << "bandPtr->reported: " << bandPtr->reported << endl;
    //cout << "bandPtr->noConn: " << bandPtr->noConn << endl;
    if(bandPtr->reported == bandPtr->noConn)
        bandPtr->reallocate();

    return;
}


void NetMnger::sendPolling()
{
    char buffer[BND_SZ_POLLING];

    // Make the packet
    buffer[0] = BND_POLLING;

    int total = 0;
    for(int i=0; i<MAX_MNG_SOCK; i++) {
        if(_mngSockTbl[i].halfConn != NULL) {
            (void)condor_write(i, buffer, BND_SZ_POLLING, 0);
            total++;
        }
    }

    //printf("sent Polling to %d ReliSocks and collecting bandwidth report...\n", total);
    // Restart timer
    alarm(pollingRate);
}


void BandInfo::reallocate()
{
    HalfConn *connPtr = halfConn;
    HalfConn *ptr;

    if (spillOut > 0) {
        printf("SpillOut = %ld\n", spillOut);
        fullUsage = 0;
        long chipIn = spillOut / noConn;
		allocated = 0;
        //capacity = 0;
        while(connPtr) {
            connPtr->maxBytes -= chipIn;
            connPtr->maxBytes = connPtr->maxBytes * (connPtr->maxPercent/100.0);
            //capacity += connPtr->maxBytes;
            allocated += connPtr->maxBytes;
            connPtr->realloc = true;
            connPtr = connPtr->next;
        }
    } else if (room > EPSILONE * noConn) {
        printf("Room = %ld\n", room);
        fullUsage = 0;
        long aportion = room/noConn;
        while(connPtr) {
            connPtr->maxBytes += aportion;
            connPtr->realloc = true;
            connPtr = connPtr->next;
        }
    } else if (++fullUsage >= FULL_USAGE_LIMIT && capacity > allocated) {
		cout << "fullUsage = " << fullUsage << endl;
        fullUsage = 0;
        //capacity *= ADVANCE_RATE;
		allocated = capacity;
        while(connPtr) {
            connPtr->maxBytes = capacity/noConn;
            connPtr->realloc = true;
            connPtr = connPtr->next;
        }
    }

    /* send out reallocations, if necessary */
    /* also update 'allocated' */
    allocated = 0;
    connPtr = halfConn;
    while(connPtr) {
        if(connPtr->realloc) {
            connPtr->sendAlloc();
            connPtr->realloc = false;
        }
        allocated += connPtr->maxBytes;
        connPtr->reported = false;
        connPtr = connPtr->next;
    }

    curUse = reported = spillOut = room = 0;

    return;
}


void HalfConn::sendAlloc()
{
    char buffer[BND_SZ_ALLOC];
    long ltemp;
    short stemp;
    int index = 1;
    char pad = 0x00;

    // Make the header
    buffer[0] = BND_ALLOC_BAND;

    // Fill the content
    ltemp = htonl((u_long) maxBytes);
    for(int i=0; i<8-sizeof(long); i++)
        buffer[index++] = pad;
    memcpy(&buffer[index], &ltemp, sizeof(long));
    index += sizeof(long);

    ltemp = htonl((u_long) window);
    for(int i=0; i<8-sizeof(long); i++)
        buffer[index++] = pad;
    memcpy(&buffer[index], &ltemp, sizeof(long));
    index += sizeof(long);

    stemp = htons((u_short) maxPercent);
    for(int i=0; i<4-sizeof(short); i++)
        buffer[index++] = pad;
    memcpy(&buffer[index], &stemp, sizeof(short));

    // Send the report to netMnger
    (void) condor_write(mngSock, buffer, BND_SZ_ALLOC, 0);
    
    //printf("Send Allocation: (fd = %d)\n", mngSock);
    //printf("\t(maxBytes: %ld, window: %d sec, percent: %d)\n",
    //       maxBytes, window, maxPercent);
}


BandInfo::BandInfo(const int ip_A,
                   const int ip_B,
                   const long bytes)
{
    ipA = ip_A;
    ipB = ip_B;
    capacity = bytes;  // Maximum bandwidth
    allocated = curUse = spillOut = room = 0;
    noConn = reported = fullUsage = 0;
    halfConn = NULL;
    next = NULL;
}


HalfConn::HalfConn(const long initMax,
                   const long sec,
                   const short percent,
                   const int mSock)
{
    maxBytes = curUse = initMax;
    window = sec;
    maxPercent = percent;
    mngSock = mSock;
    backOff = reported = realloc = false;
    next = NULL;
}
