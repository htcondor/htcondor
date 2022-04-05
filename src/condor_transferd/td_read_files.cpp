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
#include "extArray.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_ftp.h"

// smart structure
class ThreadArg
{
	public:
		ThreadArg(int prot, TransferRequest *tr) { 
			protocol = prot; 
			treq = tr;
		}

		~ThreadArg() {};

		int protocol;
		TransferRequest *treq;
};

// This handler is called when a client wishes to read files from the
// transferd's storage.
int
TransferD::read_files_handler(int /* cmd */, Stream *sock) 
{
	ReliSock *rsock = (ReliSock*)sock;
	std::string capability;
	int protocol = FTP_UNKNOWN;
	TransferRequest *treq = NULL;
	std::string fquser;
	static int transfer_reaper_id = -1;
	ThreadArg *thread_arg;
	int tid;
	ClassAd reqad;
	ClassAd respad;

	dprintf(D_ALWAYS, "Got TRANSFERD_READ_FILES!\n");

	/////////////////////////////////////////////////////////////////////////
	// make sure we are authenticated
	/////////////////////////////////////////////////////////////////////////
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
			refuse( rsock );
			return CLOSE_STREAM;
		} 
	}

	fquser = rsock->getFullyQualifiedUser();


	/////////////////////////////////////////////////////////////////////////
	// Check to see if the capability the client tells us is something that
	// we have knowledge of. We ONLY check the capability and not the
	// identity of the person in question. This allows people of different
	// identities to read the files here as long as they had the right 
	// capability. While this might not sound secure, they STILL had to have
	// authenticated as someone this daemon trusts. 
	// Similarly, check the protocol it wants to use as well.
	/////////////////////////////////////////////////////////////////////////
	rsock->decode();

	// soak the request ad from the client about what it wants to transfer
	if (!getClassAd(rsock, reqad)) {
		rsock->end_of_message();
		return CLOSE_STREAM;
	}

	rsock->end_of_message();

	reqad.LookupString(ATTR_TREQ_CAPABILITY, capability);

	rsock->encode();

	// do I know of such a capability?
	if (m_treqs.lookup(capability, treq) != 0) {
		// didn't find it. Log it and tell them to leave and close up shop
		respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
		respad.Assign(ATTR_TREQ_INVALID_REASON, "Invalid capability!");
		putClassAd(rsock, respad);
		rsock->end_of_message();

		dprintf(D_ALWAYS, "Client identity '%s' tried to read some files "
			"using capability '%s', but there was no such capability. "
			"Access denied.\n", fquser.c_str(), capability.c_str());
		return CLOSE_STREAM;
	}

	reqad.LookupInteger(ATTR_TREQ_FTP, protocol);

	// am I willing to use this protocol?
	switch(protocol) {
		case FTP_CFTP: // FileTrans protocol, I'm happy.
			break;

		default:
			respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
			respad.Assign(ATTR_TREQ_INVALID_REASON, 
				"Invalid file transfer protocol!");
			putClassAd(rsock, respad);
			rsock->end_of_message();

			dprintf(D_ALWAYS, "Client identity '%s' tried to read some files "
				"using protocol '%d', but I don't support that protocol. "
				"Access denied.\n", fquser.c_str(), protocol);
			return CLOSE_STREAM;
	}

	// nsure that this transfer request was of the downloading variety
	if (treq->get_direction() != FTPD_DOWNLOAD) {
			respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
			respad.Assign(ATTR_TREQ_INVALID_REASON, 
				"Transfer Request was not a download request!");
			putClassAd(rsock, respad);
			rsock->end_of_message();

			dprintf(D_ALWAYS, "Client identity '%s' tried to read some files "
				"to a transfer request that wasn't expecting to be read. "
				"Access denied.\n", fquser.c_str());
	}

	/////////////////////////////////////////////////////////////////////////
	// Tell the client everything was ok and how many ads we have to transfer
	/////////////////////////////////////////////////////////////////////////

	respad.Assign(ATTR_TREQ_INVALID_REQUEST, FALSE);
	respad.Assign(ATTR_TREQ_NUM_TRANSFERS, treq->get_num_transfers());
	putClassAd(rsock, respad);
	rsock->end_of_message();

	/////////////////////////////////////////////////////////////////////////
	// Set up a thread (a process under unix) to read ALL of the job files
	// for all of the ads in the TransferRequest.
	/////////////////////////////////////////////////////////////////////////

	// now create a thread, passing in the sock, which uses the file transfer
	// object to accept the files.

	if (transfer_reaper_id == -1) {
		// register this ONCE for all threads to use.
		transfer_reaper_id = daemonCore->Register_Reaper(
						"read_files_reaper",
						(ReaperHandlercpp) &TransferD::read_files_reaper,
						"read_files_reaper",
						this
						);
	}

	thread_arg = new ThreadArg(protocol, treq);

	// Start a new thread (process on Unix) to do the work
	tid = daemonCore->Create_Thread(
		(ThreadStartFunc)&TransferD::read_files_thread,
		(void *)thread_arg,
		rsock,
		transfer_reaper_id
		);
	
	if (tid == FALSE) {
		// XXX How do I handle this failure?
	}


	// associate the tid with the request so I can deal with it propery in
	// the reaper
	ASSERT( m_transferd_to_client_threads.insert(tid, treq) == 0 );

	// The stream is inherited to the thread, who does the transfer and
	// finishes the protocol, but in the parent, I'm closing it.
	return CLOSE_STREAM;
}

