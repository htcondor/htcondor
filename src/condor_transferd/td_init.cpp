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
#include "list.h"
#include "condor_classad.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "condor_ftp.h"
#include "condor_attributes.h"

static void usage(void);

TransferD::TransferD() :
	m_treqs(hashFunction),
	m_client_to_transferd_threads(hashFuncLong),
	m_transferd_to_client_threads(hashFuncLong)
{
	m_initialized = FALSE;
	m_update_sock = NULL;

	// start the timer right now, in case I get started and noone tells
	// me anything for the timeout amount of time.
	m_inactivity_timer = time(NULL);
}

TransferD::~TransferD()
{	
	std::string key;
	TransferRequest *treq;

	// delete everything in the table.
	if (m_initialized == TRUE) {
		m_treqs.startIterations();
		while(m_treqs.iterate(treq)) {
			m_treqs.getCurrentKey(key);
			m_treqs.remove(key);
			delete treq;
			treq = NULL;
		}
	}
}


/* I was told on the command line who my schedd is, so contact them
	and tell them I'm alive and ready for work. */
void
TransferD::init(int argc, char *argv[])
{
	RegisterResult ret;
	int i;
	ReliSock *usock = NULL;
	unsigned long tout;

	// XXX no error checking, assume arguments are correctly formatted.

	for (i = 1; i < argc; i++) {
		// if --schedd is given, take the sinful string of the schedd.
		if (strcmp(argv[i], "--schedd") == MATCH) {
			if (i+1 < argc) {
				g_td.m_features.set_schedd_sinful(argv[i+1]);
				i++;
			}
		}

		// if --timeout is given, take the number of seconds to timeout when
		// the transfer queue is empty. A timeout means the transferd will exit.
		if (strcmp(argv[i], "--timeout") == MATCH) {
			if (i+1 < argc) {
				tout = strtoul(argv[i+1], NULL, 10);
				g_td.m_features.set_timeout(tout);
				i++;
			}
		}

		// if --stdin is specified, then there will be a TransferRequest 
		// supplied via stdin, otherwise, get it from the schedd.
		if (strcmp(argv[i], "--stdin") == MATCH) {
			g_td.m_features.set_uses_stdin(TRUE);
		}

		// if --id is specified, then there will be an ascii identification
		// string that the schedd will use to match this transferd with its
		// request.
		if (strcmp(argv[i], "--id") == MATCH) {
			g_td.m_features.set_id(argv[i+1]);
			i++;
		}

		// if --shadow is specified, then there will be an ascii direction
		// string that the transferd will use to determine the active file
		// transfer direction. The argument should be 'upload' or 'download'.
		if (strcmp(argv[i], "--shadow") == MATCH) {
			g_td.m_features.set_shadow_direction(argv[i+1]);
			i++;
		}

		// if --help, print a usage
		if (strcmp(argv[i], "--help") == MATCH) {
			usage();
		}
	}

	////////////////////////////////////////////////////////////////////////
	// start the initialization process
	// XXX put this AFTER the timers and whatnot get registered, but before I
	// leave main_init();

	// does there happen to be any work on the stdin port? If so, suck it into
	// the transfer collection

	if (g_td.m_features.get_uses_stdin() == TRUE) {
		dprintf(D_ALWAYS, "Loading transfer request from stdin...\n");
		g_td.accept_transfer_request(stdin);
	}

	// contact the schedd, if applicable, and tell it I'm alive.
	ret = g_td.register_to_schedd(&usock);

	switch(ret) {
		case REG_RESULT_NO_SCHEDD:
			// A schedd was not specified on the command line, so a finished
			// transfer will just be logged.
			break;
		case REG_RESULT_FAILED:
			// Failed to contact the schedd... Maybe bad sinful string?
			// For now, I'll mark it as an exceptional case, since it is 
			// either programmer error, or the schedd had gone away after
			// the request to start the transferd.
			EXCEPT("Failed to register to schedd! Aborting.");
			break;

		case REG_RESULT_SUCCESS:
			// stash the socket for later use.
			m_update_sock = usock;
			break;

		default:
			EXCEPT("TransferD::init() Programmer error!");
			break;
	}
}

/* stdin holds the work for this daemon to do, so let's read it */
int
TransferD::accept_transfer_request(FILE *fin)
{
	std::string encapsulation_method_line;
	EncapMethod em;
	int rval = 0;

	/* The first line of stdin represents an encapsulation method. The
		encapsulation method I'm using at the time of writing is old classads.
		In the future, there might be new classads which have a different
		format, or something entirely different */

	if (readLine(encapsulation_method_line, fin) == FALSE) {
		EXCEPT("Failed to read encapsulation method line!");
	}
	trim(encapsulation_method_line);

	em = encap_method(encapsulation_method_line);

	/* now, call the right initialization function based upon the encapsulation
		method */
	switch (em) {

		case ENCAP_METHOD_UNKNOWN:
			EXCEPT("I don't understand the encapsulation method of the "
					"protocol: %s\n", encapsulation_method_line.c_str());
			break;

		case ENCAP_METHOD_OLD_CLASSADS:
			rval = accept_transfer_request_encapsulation_old_classads(fin);
			break;

		default:
			EXCEPT("TransferD::init(): Programmer error! encap unhandled!");
			break;
	}

	m_initialized = TRUE;

	return rval;
}

