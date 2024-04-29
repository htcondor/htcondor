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
#include "dc_transfer_queue.h"
#include "condor_attributes.h"
#include "selector.h"
#include "dc_schedd.h"

TransferQueueContactInfo::TransferQueueContactInfo() {
	m_unlimited_uploads = true;
	m_unlimited_downloads = true;
}
TransferQueueContactInfo::TransferQueueContactInfo(char const *addr,bool unlimited_uploads,bool unlimited_downloads) {
	ASSERT(addr);
	m_addr = addr;
	m_unlimited_uploads = unlimited_uploads;
	m_unlimited_downloads = unlimited_downloads;
}
void
TransferQueueContactInfo::operator=(TransferQueueContactInfo const &copy) {
	m_addr = copy.m_addr;
	m_unlimited_uploads = copy.m_unlimited_uploads;
	m_unlimited_downloads = copy.m_unlimited_downloads;
}
TransferQueueContactInfo::TransferQueueContactInfo(char const *str) {
		// expected format: limit=upload,download,...;addr=<...>
	m_unlimited_uploads = true;
	m_unlimited_downloads = true;
	while(str && *str) {
		std::string name,value;

		char const *pos = strchr(str,'=');
		if( !pos ) {
			EXCEPT("Invalid transfer queue contact info: %s",str);
		}
		formatstr(name,"%.*s",(int)(pos-str),str);
		str = pos+1;

		size_t len = strcspn(str,";");
		formatstr(value,"%.*s",(int)len,str);
		str += len;
		if( *str == ';' ) {
			str++;
		}

		if( name == "limit" ) {
			for (auto& queue: StringTokenIterator(value, ",")) {
				if( queue == "upload" ) {
					m_unlimited_uploads = false;
				}
				else if( queue == "download" ) {
					m_unlimited_downloads = false;
				}
				else {
					EXCEPT("Unexpected value %s=%s",name.c_str(),queue.c_str());
				}
			}
		}
		else if( name == "addr" ) {
			m_addr = value;
		}
		else {
			EXCEPT("unexpected TransferQueueContactInfo: %s",name.c_str());
		}
	}
}

bool
TransferQueueContactInfo::GetStringRepresentation(std::string &str) {
		// this function must produce the same format that is parsed by
		// TransferQueueContactInfo(char const *str).
		// expected format: limit=upload,download,...;addr=<...>
	char const *delim = ";";
	if( m_unlimited_uploads && m_unlimited_downloads ) {
		return false;
	}

	StringList limited_queues;
	if( !m_unlimited_uploads ) {
		limited_queues.append("upload");
	}
	if( !m_unlimited_downloads ) {
		limited_queues.append("download");
	}
	char *list_str = limited_queues.print_to_delimed_string(",");
	str = "";
	str += "limit=";
	str += list_str;
	str += delim;
	str += "addr=";
	str += m_addr;
	free(list_str);

	return true;
}

DCTransferQueue::DCTransferQueue( TransferQueueContactInfo &contact_info )
	: Daemon( DT_SCHEDD, contact_info.GetAddress(), NULL)
{
	m_unlimited_uploads = contact_info.GetUnlimitedUploads();
	m_unlimited_downloads = contact_info.GetUnlimitedDownloads();

	Init();
}

DCTransferQueue::DCTransferQueue( const DCSchedd &schedd )
	: Daemon( schedd )
{
	m_unlimited_uploads = false;
	m_unlimited_downloads = false;

	Init();
}

void
DCTransferQueue::Init()
{
	m_xfer_downloading = false;
	m_xfer_queue_sock = NULL;
	m_xfer_queue_pending = false;
	m_xfer_queue_go_ahead = false;

	timerclear( &m_last_report );
	m_next_report = 0;
	m_report_interval = 0;
	m_recent_bytes_sent = 0;
	m_recent_bytes_received = 0;
	m_recent_usec_file_read = 0;
	m_recent_usec_file_write = 0;
	m_recent_usec_net_read = 0;
	m_recent_usec_net_write = 0;
}

DCTransferQueue::~DCTransferQueue( void )
{
	ReleaseTransferQueueSlot();
}

bool
DCTransferQueue::GoAheadAlways( bool downloading ) const {
	if( downloading ) {
		return m_unlimited_downloads;
	}
	else {
		return m_unlimited_uploads;
	}
}

