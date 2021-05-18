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

#ifndef TRANSFERD_H
#define TRANSFERD_H

#include "extArray.h"
#include "file_transfer.h"
#include "condor_transfer_request.h"
#include "exit.h"

// How successful was I in registering myself to a schedd, if available?
enum RegisterResult {
	REG_RESULT_FAILED = 0,
	REG_RESULT_SUCCESS,
	REG_RESULT_NO_SCHEDD,
};

// When a TD exits, here is what it should exit with.
enum TDExitStatus
{
	TD_EXIT_TIMEOUT = EXIT_SUCCESS,			// The TD exited via time out.
	TD_EXIT_SUCCESS = TD_EXIT_TIMEOUT,		// The TD had a controlled exit.
	TD_EXIT_EXCEPT = EXIT_EXCEPTION,		// The TD exited via EXCEPT.
	TD_EXIT_DPRINTF_ERROR = DPRINTF_ERROR,	// The TD exited via dprintf exit
};

// This class hold features and info that have been specified on the 
// command line.
class Features
{
	public:
		Features()
		{
			m_uses_stdin = FALSE;
			m_schedd_sinful = "N/A";
			/* in case people forget to set a timeout, this is always 
				a good number */
			m_timeout = 20 * 60;
		}

		~Features() { }
		
		void set_schedd_sinful(char *sinful)
		{
			m_schedd_sinful = sinful ? sinful : "";
		}

		void set_schedd_sinful(std::string sinful)
		{
			m_schedd_sinful = sinful;
		}

		std::string get_schedd_sinful(void)
		{
			return m_schedd_sinful;
		}
		
		// XXX DEMO hacking
		void set_shadow_direction(std::string direction)
		{
			m_shad_dir = direction;
		}

		// XXX DEMO hacking
		std::string get_shadow_direction(void)
		{
			return m_shad_dir;
		}

		void set_id(char *id)
		{
			m_id = id ? id : "";
		}

		void set_id(std::string id)
		{
			m_id = id;
		}

		std::string get_id(void)
		{
			return m_id;
		}

		void set_uses_stdin(int b)
		{
			m_uses_stdin = b;
		}

		int get_uses_stdin(void) const
		{
			return m_uses_stdin;
		}

		void set_timeout(unsigned long tout)
		{
			m_timeout = tout;
		}

		time_t get_timeout(void) const
		{
			return m_timeout;
		}


	private:

		///////////////////////////////////////////////////////////////////////
		// Private variables:
		///////////////////////////////////////////////////////////////////////

		// --schedd <sinfulstring>
		// The schedd with which the transferd registers itself.
		std::string m_schedd_sinful;

		// --stdin
		// Presence of this flag says that the transferd will *also* grab
		// TransferObject requests from stdin. Otherwise, the default method
		// of the schedd having to contact the transferd via command port
		// is the only method.
		int m_uses_stdin;

		// --id <ascii_key>
		// The identity that the schedd will use to match this transferd
		// with the request for it.
		std::string m_id;

		// --timeout <number of seconds>
		// The number of seconds the transferd will wait while its transfer
		// queue is empty before it exits in a well known fashion.
		// NOTE: In the future, the schedd will probably be able to send new
		// timeout updates via the control channel, if that happens, they
		// should override this value.
		unsigned long m_timeout;

		// XXX DEMO hacking
		// --shadow <upload|download>
		// In this mode, the transferd must get an active request on stdin.
		// Then the transferd will simply init a file transfer object for
		// connecting to the shadow and pass it the job ad supplied.
		std::string m_shad_dir;
};

class TransferD : public Service
{
	public:
		TransferD();
		~TransferD();

		// located in the initialization implementation file

		// parse the ocmmand line arguments, soak any TransferRequests 
		// that are immediately available, call up the schedd and register
		// if needed.
		void init(int argc, char *argv[]);

		// The td will read a single treq from stdin
		int accept_transfer_request(FILE *fin);

