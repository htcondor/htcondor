/*
 * This is the test program for NetMnger
 *  - run netMnger with every printf turned on, and then
 *  - run multiple copies of this program at the same or different machines
 *      : By sending various bogus bandwidth reports, you may figure out whether
 *        the netMnger works correctly
 */

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "authentication.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_rw.h"
#include "condor_socket_types.h"
#include "reli_sock.h"

#ifdef WIN32
#include <mswsock.h>	// For TransmitFile()
#endif


int tSock;


int reportConnect(const int myIP, const short myPort,
                  const int peerIP, const short peerPort)
{
    char buffer[BND_SZ_CONN];
    int itemp;
    short stemp;
    int index = 1;
    char pad = 0x00;

    // Make the header
    buffer[0] = BND_CONN_RPT;

    // Fill the content
    itemp = htonl((u_long) my_ip_addr());
    memcpy(&buffer[index], &itemp, 4);
    index += 4;
    cout << "myIP: " << itemp << endl;

    stemp = htons((u_short) myPort);
    memcpy(&buffer[index], &stemp, 2);
    index += 2;
    cout << "myPort: " << stemp << endl;

    itemp = htonl((u_long) peerIP);
    memcpy(&buffer[index], &itemp, 4);
    index += 4;
    cout << "peerIP: " << itemp << endl;

    stemp = htons((u_short) peerPort);
    memcpy(&buffer[index], &stemp, 2);
    cout << "peerPort: " << stemp << endl;

    // Send the report to netMnger
    if(condor_write(tSock, buffer, BND_SZ_CONN) == BND_SZ_CONN)
        return TRUE;
    else
        return FALSE;
}


int main()
{
    sockaddr_in sockAddr;
    unsigned int addrLen = sizeof(sockAddr);

    // make a test socket and connect to netMnger
    tSock = socket(AF_INET, SOCK_STREAM, 0);
    if(tSock <= 0) {
        printf("socket creation failed: %s\n", strerror(errno));
        exit(-1);
    }

    // Bind tSock to an ephemeral port
    bzero(&sockAddr, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = htonl(my_ip_addr());
    sockAddr.sin_port = htons(0);
    if(::bind(tSock, (sockaddr *)&sockAddr, addrLen)) {
        printf("Failed to bind tSock to a local port: %s\n", strerror(errno));
        exit(-1);
    }

    // Connect to netMnger
    bzero(&sockAddr, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(NET_MNGER_ADDR);
    sockAddr.sin_port = htons((u_short)NET_MNGER_PORT);
    if(::connect(tSock, (sockaddr *)&sockAddr, addrLen)) {
        printf("Failed to connect to netMnger: %s\n", strerror(errno));
        exit(-1);
    }

    // report bogus connection
    unsigned long peerIP;
    printf("Peer IP address: ");
    cin >> peerIP;
    if(reportConnect(0, 8989, peerIP, 5656) != TRUE) {
        printf("Failed to report connection\n");
        exit(-1);
    }

    while(true) {
        // check if tSock is ready to read
        fd_set rdfds;
        timeval timer;
        int maxfd = tSock+1;
        FD_ZERO(&rdfds);
        FD_SET(tSock, &rdfds);
        printf("Checking if data is ready at tSock...\n");
        timer.tv_sec = 5;
        timer.tv_usec = 0;
        select(maxfd, &rdfds, 0, 0, &timer);
        if(FD_ISSET(tSock, &rdfds)) {
            // read tSock and notify user, if tSock is ready
            char opCode;
            // Read op-code
            if(condor_read(tSock, &opCode, 1, 0) != 1) {
                printf("Fail to read op-code from tSock: %s\n", strerror(errno));
                exit(-1);
            }
            if(opCode == BND_POLLING)
                printf("bandwidth report request arrived\n");
            else {
                char buffer[BND_SZ_ALLOC-1];
                long long_param = 0;
                short short_param = 0;
                long sec, bytes;
                float percent = 0.0;

                // Read a packet after op-code
                if(condor_read(tSock, buffer, BND_SZ_ALLOC-1, 0) != BND_SZ_ALLOC-1) {
                    printf("Fail to read a packet: %s\n", strerror(errno));
                    exit(-1);
                }

                // Stripe parameters
                memcpy(&long_param, &buffer[8-sizeof(long)], sizeof(long));
                bytes = ntohl((u_long)long_param);
                long_param = 0;
                memcpy(&long_param, &buffer[16-sizeof(long)], sizeof(long));
                sec = ntohl((u_long)long_param);
                memcpy(&short_param, &buffer[20-sizeof(short)], sizeof(short));
                percent = ntohs((u_short)short_param);
                cout << "Bandwidth allocation:\n";
                cout << "\t- maxBytes: " << bytes << " bytes;\n";
                cout << "\t- window: " << sec << "sec\n";
                cout << "\t- maxPercent: " << percent << "percent\n";
            }
        } else {
            printf("Nothing is ready at tScok\n");
        }

        int intSel;
        char chSel;
        unsigned long band;
        char buffer[BND_SZ_BAND];
        unsigned long temp = 0;
        char pad = 0x00;

        // ask user which action he/she wants
        printf("Select action you want:\n");
        printf("\t(1) report bandwidth\n");
        printf("\t(2) continue\n");
        printf("\t(9) exit\n\n");
        cin >> intSel;
        switch(intSel) {
            case 1:
                buffer[0] = BND_BAND_RPT;
                printf("\t==> congested? (y/n)  ");
                cin >> chSel;
                if(chSel == 'y')
                    buffer[1] = (char) true;
                else
                    buffer[1] = (char) false;
                printf("\t==> bytes: ");
                cin >> band;
                band = htonl((u_long) band);
                for(int i=0; i<8-sizeof(long); i++)
                    buffer[i+2] = pad;
                memcpy(&buffer[8-sizeof(long)+2], &band, sizeof(long));

                // Send the report to netMnger
                if(condor_write(tSock, buffer, BND_SZ_BAND) != BND_SZ_BAND) {
                    printf("Fail to send bandwidth report: %s\n", strerror(errno));
                    exit(-1);
                }
                break;
            case 2:
                break;
            case 9:
                buffer[0] = BND_CLOSE_RPT;
                if(condor_write(tSock, buffer, BND_SZ_CLOSE) != BND_SZ_CLOSE) {
                    printf("Fail to send close report: %s\n", strerror(errno));
                    exit(-1);
                }
                close(tSock);
                exit(0);
            default:
                printf("Invalid selection\n");
                break;
        }
    }
}