/* Continue reading from stdin the rest of the protocol for this encapsulation
	method */
int
TransferD::accept_transfer_request_encapsulation_old_classads(FILE *fin)
{
	int i;
	int eof, error, empty;
	const char *classad_delimitor = "---\n";
	ClassAd *ad;
	TransferRequest *treq = NULL;
	std::string cap;

	/* read the transfer request header packet upon construction */
	ad = new ClassAd;
	InsertFromFile(fin, *ad, classad_delimitor, eof, error, empty);
	if (empty == TRUE) {
		EXCEPT("Protocol faliure, can't read initial Info Packet");
	}

	// initialize the header information of the TransferRequest object.
	treq = new TransferRequest(ad);
	if (treq == NULL) {
		EXCEPT("Out of memory!");
	}

	treq->dprintf(D_ALWAYS);
	
	/* read the information packet which describes the rest of the protocol */
	if (treq->get_num_transfers() <= 0) {
		EXCEPT("Protocol error!");
	}

	// read all the work ads associated with this TransferRequest
	for (i = 0; i < treq->get_num_transfers(); i++) {
		ad = new ClassAd;
		InsertFromFile(fin, *ad, classad_delimitor, eof, error, empty);
		if (empty == TRUE) {
			EXCEPT("Expected %d transfer job ads, got %d instead.", 
				treq->get_num_transfers(), i);
		}
		dPrintAd(D_ALWAYS, *ad);
		treq->append_task(ad);
	}

	// Since stdin may only provide one transfer request currently, make up
	// a capability and shove it into the work hash
	cap = gen_capability();

	// record that I've accepted it.
	m_treqs.insert(cap, treq);

	// mark it down that we are no longer need an inactivity timer
	m_inactivity_timer = 0;

	return TRUE;
}

// Called when the schedd initially connects to the transferd to finish
// the registration process.
int
TransferD::setup_transfer_request_handler(int  /*cmd*/, Stream *sock)
{
	ReliSock *rsock = (ReliSock*)sock;
	std::string sock_id;

	dprintf(D_ALWAYS, "Got TRANSFER_CONTROL_CHANNEL!\n");

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
			errstack.push( "TransferD::setup_transfer_request_handler()", 42,
				"Failure to register transferd - Authentication failed" );
			dprintf( D_ALWAYS, "setup_transfer_request_handler() "
				"aborting: %s\n",
				errstack.getFullText().c_str() );
			refuse(rsock);
			return CLOSE_STREAM;
		} 
	}

	rsock->decode();

	///////////////////////////////////////////////////////////////
	// Register this socket with a socket handler to handle incoming requests
	///////////////////////////////////////////////////////////////

	sock_id += "<TreqChannel-Socket>";

	char* _sock_id = strdup( sock_id.c_str() );		//de-const

	// register the handler for any future transfer requests on this socket.
	int retval = daemonCore->Register_Socket((Sock*)rsock, _sock_id,
		(SocketHandlercpp)&TransferD::accept_transfer_request_handler,
		"TransferD::accept_transfer_request_handler", this, ALLOW);
	
	free( _sock_id );
	
	if (retval < 0) {
		dprintf(D_ALWAYS, "Treq: Failure to register socket\n");
		return false;
	}
	dprintf(D_ALWAYS, "Treq channel established.\n");
	dprintf(D_ALWAYS, "Accepting Transfer Requests.\n");

	return KEEP_STREAM;
}

// After the channel has been setup, now we can accept transfer requests all
// day long...
int
TransferD::accept_transfer_request_handler(Stream *sock)
{
	std::string encapsulation_method_line;
	EncapMethod em;

	dprintf(D_ALWAYS, 
		"Entering TransferD::accept_transfer_request_handler()\n");
	dprintf(D_ALWAYS, "INCOMING TRANSFER REQUEST!\n");

	/* The first line of protocol represents an encapsulation method. The
		encapsulation method I'm using at the time of writing is old classads.
		In the future, there might be new classads which have a different
		format, or something entirely different */

	sock->decode();

	if (sock->code(encapsulation_method_line) == 0) {
		EXCEPT("Schedd closed connection, I'm going away.");
	}
	sock->end_of_message();

	trim(encapsulation_method_line);

	dprintf(D_ALWAYS, "Read encap line: %s\n", 
		encapsulation_method_line.c_str());

	em = encap_method(encapsulation_method_line);

	/* now, call the right initialization function based upon the encapsulation
		method */
	switch (em) {

		case ENCAP_METHOD_UNKNOWN:
			EXCEPT("I don't understand the encapsulation method of the "
					"protocol: %s\n", encapsulation_method_line.c_str());
			break;

		case ENCAP_METHOD_OLD_CLASSADS:
			accept_transfer_request_encapsulation_old_classads(sock);
			break;

		default:
			EXCEPT("TransferD::init(): Programmer error! encap unhandled!");
			break;
	}

	m_initialized = TRUE;

	dprintf(D_ALWAYS, 
		"Leaving TransferD::accept_transfer_request_handler()\n");
	return KEEP_STREAM;
}

