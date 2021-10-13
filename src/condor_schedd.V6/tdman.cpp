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
#include "condor_daemon_client.h"
#include "dc_transferd.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "qmgmt.h"
#include "condor_qmgr.h"
#include "scheduler.h"
#include "basename.h"

TransferDaemon::TransferDaemon(std::string fquser, std::string id, TDMode status) :
	m_treqs_in_progress(hashFunction)
{
	m_fquser = fquser;
	m_id = id;
	m_status = status;

	m_sinful = "";
	m_update_sock = NULL;
	m_treq_sock = NULL;

	m_reg_func_desc = "";
	m_reg_func = NULL;
	m_reg_func_this = NULL;

	m_reap_func_desc = "";
	m_reap_func = NULL;
	m_reap_func_this = NULL;
}

TransferDaemon::~TransferDaemon()
{
	// XXX TODO
}

void
TransferDaemon::clear(void)
{
	m_status = TD_PRE_INVOKED;
	m_sinful = "";
	m_update_sock = NULL;
	m_treq_sock = NULL;
}

void
TransferDaemon::set_fquser(std::string fquser)
{
	m_fquser = fquser;
}

std::string
TransferDaemon::get_fquser(void)
{
	return m_fquser;
}

void
TransferDaemon::set_id(std::string id)
{
	m_id = id;
}

std::string
TransferDaemon::get_id(void)
{
	return m_id;
}

void
TransferDaemon::set_status(TDMode tds)
{
	m_status = tds;
}

TDMode
TransferDaemon::get_status()
{
	return m_status;
}

void
TransferDaemon::set_schedd_sinful(std::string sinful)
{
	m_schedd_sinful = sinful;
}

std::string
TransferDaemon::get_schedd_sinful()
{
	return m_schedd_sinful;
}

void
TransferDaemon::set_sinful(const std::string &sinful)
{
	m_sinful = sinful;
}

const std::string &
TransferDaemon::get_sinful()
{
	return m_sinful;
}

void
TransferDaemon::set_update_sock(ReliSock *update_sock)
{
	m_update_sock = update_sock;
}

ReliSock*
TransferDaemon::get_update_sock(void)
{
	return m_update_sock;
}

void
TransferDaemon::set_treq_sock(ReliSock *treq_sock)
{
	m_treq_sock = treq_sock;
}

ReliSock*
TransferDaemon::get_treq_sock(void)
{
	return m_treq_sock;
}

bool
TransferDaemon::add_transfer_request(TransferRequest *treq)
{
	return m_treqs.Append(treq);
}