bool
DCTransferQueue::RequestTransferQueueSlot(bool downloading,filesize_t sandbox_size,char const *fname,char const *jobid,char const *queue_user,int timeout,std::string &error_desc)
{
	ASSERT(fname);
	ASSERT(jobid);

	if( GoAheadAlways( downloading ) ) {
		m_xfer_downloading = downloading;
		m_xfer_fname = fname;
		m_xfer_jobid = jobid;
		return true;
	}
	CheckTransferQueueSlot();
	if( m_xfer_queue_sock ) {
			// A request has already been made.
			// Currently, this is a no-op, because any upload/download slot
			// is as good as any other.  In the future, there may be
			// different queues for different paths.

		ASSERT( m_xfer_downloading == downloading );
		m_xfer_fname = fname;
		m_xfer_jobid = jobid;
		return true;
	}

	time_t started = time(NULL);
	CondorError errstack;
		// Our caller has to finish this operation in the specified
		// amount of time or risk not responding to the file transfer
		// peer in time, so ignore the timeout multiplier and set the
		// timeout exactly as specified.
	m_xfer_queue_sock = reliSock( timeout, 0, &errstack, false, true );

	if( !m_xfer_queue_sock ) {
		formatstr(m_xfer_rejected_reason,
			"Failed to connect to transfer queue manager for job %s (%s): %s.",
			jobid, fname, errstack.getFullText().c_str() );
		error_desc = m_xfer_rejected_reason;
		dprintf(D_ALWAYS,"%s\n",m_xfer_rejected_reason.c_str());
		return false;
	}

	if( timeout ) {
		timeout -= time(NULL)-started;
		if( timeout <= 0 ) {
			timeout = 1;
		}
	}

	if (IsDebugLevel(D_COMMAND)) {
		int cmd = TRANSFER_QUEUE_REQUEST;
		dprintf (D_COMMAND, "DCTransferQueue::RequestTransferQueueSlot(%s,...) making connection to %s\n", getCommandStringSafe(cmd), _addr.c_str());
	}

	bool connected = startCommand(
		TRANSFER_QUEUE_REQUEST, m_xfer_queue_sock, timeout, &errstack );

	if( !connected )
	{
		delete m_xfer_queue_sock;
		m_xfer_queue_sock = NULL;
		formatstr(m_xfer_rejected_reason,
			"Failed to initiate transfer queue request for job %s (%s): %s.",
			jobid, fname, errstack.getFullText().c_str() );
		error_desc = m_xfer_rejected_reason;
		dprintf(D_ALWAYS,"%s\n",m_xfer_rejected_reason.c_str());
		return false;
	}

	m_xfer_downloading = downloading;
	m_xfer_fname = fname;
	m_xfer_jobid = jobid;

	ClassAd msg;
	msg.Assign(ATTR_DOWNLOADING,downloading);
	msg.Assign(ATTR_FILE_NAME,fname);
	msg.Assign(ATTR_JOB_ID,jobid);
	msg.Assign(ATTR_USER,queue_user);
	msg.Assign(ATTR_SANDBOX_SIZE,sandbox_size);

	m_xfer_queue_sock->encode();

	if( !putClassAd(m_xfer_queue_sock, msg) || !m_xfer_queue_sock->end_of_message() )
	{
		formatstr(m_xfer_rejected_reason,
			"Failed to write transfer request to %s for job %s "
			"(initial file %s).",
			m_xfer_queue_sock->peer_description(),
			m_xfer_jobid.c_str(), m_xfer_fname.c_str());
		error_desc = m_xfer_rejected_reason;
		dprintf(D_ALWAYS,"%s\n",m_xfer_rejected_reason.c_str());
		return false;
	}

	m_xfer_queue_sock->decode();

		// Request has been initiated.  Now sender should call
		// PollForTransferQueueSlot() to get response.
	m_xfer_queue_pending = true;
	return true;
}

