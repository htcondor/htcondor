/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "simplelist.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_transfer_request.h"

const char ATTR_IP_PROTOCOL_VERSION[] = "ProtocolVersion";
const char ATTR_IP_NUM_TRANSFERS[] = "NumTransfers";
const char ATTR_IP_TRANSFER_SERVICE[] = "TransferService";
const char ATTR_IP_PEER_VERSION[] = "PeerVersion";

// This function assumes ownership of the pointer 
TransferRequest::TransferRequest(ClassAd *ip)
{
	ASSERT(ip != NULL);

	m_pre_push_func_desc = "None";
	m_pre_push_func = NULL;
	m_pre_push_func_this = NULL;

	m_post_push_func_desc = "None";
	m_post_push_func = NULL;
	m_post_push_func_this = NULL;

	m_update_func_desc = "None";
	m_update_func = NULL;
	m_update_func_this = NULL;

	m_reaper_func_desc = "None";
	m_reaper_func = NULL;
	m_reaper_func_this = NULL;

	m_ip = ip;

	m_rejected = false;

	/* Since this schema check happens here I don't need to check the
		existance of these attributes when I use them. */
	ASSERT(check_schema() == INFO_PACKET_SCHEMA_OK);
	m_client_sock = NULL;
	m_procids = NULL;
}

TransferRequest::TransferRequest() : m_ip(new ClassAd()),
	m_procids(0),
	m_client_sock(0),
	m_rejected(false),
	m_pre_push_func(0),
	m_pre_push_func_this(0),
	m_post_push_func(0),
	m_post_push_func_this(0),
	m_update_func(0),
	m_update_func_this(0),
	m_reaper_func(0),
	m_reaper_func_this(0) { }

TransferRequest::~TransferRequest()
{
	delete m_ip;
	m_ip = NULL;
}

SchemaCheck
TransferRequest::check_schema(void)
{
	int version;

	ASSERT(m_ip != NULL);

	/* ALL info packets MUST have a protocol version number */

	/* Check to make sure it exists */
	if (m_ip->LookupExpr(ATTR_IP_PROTOCOL_VERSION) == NULL) {
		EXCEPT("TransferRequest::check_schema() Failed due to missing %s attribute",
			ATTR_IP_PROTOCOL_VERSION);
	}

	/* for now, this assumes version 0 of the protocol. */

	/* Check to make sure it resolves to an int, and determine it */
	if (m_ip->LookupInteger(ATTR_IP_PROTOCOL_VERSION, version) == 0) {
		EXCEPT("TransferRequest::check_schema() Failed. ATTR_IP_PROTOCOL_VERSION "
				"must be an integer.");
	}

	/* for now, just check existance of attribute, but not type */
	if (m_ip->LookupExpr(ATTR_IP_NUM_TRANSFERS) == NULL) {
		EXCEPT("TransferRequest::check_schema() Failed due to missing %s "
			"attribute", ATTR_IP_NUM_TRANSFERS);
	}

	if (m_ip->LookupExpr(ATTR_IP_TRANSFER_SERVICE) == NULL) {
		EXCEPT("TransferRequest::check_schema() Failed due to missing %s "
			"attribute", ATTR_IP_TRANSFER_SERVICE);
	}

	if (m_ip->LookupExpr(ATTR_IP_PEER_VERSION) == NULL) {
		EXCEPT("TransferRequest::check_schema() Failed due to missing %s "
			"attribute", ATTR_IP_PEER_VERSION);
	}


	// currently, this either excepts, or returns ok.
	return INFO_PACKET_SCHEMA_OK;
}

void
TransferRequest::set_client_sock(ReliSock *rsock)
{
	m_client_sock = rsock;
}

ReliSock*
TransferRequest::get_client_sock(void)
{
	return m_client_sock;
}

void
TransferRequest::append_task(ClassAd *ad)
{
	ASSERT(m_ip != NULL);
	m_todo_ads.Append(ad);
}

