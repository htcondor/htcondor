
#include "condor_common.h"
#include "dc_socket_manager.h"
#include "generic_stats.h"
#include "reli_sock.h"
#include "condor_daemon_core.h"
#include "daemon_command.h"
#include "condor_threads.h"

extern void **curr_dataptr;
extern void **curr_regdataptr;


static void
registerFD(int fd, Selector &selector, HandlerType handler_type)
{
	switch (handler_type)
	{
		case HANDLE_READ:
			selector.add_fd(fd, Selector::IO_READ);
			break;
		case HANDLE_READ_WRITE:
			selector.add_fd(fd, Selector::IO_READ);
			// Fall through!
		case HANDLE_WRITE:
			selector.add_fd(fd, Selector::IO_WRITE);
			// Fall through!
		case HANDLE_NONE:
			break;
	}
}


static void
registerFD(SockEnt &ent, Selector &selector)
{
	if (!ent.iosock) {return;}
	int fd = ent.iosock->get_file_desc();
	registerFD(fd, selector, ent.handler_type);
	ent.is_registered = true;
}


static void
removeFD(int fd, Selector &selector, HandlerType handler_type)
{
	switch (handler_type)
	{
		case HANDLE_READ:
			selector.delete_fd(fd, Selector::IO_READ);
			break;
		case HANDLE_READ_WRITE:
			selector.delete_fd(fd, Selector::IO_READ);
			// Fall through!
		case HANDLE_WRITE:
			selector.delete_fd(fd, Selector::IO_WRITE);
			// Fall through!
		case HANDLE_NONE:
			break;
	}
}

static void
removeFD(SockEnt &ent, Selector &selector)
{
	if (!ent.iosock) {return;}
	int fd = ent.iosock->get_file_desc();
	removeFD(fd, selector, ent.handler_type);
	ent.is_registered = false;
}


int
SockManager::registerCommandSocket(Stream *iosock, const char* iosock_descrip,
	const char *handler_descrip, Service* s, DCpermission perm, HandlerType handler_type, int is_cpp, void **prev_entry)
{

	// In sockTable, unlike the others handler tables, we allow for a NULL
	// handler and a NULL handlercpp - this means a command socket, so use
	// the default daemon core socket handler which strips off the command.
	// SO, a blank table entry is defined as a NULL iosock field.
	int result = registerSocket(iosock, iosock_descrip, NULL, NULL, handler_descrip, s, perm, handler_type, is_cpp, prev_entry);

	if (result < 0) {return result;}

	m_command_socks.push_back(result);
	m_socks[result].is_command_sock = true;

	return result;
}