// The function occurs in a seperate thread or process
int
TransferD::read_files_thread(void *targ, Stream *sock)
{	
	ThreadArg *thread_arg = (ThreadArg*)targ;
	ReliSock *rsock = (ReliSock*)sock;
	TransferRequest *treq = NULL;
	//int protocol;
	SimpleList<ClassAd*> *jad_list = NULL;
	ClassAd *jad = NULL;
	int cluster, proc;
	int old_timeout;
	int result;
	ClassAd respad;

	// even though I'm in a new process, I got here either through forking
	// or through a thread, so this memory is a copy.
	//protocol = thread_arg->protocol;
	treq = thread_arg->treq;
	delete thread_arg;

	// XXX deal with protocol value.

	////////////////////////////////////////////////////////////////////////
	// Do the transfer.
	////////////////////////////////////////////////////////////////////////

	// file transfers can take a long time....
	old_timeout = rsock->timeout(60 * 60 * 8);

	jad_list = treq->todo_tasks();

	while(jad_list->Next(jad)) {

		// first we send the jobad to the client, so they can instantiate
		// a file transfer object on their end too.
		putClassAd(rsock, *jad);
		rsock->end_of_message();

		FileTransfer ftrans;

		jad->LookupInteger(ATTR_CLUSTER_ID, cluster);
		jad->LookupInteger(ATTR_PROC_ID, proc);
		dprintf( D_ALWAYS, "TransferD::read_files_thread(): "
			"Transferring fileset for job %d.%d\n",
				cluster, proc);

		result = ftrans.SimpleInit(jad, true, true, rsock);
		if ( !result ) {
			dprintf( D_ALWAYS, "TransferD::read_files_thread(): "
				"failed to init file transfer for job %d.%d \n",
				cluster, proc );

			respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
			respad.Assign(ATTR_TREQ_INVALID_REASON, 
				"FileTransfer Object failed to SimpleInit.");
			putClassAd(rsock, respad);
			rsock->end_of_message();

			rsock->timeout(old_timeout);

			return EXIT_FAILURE;
		}

		ftrans.setPeerVersion(treq->get_peer_version().c_str());

		// We're "uploading" from here to the client.
		result = ftrans.UploadFiles();
		if ( !result ) {

			dprintf( D_ALWAYS, "TransferD::read_files_thread(): "
				"failed to transfer files for job %d.%d \n",
				cluster, proc );

			respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
			respad.Assign(ATTR_TREQ_INVALID_REASON, 
				"FileTransfer Object failed to upload.");
			putClassAd(rsock, respad);
			rsock->end_of_message();

			rsock->timeout(old_timeout);
			return EXIT_FAILURE;
		}
	}

	rsock->end_of_message();

	//////////////////////////////////////////////////////////////////////////
	// Now that the file transfer is done, tell the client everything is ok.
	//////////////////////////////////////////////////////////////////////////

	dprintf(D_ALWAYS, "Informing client of finished transfer.\n");

	rsock->encode();

	respad.Assign(ATTR_TREQ_INVALID_REQUEST, FALSE);

	// This response ad to the client will contain:
	//
	//	ATTR_TREQ_INVALID_REQUEST (set to false)
	//
	putClassAd(rsock, respad);
	rsock->end_of_message();

	delete rsock;

	return EXIT_SUCCESS;
}

