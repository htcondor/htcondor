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


#ifndef DC_MESSAGE_H
#define DC_MESSAGE_H

/* We have circular references between DCMsg (declared in this file)
 * and Daemon (declared in daemon.h), so we need to foward declare
 * DCMsg before the includes.
 */
class DCMessenger;
class DCMsg;

#include "daemon.h"
#include "classy_counted_ptr.h"
#include "dc_service.h"

class DCMessenger;
class DCMsg;

/*
  This is a base class for sending CEDAR commands.  By default, the
  caller of this class is not blocked if a TCP connection is required
  to create a security session.
*/

class DCMsg: public ClassyCountedPtr {
public:
	DCMsg(int cmd);
	virtual ~DCMsg();

	enum MessageClosureEnum {
		MESSAGE_FINISHED,  // tells DCMessenger that sock may be closed
		MESSAGE_CONTINUING // tells DCMessenger not to close sock
	};

	enum DeliveryStatus {
		DELIVERY_PENDING,
		DELIVERY_SUCCEEDED,
		DELIVERY_FAILED
	};

		/* writeMsg() is called by DCMessenger once sock is ready to
		   receive data.
		   returns: true on success; false on failure */
	virtual bool writeMsg( DCMessenger *messenger, Sock *sock ) = 0;

		/* readMsg() is called by DCMessenger once sock is ready to
		   receive data.
		   returns: true on success; false on failure */
	virtual bool readMsg( DCMessenger *messenger, Sock *sock ) = 0;

		/* messageSent() is called by DCMessenger upon successful
		   delivery of message
		   return: MESSAGE_FINISHED if sock may now be closed
		           MESSAGE_CONTINUING otherwise */
	virtual MessageClosureEnum messageSent(
				DCMessenger *messenger, Sock *sock );

		/* messageReceived() is called by DCMessenger upon successful
		   delivery of message
		   return: MESSAGE_FINISHED if sock may now be closed
		           MESSAGE_CONTINUING otherwise */
	virtual MessageClosureEnum messageReceived(
				DCMessenger *messenger, Sock *sock );

		/* messageSendFailed() is called by DCMessenger when message
		   delivery failed; details are in errstack. */
	virtual void messageSendFailed( DCMessenger *messenger );

		/* messageReceiveFailed() is called by DCMessenger when message
		   delivery failed; details are in errstack. */
	virtual void messageReceiveFailed( DCMessenger *messenger );

		/* reportSuccess() logs successful delivery of message */
	virtual void reportSuccess( DCMessenger *messenger );

		/* reportFailure() logs failure to deliver message */
	virtual void reportFailure( DCMessenger *messenger );

		/* descriptive name of this type of message */
	virtual char const *name();

		/* override default debug level (D_FULLDEBUG) */
	void setSuccessDebugLevel(int level) {m_msg_success_debug_level = level;}

		/* override default debug level (D_ALWAYS|D_FAILURE) */
	void setFailureDebugLevel(int level) {m_msg_failure_debug_level = level;}

		/* add an error message to the error stack */
	void addError( int code, char const *format, ... );

		/* this calls addError() after a failed call to sock->put()/get() */
	void sockFailed( Sock *sock );

		/* returns indicator of whether msg delivery succeeded or not */
	DeliveryStatus deliveryStatus() {return m_delivery_status;}

		/* sets msg delivery status */
	void deliveryStatus(DeliveryStatus s)
		{m_delivery_status = s;}

	friend class DCMessenger;
private:
	int m_cmd;
	int m_msg_success_debug_level;
	int m_msg_failure_debug_level;
	CondorError m_errstack;
	DCMessenger *m_messenger_callback_ref;
	DeliveryStatus m_delivery_status;

	void connectFailure( DCMessenger *messenger );

	void callMessageSendFailed( DCMessenger *messenger );
	void callMessageReceiveFailed( DCMessenger *messenger );
	MessageClosureEnum callMessageSent(
				DCMessenger *messenger, Sock *sock );
	MessageClosureEnum callMessageReceived(
				DCMessenger *messenger, Sock *sock );
};

class DCMessenger: public ClassyCountedPtr, public Service {
public:
		// This constructor is intended for use on the sending side,
		// where the peer is represented by a Daemon object.
		// Daemon should (in almost all cases) be allocated on the heap,
		// to ensure that it lasts for the lifetime of this DCMessenger
		// object.  Garbage collection is handled via classy_counted_ptr.
	DCMessenger( classy_counted_ptr<Daemon> daemon );

		// This constructor is intended for use on the receiving end
		// of a connection, where the peer is represented by an
		// existing sock, rather than a Daemon object.  This class
		// assumes ownership of sock and will delete it when the class
		// is destroyed.
	DCMessenger( Sock *sock );

	~DCMessenger();

		// Start a command, doing a non-blocking connection if necessary.
		// This operation calls inc/decRefCount() to manage garbage collection
		// of this messenger object as well as the message object.
	void startCommand( classy_counted_ptr<DCMsg> msg, Stream::stream_type st = Stream::reli_sock, int timeout = 0 );

		// Like startCommand(), except set a timer for delay seconds
		// before starting the command.
	void startCommandAfterDelay( unsigned int delay, classy_counted_ptr<DCMsg> msg, Stream::stream_type st = Stream::reli_sock, int timeout = 0 );

		// Send a message from beginning to end, right now.  By the time
		// this command returns, the message delivery status should be
		// set to success/failure, and the message delivery hooks will
		// have been called.
	void sendBlockingMsg( classy_counted_ptr<DCMsg> msg, Stream::stream_type st = Stream::reli_sock, int timeout = 0 );

		// Registers this messenger to receive notice when a message arrives
		// on this socket and then call msg->readMsg() when one does.
		// This operation calls inc/decRefCount() to manage garbage collection
		// of this messenger object as well as the message object.
		// This class also assumes ownership of sock and will delete it
		// when finished with the operation.
	void startReceiveMsg( classy_counted_ptr<DCMsg> msg, Sock *sock );

		// Write a message to a socket that is ready to receive it.
		// If you are starting a new command, simply call startCommand()
		// instead of calling this function directly.
	void writeMsg( classy_counted_ptr<DCMsg> msg, Sock *sock );

		// Read a message from a socket that is ready to be read.
		// Call startReceiveMsg() to initiate a non-blocking read
		// which will eventually call this function.
	void readMsg( classy_counted_ptr<DCMsg> msg, Sock *sock );

		// Returns information about who we are talking to.
	char const *peerDescription();

private:
		// This is called by DaemonClient when startCommand has finished.
	static void connectCallback(bool success, Sock *sock, CondorError *errstack, void *misc_data);

		// This is called by DaemonCore when the sock has data.
	int receiveMsgCallback(Stream *sock);

		// This is called by DaemonCore when the delay time has expired.
	int startCommandAfterDelay_alarm();

		// Delete a sock unless it happens to be m_sock.
	void doneWithSock(Stream *sock);

	classy_counted_ptr<Daemon> m_daemon; // our daemon client object (if any)
	Sock *m_sock;         // otherwise, we will just have a socket

	classy_counted_ptr<DCMsg> m_current_msg; // The current message waiting for a callback.
};


/*
 * DCStringMsg is a simple message consisting of one string.
 */

class DCStringMsg: public DCMsg {
public:
	DCStringMsg( int cmd, char const *str );

	bool writeMsg( DCMessenger *messenger, Sock *sock );
	bool readMsg( DCMessenger *messenger, Sock *sock );

private:
	MyString m_str;
};


#endif
