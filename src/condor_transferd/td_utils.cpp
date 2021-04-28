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
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_td.h"
#include "condor_io.h"


// generate a capability that is unique against all the capabilities presently
// generated.
std::string
TransferD::gen_capability(void)
{
	TransferRequest *dummy = NULL;
	std::string cap;

	// if this iterates for a long time, there is something very wrong.
	do {
		//randomlyGeneratePRNG(cap, "0123456789abcdefg", 64);
		randomlyGenerateInsecure(cap, "0123456789abcdefg", 64);
	} while(m_treqs.lookup(cap, dummy) == 0);

	return cap;
}

void
TransferD::refuse(Sock *sock)
{
	int val = NOT_OK;

	sock->encode();
	if (!sock->code( val )) {
		dprintf(D_ALWAYS, "Failed to refuse td connection to remote side\n");
	}
	sock->end_of_message();
}