int
SockManager::registerSocket(Stream *iosock, const char* iosock_descrip,
	SocketHandler handler, SocketHandlercpp handlercpp, const char *handler_descrip,
	Service* service, DCpermission perm, HandlerType handler_type, int is_cpp, void **prev_entry)
{
	if ( prev_entry ) {
		*prev_entry = NULL;
	}

	if ( !iosock ) {
		dprintf(D_DAEMONCORE, "Can't register NULL socket \n");
		return -1;
	}

	// Find empty slot, set to be idx.
	unsigned idx=0;
	bool found_entry = false;
	for (std::vector<SockEnt>::iterator it=m_socks.begin(); it!=m_socks.end(); it++, idx++)
	{
		if ( it->iosock == NULL )
		{
			found_entry = true;
			break;
		}
		if ( it->remove_asap && (it->servicing_tid == 0) )
		{
			found_entry = true;
			it->iosock = NULL;
			break;
		}
	}
	if (!found_entry)
	{
		idx = m_socks.size();
		m_socks.push_back(SockEnt());
	}
	// NOTE: There previously was a check that m_socks[idx].iosock == NULL;
	// I have eliminated this until we seriously reconsider doing condor threads.

	if (handler_descrip == NULL) {handler_descrip = "UnnamedHandler";}
	daemonCore->dc_stats.NewProbe("Socket", handler_descrip, AS_COUNT | IS_RCT | IF_NONZERO | IF_VERBOSEPUB);

	// Verify that this socket has not already been registered
	// Since we are scanning the entire table to do this (change this someday to a hash!),
	// at the same time update our nRegisteredSocks count by initializing it
	// to the number of slots (nSock) and then subtracting out the number of slots
	// not in use.
	m_registered = m_socks.size();
	int fd_to_register = ((Sock *)iosock)->get_file_desc();
	unsigned idx2 = 0;
	bool duplicate_found = false;
	for (std::vector<SockEnt>::iterator it=m_socks.begin(); it!=m_socks.end(); it++, idx2++)
	{       
		if (it->iosock == iosock)
		{
			idx = idx2;
			duplicate_found = true;
		}

		// fd may be -1 if doing a "fake" registration: reverse_connect_pending
		// so do not require uniqueness of fd in that case
		if ( it->iosock && fd_to_register != -1 )
		{
			if (static_cast<Sock *>(it->iosock)->get_file_desc() == fd_to_register)
			{
				idx = idx2;
				duplicate_found = true;
			}
		}

			// check if slot empty or available
		if ( (it->iosock == NULL) ||  // slot is empty
			(it->remove_asap && it->servicing_tid==0 ) )  // Slot is available.
		{
			m_registered--; // decrement count of active sockets
		}
	}
	if (duplicate_found) {
		if (prev_entry) {
			*prev_entry = new SockEnt();
			*static_cast<SockEnt*>(*prev_entry) = m_socks[idx];
		}
		else
		{
			dprintf(D_ALWAYS, "DaemonCore: Attempt to register socket %d (ptr=%p) twice (fd=%d)\n", idx, iosock, fd_to_register);
			return -2;
		}
	}

			// Check that we are within the file descriptor safety limit
			// We currently only do this for non-blocking connection attempts because
			// in most other places, the code does not check the return value
			// from Register_Socket().  Plus, it really does not make sense to 
			// enforce a limit for other cases --- if the socket already exists,
			// DaemonCore should be able to manage it for you.

	if (iosock->type() == Stream::reli_sock && static_cast<ReliSock*>(iosock)->is_connect_pending())
	{
		MyString overload_msg;
		bool overload_danger = daemonCore->TooManyRegisteredSockets( static_cast<Sock *>(iosock)->get_file_desc(), &overload_msg);

		if (overload_danger)
		{
			dprintf(D_ALWAYS,
				"Aborting registration of socket %s %s: %s\n",
				iosock_descrip ? iosock_descrip : "(unknown)",
				handler_descrip ? handler_descrip : static_cast<Sock*>(iosock)->get_sinful_peer(),
				overload_msg.Value());
			return -3;
		}
	}

	SockEnt &ent = m_socks[idx];
	ent.servicing_tid = 0;
	ent.remove_asap = false;
	ent.call_handler = false;
	ent.iosock = static_cast<Sock *>(iosock);
	switch (iosock->type())
	{
		case Stream::reli_sock:
		{
			ReliSock &rsock = *static_cast<ReliSock*>(iosock);
			ent.is_connect_pending = rsock.is_connect_pending() && !rsock.is_reverse_connect_pending();
			ent.is_reverse_connect_pending = rsock.is_reverse_connect_pending();
			break;
		}
		case Stream::safe_sock:
			ent.is_connect_pending = false;
			ent.is_reverse_connect_pending = false;
			break;
		default:
			EXCEPT("Adding CEDAR socket of unknown type");
			break;
	}
	ent.handler = handler;
	ent.handlercpp = handlercpp;
	ent.is_cpp = is_cpp;
	ent.perm = perm;
	ent.handler_type = handler_type;
	ent.service = service;
	ent.data_ptr = NULL;
	ent.waiting_for_data = false;
	ent.iosock_descrip = iosock_descrip ? iosock_descrip : static_cast<Sock*>(iosock)->get_sinful_peer();
	ent.handler_descrip = handler_descrip ? handler_descrip : "Unknown Handler";
	if (ent.handler_descrip == DaemonCommandProtocol::WaitForSocketDataString)
	{
		ent.waiting_for_data = true;
	}
	if (ent.is_connect_pending && (ent.handler || ent.handlercpp))
	{	// Note we do not do this for command sockets - we assume these are already connected!
		m_sel_all.add_fd(fd_to_register, Selector::IO_EXCEPT);
		m_sel_all.add_fd(fd_to_register, Selector::IO_WRITE);
	}
	else
	{
		registerFD(ent, m_sel_all);
	}

	// Update curr_regdataptr for SetDataPtr()
	curr_regdataptr = &(ent.data_ptr);

	// Conditionally dump what our table looks like
	dump(D_FULLDEBUG | D_DAEMONCORE, "");

	// If we are a worker thread, wake up select in the main thread
	// so the main thread re-computes the fd_sets.
	daemonCore->Wake_up_select();

	return static_cast<int>(idx);
}


