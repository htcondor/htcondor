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

#define SERVER_PORT 7678

int main()
{
	SafeSock mySock;
	int op, result;

	char c, cont;
	int integer;
	long lint;
	short sint;
	float f;
	double d;
	std::string charString;

	config();

	std::cout << "(1) Server" << std::endl;
	std::cout << "(2) Client" << std::endl;
	std::cout << "(9) Exit" << std::endl;
	std::cout << "Select: ";
	std::cin >> op;

	switch(op) {
		case 1: // Server
			result = mySock.bind( CP_IPV4, false, SERVER_PORT, false ); // outbound, port, loopback
			if(result != TRUE) {
				std::cout << "Bind failed\n";
				exit(-1);
			}
			std::cout << "Bound to [" << SERVER_PORT << "]\n";
			while(true) {
				mySock.decode();

				std::cout << "Type any key continue: ";
				std::cin >> cont;
				mySock.code(c);
				std::cout << "char: " << c << std::endl;

				std::cout << "Type any key continue: ";
				std::cin >> cont;
				mySock.code(integer);
				std::cout << "int: " << integer << std::endl;

				std::cout << "Type any key continue: ";
				std::cin >> cont;
				mySock.code(lint);
				std::cout << "long: " << lint << std::endl;

				std::cout << "Type any key continue: ";
				std::cin >> cont;
				mySock.code(sint);
				std::cout << "sint: " << sint << std::endl;

				std::cout << "Type any key continue: ";
				std::cin >> cont;
				mySock.code(f);
				std::cout << "float: " << f << std::endl;

				std::cout << "Type any key continue: ";
				std::cin >> cont;
				mySock.code(d);
				std::cout << "double: " << d << std::endl;

				std::cout << "Type any key continue: ";
				std::cin >> cont;
				mySock.code(charString);
				std::cout << "str: " << charString << std::endl;

				mySock.end_of_message();
			}
			break;
		case 2: // Client
			char serverName[30];

			// Connect to the server
			std::cout << "Server: ";
			std::cin >> serverName;
			result = mySock.connect(serverName, SERVER_PORT);
			if(result != TRUE) {
				std::cout << "Connection failed\n";
				exit(-1);
			}
			std::cout << "Connected to [" << serverName<< ", " << SERVER_PORT << "]\n";
			

			while(true) {
				mySock.encode();
				
				std::cout << "Type char: ";
				std::cin >> c;
				mySock.code(c);

				std::cout << "Type int: ";
				std::cin >> integer;
				mySock.code(integer);

				std::cout << "Type long: ";
				std::cin >> lint;
				mySock.code(lint);

				std::cout << "Type short: ";
				std::cin >> sint;
				mySock.code(sint);

				std::cout << "Type float: ";
				std::cin >> f;
				mySock.code(f);

				std::cout << "Type double: ";
				std::cin >> d;
				mySock.code(d);

				std::cout << "Type string: ";
				std::cin >> charString;
				mySock.code(charString);

				mySock.end_of_message();
			}

		case 9:
			exit(0);
		default:
			break;
	}
}