bool
TransferDaemon::push_transfer_requests(void)
{
	TransferRequest *treq = NULL;
	TreqAction ret;
	std::string capability;
	std::string rej_reason;
	char encap[] = "ENCAPSULATION_METHOD_OLD_CLASSADS\n";
	ClassAd respad;
	int invalid;

	dprintf(D_ALWAYS, 
		"Entering TransferDaemon::push_transfer_requests()\n");

	if (m_treq_sock == NULL) {
		EXCEPT("TransferDaemon::push_transfer_requests(): TD object was not "
				"up to have a real daemon backend prior to pushing requests");
	}

	// process all pending requests at once.
	m_treqs.Rewind();
	while(m_treqs.Next(treq))
	{
		/////////////////////////////////////////////////
		// Have the schedd do any last minute setup for this request 
		// before giving it to the transferd.
		/////////////////////////////////////////////////

		// Here the schedd must dig around in the treq for the procids
		// or whatever and then convert them to jobads which it puts
		// back into the treq.
		ret = treq->call_pre_push_callback(treq, this);

		switch(ret) {
			case TREQ_ACTION_CONTINUE:
				// the pre callback did whatever it wanted to, and now says to
				// continue the process of handling this request.
				break;

			case TREQ_ACTION_FORGET:
				// Remove this request from consideration by this transfer 
				// daemon.
				// It is assumed the pre callback took control of the memory
				// and had already deleted/saved it for later requeue/etc
				// and said something to the client about it.
				m_treqs.DeleteCurrent();

				// don't contact the transferd about this request, just go to
				// the next request instead.
				continue;

				break;

			case TREQ_ACTION_TERMINATE:
				// Remove this request from consideration by this transfer 
				// daemon and free the memory.
				m_treqs.DeleteCurrent();

				delete treq;

				// don't contact the transferd about this request, just go to
				// the next request instead.
				continue;

				break;

			default:
				EXCEPT("TransferDaemon::push_transfer_requests(): Programmer "
					"Error. Unknown pre TreqAction\n");
				break;
		}

		/////////////////////////////////////////////////
		// Send the request to the transferd
		/////////////////////////////////////////////////

		// Let's use the only encapsulation protocol we have at the moment.
		m_treq_sock->encode();
		if (!m_treq_sock->put(encap)) {
			dprintf(D_ALWAYS, "transferd hung up on us, not able to send transfer requests\n");
			return false;
		}
		m_treq_sock->end_of_message();

		// This only puts a small amount of the treq onto the channel. The
		// transferd doesn't need a lot of the info in the schedd's view of
		// this information.
		treq->put(m_treq_sock);
		m_treq_sock->end_of_message();

		/////////////////////////////////////////////////
		// Now the transferd will do work on the request, assigning it a 
		// capability, etc, etc, etc
		/////////////////////////////////////////////////

		m_treq_sock->decode();

		/////////////////////////////////////////////////
		// Get the classad back form the transferd which represents the
		// capability assigned to this classad and the file transfer protocols
		// with which the transferd is willing to speak to the submitter.
		// Also, there could be errors and what not in the ad, so the
		// post_push callback better inspect it and see if it likes what it
		// say.
		/////////////////////////////////////////////////

		// This response ad from the transferd about the request just give
		// to it will contain:
		//
		//	ATTR_TREQ_INVALID_REQUEST (set to true)
		//	ATTR_TREQ_INVALID_REASON
		//
		//	OR
		//
		//	ATTR_TREQ_INVALID_REQUEST (set to false)
		//	ATTR_TREQ_CAPABILITY
		//
		if (!getClassAd(m_treq_sock, respad)) {
			dprintf(D_ALWAYS, "Transferd could not get response ad from client\n");
			return false;
		}
		m_treq_sock->end_of_message();

		/////////////////////////////////////////////////
		// Fix up the treq with what the transferd said.
		/////////////////////////////////////////////////

		respad.LookupInteger(ATTR_TREQ_INVALID_REQUEST, invalid);
		if (invalid == FALSE) {
			dprintf(D_ALWAYS, "Transferd said request was valid.\n");

			respad.LookupString(ATTR_TREQ_CAPABILITY, capability);
			treq->set_capability(capability);

			// move it from the original enqueue list to the in progress hash
			// keyed by its capability
			m_treqs.DeleteCurrent();
			ASSERT(m_treqs_in_progress.insert(treq->get_capability(), treq) == 0);

		} else {
			dprintf(D_ALWAYS, "Transferd said request was invalid.\n");

			// record into the treq that the transferd rejected it.
			treq->set_rejected(true);
			respad.LookupString(ATTR_TREQ_INVALID_REASON, rej_reason);
			treq->set_rejected_reason(rej_reason);
		}

		/////////////////////////////////////////////////
		// Here the schedd will take the capability and tell the waiting
		// clients the answer and whatever else it wants to do.
		// WARNING: The schedd may decide to get rid of this request if the
		// transferd rejected it.
		/////////////////////////////////////////////////

		ret = treq->call_post_push_callback(treq, this);

		// XXX If the transferd rejected the request, I could be a little more
		// intelligent about how to handle a termination/forget from the 
		// callback.... The protocol is that if the transferd rejected the
		// treq, then the transferd doesn't have a record of it.

		switch(ret) {
			case TREQ_ACTION_CONTINUE:
				// the post callback did whatever it wanted to, and now says to
				// continue the process of handling this request.
				break;

			case TREQ_ACTION_FORGET:
				// It is assumed the post callback took control of the memory
				// and had already deleted/saved it for later requeue/etc
				// and said something to the client about it.
				m_treqs.DeleteCurrent();

				// XXX This is a complicated action to implement, since it
				// means we have to contact the transferd and tell it to
				// abort the request.

				EXCEPT("TransferDaemon::push_transfer_requests(): NOT "
					"IMPLEMENTED: aborting a treq after the push to the "
					"transferd. To implement this functionality, you must "
					"contact the transferd here and have it also abort "
					"the request.");

				break;

			case TREQ_ACTION_TERMINATE:
				// The callback has finished with this memory and we may delete
				// it.
				m_treqs.DeleteCurrent();
				delete treq;

				// XXX This is a complicated action to implement, since it
				// means we have to contact the transferd and tell it to
				// abort the request.

				EXCEPT("TransferDaemon::push_transfer_requests(): NOT "
					"IMPLEMENTED: aborting a treq after the push to the "
					"transferd. To implement this functionality, you must "
					"contact the transferd here and have it also abort "
					"the request.");

				break;

			default:
				EXCEPT("TransferDaemon::push_transfer_requests(): Programmer "
					"Error. Unknown post TreqAction\n");
				break;
		}
	}

	dprintf(D_ALWAYS,
		"Leaving TransferDaemon::push_transfer_requests()\n");

	return true;
}

