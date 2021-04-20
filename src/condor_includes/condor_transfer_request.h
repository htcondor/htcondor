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

#ifndef TRANSFER_REQUEST_H
#define TRANSFER_REQUEST_H

#include "file_transfer.h"
#include "proc.h"
#include "condor_classad.h"
#include "condor_ftp.h"

// Used to determine the type of protocol encapsulation the transferd desires.
enum EncapMethod {
	ENCAP_METHOD_UNKNOWN = 0,
	ENCAP_METHOD_OLD_CLASSADS,
};

// This enum is mostly usd by the TransferRequest objects methods 
enum SchemaCheck {
	INFO_PACKET_SCHEMA_UNKNOWN = 0,
	INFO_PACKET_SCHEMA_OK,
	INFO_PACKET_SCHEMA_NOT_OK,
};

/* This describes who is supposed to initiate the processing of a specific
	transfer request. Either some other client program, which is passive,
	or the transferd itself, which is active. */
enum TreqMode {
	TREQ_MODE_UNKNOWN = 0,
	TREQ_MODE_ACTIVE,
	TREQ_MODE_PASSIVE,
	TREQ_MODE_ACTIVE_SHADOW, /* XXX DEMO mode */
};

/* This describes what actions the schedd would like to perform with the 
	transfer request when the callback engine calls the schedd's callbacks. 
*/
enum TreqAction {
	TREQ_ACTION_CONTINUE,	/* continue processing to the next stage */
	TREQ_ACTION_FORGET,		/* remove the treq from all internal structures,
								but don't delete the pointer. Assume the
								callback took control of the memory. */
	TREQ_ACTION_TERMINATE,	/* the callback handler has decided to end the
								processing of the transfer request. In this
								case, the caller frame of the callback will
								remove the treq from its internal data
								structres and delete the pointer. */
};

// Move to a different header file when done!
extern const char ATTR_IP_PROTOCOL_VERSION[];
extern const char ATTR_IP_NUM_TRANSFERS[];
extern const char ATTR_IP_TRANSFER_SERVICE[];
extern const char ATTR_IP_PEER_VERSION[];

// forward declarations to make the typedef below function properly.
class TransferRequest;
class TransferDaemon;

// The types of the callback handlers for each stage of the transfer request
// processing that the schedd has a say in.
typedef TreqAction 
	(Service::*TreqPrePushCallback)(TransferRequest*, TransferDaemon*);
typedef TreqAction
	(Service::*TreqPostPushCallback)(TransferRequest*, TransferDaemon*);
typedef TreqAction
	(Service::*TreqUpdateCallback)(TransferRequest*, TransferDaemon*, 
	ClassAd *update);
typedef TreqAction 
	(Service::*TreqReaperCallback)(TransferRequest*);

// This class is a delegation class the represents a particular request from
// anyone (usually the schedd) to transfer some files associated with a set of
// jobs. Later, someone will come by and whatever they say must match a
// previous request. 
class TransferRequest
{
	public:
		// I can initialize all of my internal variables via a classad with
		// a special schema.
		TransferRequest(ClassAd *ip); // assume ownership of pointer
		TransferRequest(); // init with empty classad
		~TransferRequest();

		/////////////////////////////////////////////////////////////////////
		// These functions describe information that will be serialized
		// to the transfer daemon. Included with this information in the
		// serialization are the internal job ads.
		/////////////////////////////////////////////////////////////////////

		// This transfer request is either an upload or a download */
		void set_direction(int dir);
		int get_direction(void);

		// What is the version string of the peer I'm talking to?
		// This could be the empty string if there is no version.
		// this will make a copy when you assign it to something.
		void set_peer_version(const std::string &pv);
		std::string get_peer_version(void);

		// what version is the info packet
		void set_protocol_version(int);
		int get_protocol_version(void);

		// what protocol does the file transfer client want to use?
		void set_xfer_protocol(int);
		int get_xfer_protocol(void);

		// See if this request had been gotten intially via a constraint
		// expression. This affects the protocol the transferd uses to speak
		// to the client.
		void set_used_constraint(bool con);
		bool get_used_constraint(void);

		// Should this request be handled Passively, Actively, or Active Shadow
		//void set_transfer_service(TreqMode mode);
		void set_transfer_service(const std::string &str);
		TreqMode get_transfer_service(void);

