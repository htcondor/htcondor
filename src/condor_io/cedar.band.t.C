/* This is one of test program suite for ReliSock's bandwidth regulation
 * The program is for an echo server and a client
 *
 *  - run the NetMnger
 *  - run multiple pair of server and client at the same machine and/or
 *    at different machines. The best way will be increasing the number
 *    of server/client pairs as you feel comfortable with this test suite
 */
#include "condor_common.h"

#include "condor_config.h"
#include "condor_network.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "test.h"

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

long k = 0;


int main(int argc, char *argv[])
{
    unsigned long total = 0;
    time_t cur, prev, elapsed;

	ReliSock listenSock;
	ReliSock clientSock;
	ReliSock *socket;
	int op, result, port;
    int interval;
	int iterations;

	char *charString = (char *) malloc(200000);
	int len;

	cout << "(1) Bandwidth regulated" << endl;
	cout << "(2) Bandwidth NOT regulated" << endl;
	cout << "Select: ";
	cin >> op;
    if(op == 1) {
        if(listenSock.bandRegulate() != TRUE) {
            cerr << "bandRegulate of listenSock failed\n";
            exit(-1);
        }
        if(clientSock.bandRegulate() != TRUE) {
            cerr << "bandRegulate of clientSock failed\n";
            exit(-1);
        }
    }

    cout << endl;
	cout << "(1) Server" << endl;
	cout << "(2) Client" << endl;
	cout << "(9) Exit" << endl;
	cout << "Select: ";
	cin >> op;

	switch(op) {
		case 1: // Server
			cout << "Port: ";
			cin >> port;
			listenSock.listen(port);
			socket = listenSock.accept();
            if (!socket) {
                cerr << "Accept failed\n";
                exit(0);
            }
			cout << "connected to a client" << endl;
            //socket->timeout(5);
			while(true) {
				socket->decode();
				socket->code(charString);
				socket->end_of_message();
                //if(interval > 0)
                    //sleep(interval);
			}
			break;
		case 2: // Client
			char serverName[30];
			int sec, bytes;
			float percent;
			for(int i=0; i<200000; i++) {
				charString[i] = 'a';
			}
			charString[199999] = '\0';

			// Connect to the server
			cout << "Server: ";
			cin >> serverName;
			cout << "Port: ";
			cin >> port;
            cout << "pause between sends(msec): ";
            cin >> interval;
			cout << "number of iteration: ";
			cin >> iterations;
			result = clientSock.connect(serverName, port);
			if(result != TRUE) {
				cout << "Connection failed\n";
				exit(-1);
			}
			cout << "Connected to [" << serverName<< ", " << SERVER_PORT << "]\n";
			cout << "====================================\n";
			cout << "iterations: " << iterations << endl;
			cout << "====================================\n";
            //clientSock.timeout(5);
            (void) time(&prev);
			for (int i=0; i<iterations; i++) {
			for (int j=0; j<iterations; j++) {
			for (int k=0; k<iterations; k++) {
                struct timeval timer;

				clientSock.encode();
				clientSock.code(charString);
				clientSock.end_of_message();
                total += 200000;
                if(interval > 0) {
                    // Sleep for few milli seconds
                    timer.tv_sec = 0;
                    timer.tv_usec = interval*1000;
                    (void) select( 5, NULL, NULL, NULL, &timer );
                }
                (void) time(&cur);
                elapsed = cur - prev;
                if(elapsed >= 5) {
                    cerr << cur << " " << total << endl;
                    prev += 5;
                    total = 0;
                }
			}}}
            break;

		case 9:
			exit(0);
		default:
			break;
	}
}
