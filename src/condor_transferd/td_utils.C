#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_td.h"
#include "MyString.h"
#include "condor_io.h"


// generate a capability that is unique against all the capabilities presently
// generated.
MyString
TransferD::gen_capability(void)
{
	TransferRequest *dummy = NULL;
	MyString cap;

	// if this iterates for a long time, there is something very wrong.
	do {
		cap.randomlyGenerate("0123456789abcdefg", 64);
	} while(m_treqs.lookup(cap, dummy) == 0);

	return cap;
}

void
TransferD::refuse(Sock *sock)
{
	int val = NOT_OK;

	sock->encode();
	sock->code( val );
	sock->eom();
}
