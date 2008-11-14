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


#include "stdafx.h"
#include "smtp.h"
#if _MSC_VER < 1300 // if we're not compiling with MSVS.NET
// use the old iostream headers, which cause the old MFC libraries
// to be used as well. MSVC6's new MFC libraries
// that deal with io seem to be problematic.
#include <iostream.h>
#else 
// on .NET we must use the new MFC io libraries and they seem to work ok.
#include <iostream>
using namespace std;
#endif // we're not in .NET



CString my_get_user() {
	CString retval;
	char temp[100];
	unsigned long sizetemp = (long)sizeof(temp);
	GetUserName(temp, &sizetemp);
	retval = temp;
	return temp;
}

CString my_get_hostname() {
	CString retval;
	char temp[100];
	int blah;

	blah = gethostname(temp,sizeof(temp));

	if (blah) {
		blah = WSAGetLastError();
	}
	retval = temp;
	return temp;
}

CString my_getline() {
	CString retval;
	char buff[100];

	cin.get(buff, 100, '\n');
	
	retval += buff;

	char c;
	if(cin.get(c) && c != '\n') {
		cin.putback(c);
		retval += my_getline();
	}

	retval += '\n';
	return retval;
}

int usage(const char *myname)
{

	printf(
		"\n USAGE: %s [-s subject] -relay smtp-relay-hostname user@address [user@address ...]\n"
		" Then give the message to send via stdin.\n",
		myname);

	exit(1);
}

int main( int argc, char *argv[ ], char *envp[ ]) {
	CString subject, relay, recipient;  // Stuff passed in on the command line
	CString header;						// For the generated header
	CString body;						// The whole message
	CString sender = "@";
	//char buff[100];
	int counter;

	int tosomeone = 0, arelay =0;
	
	CSMTPMessage m;



	// Figure out who's sending this

	for (counter=1; counter < argc; counter++) {

		if (strncmp(argv[counter],"-s", 3) == 0) {
//			subject = argv[++counter];
			m.m_sSubject = argv[++counter];
		} else if (strncmp(argv[counter],"-relay", 7) == 0) {
			relay = argv[++counter];
			arelay++;
		} else {
//			recipient = argv[counter];
//			printf("I set recipient to: [%s]\n", recipient);
			m.AddRecipient(CSMTPAddress(argv[counter]));
			tosomeone++;
		}
	}

	if (!tosomeone) {
		printf("You forgot to add who you are sending this message too.\n");
		usage(argv[0]);	// never returns
	}

	if (!arelay) {
		// Someday, nicely figure out the relay.  
		// For now, consider no relay specified to be an error.
		usage(argv[0]);	// never returns
	}

	if (!AfxSocketInit()) {
		TRACE(_T("Failed to initialise the Winsock stack\n"));
		return 1;
	}

	while (!cin.eof()) {
		body += my_getline();
	}

	//printf("The body is [%s]\n",body);
	sender = my_get_user() + sender + my_get_hostname();

	CSMTPConnection smtp;
	if (!smtp.Connect(relay)) {
		return 1;
	}
	
	//m.AddRecipient(CSMTPAddress(recipient);
	m.m_From = CSMTPAddress(sender);
	//m.m_sSubject = subject;
	m.AddBody(body);
	if (!smtp.SendMessage(m)) {
		return 1;
	}
	//printf("The End.\n");
	return 0;
}