bool
TransferDaemon::update_transfer_request(ClassAd *update)
{
	std::string cap;
	TransferRequest *treq = NULL;
	int ret;
	TransferDaemon *td = NULL;

	dprintf(D_ALWAYS,
		"Entering TransferDaemon::update_transfer_request()\n");
	
	////////////////////////////////////////////////////////////////////////
	// The update ad contains the capability for a treq, so let's find the
	// treq.
	////////////////////////////////////////////////////////////////////////
	update->LookupString(ATTR_TREQ_CAPABILITY, cap);

	// If the post push callback the schedd terminated the treq,
	// we can still get an update from the transferd about it if the transferd
	// hadn't been informed of the termination of the request.
	// So here we'll just log this fact, but ignore the update, since it is 
	// for a nonexistant treq as far as the schedd is concerned.
	if (m_treqs_in_progress.lookup(cap, treq) != 0) {
		dprintf(D_ALWAYS, "TransferDaemon::update_transfer_request(): "
			"Got update for treq that the schedd had already thrown away, "
			"ignoring.\n");
		return TRUE;
	}

	// let the schedd look at the update and figure out if it wants to do
	// anything with it.
	dprintf(D_ALWAYS, "TransferDaemon::update_transfer_request(): "
		"Calling schedd update callback\n");

	ret = treq->call_update_callback(treq, td, update);

	switch(ret) {
		case TREQ_ACTION_CONTINUE:
			// Don't delete the request from our in progress table, and assume
			// there will be more updates for the schedd's update handler
			// to process. In effect, do nothing and wait.
			break;

		case TREQ_ACTION_FORGET:
			// It is assumed the update callback took control of the memory
			// and had already deleted/saved it for later requeue/etc.
			// For our purposes, we remove it from consideration from our table
			// which in effect means it is removed from the transfer daemons
			// responsibility.
			ASSERT(m_treqs_in_progress.remove(cap) == 0);

			break;

		case TREQ_ACTION_TERMINATE:
			// the callback is done with this task and its memory, so we get 
			// rid of it.
			ASSERT(m_treqs_in_progress.remove(cap) == 0);

			delete treq;

			break;

		default:
			EXCEPT("TransferDaemon::update_transfer_request(): Programmer "
				"Error. Unknown update TreqAction\n");
			break;
	}

	dprintf(D_ALWAYS,
		"Leaving TransferDaemon::update_transfer_request()\n");
	
	return TRUE;
}

void
TransferDaemon::reap_all_transfer_requests(void)
{
	TransferRequest *treq = NULL;
	TreqAction ret;
	std::string key;

	////////////////////////////////////////////////////////////////////////
	// For each transfer request, call the associated reaper for it.
	// Also call the reaper for ones that were in progress, because we'll
	// assume they are incomplete and should be assumed to have not been tried
	// at all. 
	////////////////////////////////////////////////////////////////////////

	// invoke the reapers for requests
	m_treqs.Rewind();
	while(m_treqs.Next(treq))
	{
		m_treqs.DeleteCurrent();
		
		ret = treq->call_reaper_callback(treq);

		// for now, we only support this return call from the callback
		ASSERT(ret == TREQ_ACTION_TERMINATE);

		delete treq;
	}

	// invoke all of the reapers for stuff the transferd was in the middle
	// of doing. 
	m_treqs_in_progress.startIterations();
	while(m_treqs_in_progress.iterate(key, treq))
	{
		m_treqs_in_progress.remove(key);

		ret = treq->call_reaper_callback(treq);

		// for now, we only support this return call from the callback
		ASSERT(ret == TREQ_ACTION_TERMINATE);

		delete treq;
	}
}

