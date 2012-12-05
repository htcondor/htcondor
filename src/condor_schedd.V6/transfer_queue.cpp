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
#include "transfer_queue.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_commands.h"
#include "condor_config.h"
#include "dc_transfer_queue.h"
#include "condor_email.h"

TransferQueueRequest::TransferQueueRequest(ReliSock *sock,char const *fname,char const *jobid,char const *queue_user,bool downloading,time_t max_queue_age):
	m_sock(sock),
	m_queue_user(queue_user),
	m_jobid(jobid),
	m_fname(fname),
	m_downloading(downloading),
	m_max_queue_age(max_queue_age)
{
	m_gave_go_ahead = false;
	m_time_born = time(NULL);
	m_time_go_ahead = 0;
}

TransferQueueRequest::~TransferQueueRequest() {
	if( m_sock ) {
		daemonCore->Cancel_Socket( m_sock );
		delete m_sock;
		m_sock = NULL;
	}
}

char const *
TransferQueueRequest::Description() {
	m_description.formatstr("%s %s job %s for %s (initial file %s)",
					m_sock ? m_sock->peer_description() : "",
					m_downloading ? "downloading" : "uploading",
					m_jobid.Value(),
					m_queue_user.Value(),
					m_fname.Value());
	return m_description.Value();
}

bool
TransferQueueRequest::SendGoAhead(XFER_QUEUE_ENUM go_ahead,char const *reason) {
	ASSERT( m_sock );

	m_sock->encode();
	ClassAd msg;
	msg.Assign(ATTR_RESULT,(int)go_ahead);
	if( reason ) {
		msg.Assign(ATTR_ERROR_STRING,reason);
	}

	if(!msg.put( *m_sock ) || !m_sock->end_of_message()) {
		dprintf(D_ALWAYS,
				"TransferQueueRequest: failed to send GoAhead to %s\n",
				Description() );
		return false;
	}

	m_gave_go_ahead = true;
	m_time_go_ahead = time(NULL);
	return true;
}

TransferQueueManager::TransferQueueManager() {
	m_max_uploads = 0;
	m_max_downloads = 0;
	m_check_queue_timer = -1;
	m_default_max_queue_age = 0;
	m_uploading = 0;
	m_downloading = 0;
	m_waiting_to_upload = 0;
	m_waiting_to_download = 0;
	m_upload_wait_time = 0;
	m_download_wait_time = 0;
	m_round_robin_counter = 0;
	m_round_robin_garbage_counter = 0;
	m_round_robin_garbage_time = time(NULL);
}

TransferQueueManager::~TransferQueueManager() {
	TransferQueueRequest *client = NULL;

	m_xfer_queue.Rewind();
	while( m_xfer_queue.Next( client ) ) {
		delete client;
	}
	m_xfer_queue.Clear();

	if( m_check_queue_timer != -1 ) {
		daemonCore->Cancel_Timer( m_check_queue_timer );
	}
}

void
TransferQueueManager::InitAndReconfig() {
	m_max_downloads = param_integer("MAX_CONCURRENT_DOWNLOADS",10,0);
	m_max_uploads = param_integer("MAX_CONCURRENT_UPLOADS",10,0);
	m_default_max_queue_age = param_integer("MAX_TRANSFER_QUEUE_AGE",3600*2,0);
}

void
TransferQueueManager::RegisterHandlers() {
	int rc = daemonCore->Register_Command(
		TRANSFER_QUEUE_REQUEST,
		"TRANSFER_QUEUE_REQUEST",
		(CommandHandlercpp)&TransferQueueManager::HandleRequest,
		"TransferQueueManager::HandleRequest",
		this,
		WRITE );
	ASSERT( rc >= 0 );
}

