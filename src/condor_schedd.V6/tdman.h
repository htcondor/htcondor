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

#ifndef _CONDOR_TDMAN_H_
#define _CONDOR_TDMAN_H_

#include "condor_daemon_core.h"
#include "../condor_transferd/condor_td.h"
#include "HashTable.h"

// used for the registration and reaping of a transferd
enum TdAction {
	TD_ACTION_CONTINUE,		/* everything is ok, keep going */
	TD_ACTION_TERMINATE		/* stop the transferd and remove the object */
};

// After a transferd registers successfully, this will be called back.
typedef TdAction (Service::*TDRegisterCallback)(TransferDaemon *td);

// When a transferd dies, this will be called back
typedef TdAction (Service::*TDReaperCallback)(long pid, int status, 
	TransferDaemon *td);

// This holds the status for a particular transferd
enum TDMode {
	// The representative object of the transfer daemon has been made, but 
	// the work to invoke it hasn't been completed yet.
	TD_PRE_INVOKED,
	// the transferd has been invoked, but hasn't registered
	TD_INVOKED,
	// the transferd has been registered and is available for use
	TD_REGISTERED,
	// someone has come back to the schedd and said the registered transferd
	// is not connectable or wasn't there, or whatever.
	TD_MIA
};

// smart structure;
// identification of a transferd for continuation purposes across registered
// callback funtions.
class TDUpdateContinuation
{
	public:
		TDUpdateContinuation(std::string s, std::string f, std::string i, std::string n)
		{
			sinful = s;
			fquser = f;
			id = i;
			name = n;
		}

		// sinful string of the td
		std::string sinful;
		// fully qualified user the td is running as
		std::string fquser;
		// the id string the schedd gave to the ransferd
		std::string id;
		// the registration name for the socket handler
		std::string name;
};

// This represents the invocation, and current status, of a transfer daemon
class TransferDaemon 
{
	public:
		TransferDaemon(std::string fquser, std::string id, TDMode status);
		~TransferDaemon();

		// A handler that gets called when this transferd registers itself
		// properly.
		void set_reg_callback(std::string desc, TDRegisterCallback callback, 
			Service *base);
		void get_reg_callback(std::string &desc, TDRegisterCallback &callback,
			Service *&base);
		TdAction call_reg_callback(TransferDaemon *td);

		// If the transfer daemon has died, then this callback gets invoked
		// so the schedd can clean things up a bit.
		void set_reaper_callback(std::string desc, TDReaperCallback callback,
			Service *base);
		void get_reaper_callback(std::string &desc, TDReaperCallback &callback,
			Service *&base);
		TdAction call_reaper_callback(long pid, int status, TransferDaemon *td);

		// This transferd had been started on behalf of a fully qualified user
		// This records who that user is.
		void set_fquser(std::string fquser);
		std::string get_fquser(void);

		// Set the specific ID string associated with this td.
		void set_id(std::string id);
		std::string get_id(void);

		// The schedd changes the state of this object according to what it
		// knows about the status of the daemon itself.
		void set_status(TDMode s);
		TDMode get_status(void);

		// To whom should this td report?
		void set_schedd_sinful(std::string sinful);
		std::string get_schedd_sinful(void);

		// Who is this transferd (after it registers)
		void set_sinful(const std::string &sinful);
		const std::string& get_sinful(void);

		// The socket the schedd uses to listen to updates from the td.
		// This is also what was the original registration socket.
		void set_update_sock(ReliSock *update_sock);
		ReliSock* get_update_sock(void);

		// The socket one must use to send to a new transfer request to
		// this daemon.
		void set_treq_sock(ReliSock *treq_sock);
		ReliSock* get_treq_sock(void);

		// If I happen to have a transfer request when making this object,
		// store them here until the transferd registers and I can deal 
		// with it then. This object assumes ownership of the memory unless
		// false is returned.
		bool add_transfer_request(TransferRequest *treq);

