#include "stdafx.h"
#include "smtp.h"
#include <iostream.h>


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
		return 1;
	}

	if (!arelay) {
		// Nicely figure out the relay.
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