int TransferQueueManager::HandleRequest(int cmd,Stream *stream)
{
	ReliSock *sock = (ReliSock *)stream;
	ASSERT( cmd == TRANSFER_QUEUE_REQUEST );

	ClassAd msg;
	sock->decode();
	if( !msg.initFromStream( *sock ) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"TransferQueueManager: failed to receive transfer request "
				"from %s.\n", sock->peer_description() );
		return FALSE;
	}

	bool downloading = false;
	MyString fname;
	MyString jobid;
	MyString queue_user;
	if( !msg.LookupBool(ATTR_DOWNLOADING,downloading) ||
		!msg.LookupString(ATTR_FILE_NAME,fname) ||
		!msg.LookupString(ATTR_JOB_ID,jobid) ||
		!msg.LookupString(ATTR_USER,queue_user) )
	{
		MyString msg_str;
		msg.sPrint(msg_str);
		dprintf(D_ALWAYS,"TransferQueueManager: invalid request from %s: %s\n",
				sock->peer_description(), msg_str.Value());
		return FALSE;
	}

		// Currently, we just create the client with the default max queue
		// age.  If it becomes necessary to customize the maximum age
		// on a case-by-case basis, it should be easy to adjust.

	TransferQueueRequest *client =
		new TransferQueueRequest(
			sock,
			fname.Value(),
			jobid.Value(),
			queue_user.Value(),
			downloading,
			m_default_max_queue_age);

	if( !AddRequest( client ) ) {
		delete client;
		return KEEP_STREAM; // we have already closed this socket
	}

	return KEEP_STREAM;
}

bool
TransferQueueManager::AddRequest( TransferQueueRequest *client ) {
	ASSERT( client );

	MyString error_desc;
	if( daemonCore->TooManyRegisteredSockets(client->m_sock->get_file_desc(),&error_desc))
	{
		dprintf(D_FULLDEBUG,"TransferQueueManager: rejecting %s to avoid overload: %s\n",
				client->Description(),
				error_desc.Value());
		client->SendGoAhead(XFER_QUEUE_NO_GO,error_desc.Value());
		return false;
	}

	dprintf(D_FULLDEBUG,
			"TransferQueueManager: enqueueing %s.\n",
			client->Description());

	int rc = daemonCore->Register_Socket(client->m_sock,
		"<file transfer request>",
		(SocketHandlercpp)&TransferQueueManager::HandleDisconnect,
		"HandleDisconnect()", this, ALLOW);

	if( rc < 0 ) {
		dprintf(D_ALWAYS,
				"TransferQueueManager: failed to register socket for %s.\n",
				client->Description());
		return false;
	}

	ASSERT( daemonCore->Register_DataPtr( client ) );

	m_xfer_queue.Append( client );

	TransferQueueChanged();

	return true;
}

int
TransferQueueManager::HandleDisconnect( Stream *sock )
{
	TransferQueueRequest *client;
	m_xfer_queue.Rewind();
	while( m_xfer_queue.Next( client ) ) {
		if( client->m_sock == sock ) {
			dprintf(D_FULLDEBUG,
					"TransferQueueManager: dequeueing %s.\n",
					client->Description());

			delete client;
			m_xfer_queue.DeleteCurrent();

			TransferQueueChanged();

			return KEEP_STREAM; // we have already deleted socket
		}
	}

		// should never get here
	m_xfer_queue.Rewind();
	MyString clients;
	while( m_xfer_queue.Next( client ) ) {
		clients.formatstr_cat(" (%p) %s\n",
					 client->m_sock,client->m_sock->peer_description());
	}
	EXCEPT("TransferQueueManager: ERROR: disconnect from client (%p) %s;"
		   " not found in list: %s\n",
		   sock,
		   sock->peer_description(),
		   clients.Value());
	return FALSE; // close socket
}

void
TransferQueueManager::TransferQueueChanged() {
	if( m_check_queue_timer != -1 ) {
			// already have a timer set
		return;
	}
	m_check_queue_timer = daemonCore->Register_Timer(
		0,(TimerHandlercpp)&TransferQueueManager::CheckTransferQueue,
		"CheckTransferQueue",this);
}

int
TransferQueueManager::GetRoundRobinRecency(const std::string &queue)
{
	std::map< std::string,unsigned int >::iterator it = m_round_robin_recency.find(queue);
	if( it == m_round_robin_recency.end() ) {
		return 0;
	}
	return it->second;
}