		// After the treq channel from the schedd to the td has been
		// setup, just keep accepting more work from it in a registered
		// socket handler...
		int setup_transfer_request_handler(int cmd, Stream *sock);
		int accept_transfer_request_handler(Stream *sock);

		void register_handlers(void);
		void register_timers(void);

		// located in the maintenance implementation file
		void reconfig(void);
		void shutdown_fast(void);
		void shutdown_graceful(void);
		void transferd_exit(void);

		// This function digs through the TransferRequest list and finds all
		// TransferRequests where the transferd itself has to initiate a
		// movement of file from the transferd's storage to/from another
		// transferd, or to some initial directory. 
		int initiate_active_transfer_requests(void);

		// in response to a condor_squawk command, dump a classad of the
		// internal state.
		int dump_state_handler(int cmd, Stream *sock);

		// how files are written into the transferd
		int write_files_handler(int cmd, Stream *sock);
		static int write_files_thread(void *targ, Stream *sock);
		int write_files_reaper(int tid, int exit_status);

		// how files are read from the transferd
		int read_files_handler(int cmd, Stream *sock);
		static int read_files_thread(void *targ, Stream *sock);
		int read_files_reaper(int tid, int exit_status);

		// a periodic timer to process any active requests
		int process_active_requests_timer(void);

		// a periodic timer to calculate whether or not the transferd should
		// exit due to having an empty queue for too long.
		void exit_due_to_inactivity_timer(void) const;

		// handler for any exiting process.
		int reaper_handler(int pid, int exit_status);

		// Look through all of my TransferRequests for one that contains
		// a particular job id. 
		TransferRequest* find_transfer_request(int c, int p);

		// a set of features dictated by command line which the transferd 
		// will consult when wishing to do various things.
		Features m_features;

	private:
		///////////////////////////////////////////////////////////////////////
		// Private functions
		///////////////////////////////////////////////////////////////////////

		// Shove a go away message on the stream.
		static void refuse(Sock *sock);

		// read the information using the old classads encapsulation
		int accept_transfer_request_encapsulation_old_classads(FILE *fin);
		int accept_transfer_request_encapsulation_old_classads(Stream *sock);

		// Call up the schedd I was informed about to tell them I'm alive.
		// the returned pointer (if valid) is the connection that the 
		// transferd will use to send status updates about completed transfers.
		RegisterResult register_to_schedd(ReliSock **regsock_ptr);

		// process a single active request
		int process_active_request(TransferRequest *treq);
		int process_active_shadow_request(TransferRequest *treq); // XXX demo
		int active_shadow_transfer_completed( FileTransfer *ftrans );

		// generate a capability unique to the capabilities currently known
		std::string gen_capability(void);

		////////////////////////////////////////////////////////////////////
		// Private variables
		////////////////////////////////////////////////////////////////////

		// Has the init() call been called? 
		int m_initialized;

		// This is a timestamp of when the request queue became empty.
		// When the request queue has something in it, this is set to zero.
		time_t m_inactivity_timer;

		// The list of transfers that have been requested of me to do when
		// someone contacts me.
		// Key: capability, Value: TransferRequest
		HashTable<std::string, TransferRequest*> m_treqs;

		// Associate a pid with a transfer request so the reaper can
		// figure out which transfer request failed/succeeded.
		// This hash table is for clients writing to the transferd.
		// Key: pid from Create_Thread, Value: TransferRequest
		HashTable<long, TransferRequest*> m_client_to_transferd_threads;

		// Associate a pid with a transfer request so the reaper can
		// figure out which transfer request failed/succeeded.
		// Key: pid from Create_Thread, Value: TransferRequest
		// This hash table is for the transferd writing to the client.
		HashTable<long, TransferRequest*> m_transferd_to_client_threads;

		// After the transferd registers to the schedd, the connection to
		// the schedd is stored here. This allows the transferd to send
		// update messaged to the schedd about transfers that have completed.
		// This is NULL if there is no schedd to contact.
		ReliSock *m_update_sock;
};

extern TransferD g_td;

#endif




