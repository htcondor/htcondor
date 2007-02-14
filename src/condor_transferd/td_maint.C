#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "MyString.h"
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
TransferD::dump_state_handler(int cmd, Stream *sock)
{
	ClassAd state;
	MyString tmp;

	dprintf(D_ALWAYS, "Got a DUMP_STATE!\n");

	// what uid am I running under?
	tmp.sprintf("Uid = %d", getuid());
	state.InsertOrUpdate(tmp.Value());

	// count how many pending requests I've had
	tmp.sprintf("OutstandingTransferRequests = %d", m_treqs.getNumElements());
	state.InsertOrUpdate(tmp.Value());

	// add more later

	sock->encode();

	state.put(*sock);

	sock->eom();

	// all done with this stream, so close it.
	return !KEEP_STREAM;
}

// inspect the transfer request data structures and exit if they have been
// empty for too long.
int
TransferD::exit_due_to_inactivity_timer(void)
{
	time_t now;

	if (m_inactivity_timer == 0) {
		// When we accept a transfer request, we set this to zero
		// which signifies that we are not in a timeout possible
		// state.
		
		return TRUE;
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

	return TRUE;
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