void 
TransferDaemon::set_reg_callback(std::string desc, TDRegisterCallback callback, 
	Service *base)
{
	m_reg_func_desc = desc;
	m_reg_func = callback;
	m_reg_func_this = base;
}

void 
TransferDaemon::get_reg_callback(std::string &desc, TDRegisterCallback &callback, 
	Service *&base)
{
	desc = m_reg_func_desc;
	callback = m_reg_func;
	base = m_reg_func_this;
}

TdAction 
TransferDaemon::call_reg_callback(TransferDaemon *td)
{
	// if no regfunc had been set up, then just return the continue status
	// signifying that the creator of the td object didn't want to care if
	// the td had registered itself

	if (m_reg_func == NULL) {
		return TD_ACTION_CONTINUE;
	}

	return (m_reg_func_this->*(m_reg_func))(td);
}

void 
TransferDaemon::set_reaper_callback(std::string desc, TDReaperCallback callback,
	Service *base)
{
	m_reap_func_desc = desc;
	m_reap_func = callback;
	m_reap_func_this = base;
}

void 
TransferDaemon::get_reaper_callback(std::string &desc, 
	TDReaperCallback &callback, Service *&base)
{
	desc = m_reap_func_desc;
	callback = m_reap_func;
	base = m_reap_func_this;
}

TdAction 
TransferDaemon::call_reaper_callback(long pid, int status, TransferDaemon *td)
{
	// if no reaper had been set up, then just return the termination status
	// signifying that the creator of the td object didn't want to care if
	// the td went away.

	if (m_reap_func == NULL) {
		return TD_ACTION_TERMINATE;
	}

	return (m_reap_func_this->*(m_reap_func))(pid, status, td);
}


////////////////////////////////////////////////////////////////////////////
// The transfer daemon manager class
////////////////////////////////////////////////////////////////////////////

TDMan::TDMan()
{
	m_td_table = 
		new HashTable<std::string, TransferDaemon*>(hashFunction);
	m_id_table = 
		new HashTable<std::string, std::string>(hashFunction);
	m_td_pid_table = 
		new HashTable<long, TransferDaemon*>(hashFuncLong);
}

TDMan::~TDMan()
{
	/* XXX fix up to go through them and delete everything inside of them. */
	delete m_td_table;
	delete m_id_table;
	delete m_td_pid_table;
}

void TDMan::register_handlers(void)
{
	 daemonCore->Register_Command(TRANSFERD_REGISTER,
			"TRANSFERD_REGISTER",
			(CommandHandlercpp)&TDMan::transferd_registration,
			"transferd_registration", this, WRITE);
}

TransferDaemon*
TDMan::find_td_by_user(std::string fquser)
{
	int ret;
	TransferDaemon *td = NULL;

	ret = m_td_table->lookup(fquser, td);

	if (ret != 0) {
		// not found
		return NULL;
	}

	return td;
}

TransferDaemon*
TDMan::find_td_by_ident(std::string id)
{
	int ret;
	std::string fquser;
	TransferDaemon *td = NULL;

	// look up the fquser associated with this id
	ret = m_id_table->lookup(id, fquser);
	if (ret != 0) {
		// not found
		return NULL;
	}

	// look up the transferd for that user.
	ret = m_td_table->lookup(fquser, td);
	if (ret != 0) {
		// not found
		return NULL;
	}

	return td;
}