		// If there are any pending requests, do them, and respond to the
		// client paired with those requests.
		bool push_transfer_requests(void);

		// When a transferd daemon produces an update, the manager will 
		// give it to the td object for it to do what it will with it.
		// This function does NOT own the memory passed to it.
		bool update_transfer_request(ClassAd *update);

		// The schedd's transfer request reaper is given control of the memory
		// of each transfer request as it is being reaped by the schedd.
		void reap_all_transfer_requests(void);

		// If I want to restart a real transferd associated with this object,
		// then clear out the parts that represent a living daemon.
		void clear(void);

	private:
		// The sinful string of this transferd after registration
		std::string m_sinful;

		// the owner of the transferd
		std::string m_fquser;

		// The id string associated with this td
		std::string m_id;

		// The schedd to which this td must report
		std::string m_schedd_sinful;

		// current status about this transferd I requested
		TDMode m_status;

		// Storage of Transfer Requests when first enqueued
		SimpleList<TransferRequest*> m_treqs;

		// Storage of Transfer Requests when transferd is doing its work
		// Key: capability, Value: TransferRequest
		HashTable<std::string, TransferRequest*> m_treqs_in_progress;

		// The registration socket that the schedd also receives updates on.
		ReliSock *m_update_sock; 

		// The socket the schedd initiated to send treqs to the td
		ReliSock *m_treq_sock;

		// When the tranferd wakes up and registers, call this when the
		// registration process in complete
		std::string m_reg_func_desc;
		TDRegisterCallback m_reg_func;
		Service *m_reg_func_this;

		// If the transferd dies, invoke this callback with its identification
		// and how it died.
		std::string m_reap_func_desc;
		TDReaperCallback m_reap_func;
		Service *m_reap_func_this;
};

class TDMan : public Service
{
	public:
		TDMan();
		~TDMan();

		// returns NULL if no td is currently available. Returns the
		// transfer daemon object invoked for this user. If no such transferd
		// exists, then return NULL;
		TransferDaemon* find_td_by_user(std::string fquser);

		// when the td registers itself, figure out to which of my objects its
		// identity string pairs.
		TransferDaemon* find_td_by_ident(std::string id);

		// I've determined that I have to create a transfer daemon, so have the
		// caller set up a TransferDaemon object, and then I'll be responsible
		// for starting it.
		// The caller has specified the fquser and id in the object.
		// This function will dig around in the td object and fire up a 
		// td according to what it finds.
		// Returns true if everything went ok, false if the transferd could
		// not be started.
		bool invoke_a_td(TransferDaemon *td);

		// install some daemon core handlers to deal with transferd's wanting
		// to talk to me.
		void register_handlers(void);

		// what to do when a td dies or exits
		// we implicitly required that sizeof(int) == sizeof(long) here.
		// int transferd_reaper(long pid, int status);
		int transferd_reaper(int pid, int status);

		// deal with a td that comes back to register itself to me.
		int transferd_registration(int cmd, Stream *sock);

		// handle updates from a transferd
		int transferd_update(Stream *sock);

		// same thing like in Scheduler object.
		void refuse(Stream *s);

	private:
		// This is where I store the table of transferd objects, each
		// representing the status and connection to a transferd.
		// Key: fquser, Value: transferd
		HashTable<std::string, TransferDaemon*> *m_td_table;

		// Store an association between an id given to a specific transferd
		// and the user that id ultimately identifies.
		// Key: id, Value: fquser
		HashTable<std::string, std::string> *m_id_table;

		// a table of pids associated with running transferds so reapers
		// can do their work, among other things. This is a hash table
		// of alias pointers into m_td_table.
		// Key: pid of td process, Value: alias transferd
		HashTable<long, TransferDaemon*> *m_td_pid_table;
	
	// NOTE: When we get around to implementing multiple tds per user with
	// different id strings, then find_any_td must return a list of tds and
	// the hash tables value must be that corresponding list.
};

#endif /* _CONDOR_TDMAN_H_ */








