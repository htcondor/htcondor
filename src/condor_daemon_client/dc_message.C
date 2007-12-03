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
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "dc_message.h"
#include "daemon_core_sock_adapter.h"

DCMsg::DCMsg(int cmd) {
	m_cmd = cmd;
	m_msg_success_debug_level = D_FULLDEBUG;
	m_msg_failure_debug_level = (D_ALWAYS|D_FAILURE);
	m_delivery_status = DELIVERY_PENDING;
}

DCMsg::~DCMsg() {
}

char const *
DCMsg::name()
{
	return getCommandString( m_cmd );
}

void
DCMsg::callMessageSendFailed( DCMessenger *messenger )
{
	deliveryStatus( DELIVERY_FAILED );
	messageSendFailed( messenger );
}

void
DCMsg::callMessageReceiveFailed( DCMessenger *messenger )
{
	deliveryStatus( DELIVERY_FAILED );
	messageReceiveFailed( messenger );
}

DCMsg::MessageClosureEnum
DCMsg::callMessageSent(
				DCMessenger *messenger, Sock *sock )
{
	deliveryStatus( DELIVERY_SUCCEEDED );
	return messageSent( messenger, sock );
}

DCMsg::MessageClosureEnum
DCMsg::callMessageReceived(
				DCMessenger *messenger, Sock *sock )
{
	deliveryStatus( DELIVERY_SUCCEEDED );
	return messageReceived( messenger, sock );
}

DCMsg::MessageClosureEnum
DCMsg::messageSent( DCMessenger *messenger, Sock *)
{
	reportSuccess( messenger );

	return MESSAGE_FINISHED;
}

DCMsg::MessageClosureEnum
DCMsg::messageReceived( DCMessenger *messenger, Sock *)
{
	reportSuccess( messenger );

	return MESSAGE_FINISHED;
}

void
DCMsg::messageSendFailed( DCMessenger *messenger )
{
	reportFailure( messenger );
}

void
DCMsg::messageReceiveFailed( DCMessenger *messenger )
{
	reportFailure( messenger );
}

void
DCMsg::reportSuccess( DCMessenger *messenger )
{
	dprintf( m_msg_success_debug_level, "Sent %s to %s\n",
			 getCommandString( m_cmd),
			 messenger->peerDescription() );
}

void
DCMsg::reportFailure( DCMessenger *messenger )
{
	dprintf( m_msg_failure_debug_level, "Failed to send %s to %s: %s\n",
			 getCommandString( m_cmd ),
			 messenger->peerDescription(),
			 m_errstack.getFullText() );
}

void
DCMsg::addError( int code, char const *format, ... )
{
	va_list args;
	va_start(args, format);

	MyString msg;
	msg.vsprintf( format,args );
	m_errstack.push( "CEDAR", code, msg.Value() );

	va_end(args);
}

void
DCMsg::sockFailed( Sock *sock )
{
		// TODO: get specific error message from sock, if possible
	if( sock->is_encode() ) {
		addError( CEDAR_ERR_PUT_FAILED, "failed writing to socket" );
	}
	else {
		addError( CEDAR_ERR_GET_FAILED, "failed reading from socket" );
	}
}


DCMessenger::DCMessenger( classy_counted_ptr<Daemon> daemon )
{
	m_daemon = daemon;
	m_sock = NULL;
	m_current_msg = NULL;
}

DCMessenger::DCMessenger( Sock *sock )
{
	m_sock = sock;
	m_current_msg = NULL;
}

DCMessenger::~DCMessenger()
{
		// should never get deleted in the middle of a pending operation
	ASSERT(!m_current_msg.get());

	if( m_sock ) {
		delete m_sock;
	}
}

char const *DCMessenger::peerDescription()
{
	if( m_daemon.get() ) {
		return m_daemon->idStr();
	}
	if( m_sock ) {
		return m_sock->get_sinful_peer();
	}
	EXCEPT("No daemon or sock object in DCMessenger::peerDescription()");
	return NULL;
}