void
TransferRequest::set_procids(std::vector<PROC_ID> *procs)
{
	ASSERT(m_ip != NULL);

	m_procids = procs;
}

// do not free this returned pointer
std::vector<PROC_ID>*
TransferRequest::get_procids(void)
{
	ASSERT(m_ip != NULL);

	return m_procids;
}

void
TransferRequest::dprintf(unsigned int lvl)
{
	std::string pv;

	ASSERT(m_ip != NULL);

	pv = get_peer_version();

	::dprintf(lvl, "TransferRequest Dump:\n");
	::dprintf(lvl, "\tProtocol Version: %d\n", get_protocol_version());
	::dprintf(lvl, "\tServer Mode: %u\n", get_transfer_service());
	::dprintf(lvl, "\tNum Transfers: %d\n", get_num_transfers());
	::dprintf(lvl, "\tPeer Version: %s\n", pv.c_str());
}

void
TransferRequest::set_num_transfers(int nt)
{
	ASSERT(m_ip != NULL);

	m_ip->Assign( ATTR_IP_NUM_TRANSFERS, nt );
}

int
TransferRequest::get_num_transfers(void)
{
	int num;

	ASSERT(m_ip != NULL);

	m_ip->LookupInteger(ATTR_IP_NUM_TRANSFERS, num);

	return num;
}

void
TransferRequest::set_transfer_service(const std::string &mode)
{
	m_ip->Assign( ATTR_IP_TRANSFER_SERVICE, mode );
}

#if 0
void
TransferRequest::set_transfer_service(TreqMode  /*mode*/)
{
	// XXX TODO
}
#endif


TreqMode
TransferRequest::get_transfer_service(void)
{
	std::string mode;

	ASSERT(m_ip != NULL);

	m_ip->LookupString(ATTR_IP_TRANSFER_SERVICE, mode);

	return ::transfer_mode(mode);
}

void
TransferRequest::set_protocol_version(int pv)
{
	ASSERT(m_ip != NULL);

	m_ip->Assign( ATTR_IP_PROTOCOL_VERSION, pv );
}

int
TransferRequest::get_protocol_version(void)
{
	int version; 

	ASSERT(m_ip != NULL);

	m_ip->LookupInteger(ATTR_IP_PROTOCOL_VERSION, version);

	return version;
}

void
TransferRequest::set_xfer_protocol(int xp)
{
	ASSERT(m_ip != NULL);

	m_ip->Assign( ATTR_TREQ_FTP, xp );
}

int
TransferRequest::get_xfer_protocol(void)
{
	int xfer_protocol; 

	ASSERT(m_ip != NULL);

	m_ip->LookupInteger(ATTR_TREQ_FTP, xfer_protocol);

	return xfer_protocol;
}

void
TransferRequest::set_direction(int dir)
{
	ASSERT(m_ip != NULL);

	m_ip->Assign( ATTR_TREQ_DIRECTION, dir );
}

int
TransferRequest::get_direction(void)
{
	int dir; 

	ASSERT(m_ip != NULL);

	m_ip->LookupInteger(ATTR_TREQ_DIRECTION, dir);

	return dir;
}


void
TransferRequest::set_used_constraint(bool con)
{
	ASSERT(m_ip != NULL);

	m_ip->Assign( ATTR_TREQ_HAS_CONSTRAINT, con );
}

bool
TransferRequest::get_used_constraint(void)
{
	bool con; 

	ASSERT(m_ip != NULL);

	m_ip->LookupBool(ATTR_TREQ_HAS_CONSTRAINT, con);

	return con;
}

void
TransferRequest::set_peer_version(const std::string &pv)
{
	ASSERT(m_ip != NULL);

	m_ip->Assign( ATTR_IP_PEER_VERSION, pv );
}

// This will make a copy when you assign the return value to something.
std::string
TransferRequest::get_peer_version(void)
{
	std::string pv;

	ASSERT(m_ip != NULL);

	m_ip->LookupString(ATTR_IP_PEER_VERSION, pv);

	return pv;
}