int
SockManager::cancelSocket(Stream* insock, void *prev_entry)
{
	if (!insock) {return false;}

	int idx=0, idx2 = -1;
	for (std::vector<SockEnt>::const_iterator it=m_socks.begin(); it!=m_socks.end(); it++, idx++)
	{
		if (it->iosock == insock)
		{
			idx2 = idx;
			break;
		}
	}
	if (idx2 == -1)
	{
		dprintf( D_ALWAYS,"Cancel_Socket: called on non-registered socket!\n");
		if (insock)
		{
			dprintf(D_ALWAYS,"Offending socket number %d to %s\n",
				static_cast<Sock*>(insock)->get_file_desc(),
				insock->peer_description());
		}
		dump(D_DAEMONCORE, "");
		return false;
	}
	SockEnt &ent = m_socks[idx];

		// Clear any data_ptr which go to this entry we just removed
	if (curr_regdataptr == &(ent.data_ptr)) {curr_regdataptr = NULL;}
	if (curr_dataptr == &(ent.data_ptr)) {curr_dataptr = NULL;}

	if ((ent.servicing_tid == 0) || ent.servicing_tid == CondorThreads::get_handle()->get_tid() || prev_entry)
	{
		dprintf(D_DAEMONCORE,"Cancel_Socket: cancelled socket %d <%s> %p\n",
			idx, ent.iosock_descrip.c_str(), ent.iosock);
		// Remove entry; mark it is available for next add via iosock=NULL
		if (prev_entry)
		{
			SockEnt &prev_entry_ref = *static_cast<SockEnt *>(prev_entry);
			prev_entry_ref.servicing_tid = ent.servicing_tid;
			ent = prev_entry_ref;
		}
		else
		{
			removeFD(ent, m_sel_all);
			if (ent.is_connect_pending)
			{
				m_sel_all.delete_fd(ent.iosock->get_file_desc(), Selector::IO_WRITE);
				m_sel_all.delete_fd(ent.iosock->get_file_desc(), Selector::IO_EXCEPT);
			}
			if (ent.is_command_sock) {removeFD(ent, m_sel_comm);}
			ent.iosock = NULL;
		}
	}
	else
	{
		dprintf(D_DAEMONCORE,"Cancel_Socket: deferred cancel socket %d <%s> %p\n",
			idx, ent.iosock_descrip.c_str(), ent.iosock);
		ent.remove_asap = true;
	}

	if (!prev_entry)
	{
		m_registered--;
	}

	dump(D_FULLDEBUG | D_DAEMONCORE, "");

	// If we are a worker thread, wake up select in the main thread
	// so the main thread re-computes the fd_sets.
	daemonCore->Wake_up_select();

	return true;
}


bool
SockManager::isRegistered(Stream *stream)
{
	for (std::vector<SockEnt>::const_iterator it=m_socks.begin(); it!=m_socks.end(); it++)
	{
		if (stream == it->iosock) {return true;}
	}
	return false;
}


time_t
SockManager::updateSelector()
{
	time_t min_deadline = 0;
	for (std::vector<SockEnt>::iterator it=m_socks.begin(); it!=m_socks.end(); it++)
	{
		if (!it->iosock || (it->servicing_tid != 0)) {continue;}
		else if (it->remove_asap)
		{
			it->iosock = NULL;
			continue;
		}
		else if (it->is_reverse_connect_pending || it->is_connect_pending) {continue;}
		else if (it->is_registered)
		{
			registerFD(*it, m_sel_all);
		}
		time_t deadline = it->iosock->get_deadline();
		if (deadline)
		{
			if (min_deadline == 0 || min_deadline > deadline) {
				min_deadline = deadline;
			}
		}
	}
	return min_deadline;
}


void
SockManager::watchFDs(const std::vector<std::pair<int, HandlerType> > &fds_to_watch)
{
	for (std::vector<std::pair<int, HandlerType> >::const_iterator it = fds_to_watch.begin(); it!=fds_to_watch.end(); it++)
	{
		registerFD(it->first, m_sel_all, it->second);
	}
}


