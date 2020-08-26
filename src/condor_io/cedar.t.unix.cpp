/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"

#include "condor_config.h"
#include "condor_io.h"
#include "condor_debug.h"

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#define SERVER_PORT 7678

using namespace std;

int main()
{
	SafeSock mySock;
	int op, result;

	char c, cont, *charString;
	int integer;
	long lint;
	short sint;
	float f;
	double d;

	config();

	charString =  0;

	cout << "(1) Server" << endl;
	cout << "(2) Client" << endl;
	cout << "(9) Exit" << endl;
	cout << "Select: ";
	cin >> op;

	switch(op) {
		case 1: // Server
			result = mySock.bind( CP_IPV4, false, SERVER_PORT, false ); // outbound, port, loopback
			if(result != TRUE) {
				cout << "Bind failed\n";
				exit(-1);
			}
			cout << "Bound to [" << SERVER_PORT << "]\n";
			while(true) {
				mySock.decode();

				cout << "Type any key continue: ";
				cin >> cont;
				mySock.code(c);
				cout << "char: " << c << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				mySock.code(integer);
				cout << "int: " << integer << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				mySock.code(lint);
				cout << "long: " << lint << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				mySock.code(sint);
				cout << "sint: " << sint << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				mySock.code(f);
				cout << "float: " << f << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				mySock.code(d);
				cout << "double: " << d << endl;

				cout << "Type any key continue: ";
				cin >> cont;
				mySock.code(charString);
				cout << "str: " << charString << endl;

				mySock.end_of_message();
			}
			break;
		case 2: // Client
			char serverName[30];

			// Connect to the server
			cout << "Server: ";
			cin >> serverName;
			result = mySock.connect(serverName, SERVER_PORT);
			if(result != TRUE) {
				cout << "Connection failed\n";
				exit(-1);
			}
			cout << "Connected to [" << serverName<< ", " << SERVER_PORT << "]\n";
			
			charString = (char *) malloc(100);

			while(true) {
				mySock.encode();
				
				cout << "Type char: ";
				cin >> c;
				mySock.code(c);

				cout << "Type int: ";
				cin >> integer;
				mySock.code(integer);

				cout << "Type long: ";
				cin >> lint;
				mySock.code(lint);

				cout << "Type short: ";
				cin >> sint;
				mySock.code(sint);

				cout << "Type float: ";
				cin >> f;
				mySock.code(f);

				cout << "Type double: ";
				cin >> d;
				mySock.code(d);

				cout << "Type string: ";
				cin >> charString;
				mySock.code(charString);

				mySock.end_of_message();
			}

		case 9:
			exit(0);
		default:
			break;
	}
}