// do NOT delete this pointer, it is an alias pointer into the transfer request
SimpleList<ClassAd*>*
TransferRequest::todo_tasks(void)
{
	ASSERT(m_ip != NULL);

	return &m_todo_ads;
}

void
TransferRequest::set_capability(const std::string &capability)
{
	m_cap = capability;
}

const std::string&
TransferRequest::get_capability()
{
	return m_cap;
}

void
TransferRequest::set_rejected_reason(const std::string &reason)
{
	m_rejected_reason = reason;
}

const std::string&
TransferRequest::get_rejected_reason()
{
	return m_rejected_reason;
}

void 
TransferRequest::set_rejected(bool val)
{
	m_rejected = val;
}

bool 
TransferRequest::get_rejected(void) const
{
	return m_rejected;
}

void 
TransferRequest::set_pre_push_callback(std::string desc, 
	TreqPrePushCallback callback, Service *base)
{
	m_pre_push_func_desc = desc;
	m_pre_push_func = callback;
	m_pre_push_func_this = base;
}

TreqAction
TransferRequest::call_pre_push_callback(TransferRequest *treq, 
	TransferDaemon *td)
{
	return (m_pre_push_func_this->*(m_pre_push_func))(treq, td);
}

void
TransferRequest::set_post_push_callback(std::string desc, 
	TreqPostPushCallback callback, Service *base)
{
	m_post_push_func_desc = desc;
	m_post_push_func = callback;
	m_post_push_func_this = base;
}

TreqAction
TransferRequest::call_post_push_callback(TransferRequest *treq, 
	TransferDaemon *td)
{
	return (m_post_push_func_this->*(m_post_push_func))(treq, td);
}

void 
TransferRequest::set_update_callback(std::string desc, 
	TreqUpdateCallback callback, Service *base)
{
	m_update_func_desc = desc;
	m_update_func = callback;
	m_update_func_this = base;
}

TreqAction
TransferRequest::call_update_callback(TransferRequest *treq, 
	TransferDaemon *td, ClassAd *update)
{
	return (m_update_func_this->*(m_update_func))(treq, td, update);
}

void 
TransferRequest::set_reaper_callback(std::string desc, 
	TreqReaperCallback callback, Service *base)
{
	m_reaper_func_desc = desc;
	m_reaper_func = callback;
	m_reaper_func_this = base;
}

TreqAction
TransferRequest::call_reaper_callback(TransferRequest *treq)
{
	return (m_reaper_func_this->*(m_reaper_func))(treq);
}


// The transferd only needs a small amount of the information from this
// transfer request seriazed to it from the schedd. Basically the m_ip
// classad which represents the header information for this treq, and
// the job ads.
int
TransferRequest::put(Stream *sock)
{
	ClassAd *ad = NULL;

	sock->encode();

	// shove the internal header classad across
	putClassAd(sock, *m_ip);
	sock->end_of_message();

	// now dump all of the jobads through
	m_todo_ads.Rewind();
	while(m_todo_ads.Next(ad))
	{
		putClassAd(sock, *ad);
		sock->end_of_message();
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
// utility functions for enum conversions.

EncapMethod
encap_method(const std::string &line)
{
	if (line == "ENCAPSULATION_METHOD_OLD_CLASSADS") {
		return ENCAP_METHOD_OLD_CLASSADS;
	}

	return ENCAP_METHOD_UNKNOWN;
}

TreqMode
transfer_mode(std::string mode)
{
	return transfer_mode(mode.c_str());
}

TreqMode
transfer_mode(const char *mode)
{
	if ( 0 == strcmp(mode, "Active") ) {
		return TREQ_MODE_ACTIVE;
	}

	if ( 0 == strcmp(mode, "ActiveShadow") ) {
		return TREQ_MODE_ACTIVE_SHADOW; /* XXX DEMO mode */
	}

	if ( 0 == strcmp(mode, "Passive") ) {
		return TREQ_MODE_PASSIVE;
	}

	return TREQ_MODE_UNKNOWN;
}

