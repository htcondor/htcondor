/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
	SafeSock mySock;
	int op, result;

	char c, cont, *charString;
	int integer, len;
	long lint;
	short sint;
	float f;
	double d;

	charString = (char *)malloc(100);

	result = mySock.set_os_buffers(3000000, false);
	cout << "buffer size set to " << result << endl;

	cout << "(1) Server" << endl;
	cout << "(2) Client" << endl;
	cout << "(9) Exit" << endl;
	cout << "Select: ";
	cin >> op;

	switch(op) {
		case 1: // Server
			result = mySock.bind(SERVER_PORT);
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

				cout << "Type any key continue: ";
				cin >> cont;
				mySock.code(charString, len);
				cout << "str[" << len << "] " << charString << endl;

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