void DCMessenger::startCommand( classy_counted_ptr<DCMsg> msg, Stream::stream_type st, int timeout )
{
	MyString error;
		// For a UDP message, we may need to register two sockets, one for
		// the SafeSock and another for a ReliSock to establish the
		// security session.
	if( daemonCoreSockAdapter.TooManyRegisteredSockets(-1,&error,st==Stream::safe_sock?2:1) ) {
			// Try again in a sec
			// Eventually, it would be better to queue this centrally
			// (i.e. in DaemonCore) rather than having an independent
			// timer for each case.  Then it would be possible to control
			// priority of different messages etc.
		dprintf(D_FULLDEBUG, "Delaying delivery of %s to %s, because %s\n",
				msg->name(),peerDescription(),error.Value());
		startCommandAfterDelay( 1, msg, st, timeout );
		return;
	}

		// Currently, there may be only one pending operation per messenger.
	ASSERT(!m_current_msg.get());

	m_current_msg = msg;

	incRefCount();
	m_daemon->startCommand_nonblocking (
		msg->m_cmd,
		st,
		timeout,
		&msg->m_errstack,
		&DCMessenger::connectCallback,
		this );
}

void
DCMessenger::sendBlockingMsg( classy_counted_ptr<DCMsg> msg, Stream::stream_type st, int timeout )
{
	Sock *sock = m_daemon->startCommand (
		msg->m_cmd,
		st,
		timeout,
		&msg->m_errstack);

	if( !sock ) {
		msg->callMessageSendFailed( this );
		return;
	}

	writeMsg( msg, sock );
}

void
DCMessenger::doneWithSock(Stream *sock)
{
		// If sock == m_sock, it will be cleaned up when the messenger
		// is deleted.  Otherwise, do it now.
	if( sock != m_sock ) {
		if( sock ) {
			delete sock;
		}
	}
}

void
DCMessenger::connectCallback(bool success, Sock *sock, CondorError *, void *misc_data)
{
	ASSERT(misc_data);

	DCMessenger *self = (DCMessenger *)misc_data;
	classy_counted_ptr<DCMsg> msg = self->m_current_msg;
	self->m_current_msg = NULL;

	if(!success) {
		msg->callMessageSendFailed( self );
		self->doneWithSock(sock);
	}
	else {
		ASSERT(sock);
		self->writeMsg( msg, sock );
	}

	self->decRefCount();
}

void DCMessenger::writeMsg( classy_counted_ptr<DCMsg> msg, Sock *sock )
{
	ASSERT( msg.get() );
	ASSERT( sock );

	incRefCount();

		/* Some day, we may send message asynchronously and call
		   messageSent() later, after the delivery.  For now, we do it
		   all synchronously, right here. */

	sock->encode();

	if( !msg->writeMsg( this, sock ) ) {
		msg->callMessageSendFailed( this );
		doneWithSock(sock);
	}
	else if( !sock->eom() ) {
		msg->addError( CEDAR_ERR_EOM_FAILED, "failed to send EOM" );
		msg->callMessageSendFailed( this );
		doneWithSock(sock);
	}
	else {
			// Success
		DCMsg::MessageClosureEnum closure = msg->callMessageSent( this, sock );

		switch( closure ) {
		case DCMsg::MESSAGE_FINISHED:
			doneWithSock(sock);
			break;
		case DCMsg::MESSAGE_CONTINUING:
			break;
		}
	}

	decRefCount();
}