bool
TDMan::invoke_a_td(TransferDaemon *td)
{
	TransferDaemon *found_td = NULL;
	std::string found_id;
	ArgList args;
	char *td_path = NULL;
	pid_t pid;
	std::string args_display;
	static int rid = -1;

	ASSERT(td != NULL);

	// I might be asked to reinvoke something that may have died
	// previously, but I better not invoke one already registered.

	switch(td->get_status()) {
		case TD_PRE_INVOKED:
			// all good
			break;
		case TD_INVOKED:
			// daemon died after invocation, but before registering.
			break;
		case TD_REGISTERED:
			// disallow reinvocation due to daemon being considered alive.
			return false;
			break;
		case TD_MIA:
			// XXX do I need this?
			break;
			
	}

	//////////////////////////////////////////////////////////////////////
	// Store this object into the internal tables

	// I might be calling this again with the same td, so don't insert it
	// twice into the tables...

	// store it into the general activity table
	if (m_td_table->lookup(td->get_fquser(), found_td) != 0) {
		m_td_table->insert(td->get_fquser(), td);
	}

	// build the association with the id
	if (m_id_table->lookup(td->get_id(), found_id) != 0) {
		m_id_table->insert(td->get_id(), td->get_fquser());
	}


	//////////////////////////////////////////////////////////////////////
	// What executable am I going to be running?

	td_path = param("TRANSFERD");
	if (td_path == NULL) {
		EXCEPT("TRANSFERD must be defined in the config file!");
	}

	//////////////////////////////////////////////////////////////////////
	// What is the argument list for the transferd?

	// what should my argv[0] be?
	args.AppendArg(condor_basename(td_path));

	// This is a daemoncore process
	args.AppendArg("-f");

	// report back to this schedd
	args.AppendArg("--schedd");
	args.AppendArg(daemonCore->InfoCommandSinfulString());

	// what id does it have?
	args.AppendArg("--id");
	args.AppendArg(td->get_id());

	// give it a timeout of 20 minutes (hard coded for now...)
	args.AppendArg("--timeout");
	args.AppendArg(20*60);

	//////////////////////////////////////////////////////////////////////
	// Which reaper should handle the exiting of this program

	// set up a reaper to handle the exiting of this daemon
	if (rid == -1) {
		rid = daemonCore->Register_Reaper("transferd_reaper",
			(ReaperHandlercpp)&TDMan::transferd_reaper,
			"transferd_reaper",
			this);
	}

	//////////////////////////////////////////////////////////////////////
	// Create the process

	args.GetArgsStringForDisplay(args_display);
	dprintf(D_ALWAYS, "Invoking for user '%s' a transferd: %s\n", 
		td->get_fquser().c_str(),
		args_display.c_str());

	// XXX needs to be the user, not root!
	pid = daemonCore->Create_Process( td_path, args, PRIV_ROOT, rid );
	dprintf(D_ALWAYS, "Created transferd with pid %d\n", pid);

	if (pid == FALSE) {
		// XXX TODO
		EXCEPT("failed to create transferd process.");
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////
	// Perform associations

	// consistancy check. Pid must not be in the table.
	ASSERT(m_td_pid_table->lookup((long)pid, found_td) != 0);

	// insert into the pid table for TDMann reaper to use...
	m_td_pid_table->insert((long)pid, td);
	
	free(td_path);

	td->set_status(TD_INVOKED);

	return TRUE;
}

void
TDMan::refuse(Stream *s)
{
	s->encode();
	s->put( NOT_OK );
	s->end_of_message();
}

// the reaper for when a transferd goes away or dies.
int
TDMan::transferd_reaper(int pid, int status) 
{
	std::string fquser;
	std::string id;
	TransferDaemon *dead_td = NULL;
	TdAction ret;

	dprintf(D_ALWAYS, "TDMan: Reaped transferd pid %d with status %d\n", 
		pid, status);

	//////////////////////////////////////////////////////////////////////
	// Remove the td object from the tdmanager object. We do this right here
	// so that if a transfer request reaper wants to invoke another one, it
	// won't find this one which is already dead.
	//////////////////////////////////////////////////////////////////////

	// remove the alias from the pid table
	m_td_pid_table->lookup((long)pid, dead_td);
	ASSERT(dead_td != NULL);
	ASSERT(m_td_pid_table->remove((long)pid) == 0);

	id = dead_td->get_id();
	fquser = dead_td->get_fquser();

	// remove the association entry from the id table
	ASSERT(m_id_table->remove(id) == 0);

	// remove the td object from the fquser storage table
	ASSERT(m_td_table->remove(fquser) == 0);

	//////////////////////////////////////////////////////////////////////
	// Pass this object to the schedd td reaper function so it can inspect
	// it if it'd like. However, it must not delete the pointer.
	//////////////////////////////////////////////////////////////////////

	ret = dead_td->call_reaper_callback(pid, status, dead_td);
	switch(ret) {
		case TD_ACTION_TERMINATE:
			/* What I'm expecting in most cases */
			delete dead_td;
			break;

		case TD_ACTION_CONTINUE:
			EXCEPT("TDMan::transferd_reaper(): Programmer Error! You must "
				"return TD_ACTION_TERMINATE only from a transferd reaper!");
			break;

		default:
			EXCEPT("TDMan::transferd_reaper(): Programmer Error! "
				"Unknown return code from td reaper callback");
			break;
	}
	
	return TRUE;
}


// The handler for TRANSFERD_REGISTER.
// This handler checks to make sure this is a valid transferd. If valid,
// the schedd may then shove over a transfer request to the transferd on the
// open relisock. The relisock is kept open the life of the transferd, and
// the schedd sends new transfer requests whenever it wants to.
int 
TDMan::transferd_registration(int cmd, Stream *sock)
{
	ReliSock *rsock = (ReliSock*)sock;
	std::string td_sinful;
	std::string td_id;
	std::string fquser;
	TDUpdateContinuation *tdup = NULL;
	TransferDaemon *td = NULL;
	TdAction ret;
	ClassAd regad;
	ClassAd respad;

	(void)cmd; // quiet the compiler

	dprintf(D_ALWAYS, "Entering TDMan::transferd_registration()\n");

	dprintf(D_ALWAYS, "Got TRANSFERD_REGISTER message!\n");

	rsock->decode();

	///////////////////////////////////////////////////////////////
	// make sure we are authenticated
	///////////////////////////////////////////////////////////////
	if( ! rsock->triedAuthentication() ) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(rsock, WRITE, &errstack) ) {
				// we failed to authenticate, we should bail out now
				// since we don't know what user is trying to perform
				// this action.
				// TODO: it'd be nice to print out what failed, but we
				// need better error propagation for that...
			errstack.push( "SCHEDD::TDMan", 42,
					"Failure to register transferd - Authentication failed" );
			dprintf( D_ALWAYS, "transferd_registration() aborting: %s\n",
					 errstack.getFullText().c_str() );
			refuse( rsock );
			dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
			return CLOSE_STREAM;
		}
	}	

	///////////////////////////////////////////////////////////////
	// Figure out who this user is.
	///////////////////////////////////////////////////////////////

	fquser = rsock->getFullyQualifiedUser();
	if (fquser == "") {
		dprintf(D_ALWAYS, "Transferd identity is unverifiable. Denied.\n");
		refuse(rsock);
		dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
		return CLOSE_STREAM;
	}

	///////////////////////////////////////////////////////////////
	// Decode the registration message from the transferd
	///////////////////////////////////////////////////////////////

	rsock->decode();

	// This is the initial registration ad from the transferd:
	//	ATTR_TD_SINFUL
	//	ATTR_TD_ID
	if (!getClassAd(rsock, regad)) {
		dprintf(D_ALWAYS, "Remote side hung up on transferd\n");
		return CLOSE_STREAM;
	}
	rsock->end_of_message();
	regad.LookupString(ATTR_TREQ_TD_SINFUL, td_sinful);
	regad.LookupString(ATTR_TREQ_TD_ID, td_id);

	dprintf(D_ALWAYS, "Transferd %s, id: %s, owned by '%s' "
		"attempting to register!\n",
		td_sinful.c_str(), td_id.c_str(), fquser.c_str());

	///////////////////////////////////////////////////////////////
	// Determine if I requested a transferd for this identity. Close if not.
	///////////////////////////////////////////////////////////////

	rsock->encode();

	// See if I have a transferd by that id
	td = find_td_by_ident(td_id);
	if (td == NULL) {
		// guess not, refuse it
		dprintf(D_ALWAYS, 
			"Did not request a transferd with that id for any user. "
			"Refuse.\n");

		respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
		respad.Assign(ATTR_TREQ_INVALID_REASON, 
			"Did not request a transferd with that id for any user.");
		putClassAd(rsock, respad);
		rsock->end_of_message();

		dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
		return CLOSE_STREAM;
	}

	// then see if the user of that td jives with how it authenticated
	if (td->get_fquser() != fquser) {
		// guess not, refuse it
		dprintf(D_ALWAYS, 
			"Did not request a transferd with that id for this specific user. "
			"Refuse.\n");

		respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
		respad.Assign(ATTR_TREQ_INVALID_REASON, 
			"Did not request a transferd with that id for this specific user.");
		putClassAd(rsock, respad);
		rsock->end_of_message();

		dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
		return CLOSE_STREAM;
	}

	// is it in the TD_INVOKED state?
	if (td->get_status() != TD_INVOKED) {
		// guess not, refuse it

		dprintf(D_ALWAYS, 
			"Transferd for user not in TD_PRE_INVOKED state. Refuse.\n");

		respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
		respad.Assign(ATTR_TREQ_INVALID_REASON, 
			"Transferd for user not in TD_PRE_INVOKED state.\n");
		putClassAd(rsock, respad);
		rsock->end_of_message();

		dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
		return CLOSE_STREAM;
	}

	// send back a good reply if all the above passed
	respad.Assign(ATTR_TREQ_INVALID_REQUEST, FALSE);
	putClassAd(rsock, respad);
	rsock->end_of_message();

	///////////////////////////////////////////////////////////////
	// Set up some parameters in the TD object which represent
	// the backend of this object (which is a true daemon)
	///////////////////////////////////////////////////////////////

	td->set_status(TD_REGISTERED);
	td->set_sinful(td_sinful);
	td->set_update_sock(rsock);

	///////////////////////////////////////////////////////////////
	// Call back to the transferd so we have two asynch sockets, one for
	// new transfer requests to go down, the other for updates to come back.
	///////////////////////////////////////////////////////////////

	// WARNING WARNING WARNING //
	// WARNING WARNING WARNING //
	// WARNING WARNING WARNING //
	// WARNING WARNING WARNING //

	// At this point, we connect back to the transferd, so this means that in 
	// the protocol, the registration function in the transferd must now have
	// ended and the transferd has gone back to daemonCore. Otherwise, 
	// deadlock. We stash the socket which we will be sending transfer
	// requests down into the td object.

	dprintf(D_ALWAYS, "Registration is valid...\n");
	dprintf(D_ALWAYS, "Creating TransferRequest channel to transferd.\n");
	CondorError errstack;
	ReliSock *td_req_sock = NULL;
	DCTransferD dctd(td_sinful.c_str());

	// XXX This connect in here should be non-blocking....
	if (dctd.setup_treq_channel(&td_req_sock, 20, &errstack) == false) {
		dprintf(D_ALWAYS, "Refusing errant transferd.\n");
		refuse(rsock);
		dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
		return CLOSE_STREAM;
	}

	// WARNING WARNING WARNING //
	// WARNING WARNING WARNING //
	// WARNING WARNING WARNING //
	// WARNING WARNING WARNING //
	// Transferd must have gone back to daemoncore at this point in the
	// protocol.

	td->set_treq_sock(td_req_sock);
	dprintf(D_ALWAYS, 
		"TransferRequest channel created. Transferd appears stable.\n");

	///////////////////////////////////////////////////////////////
	// Register a call back socket for future updates from this transferd
	///////////////////////////////////////////////////////////////

	// Now, let's give a good name for the update socket.
	std::string sock_id;
	sock_id += "<Update-Socket-For-TD-";
	sock_id += td_sinful;
	sock_id += "-";
	sock_id += fquser;
	sock_id += "-";
	sock_id += td_id;
	sock_id += ">";

	daemonCore->Register_Socket(sock, sock_id.c_str(),
		(SocketHandlercpp)&TDMan::transferd_update,
		"TDMan::transferd_update", this, ALLOW);
	
	// stash an identifier with the registered socket so I can find this
	// transferd later when this socket gets an update. I can't just shove
	// the transferd pointer here since it might have been removed by other
	// handlers if they determined the daemon went away. Instead I'll push an 
	// identifier so I can see if it still exists in the pool before messing
	// with it.
	tdup = new TDUpdateContinuation(td_sinful, fquser, td_id, sock_id);
	ASSERT(tdup);

	// set up the continuation for TDMan::transferd_update()
	daemonCore->Register_DataPtr(tdup);

	///////////////////////////////////////////////////////////////
	// If any registration callback exists, then call it so the user of
	// the factory object knows that the registration happened.
	// This callback could send stuff down the transferd control channel
	// if it so desired.
	///////////////////////////////////////////////////////////////

	ret = td->call_reg_callback(td);
	switch(ret) {
		case TD_ACTION_CONTINUE:
			/* good, this is what I expect */
			break;

		case TD_ACTION_TERMINATE:
			EXCEPT("TDMan::transferd_registration() Programmer Error! "
				"Termination of transferd in registration callback not "
				"implemented.");
			break;

		default:
			EXCEPT("TDMan::transferd_registration() Programmer Error! "
				"Unknown return code from the registration callback!");
			break;
	}

	///////////////////////////////////////////////////////////////
	// Push any pending requests for this transfer daemon.
	// This has the potential to call callback into the schedd.
	// We do this after the registration callback has happened just
	// in case the registration callback wanted to push some more 
	// transfer requests into this transferd and we can batch them with
	// whatever else is in the transfer request queue.
	///////////////////////////////////////////////////////////////

	dprintf(D_ALWAYS, 
		"TDMan::transferd_registration() pushing queued requests\n");

	td->push_transfer_requests();

	dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
	return KEEP_STREAM;
}