/* Continue reading from rsock the rest of the protcol for this encapsulation
	method */
int
TransferD::accept_transfer_request_encapsulation_old_classads(Stream *sock)
{
	int i;
	ClassAd *ad = NULL;
	TransferRequest *treq = NULL;
	std::string cap;
	ClassAd respad;

	dprintf(D_ALWAYS,
		"Entering "
		"TransferD::accept_transfer_request_encapsulation_old_classads()\n");

	sock->decode();

	/////////////////////////////////////////////////////////////////////////
	// Accept the transfer request from the schedd.
	/////////////////////////////////////////////////////////////////////////

	/* read the transfer request header packet upon construction */
	ad = new ClassAd();
	if (getClassAd(sock, *ad) == false) {
		// XXX don't fail here, just go back to daemoncore
		EXCEPT("XXX Couldn't init initial ad from stream!");
	}
	sock->end_of_message();

	dprintf(D_ALWAYS, "Read treq header.\n");

	// initialize the header information of the TransferRequest object.
	treq = new TransferRequest(ad);
	if (treq == NULL) {
		EXCEPT("Out of memory!");
	}

	/* read the information packet which describes the rest of the protocol */
	if (treq->get_num_transfers() <= 0) {
		EXCEPT("Protocol error!");
	}

	// read all the work ads associated with this TransferRequest
	for (i = 0; i < treq->get_num_transfers(); i++) {
		ad = new ClassAd();
		if (ad == NULL) {
			EXCEPT("Out of memory!");
		}
		if (getClassAd(sock, *ad) == false) {
			EXCEPT("Expected %d transfer job ads, got %d instead.", 
				treq->get_num_transfers(), i);
		}
		sock->end_of_message();
		dprintf(D_ALWAYS, "Read treq job ad[%d].\n", i);
		treq->append_task(ad);
	}
	sock->end_of_message();

	sock->encode();

	/////////////////////////////////////////////////////////////////////////
	// See if I can honor this request's protocol choice
	/////////////////////////////////////////////////////////////////////////

	switch(treq->get_xfer_protocol())
	{
		case FTP_CFTP: // Transferd may use the FileTransfer Object protocol
			respad.Assign(ATTR_TREQ_INVALID_REQUEST, FALSE);
			break;

		default:
			dprintf(D_ALWAYS, "Transfer Request uses an unsupported file "
				"transfer protocol. Rejecting it.\n");

			// Currently, I don't support anything else....
			respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
			respad.Assign(ATTR_TREQ_INVALID_REASON, 
				"Transferd doesn't support client required file transfer "
				"protocol.");

			// tell the schedd we don't want to do this request
			putClassAd(sock, respad);
			sock->end_of_message();
			delete treq;

			// wait for the next request to come in....
			return KEEP_STREAM;
			break;
	}

	/////////////////////////////////////////////////////////////////////////
	// Create a capability for this request, making sure it is unique to all
	// rest of them, then send it back.
	/////////////////////////////////////////////////////////////////////////

	cap = gen_capability();
	treq->set_capability(cap);

	respad.Assign(ATTR_TREQ_CAPABILITY, cap);

	dprintf(D_ALWAYS, "Assigned capability to treq: %s.\n", cap.c_str());

	// This respose ad will contain:
	//
	//	ATTR_TREQ_INVALID_REQUEST (set to true)
	//	ATTR_TREQ_INVALID_REASON
	//
	//	OR
	//
	//	ATTR_TREQ_INVALID_REQUEST (set to false)
	//	ATTR_TREQ_CAPABILITY
	//
	putClassAd(sock, respad);
	sock->end_of_message();

	dprintf(D_ALWAYS, "Reported capability back to schedd.\n");

	// If nothing times out or broke connection, and I think the schedd has
	// gotten the above information, then queue this request to deal with
	// at the appropriate time.
	m_treqs.insert(cap, treq);

	// get ready to read another treq.
	sock->decode();

	dprintf(D_ALWAYS, "Waiting for another transfer request from schedd.\n");

	dprintf(D_ALWAYS,
		"Leaving "
		"TransferD::accept_transfer_request_encapsulation_old_classads()\n");

	return KEEP_STREAM;
}

