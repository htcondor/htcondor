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

int main()
{
	ReliSock listenSock;
	ReliSock clientSock;
	ReliSock *socket;
	int op, result;

	char c, cont, *charString;
	int integer, len;
	long lint;
	short sint;
	float f;
	double d;

	charString = (char *)malloc(100);

	cout << "(1) Server" << endl;
	cout << "(2) Client" << endl;
	cout << "(9) Exit" << endl;
	cout << "Select: ";
	cin >> op;

	switch(op) {
		case 1: // Server
			listenSock.listen(SERVER_PORT);
			socket = listenSock.accept();
			cout << "connected to a client" << endl;
			while(true) {
				socket->decode();

				cout << "Type any key continue: ";
				cin >> cont;
				socket->code(c);
				cout << "char: " << c << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				socket->code(integer);
				cout << "int: " << integer << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				socket->code(lint);
				cout << "long: " << lint << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				socket->code(sint);
				cout << "sint: " << sint << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				socket->code(f);
				cout << "float: " << f << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				socket->code(d);
				cout << "double: " << d << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				socket->code(charString);
				cout << "str: " << charString << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				socket->code(charString, len);
				cout << "str[" << len << "] " << charString << endl;

				socket->end_of_message();
			}
			break;
		case 2: // Client
			char serverName[30];

			// Connect to the server
			cout << "Server: ";
			cin >> serverName;
			result = clientSock.connect(serverName, SERVER_PORT);
			if(result != TRUE) {
				cout << "Connection failed\n";
				exit(-1);
			}
			cout << "Connected to [" << serverName<< ", " << SERVER_PORT << "]\n";
			while(true) {
				clientSock.encode();
				
				cout << "Type char: ";
				cin >> c;
				clientSock.code(c);

				cout << "Type int: ";
				cin >> integer;
				clientSock.code(integer);

				cout << "Type long: ";
				cin >> lint;
				clientSock.code(lint);

				cout << "Type short: ";
				cin >> sint;
				clientSock.code(sint);

				cout << "Type float: ";
				cin >> f;
				clientSock.code(f);

				cout << "Type double: ";
				cin >> d;
				clientSock.code(d);

				cout << "Type string: ";
				cin >> charString;
				clientSock.code(charString);

				cout << "Type string: ";
				cin >> charString;
				clientSock.code(charString);

				clientSock.end_of_message();
			}

		case 9:
			exit(0);
		default:
			break;
	}
}