int
TransferD::read_files_reaper(int tid, int exit_status)
{
	TransferRequest *treq = NULL;
	std::string str;
	ClassAd result;
	int exit_code;
	int signal;

	dprintf(D_ALWAYS, "TransferD::read_files_reaper(): "
		"A file transfer into the transferd has completed: "
		"tid %d, status: %d\n",
		tid, exit_status);
	
	/////////////////////////////////////////////////////////////////////////
	// Consistancy check to make sure I asked to do the transfer
	/////////////////////////////////////////////////////////////////////////
	if (m_transferd_to_client_threads.lookup((long)tid, treq) != 0) 
	{
		EXCEPT("TransferD::read_files_reaper(): "
			"Programmer error: I have no record of it! ");
	}
	// remove it from the thread hash now that I'm dealing with it.
	m_transferd_to_client_threads.remove((long)tid);

	/////////////////////////////////////////////////////////////////////////
	// Determine status ad.
	/////////////////////////////////////////////////////////////////////////

	// The schedd will know who I'm talking about cause it has the
	// same capability for this transfer request.
	str = treq->get_capability();
	result.Assign(ATTR_TREQ_CAPABILITY, str);

	// figure out what the exit_status means and encode it into the result ad
	if (WIFSIGNALED(exit_status)) {
		signal = WTERMSIG(exit_status);
		dprintf(D_ALWAYS, "Thread exited with signal: %d\n", signal);

		result.Assign(ATTR_TREQ_SIGNALED, TRUE);
		result.Assign(ATTR_TREQ_SIGNAL, signal);
		result.Assign(ATTR_TREQ_UPDATE_STATUS, "NOT OK");
		formatstr(str, "Died with signal %d", signal);
		result.Assign(ATTR_TREQ_UPDATE_REASON, str);

	} else {
		exit_code = WEXITSTATUS(exit_status);

		result.Assign(ATTR_TREQ_EXIT_CODE, exit_code);

		dprintf(D_ALWAYS, "Thread exited with exit code: %d\n", exit_code);
		switch(exit_code) {
			case EXIT_SUCCESS:
				result.Assign(ATTR_TREQ_UPDATE_STATUS, "OK");
				result.Assign(ATTR_TREQ_UPDATE_REASON, "Successful transfer");
				result.Assign(ATTR_TREQ_SIGNALED, FALSE);
				break;

			default:
				result.Assign(ATTR_TREQ_UPDATE_STATUS, "NOT OK");
				formatstr(str, "File transfer exited with incorrect exit code %d",
					exit_code);
				result.Assign(ATTR_TREQ_UPDATE_REASON, str);
				result.Assign(ATTR_TREQ_SIGNALED, FALSE);
				break;
		}
	}

	/////////////////////////////////////////////////////////////////////////
	// Call back schedd with status ad. If failed, don't repeat
	// it, the schedd will send another transfer request if it wants it
	// done again.
	/////////////////////////////////////////////////////////////////////////

	// XXX Detail what is in this ad.
	m_update_sock->encode();
	putClassAd(m_update_sock, result);
	m_update_sock->end_of_message();

	// now remove the treq forever from our knowledge
	m_treqs.remove(treq->get_capability());

	// bye bye.
	delete treq;
	
	// Now, if the hash is empty, mark it down as the start of our inactivity
	// timer
	if (m_treqs.getNumElements() == 0) {
		dprintf(D_ALWAYS, 
			"Last transfer request handled. Becoming inactive.\n");
		m_inactivity_timer = time(NULL);
	}

	return TRUE;
}