void
SockManager::removeFDs(const std::vector<std::pair<int, HandlerType> > &fds_to_watch)
{
	for (std::vector<std::pair<int, HandlerType> >::const_iterator it = fds_to_watch.begin(); it!=fds_to_watch.end(); it++)
	{
		removeFD(it->first, m_sel_all, it->second);
	}
}


void
SockManager::dump(int flag, const char *indent)
{
	unsigned idx=0;
	for (std::vector<SockEnt>::const_iterator it=m_socks.begin(); it!=m_socks.end(); it++, idx++)
	{
		if (it->iosock)
		{
			dprintf(flag, "%s%u: %d %s %s\n",
				indent, idx, static_cast<Sock *>(it->iosock)->get_file_desc(),
				it->iosock_descrip.c_str(), it->handler_descrip.c_str());
		}
	}
	dprintf(flag, "\n");
}


int
SockManager::getReadyCommandSocket()
{
	bool recheck = m_pos_comm > 0;
	for (std::vector<int>::const_iterator it=m_command_socks.begin()+m_pos_comm; it!=m_command_socks.end(); it++, m_pos_comm++)
	{
		SockEnt &ent = m_socks[*it];
		if (!ent.iosock || ent.remove_asap || ent.servicing_tid ||
			ent.is_reverse_connect_pending || ent.is_connect_pending)
		{
			continue;
		}
		if (m_sel_comm.fd_ready(ent.iosock->get_file_desc(), Selector::IO_READ) || m_sel_comm.fd_ready(ent.iosock->get_file_desc(), Selector::IO_WRITE))
		{
			if (recheck && ent.handler_type == HANDLE_READ)
			{
				Selector sel;
				sel.set_timeout(0);
				sel.add_fd(ent.iosock->get_file_desc(), Selector::IO_READ);
				sel.execute();
				if (sel.timed_out()) {continue;}
			}
			ent.call_handler = true;
			return *it;
		}
	}
	return -1;
}


int
SockManager::getReadySocket(time_t now)
{
	bool recheck = (m_pos_all > 0);
	for (std::vector<SockEnt>::iterator it=m_socks.begin()+m_pos_all; it!=m_socks.end(); it++, m_pos_all++)
	{
		if (!it->iosock || it->remove_asap || it->servicing_tid ||
			it->is_reverse_connect_pending)
		{
			continue;
		}
		time_t deadline = it->iosock->get_deadline();
		bool timed_out = deadline && (deadline < now);
		if (it->is_connect_pending && (m_sel_all.fd_ready(it->iosock->get_file_desc(), Selector::IO_WRITE) ||
			m_sel_all.fd_ready(it->iosock->get_file_desc(), Selector::IO_EXCEPT)) &&
			(it->iosock->do_connect_finish() != CEDAR_EWOULDBLOCK))
		{
			it->call_handler = true;
			return m_pos_all++;
		}
		else if (m_sel_all.fd_ready(it->iosock->get_file_desc(), Selector::IO_READ) ||
			m_sel_all.fd_ready(it->iosock->get_file_desc(), Selector::IO_WRITE) ||
			timed_out)
		{
			// If we invoke more than one callback per call to execute(), then we run the risk
			// of deadlock because the callback for socket A also drains socket B's buffer.  Then,
			// if socket B's callback can't handle non-blocking reads, we end up with getting
			// stuck in socket B's callback for an undetermined amount of time.
			// As this is most drastic for read sockets, we only recheck then.
			if (!timed_out && recheck && it->handler_type == HANDLE_READ)
			{
				Selector sel;
				sel.set_timeout(0);
				sel.add_fd(it->iosock->get_file_desc(), Selector::IO_READ);
				sel.execute();
				if (sel.timed_out()) {continue;}
			}
			it->call_handler = true;
			return m_pos_all++;
		}
	}
	return -1;
}


Sock *
SockManager::getInitialCommandSocket()
{
	for (std::vector<int>::const_iterator it=m_command_socks.begin(); it != m_command_socks.end(); it++)
	{
		SockEnt &ent = m_socks[*it];
		if (ent.iosock && ent.is_command_sock && !ent.remove_asap)
		{
			return ent.iosock;
		}
	}
	return NULL;
}

