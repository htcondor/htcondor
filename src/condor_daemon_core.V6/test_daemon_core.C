#include <stdio.h>
#include "condor_common.h"
#include "condor_daemon_core.h"

extern "C" int SetSyscalls(int val){return val;}
extern char* myName;
char* mySubSystem = "COLLECTOR";

class Foo : public Service
{
	public:

		Foo();
		int timer5();
		int timerone();
		int com1(int, Stream*);

};

int
Foo::timer5()
{
	printf("*** In timer5()\n");
	return TRUE;
}

int
Foo::timerone()
{
	int port;

	printf("*** In OneTimeTimer()\n");

	if ( (port=daemonCore->InfoCommandPort()) == -1 ) {
		printf("OneTimer(): InfoCommandPort FAILED\n");
		return FALSE;
	}

	printf("*** going to port %d\n",port);

	SafeSock* rsock = new SafeSock("127.0.0.1",port);
	rsock->encode();
	rsock->snd_int(1,FALSE);
	char *buf="Please Work!";
	rsock->code(buf);
	rsock->eom();
	delete rsock;

	return TRUE;
}

int
Foo::com1(int command, Stream* stream)
{
	char *thestring = NULL;

	printf("*** In com1()\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
	if ( !stream->code(thestring) ) {
		printf("ERROR DECODING THESTRING!\n");
	} else {
		printf("*** com1(): thestring=<%s>\n",thestring);
	}
		
	if ( thestring )
		free(thestring);

	return TRUE;
}

int sig9(Service* s,int sig)
{
	printf("*** HEY MAN!! I got sig %d\n",sig);

#ifdef WIN32
	Sleep(1000);
#else
	sleep(1);
#endif

	daemonCore->Send_Signal( daemonCore->getpid(), 9 );

	return TRUE;
}

Foo::Foo()
{
	daemonCore->Register_Timer(5,5,(Eventcpp)timer5,"Five Second Timer",this);
	daemonCore->Register_Timer(3,(Eventcpp)timerone,"One-Time Timer",this);
	daemonCore->Register_Command(1,"Command One",(CommandHandlercpp)com1,"com1()",this);
	daemonCore->Register_Signal(9,"SIG9",(SignalHandler)sig9,"sig9()");
}

main_init(int argc, char *argv[])
{
	Foo*	f;
	
	printf("*** In main_init()\n");

	f = new Foo();

	daemonCore->Send_Signal(daemonCore->getpid(),9);

	return TRUE;
}


