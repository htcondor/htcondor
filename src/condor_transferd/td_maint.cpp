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

void
TransferD::reconfig(void)
{
}

void
TransferD::shutdown_fast(void)
{
}

void
TransferD::shutdown_graceful(void)
{
}

void
TransferD::transferd_exit(void)
{
}

int
TransferD::dump_state_handler(int  /*cmd*/, Stream *sock)
{
	ClassAd state;

	dprintf(D_ALWAYS, "Got a DUMP_STATE!\n");

	// what uid am I running under?
	state.Assign("Uid", getuid());

	// count how many pending requests I've had
	state.Assign("OutstandingTransferRequests", (int)m_treqs.getNumElements());

	// add more later

	sock->encode();

	putClassAd(sock, state);

	sock->end_of_message();

	// all done with this stream, so close it.
	return !KEEP_STREAM;
}

// inspect the transfer request data structures and exit if they have been
// empty for too long.
void
TransferD::exit_due_to_inactivity_timer(void) const
{
	time_t now;

	if (m_inactivity_timer == 0) {
		// When we accept a transfer request, we set this to zero
		// which signifies that we are not in a timeout possible
		// state.
		
		return;
	}

	// When the last transfer request is processed,
	// then this is set to that unix time. Then, as this timer
	// goes off, we compare "now" against that last timestamp of
	// when something last left the queue and if the difference
	// is greater than the timeout value, the transferd exits.

	now = time(NULL);

	if ((now - m_inactivity_timer) > g_td.m_features.get_timeout()) {
		// nothing to clean up, so exit;
		DC_Exit(TD_EXIT_TIMEOUT);
	}
}

int
TransferD::reaper_handler(int pid, int exit_status)
{
	if( WIFSIGNALED(exit_status) ) {
		dprintf( D_ALWAYS, "Unknown process exited, pid=%d, signal=%d\n", pid,
				 WTERMSIG(exit_status) );
	} else {
		dprintf( D_ALWAYS, "Unknown process exited, pid=%d, status=%d\n", pid,
				 WEXITSTATUS(exit_status) );
	}

	return TRUE;
}