void DCMessenger::startReceiveMsg( classy_counted_ptr<DCMsg> msg, Sock *sock )
{
		// Currently, only one pending message per messenger.
	ASSERT( !m_current_msg.get() );

	MyString name;
	name.sprintf("<%s>", msg->name());

	incRefCount();

	int reg_rc = daemonCoreSockAdapter.
		Register_Socket( sock, peerDescription(),
						 (SocketHandlercpp)&DCMessenger::receiveMsgCallback,
						 name.Value(), this, ALLOW );
	if(reg_rc < 0) {
		msg->addError(
			CEDAR_ERR_REGISTER_SOCK_FAILED,
			"failed to register socket (Register_Socket returned %d)",
			reg_rc );
		msg->callMessageSendFailed( this );
		doneWithSock(sock);
		decRefCount();
		return;
	}

	m_current_msg = msg; // prevent msg from going out of reference
	ASSERT(daemonCoreSockAdapter.Register_DataPtr( msg.get() ));
}

int
DCMessenger::receiveMsgCallback(Stream *sock)
{
	classy_counted_ptr<DCMsg> msg = (DCMsg *) daemonCoreSockAdapter.GetDataPtr();
	ASSERT(msg.get());
	DCMessenger *self = msg->m_messenger_callback_ref;
	ASSERT(self);

	m_current_msg = NULL;

	daemonCoreSockAdapter.Cancel_Socket( sock );

	if( !msg->readMsg( this, (Sock *)sock ) ) {
		msg->callMessageReceiveFailed( this );
		doneWithSock(sock);
	}
	else {
		ASSERT( sock );
		readMsg( msg, (Sock *)sock );
	}

	decRefCount();
	return KEEP_STREAM;
}

void
DCMessenger::readMsg( classy_counted_ptr<DCMsg> msg, Sock *sock )
{
	ASSERT( msg.get() );
	ASSERT( sock );

	incRefCount();

	sock->decode();

	if( !msg->readMsg( this, sock ) ) {
		msg->callMessageReceiveFailed( this );
	}
	else if( !sock->eom() ) {
		msg->addError( CEDAR_ERR_EOM_FAILED, "failed to read EOM" );
		msg->callMessageReceiveFailed( this );
	}
	else {
			// Success
		DCMsg::MessageClosureEnum closure = msg->callMessageReceived( this, sock );

		switch( closure ) {
		case DCMsg::MESSAGE_FINISHED:
			doneWithSock(sock);
			break;
		case DCMsg::MESSAGE_CONTINUING:
			break;
		}
	}

	decRefCount();
}

struct QueuedCommand {
	classy_counted_ptr<DCMsg> msg;
	Stream::stream_type stream_type;
	int timeout;
	int timer_handle;
};

void
DCMessenger::startCommandAfterDelay( unsigned int delay, classy_counted_ptr<DCMsg> msg, Stream::stream_type st, int timeout )
{
	QueuedCommand *qc = new QueuedCommand;
	qc->msg = msg;
	qc->stream_type = st;
	qc->timeout = timeout;

	incRefCount();
	qc->timer_handle = daemonCoreSockAdapter.Register_Timer(
		delay,
		(Eventcpp)&DCMessenger::startCommandAfterDelay_alarm,
		"DCMessenger::startCommandAfterDelay",
		this );
	ASSERT(qc->timer_handle != -1);
	daemonCoreSockAdapter.Register_DataPtr( qc );
}

int DCMessenger::startCommandAfterDelay_alarm()
{
	QueuedCommand *qc = (QueuedCommand *)daemonCoreSockAdapter.GetDataPtr();
	ASSERT(qc);

	startCommand(qc->msg,qc->stream_type,qc->timeout);

	delete qc;
	decRefCount();



	return TRUE;
}

DCStringMsg::DCStringMsg( int cmd, char const *str ):
	DCMsg( cmd )
{
	m_str = str;
}

bool DCStringMsg::writeMsg( DCMessenger *, Sock *sock )
{
	if( !sock->put( m_str.Value() ) ) {
		sockFailed( sock );
		return false;
	}
	return true;
}

bool DCStringMsg::readMsg( DCMessenger *, Sock *sock )
{
	char *str = NULL;
	if( !sock->get( str ) ){
		sockFailed( sock );
		return false;
	}
	m_str = str;
	free(str);

	return true;
}