// When a transferd finishes sending some files, it informs the schedd when the
// transfer was correctly sent. NOTE: Maybe add when the transferd thinks there
// are problems, like files not found and stuff like that.
int 
TDMan::transferd_update(Stream *sock)
{
	ReliSock *rsock = (ReliSock*)sock;
	TDUpdateContinuation *tdup = NULL;
	ClassAd update;
	std::string cap;
	std::string status;
	std::string reason;
	TransferDaemon *td = NULL;

	/////////////////////////////////////////////////////////////////////////
	// Grab the continuation from the registration handler
	/////////////////////////////////////////////////////////////////////////
	
	// We don't delete this pointer until we know this socket is closed...
	// This allows us to recycle it across many updates from the same td.
	tdup = (TDUpdateContinuation*)daemonCore->GetDataPtr();
	ASSERT(tdup != NULL);

	dprintf(D_ALWAYS, "Transferd update from: addr(%s) fquser(%s) id(%s)\n", 
		tdup->sinful.c_str(), tdup->fquser.c_str(), tdup->id.c_str());

	/////////////////////////////////////////////////////////////////////////
	// Get the resultant status classad from the transferd
	/////////////////////////////////////////////////////////////////////////

	// grab the classad from the transferd
	if (getClassAd(rsock, update) == false) {
		// Hmm, couldn't get the update, clean up shop.
		dprintf(D_ALWAYS, "Update socket was closed. "
			"Transferd for user: %s with id: %s at location: %s will soon be "
			"reaped.\n", 
			tdup->fquser.c_str(), tdup->id.c_str(), tdup->sinful.c_str());

		delete tdup;
		daemonCore->SetDataPtr(NULL);
		return CLOSE_STREAM;
	}
	rsock->end_of_message();

	update.LookupString(ATTR_TREQ_CAPABILITY, cap);
	update.LookupString(ATTR_TREQ_UPDATE_STATUS, status);
	update.LookupString(ATTR_TREQ_UPDATE_REASON, reason);

	dprintf(D_ALWAYS, "Update was: cap = %s, status = %s,  reason = %s\n",
		cap.c_str(), status.c_str(), reason.c_str());

	/////////////////////////////////////////////////////////////////////////
	// Find the matching transfer daemon with the id in question.
	/////////////////////////////////////////////////////////////////////////

	td = find_td_by_ident(tdup->id);
	// XXX This is a little strange. It would mean that somehow the 
	// td object got deleted by someone, but we didn't deal with the fact
	// we could still get updates for it. I'd consider this a consistancy
	// check that we have a valid td object when a daemon is in existence
	// and it better be true.
	ASSERT(td != NULL);

	/////////////////////////////////////////////////////////////////////////
	// Pass the update ad directly to the transfer daemon and let it deal
	// with it locally by calling the right schedd callbacks and such.
	/////////////////////////////////////////////////////////////////////////

	td->update_transfer_request(&update);

	/////////////////////////////////////////////////////////////////////////
	// Keep the stream for the next update
	/////////////////////////////////////////////////////////////////////////

	return KEEP_STREAM;
}