		// How many transfers am I going to process? Each transfer is on
		// behalf of a job
		void set_num_transfers(int);
		int get_num_transfers(void);

		/////////////////////////////////////////////////////////////////////
		// This deals with manipulating the payload of ads(tasks) to work on
		/////////////////////////////////////////////////////////////////////

		// stuff the array pf procids I got from the submit client into
		// here so the schedd knows what to do just before they get pushed
		// to the td.
		void set_procids(std::vector<PROC_ID> *jobs);
		std::vector<PROC_ID>* get_procids(void);

		// add a jobad to the transfer request, this accepts ownership
		// of the memory passed to it.
		void append_task(ClassAd *jobad);

		// return the todo list for processing. Kinda of a bad break of
		// encapsulation, but it makes iterating over this thing so much
		// easier.
		SimpleList<ClassAd *>* todo_tasks(void);

		/////////////////////////////////////////////////////////////////////
		// Sometimes I need to stash a client socket or capability
		/////////////////////////////////////////////////////////////////////

		void set_client_sock(ReliSock *rsock);
		ReliSock* get_client_sock(void);

		void set_capability(const std::string &capability);
		const std::string& get_capability(void);

		/////////////////////////////////////////////////////////////////////
		// Various kinds of status this request can be in 
		/////////////////////////////////////////////////////////////////////

		void set_rejected(bool val);
		bool get_rejected(void) const;

		void set_rejected_reason(const std::string &reason);
		const std::string& get_rejected_reason(void);

		/////////////////////////////////////////////////////////////////////
		// Callback at various processing points of this request so the 
		// schedd can do work.
		/////////////////////////////////////////////////////////////////////
		void set_pre_push_callback(std::string desc, 
			TreqPrePushCallback callback, Service *base);
		TreqAction call_pre_push_callback(TransferRequest *treq, 
			TransferDaemon *td);

		void set_post_push_callback(std::string desc,
			TreqPostPushCallback callback, Service *base);
		TreqAction call_post_push_callback(TransferRequest *treq, 
			TransferDaemon *td);

		void set_update_callback(std::string desc, 
			TreqUpdateCallback callback, Service *base);
		TreqAction call_update_callback(TransferRequest *treq, 
			TransferDaemon *td, ClassAd *update);

		void set_reaper_callback(std::string desc, 
			TreqReaperCallback callback, Service *base);
		TreqAction call_reaper_callback(TransferRequest *treq);

		/////////////////////////////////////////////////////////////////////
		// Utility functions
		/////////////////////////////////////////////////////////////////////

		// Dump the packet to specified debug level
		void dprintf(unsigned int lvl);

		// serialize the header information and jobads to this socket
		int put(Stream *sock);

	private:
		// Inspect the information packet during construction and verify the 
		// schemas I am prepared to handle.
		SchemaCheck check_schema(void);

		// this is the information packet this work packet has.
		ClassAd *m_ip;

		// Here is the list of jobads associated with this transfer request.
		SimpleList<ClassAd *> m_todo_ads;

		// Here is the original array of procids I got from the client
		std::vector<PROC_ID> *m_procids;

		// In the schedd's codebase, it needs to stash a client socket into
		// the request to deal with across callbacks.
		ReliSock *m_client_sock;

		// Allow the stashing of a capability a td gave for this request here.
		std::string m_cap;

		// If the transferd rejects this request, this is for the schedd
		// to record that fact.
		bool m_rejected;
		std::string m_rejected_reason;

		// the various callbacks
		std::string m_pre_push_func_desc;
		TreqPrePushCallback m_pre_push_func;
		Service *m_pre_push_func_this;

		std::string m_post_push_func_desc;
		TreqPostPushCallback m_post_push_func;
		Service *m_post_push_func_this;

		std::string m_update_func_desc;
		TreqUpdateCallback m_update_func;
		Service *m_update_func_this;

		std::string m_reaper_func_desc;
		TreqReaperCallback m_reaper_func;
		Service *m_reaper_func_this;
};

/* converts a protcol ASCII line to an enum which represents and encapsulation
	method for the protocol. */
EncapMethod encap_method(const std::string &line);

/* converts an ASCII representation of a transfer request mode into the enum */
TreqMode transfer_mode(std::string mode);
TreqMode transfer_mode(const char *mode);


#endif