void
TransferQueueManager::SetRoundRobinRecency(const std::string &queue)
{
	unsigned int old_counter = m_round_robin_counter;

	m_round_robin_recency[queue] = ++m_round_robin_counter;

		// clear history if the counter wraps, so we do not keep favoring those who have wrapped
	if( m_round_robin_counter < old_counter ) {
		m_round_robin_recency.clear();
	}
}

void
TransferQueueManager::CollectRoundRobinGarbage()
{
		// To prevent unbounded growth, remove entries in the round
		// robin recency map that have not been touched in the past
		// hour.

	time_t now = time(NULL);
		// use abs() here so big clock jumps do not cause long
		// periods of no garbage collection
	if( abs((int)(now - m_round_robin_garbage_time)) > 3600 ) {
		int num_removed = 0;

		for( std::map< std::string,unsigned int >::iterator it = m_round_robin_recency.begin();
			 it != m_round_robin_recency.end(); )
		{
			if( it->second < m_round_robin_garbage_counter ) {
					// increment the iterator before calling erase, while it is still valid
				m_round_robin_recency.erase(it++);
				++num_removed;
			}
			else {
				++it;
			}
		}

		if( num_removed ) {
			dprintf(D_ALWAYS,"TransferQueueManager::CollectRoundRobinGarbage: removed %d entries.\n",num_removed);
		}

		m_round_robin_garbage_time = now;
		m_round_robin_garbage_counter = m_round_robin_counter;
	}
}