bool
DCTransferQueue::PollForTransferQueueSlot(int timeout,bool &pending,std::string &error_desc)
{
	if( GoAheadAlways( m_xfer_downloading ) ) {
		return true;
	}
	CheckTransferQueueSlot();

	if( !m_xfer_queue_pending ) {
		// status of request is known
		pending = false;
		if( !m_xfer_queue_go_ahead ) {
			error_desc = m_xfer_rejected_reason;
		}
		return m_xfer_queue_go_ahead;
	}

	Selector selector;
	selector.add_fd( m_xfer_queue_sock->get_file_desc(), Selector::IO_READ );
	time_t start = time(NULL);
	do {
		int t = timeout - (time(NULL) - start);
		selector.set_timeout( t >= 0 ? t : 0 );
		selector.execute();
	} while( selector.signalled() );

	if( selector.timed_out() ) {
			// It is expected that we may time out while waiting for a
			// response.  The caller should keep calling this function
			// periodically until we get a result.
		pending = true;
		return false;
	}

	m_xfer_queue_sock->decode();
	ClassAd msg;
	if( !getClassAd(m_xfer_queue_sock, msg) ||
		!m_xfer_queue_sock->end_of_message() )
	{
		formatstr(m_xfer_rejected_reason,
			"Failed to receive transfer queue response from %s for job %s "
			"(initial file %s).",
			m_xfer_queue_sock->peer_description(),
			m_xfer_jobid.c_str(),
			m_xfer_fname.c_str());
		goto request_failed;
	}

	int result; // this should be one of the values in XFER_QUEUE_ENUM
	if( !msg.LookupInteger(ATTR_RESULT,result) ) {
		std::string msg_str;
		sPrintAd(msg_str, msg);
		formatstr(m_xfer_rejected_reason,
			"Invalid transfer queue response from %s for job %s (%s): %s",
			m_xfer_queue_sock->peer_description(),
			m_xfer_jobid.c_str(),
			m_xfer_fname.c_str(),
			msg_str.c_str());
		goto request_failed;
	}

	if( result == XFER_QUEUE_GO_AHEAD ) {
		m_xfer_queue_go_ahead = true;
	}
	else {
		m_xfer_queue_go_ahead = false;
		std::string reason;
		msg.LookupString(ATTR_ERROR_STRING,reason);
		formatstr(m_xfer_rejected_reason,
			"Request to transfer files for %s (%s) was rejected by %s: %s",
			m_xfer_jobid.c_str(), m_xfer_fname.c_str(),
			m_xfer_queue_sock->peer_description(),
			reason.c_str());

		goto request_failed;
	}

	{
		int report_interval = 0;
		if( msg.LookupInteger(ATTR_REPORT_INTERVAL,report_interval) ) {
			m_report_interval = (unsigned)report_interval;
			condor_gettimestamp( m_last_report );
			m_next_report = m_last_report.tv_sec + m_report_interval;
		}
	}

	m_xfer_queue_pending = false;
	pending = m_xfer_queue_pending;
	return true;

 request_failed:
	error_desc = m_xfer_rejected_reason;
	dprintf(D_ALWAYS, "%s\n", m_xfer_rejected_reason.c_str());
	m_xfer_queue_pending = false;
	m_xfer_queue_go_ahead = false;
	pending = m_xfer_queue_pending;
	return false;
}

void
DCTransferQueue::ReleaseTransferQueueSlot()
{
	if( m_xfer_queue_sock ) {
		if( m_report_interval ) {
			SendReport(time(NULL),true);
		}
		delete m_xfer_queue_sock;
		m_xfer_queue_sock = NULL;
	}
	m_xfer_queue_pending = false;
	m_xfer_queue_go_ahead = false;
	m_xfer_rejected_reason = "";
}

bool
DCTransferQueue::CheckTransferQueueSlot()
{
	if( !m_xfer_queue_sock ) {
		return false;
	}
	if( m_xfer_queue_pending ) {
			// If connection closes while our status is still pending,
			// we will find out in PollForTransferQueueSlot(), so no
			// need to do anything here.
		return false;
	}

	Selector selector;
	selector.add_fd( m_xfer_queue_sock->get_file_desc(), Selector::IO_READ );
	selector.set_timeout( 0 );
	selector.execute();

	if( selector.has_ready() ) {
			// If the socket ever selects true for read, this means the
			// transfer queue manager has either died or taken away our
			// transfer slot.

		formatstr(m_xfer_rejected_reason,
			"Connection to transfer queue manager %s for %s has gone bad.",
			m_xfer_queue_sock->peer_description(), m_xfer_fname.c_str());
		dprintf(D_ALWAYS,"%s\n",m_xfer_rejected_reason.c_str());

		m_xfer_queue_go_ahead = false;
		return false;
	}

		// All is quiet on our connection to the transfer queue manager.
	return true;
}

void
DCTransferQueue::SendReport(time_t now,bool disconnect)
{
	std::string report;
	struct timeval now_usec;
	condor_gettimestamp( now_usec );
	long interval = timersub_usec( now_usec, m_last_report );
	if( interval < 0 ) {
		interval = 0;
	}
	formatstr(report,"%u %u %u %u %u %u %u %u",
			  (unsigned)now,
			  (unsigned)interval,
			  m_recent_bytes_sent,
			  m_recent_bytes_received,
			  m_recent_usec_file_read,
			  m_recent_usec_file_write,
			  m_recent_usec_net_read,
			  m_recent_usec_net_write);

	if( m_xfer_queue_sock ) {
		m_xfer_queue_sock->encode();
		if ( !m_xfer_queue_sock->put(report.c_str()) ||
			 !m_xfer_queue_sock->end_of_message() )
		{
			dprintf(D_FULLDEBUG,"Failed to send transfer queue i/o report.\n");
		}
		if( disconnect ) {
				// Tell the server we are done.
			if (!m_xfer_queue_sock->put("")) {
				dprintf(D_ALWAYS,"Failed to send disconnect request.\n");
			}
			m_xfer_queue_sock->end_of_message();
		}
	}

	m_recent_bytes_sent = 0;
	m_recent_bytes_received = 0;
	m_recent_usec_file_read = 0;
	m_recent_usec_file_write = 0;
	m_recent_usec_net_read = 0;
	m_recent_usec_net_write = 0;

	m_last_report = now_usec;
	m_next_report = now + m_report_interval;
}