// This function calls up the schedd passed in on the command line and 
// registers the transferd as being available for the schedd's use.
RegisterResult
TransferD::register_to_schedd(ReliSock **regsock_ptr)
{
	CondorError errstack;
	std::string sname;
	std::string id;
	const char *sinful;
	bool rval;
	
	if (*regsock_ptr != NULL) {
		*regsock_ptr = NULL;
	}

	sname = m_features.get_schedd_sinful();
	id = m_features.get_id();

	if (sname == "N/A") {
		// no schedd supplied with which to register
		dprintf(D_ALWAYS, "No schedd specified to which to register.\n");
		return REG_RESULT_NO_SCHEDD;
	}
	
	// what is my sinful string?
	sinful = daemonCore->InfoCommandSinfulString(-1);
	ASSERT(sinful);

	dprintf(D_FULLDEBUG, "Registering myself(%s) to schedd(%s)\n",
		sinful, sname.c_str());

	// hook up to the schedd.
	DCSchedd schedd(sname.c_str(), NULL);

	// register myself, give myself 1 minute to connect.
	rval = schedd.register_transferd(sinful, id, 20*3, regsock_ptr, &errstack);

	if (rval == false) {
		// emit why 
		dprintf(D_ALWAYS, "TransferRequest::register_to_schedd(): Failed to "
			"register. Schedd gave reason '%s'\n", errstack.getFullText().c_str());
		return REG_RESULT_FAILED;
	}

	// WARNING WARNING WARNING WARNING //
	// WARNING WARNING WARNING WARNING //
	// WARNING WARNING WARNING WARNING //
	// WARNING WARNING WARNING WARNING //
	// WARNING WARNING WARNING WARNING //

	// Here, I must infact go back to daemon core without closing or doing
	// anything with the socket. This is because the schedd is going to
	// reconnect back to me, and I can't deadlock.

	dprintf(D_FULLDEBUG, 
		"Succesfully registered, awaiting treq channel message....\n");

	return REG_RESULT_SUCCESS;
}

void
TransferD::register_handlers(void)
{
	// for condor squawk.
	daemonCore->Register_Command(DUMP_STATE,
			"DUMP_STATE",
			(CommandHandlercpp)&TransferD::dump_state_handler,
			"dump_state_handler", this, READ);

	// The schedd will open a permanent connection to the transferd via this
	// particular handler and periodically give file transfer requests to the
	// transferd for subsequent processing.
	daemonCore->Register_Command(TRANSFERD_CONTROL_CHANNEL,
			"TRANSFERD_CONTROL_CHANNEL",
			(CommandHandlercpp)&TransferD::setup_transfer_request_handler,
			"setup_transfer_request_handler", this, WRITE);

	// write files into the storage area the transferd is responsible for, this
	// could be spool, or the initial dir.
	daemonCore->Register_Command(TRANSFERD_WRITE_FILES,
			"TRANSFERD_WRITE_FILES",
			(CommandHandlercpp)&TransferD::write_files_handler,
			"write_files_handler", this, WRITE);

	// read files from the storage area the transferd is responsible for, this
	// could be spool, or the initial dir.
	daemonCore->Register_Command(TRANSFERD_READ_FILES,
			"TRANSFERD_READ_FILES",
			(CommandHandlercpp)&TransferD::read_files_handler,
			"read_files_handler", this, READ);
	
	// register the reaper, this is for any process which isn't a read/write
	// file transfer object.
	daemonCore->Register_Reaper("Reaper", 
		(ReaperHandlercpp)&TransferD::reaper_handler, "Reaper", this);
}

void
TransferD::register_timers(void)
{
	// begin processing any active requests, if there was information passed in
	// via stdin, then this'll get acted on very quickly
/*	daemonCore->Register_Timer( 0, 20,*/
/*		(TimerHandlercpp)&TransferD::process_active_requests_timer,*/
/*		"TransferD::process_active_requests_timer", this );*/

	daemonCore->Register_Timer( 0, 20,
		(TimerHandlercpp)&TransferD::exit_due_to_inactivity_timer,
		"TransferD::exit_due_to_inactivity_timer", this );
}


void usage(void)
{
	dprintf(D_ALWAYS, 
		"Usage info:\n"
		"--schedd <sinful>: Address of the schedd the transferd will contact\n"
		"--stdin:           Accept a transfer request on stdin\n"
		"--id <ascii>:      Used by the schedd to pair transferds to requests\n"
		"--shadow <upload|download>:\n"
		"                   Used with --stdin, transferd connects to shadow.\n"
		"                   This is demo mode with the starter.\n");

	DC_Exit(0);
}