void
TransferQueueManager::CheckTransferQueue() {
	TransferQueueRequest *client = NULL;
	int downloading = 0;
	int uploading = 0;
	bool clients_waiting = false;

	m_check_queue_timer = -1;
	m_waiting_to_upload = 0;
	m_waiting_to_download = 0;
	m_upload_wait_time = 0;
	m_download_wait_time = 0;

	m_xfer_queue.Rewind();
	while( m_xfer_queue.Next(client) ) {
		if( client->m_gave_go_ahead ) {
			if( client->m_downloading ) {
				downloading += 1;
			}
			else {
				uploading += 1;
			}
		}
	}

		// schedule new transfers
	while( uploading < m_max_uploads || m_max_uploads <= 0 ||
		   downloading < m_max_downloads || m_max_downloads <= 0 )
	{
		TransferQueueRequest *best_client = NULL;
		int best_recency = 0;

		m_xfer_queue.Rewind();
		while( m_xfer_queue.Next(client) ) {
			if( client->m_gave_go_ahead ) {
				continue;
			}
			if( (client->m_downloading && 
				(downloading < m_max_downloads || m_max_downloads <= 0)) ||
				((!client->m_downloading) &&
				(uploading < m_max_uploads || m_max_uploads <= 0)) )
			{
				if( !best_client ) {
					best_client = client;
					best_recency = GetRoundRobinRecency(client->m_queue_user);
				}
				else if( !best_client->m_downloading && client->m_downloading ) {
					best_client = client;
					best_recency = GetRoundRobinRecency(client->m_queue_user);
				}
				else if( best_client->m_queue_user != client->m_queue_user ) {
					int client_recency = GetRoundRobinRecency(client->m_queue_user);
					if( best_recency > client_recency ) {
						best_client = client;
						best_recency = client_recency;
					}
				}
			}
		}

		client = best_client;
		if( !client ) {
			break;
		}

		dprintf(D_FULLDEBUG,
				"TransferQueueManager: sending GoAhead to %s.\n",
				client->Description() );

		if( !client->SendGoAhead() ) {
			dprintf(D_FULLDEBUG,
					"TransferQueueManager: failed to send GoAhead; "
					"dequeueing %s.\n",
					client->Description() );

			delete client;
			m_xfer_queue.Delete(client);

			TransferQueueChanged();
		}
		else {
			SetRoundRobinRecency(client->m_queue_user);
			if( client->m_downloading ) {
				downloading += 1;
			}
			else {
				uploading += 1;
			}
		}
	}


		// now that we have finished scheduling new transfers,
		// examine requests that are still waiting
	m_xfer_queue.Rewind();
	while( m_xfer_queue.Next(client) ) {
		if( !client->m_gave_go_ahead
			&& ( (client->m_downloading && downloading==0)
 			     || (!client->m_downloading && uploading==0) ) )
		{
				// This request has not been granted, but no requests
				// are active.  This shouldn't happen for simple
				// upload/download requests, but for future generality, we
				// check for this case and treat it appropriately.
			dprintf(D_ALWAYS,"TransferQueueManager: forcibly "
					"dequeueing entry for %s, "
					"because it is not allowed by the queue policy.\n",
					client->Description());
			delete client;
			m_xfer_queue.DeleteCurrent();
			TransferQueueChanged();
			continue;
		}

		if( !client->m_gave_go_ahead ) {
			clients_waiting = true;

			int age = time(NULL) - client->m_time_born;
			if( client->m_downloading ) {
				m_waiting_to_download++;
				if( age > m_download_wait_time ) {
					m_download_wait_time = age;
				}
			}
			else {
				m_waiting_to_upload++;
				if( age > m_upload_wait_time ) {
					m_upload_wait_time = age;
				}
			}
		}
	}

	m_uploading = uploading;
	m_downloading = downloading;


	if( clients_waiting ) {
			// queue is full; check for ancient clients
		m_xfer_queue.Rewind();
		while( m_xfer_queue.Next(client) ) {
			if( client->m_gave_go_ahead ) {
				int age = time(NULL) - client->m_time_go_ahead;
				int max_queue_age = client->m_max_queue_age;
				if( max_queue_age > 0 && max_queue_age < age ) {
						// Killing this client will not stop the current
						// file that is being transfered by it (which
						// presumably has stalled for some reason).  However,
						// it should prevent any additional files in the
						// sandbox from being transferred.
					dprintf(D_ALWAYS,"TransferQueueManager: forcibly "
							"dequeueing  ancient (%ds old) entry for %s, "
							"because it is older than "
							"MAX_TRANSFER_QUEUE_AGE=%ds.\n",
							age,
							client->Description(),
							max_queue_age);


					FILE *email = email_admin_open("file transfer took too long");
					if( !email ) {
							// Error sending the message
						dprintf( D_ALWAYS, 
								 "ERROR: Can't send email to the Condor "
								 "Administrator\n" );
					} else {
						fprintf( email,
								 "A file transfer for\n%s\ntook longer than MAX_TRANSFER_QUEUE_AGE=%ds,\n"
								 "so this transfer is being removed from the transfer queue,\n"
								 "which will abort further transfers for this attempt to run this job.\n\n"
								 "To avoid this timeout, MAX_TRANSFER_QUEUE_AGE may be increased,\n"
								 "but be aware that transfers which take a long time will delay other\n"
								 "transfers from starting if the maximum number of concurrent transfers\n"
								 "is exceeded.  Therefore, it is advisable to also review the settings\n"
								 "of MAX_CONCURRENT_UPLOADS and/or MAX_CONCURRENT_DOWNLOADS.\n\n"
								 "The transfer queue currently has %d/%d uploads,\n"
								 "%d/%d downloads, %d transfers waiting %ds to upload,\n"
								 "and %d transfers waiting %ds to download.\n",
								 client->Description(),
								 max_queue_age,
								 m_uploading,
								 m_max_uploads,
								 m_downloading,
								 m_max_downloads,
								 m_waiting_to_upload,
								 m_upload_wait_time,
								 m_waiting_to_download,
								 m_download_wait_time
								 );

						email_close ( email );
					}

					delete client;
					m_xfer_queue.DeleteCurrent();
					TransferQueueChanged();
						// Only delete more ancient clients if the
						// next pass of this function finds there is pressure
						// on the queue.
					break;
				}
			}
		}
	}

	CollectRoundRobinGarbage();
}

bool
TransferQueueManager::GetContactInfo(char const *command_sock_addr, std::string &contact_str)
{
	TransferQueueContactInfo contact(
		command_sock_addr,
		m_max_uploads == 0,
		m_max_downloads == 0);
	return contact.GetStringRepresentation(contact_str);
}
