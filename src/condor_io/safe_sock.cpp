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


 

/* Note: this code needs to have error handling overhauled */

#include "condor_common.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_config.h"
#include "condor_netdb.h"
#include "selector.h"
#include "condor_sockfunc.h"
#include "condor_sinful.h"
#include "ipv6_hostname.h"
#include "condor_daemon_core.h"

_condorMsgID SafeSock::_outMsgID = {0, 0, 0, 0};
unsigned long SafeSock::_noMsgs = 0;
unsigned long SafeSock::_whole = 0;
unsigned long SafeSock::_deleted = 0;
unsigned long SafeSock::_avgSwhole = 0;
unsigned long SafeSock::_avgSdeleted = 0;


/* 
   NOTE: All SafeSock constructors initialize with this, so you can
   put any shared initialization code here.  -Derek Wright 3/12/99
*/
void SafeSock::init()
{
	_special_state = safesock_none;

	for(int i=0; i<SAFE_SOCK_HASH_BUCKET_SIZE; i++)
		_inMsgs[i] = NULL;
	_msgReady = false;
	_longMsg = NULL;
	_tOutBtwPkts = SAFE_SOCK_MAX_BTW_PKT_ARVL;

	// initialize msgID
	if(_outMsgID.msgNo == 0) { // first object of this class
		// [TODO:IPv6] Remove it!
		//_outMsgID.ip_addr = (unsigned long)my_ip_addr();

		_outMsgID.ip_addr = get_csrng_uint();
		_outMsgID.pid = get_csrng_uint() % 65536;
		_outMsgID.time = get_csrng_uint();
		_outMsgID.msgNo = get_csrng_uint();
	}

    mdChecker_     = NULL;

	m_udp_network_mtu = -1;
	m_udp_loopback_mtu = -1;
}


SafeSock::SafeSock() 				/* virgin safesock	*/
	: Sock()
{
	init();
}

SafeSock::SafeSock(const SafeSock & orig) 
	: Sock(orig)
{
	init();
	// now copy all cedar state info via the serialize() method
	std::string buf;
	orig.serialize(buf);	// get state from orig sock
	deserialize(buf.c_str());	// put the state into the new sock
}

Stream *
SafeSock::CloneStream()
{
	return new SafeSock(*this);
}

SafeSock::~SafeSock()
{
	_condorInMsg *tempMsg, *delMsg;

	for(int i=0; i<SAFE_SOCK_HASH_BUCKET_SIZE; i++) {
		tempMsg = _inMsgs[i];
		while(tempMsg) {
			delMsg = tempMsg;
			tempMsg = tempMsg->nextMsg;
			delete delMsg;
		}
		_inMsgs[i] = NULL;
	}
	close();

    delete mdChecker_;
}

int SafeSock::close()
{
	return Sock::close();
}


/* End of the current message
 * This method will be called when the application program reaches
 * the end of the current message.
 * In the encode mode, the current message is marshalled with _outMsgID
 * and sent to _who, and
 *	@returns: TRUE, if the message has been successfully sent
 *	        : FALSE, if failed
 *
 * In the decode mode, the current message is deleted from the input buffer,
 * if it is long, and _msgReady is set to false
 *	@returns: TRUE, if all the data of the current message has been consumed
 *	          FALSE, if not
 */
int SafeSock::end_of_message()
{
	int ret_val = FALSE;
	int sent;
        unsigned char * md = 0;

	switch(_coding){
		case stream_encode:
                    if (mdChecker_) {
	    		md = mdChecker_->computeMD();
		    }
                    sent = _outMsg.sendMsg(_sock, _who, 
				           _outMsgID, md);
		    if (md) {
		    	free(md);
		    }
                    _outMsgID.msgNo++; // It doesn't hurt to increment msgNO even if fails
                    resetCrypto();

                    if (sent < 0) {
                        return FALSE;
                    } else {
                        return TRUE;
                    }
		case stream_decode:
			if(_msgReady) {
				if(_longMsg) { // long message is ready
					if(_longMsg->consumed())
						ret_val = TRUE;
					// unlink the message
					if(_longMsg->prevMsg)
						_longMsg->prevMsg->nextMsg = _longMsg->nextMsg;
					else {
						int index = labs(_longMsg->msgID.ip_addr +
						                 _longMsg->msgID.time +
								     _longMsg->msgID.msgNo) % SAFE_SOCK_HASH_BUCKET_SIZE;
						_inMsgs[index] = _longMsg->nextMsg;
					}
					if(_longMsg->nextMsg)
						_longMsg->nextMsg->prevMsg = _longMsg->prevMsg;
					// delete the message
					delete _longMsg;
					_longMsg = NULL;
				} else { // short message is ready
					if(_shortMsg.consumed())
						ret_val = TRUE;
					_shortMsg.reset();
				}
				_msgReady = false;
			} else { 
				// message is not ready
				ret_val = TRUE;
			}
            resetCrypto();
			setTriedAuthentication(false);
			break;

		default:
            resetCrypto();
            setTriedAuthentication(false);
			break;
	}
			
	if ( allow_empty_message_flag ) {
		allow_empty_message_flag = FALSE;
		ret_val = TRUE;
	}

	return ret_val;
}

bool
SafeSock::peek_end_of_message()
{
	if(_msgReady) {
		if(_longMsg) { // long message is ready
			if(_longMsg->consumed()) {
				return true;
			}
		} else { // short message is ready
			if(_shortMsg.consumed())
				return true;
		}
	}
	return false;
}

MSC_DISABLE_WARNING(6262) // function uses 64k of stack
const char *
SafeSock::my_ip_str() const
{
	//
	// FIXME: Do we still need to create and destroy a socket every time?
	//

	if(_state != sock_connect) {
		dprintf(D_ALWAYS,"ERROR: SafeSock::my_ip_str() called on socket that is not in connected state\n");
		return NULL;
	}

	if(_my_ip_buf[0]) {
		// return cached result
		return _my_ip_buf;
	}

	SafeSock s;
	if( ! s.bind(_who.get_protocol(), true, 0, false) )
	{
		dprintf(D_ALWAYS,"ERROR: SafeSock::my_ip_str()'s attempt to bind a new SafeSock failed.\n");
		return NULL;
	}

	if (s._state != sock_bound) {
		dprintf(D_ALWAYS,
		        "SafeSock::my_ip_str() failed to bind: _state = %d\n",
			  s._state);
		return NULL;
	}

	if (condor_connect(s._sock, _who) != 0) {
#if defined(WIN32)
		int lasterr = WSAGetLastError();
		dprintf( D_ALWAYS, "SafeSock::my_ip_str() failed to connect, errno = %d\n",
				 lasterr );
#else
		dprintf( D_ALWAYS, "SafeSock::my_ip_str() failed to connect, errno = %d\n",
				 errno );
#endif
		return NULL;
	}

	condor_sockaddr addr;
	addr = s.my_addr();
	strcpy(_my_ip_buf, addr.to_ip_string().c_str());
	return _my_ip_buf;
}
MSC_RESTORE_WARNING(6262) // function uses 64k of stack

int SafeSock::connect(
	char const	*host,
	int		port,
	bool,
	CondorError * errorStack
	)
{
	if (!host || port < 0) return FALSE;

	std::string addr;
	if( chooseAddrFromAddrs( host, addr, &_who ) ) {
		host = addr.c_str();
		set_connect_addr(addr.c_str());
	} else {
		_who.clear();
		if (!Sock::guess_address_string(host, port, _who))
			return FALSE;

		if (host[0] == '<') {
			set_connect_addr(host);
		} else {
			set_connect_addr(_who.to_sinful().c_str());
		}
	}
	addr_changed();

	// now that we have set _who (useful for getting informative
	// peer_description), see if we should do a reverse connect
	// instead of a forward connect
	int retval=special_connect(host,port,true,errorStack);
	if( retval != CEDAR_ENOCCB ) {
		return retval;
	}

	/* we bind here so that a sock may be	*/
	/* assigned to the stream if needed		*/
	/* TRUE means this is an outgoing connection */
	if (_state == sock_virgin || _state == sock_assigned) {
		bind( _who.get_protocol(), true, 0, false );
	}

	if (_state != sock_bound) {
		dprintf(D_ALWAYS,
		        "SafeSock::connect bind() failed: _state = %d\n",
			  _state); 
		return FALSE;
	}
	
	// Set MTU based on if we are connecting to loopback or not.  Do this
	// in such a way that we only param once for this safesock, instead of param-ing
	// every time we do a connect, because often times one safe sock is created
	// and then used to connect to different hosts over and over.
	if (m_udp_network_mtu == -1) {
		m_udp_network_mtu = param_integer("UDP_NETWORK_FRAGMENT_SIZE",
									DEFAULT_SAFE_MSG_FRAGMENT_SIZE);
	}
	if (m_udp_loopback_mtu == -1) {
		m_udp_loopback_mtu = param_integer("UDP_LOOPBACK_FRAGMENT_SIZE",
									SAFE_MSG_MAX_PACKET_SIZE-SAFE_MSG_HEADER_SIZE-1);
	}
	if ( _who.is_loopback() ) {
		_outMsg.set_MTU( m_udp_loopback_mtu );
	} else {
		_outMsg.set_MTU( m_udp_network_mtu );
	}

	_state = sock_connect;
	return TRUE;
}

/* Put 'sz' bytes of data to _outMsg buffer
 * This method puts 'sz' bytes into the message, while adding packets as needed
 *	@returns: the number of bytes actually stored into _outMsg buffer,
 *	          including '0'
 */
int SafeSock::put_bytes(const void *data, int sz)
{
	int bytesPut;


    // Check to see if we need to encrypt
    // This works only because putn will actually put all 
    if (get_encryption()) {
		int l_out;
		unsigned char * dta = 0;
        if (!wrap((const unsigned char *)data, sz, dta , l_out)) {
            dprintf(D_SECURITY, "Encryption failed\n");
            return -1;  // encryption failed!
        }
			// Now, add to the MAC
		if (mdChecker_) {
			mdChecker_->addMD(dta, sz);
		}

		bytesPut = _outMsg.putn((char *)dta, sz);
    
		free(dta);
		return bytesPut;

    } else {
			// Now, add to the MAC
		if (mdChecker_) {
			mdChecker_->addMD((const unsigned char *)data, sz);
		}

		bytesPut = _outMsg.putn((const char *)data, sz);
		return bytesPut;
	}
}


/*
 * copy n bytes from the ready message into 'dta'
 * This method copies next 'size' bytes either from '_longMsg' or from '_shortMsg',
 * depending on whether the ready message is long or short one
 *
 * param: dta - buffer to which data will be copied
 *        size - 'size' bytes
 * return: the number of bytes actually copied, if success
 *         -1, if failed
 */
int SafeSock::get_bytes(void *dta, int size)
{
	ASSERT( size > 0 );
	while(!_msgReady) {
		if(_timeout > 0) {
			Selector selector;
			selector.set_timeout( _timeout );
			selector.add_fd( _sock, Selector::IO_READ );
				
			selector.execute();
			if ( selector.timed_out() ) {
				return 0;
			} else if ( !selector.has_ready() ) {
				dprintf(D_NETWORK, "select returns %d, recv failed\n",
						selector.select_retval());
				return 0;
			}
		}
		(void)handle_incoming_packet();
	}

	int readSize, length;
    unsigned char * dec;

	if (get_encryption()) {
		if(_longMsg) {
				// long message 
			readSize = _longMsg->getn((char *)dta, size);
		}
		else { 
				// short message
			readSize = _shortMsg.getn((char *)dta, size);
		}

		if (readSize == size) {
			unwrap((unsigned char *) dta, readSize, dec, length);
			memcpy(dta, dec, readSize);
			free(dec);
			return readSize;
		} else {
			dprintf(D_NETWORK,
					"SafeSock::get_bytes - failed because bytes read is different from bytes requested\n");
			return -1;
		}
	} else {
			// no encryption
		if(_longMsg) {
				// long message 
			readSize = _longMsg->getn((char *)dta, size);
		}
		else { 
				// short message
			readSize = _shortMsg.getn((char *)dta, size);
		}

		if (readSize == size) {
			return readSize;
		} else {
			dprintf(D_NETWORK,
					"SafeSock::get_bytes - failed because bytes read is different from bytes requested\n");
			return -1;
		}
	}
}


/* Get arbitrary bytes of data
 * This method copies arbitrary bytes from the ready message
 * to the buffer pointed by 'ptr', starting from the current position
 * until the given 'delim' get encountered.
 *	@returns: the result from _longMsg.getPtr or _shortMsg.getPtr
 *	          : the number of bytes in the buffer pointed by 'ptr'
 *	          : -1, if failed
 *
 * Notice: the buffer pointed by 'ptr' will be allocated this time and deleted
 *         later by the next call to this method.
 *         So 'ptr' must point nothing and caller must NOT free 'ptr'
 */
int SafeSock::get_ptr(void *&ptr, char delim)
{
	int size;

	while(!_msgReady) {
		if(_timeout > 0) {
			Selector selector;
			selector.set_timeout( _timeout );
			selector.add_fd( _sock, Selector::IO_READ );
				
			selector.execute();
			if ( selector.timed_out() ) {
				return 0;
			} else if ( !selector.has_ready() ) {
					dprintf(D_NETWORK,
					        "select returns %d, recv failed\n",
						  selector.select_retval() );
					return 0;
			}
		}
		(void)handle_incoming_packet();
	}

	if(_longMsg) { // long message
		size = _longMsg->getPtr(ptr, delim);
		return size;
	}
	else { // short message
		size = _shortMsg.getPtr(ptr, delim);
		return size;
	}
}


int SafeSock::peek(char &c)
{
	while(!_msgReady) {
		if(_timeout > 0) {
			Selector selector;
			selector.set_timeout( _timeout );
			selector.add_fd( _sock, Selector::IO_READ );
				
			selector.execute();
			if ( selector.timed_out() ) {
				return 0;
			} else if ( !selector.has_ready() ) {
					dprintf(D_NETWORK,
					        "select returns %d, recv failed\n",
						  selector.select_retval() );
					return 0;
			}
		}
		(void)handle_incoming_packet();
	}

	if(_longMsg) // long message
		return _longMsg->peek(c);
	else // short message
		return _shortMsg.peek(c);
}


/* Receive a packet
 *
 * This method receives a packet from _sock UDP socket and buffer it appropriatly
 * return: TRUE, if a message becomes ready
 *         FALSE, if not ready
 * side-effect: _who will be updated to the address of the sender
 */
int SafeSock::handle_incoming_packet()
{

	bool last;
	int seqNo, length;
	_condorMsgID mID;
	void* data;
	int index;
	int received;
	_condorInMsg *tempMsg, *delMsg, *prev = NULL;
	time_t curTime;

    addr_changed(); // Not yet, but it is about to

	if( _msgReady ) {
		char const *existing_msg_type;
		bool existing_consumed;
		if( _longMsg ) {
			existing_msg_type = "long";
			existing_consumed = _longMsg->consumed();
		}
		else {
			existing_msg_type = "short";
			existing_consumed = _shortMsg.consumed();
		}
		dprintf( D_ALWAYS,
				 "ERROR: receiving new UDP message but found a %s "
				 "message still waiting to be closed (consumed=%d). "
				 "Closing it now.\n",
				 existing_msg_type, existing_consumed );

			// Now force end_of_message() to be called.  This is especially
			// important if we receive a short UDP message and a long
			// message is still unclosed, because the long message will
			// continue to act as the source for all read operations.

		stream_coding saved_coding = _coding;
		_coding = stream_decode;
		end_of_message();
		_coding = saved_coding;
	}


	received = condor_recvfrom(_sock, _shortMsg.dataGram, 
							   SAFE_MSG_MAX_PACKET_SIZE, 0, _who);

	if(received < 0) {
		dprintf(D_NETWORK, "recvfrom failed: errno = %d\n", errno);
		return FALSE;
	}

	if (IsDebugLevel(D_NETWORK))
    	dprintf( D_NETWORK, "RECV %d bytes at %s from %s\n",
			 received, sock_to_string(_sock), _who.to_sinful().c_str());

	length = received;
    _shortMsg.reset(); // To be sure
	
	bool is_full_message = _shortMsg.getHeader(received, last, seqNo, length, mID, data);
	if ( length <= 0 || length > SAFE_MSG_MAX_PACKET_SIZE ) {
		// someone maybe sending us random garbage datagrams?
		dprintf(D_ALWAYS,"IO: Incoming datagram improperly sized\n");
		return FALSE;
	}

    if ( is_full_message ) {
        _shortMsg.curIndex = 0;
        _msgReady = true;
        _whole++;
        if(_whole == 1)
            _avgSwhole = length;
        else
            _avgSwhole = ((_whole - 1) * _avgSwhole + length) / _whole;
        
        _noMsgs++;
        dprintf( D_NETWORK, "\tFull msg [%d bytes]\n", length);
        return TRUE;
    }

    dprintf( D_NETWORK, "\tFrag [%d bytes]\n", length);
    
    /* long message */
    curTime = (unsigned long)time(NULL);
    index = labs(mID.ip_addr + mID.time + mID.msgNo) % SAFE_SOCK_HASH_BUCKET_SIZE;
    tempMsg = _inMsgs[index];
    while(tempMsg != NULL && !same(tempMsg->msgID, mID)) {
        prev = tempMsg;
        tempMsg = tempMsg->nextMsg;
        // delete 'timeout'ed message
        if(curTime - prev->lastTime > _tOutBtwPkts) {
            dprintf(D_NETWORK, "found timed out msg: cur=%lu, msg=%lu\n",
                    curTime, prev->lastTime);
            delMsg = prev;
            prev = delMsg->prevMsg;
            if(prev)
                prev->nextMsg = delMsg->nextMsg;
            else  // delMsg is the 1st message in the chain
                _inMsgs[index] = tempMsg;
            if(tempMsg)
                tempMsg->prevMsg = prev;
            _deleted++;
            if(_deleted == 1)
                _avgSdeleted = delMsg->msgLen;
            else     {
                _avgSdeleted = ((_deleted - 1) * _avgSdeleted + delMsg->msgLen) / _deleted;
            }   
            dprintf(D_NETWORK, "Deleting timeouted message:\n");
            delMsg->dumpMsg();
            delete delMsg;
        }   
    }   
    if(tempMsg != NULL) { // found
        if (seqNo == 0) {
            tempMsg->set_sec(_shortMsg.isDataHashed(),
                    _shortMsg.md(),
                    _shortMsg.isDataEncrypted());
        }
        bool rst = tempMsg->addPacket(last, seqNo, length, data);
        //dprintf(D_NETWORK, "new packet added\n");
        //tempMsg->dumpMsg();
        if (rst) {
            _longMsg = tempMsg;
            _msgReady = true;
            _whole++;
            if(_whole == 1)
                _avgSwhole = _longMsg->msgLen;
            else
                _avgSwhole = ((_whole - 1) * _avgSwhole + _longMsg->msgLen) / _whole;
            return TRUE;
        }
        return FALSE;
    } else { // not found
        if(prev) { // add a new message at the end of the chain
            prev->nextMsg = new _condorInMsg(mID, last, seqNo, length, data, 
                                             _shortMsg.isDataHashed(),
                                             _shortMsg.md(), 
                                             _shortMsg.isDataEncrypted(), prev);
            if(!prev->nextMsg) {    
                EXCEPT("Error:handle_incomming_packet: Out of Memory");
            }
            //dprintf(D_NETWORK, "new msg created\n");
            //prev->nextMsg->dumpMsg();
        } else { // first message in the bucket
            _inMsgs[index] = new _condorInMsg(mID, last, seqNo, length, data, 
                                              _shortMsg.isDataHashed(),
                                              _shortMsg.md(), 
                                              _shortMsg.isDataEncrypted(), NULL);
            if(!_inMsgs[index]) {
                EXCEPT("Error:handle_incomming_packet: Out of Memory");
            }
            //dprintf(D_NETWORK, "new msg created\n");
            //_inMsgs[index]->dumpMsg();
        }
        _noMsgs++;
        return FALSE;
    }   
}


void SafeSock::getStat(unsigned long &noMsgs,
			     unsigned long &noWhole,
			     unsigned long &noDeleted,
			     unsigned long &avgMsgSize,
                       unsigned long &szComplete,
			     unsigned long &szDeleted)
{
	noMsgs = _noMsgs;
	noWhole = _whole;
	noDeleted = _deleted;
	avgMsgSize = _outMsg.getAvgMsgSize();
	szComplete = _avgSwhole;
	szDeleted = _avgSdeleted;
}

void SafeSock::resetStat()
{
	_noMsgs = 0;
	_whole = 0;
	_deleted = 0;
	_avgSwhole = 0;
	_avgSdeleted = 0;
}


#ifndef WIN32
	// interface no longer supported
int SafeSock::attach_to_file_desc(int fd)
{
	if (_state != sock_virgin) return FALSE;

	_sock = fd;
	_state = sock_connect;
	timeout(0); // make certain in block mode
	return TRUE;
}
#endif


void SafeSock::serialize(std::string& outbuf) const
{
	Sock::serialize(outbuf);

	formatstr_cat( outbuf, "%d*%s*", _special_state, _who.to_sinful().c_str() );
}

const char * SafeSock::deserialize(const char *buf)
{
	char * sinful_string = NULL;
	const char *ptmp, *ptr = NULL;
    
	ASSERT(buf);
	// here we want to restore our state from the incoming buffer

	// first, let our parent class restore its state
	ptmp = Sock::deserialize(buf);
	ASSERT( ptmp );
	int itmp;
	int citems = sscanf(ptmp,"%d*",&itmp);
	if (1 == citems)
		_special_state=safesock_state(itmp);
    // skip through this
    ptmp = strchr(ptmp, '*');
    if(ptmp) ptmp++;

    // Now, see if we are 6.3 or 6.2
    if (ptmp && (ptr = strchr(ptmp, '*')) != NULL) {
        // We are 6.3
		sinful_string = new char [1 + ptr - ptmp];
        memcpy(sinful_string, ptmp, ptr - ptmp);
		sinful_string[ptr - ptmp] = 0;
        ptmp = ++ptr;
    }
    else if(ptmp) {
		size_t sinful_len = strlen(ptmp);
		sinful_string = new char [1 + sinful_len];
        citems = sscanf(ptmp,"%s",sinful_string);
		if (1 != citems) sinful_string[0] = 0;
		sinful_string[sinful_len] = 0;
    }

	_who.from_sinful(sinful_string);
	delete [] sinful_string;

	return NULL;
}

const char * SafeSock :: isIncomingDataHashed()
{
    char c;
    if (!peek(c)) {
        return 0;
    }
    else {
        if(_longMsg) {
            // long message 
            return _longMsg->isDataHashed();
        }
        else { // short message
            return _shortMsg.isDataHashed();
        }
    }
}

const char * SafeSock :: isIncomingDataEncrypted()
{
    char c;
    if (!peek(c)) {
        return 0;
    }
    else {
        if(_longMsg) {
            // long message 
            return _longMsg->isDataEncrypted();
        }
        else { // short message
            return _shortMsg.isDataEncrypted();
        }
    }
}

bool SafeSock :: set_encryption_id(const char * keyId)
{
    return _outMsg.set_encryption_id(keyId);
}

bool SafeSock :: init_MD(CONDOR_MD_MODE /* mode */, KeyInfo * key, const char * keyId)
{
    bool inited = true;
   
    if (mdChecker_) {   
        delete mdChecker_;
        mdChecker_ = 0;
    }
    if (key) {
        mdChecker_ = new Condor_MD_MAC(key);
    }

    if (_longMsg) {
        inited = _longMsg->verifyMD(mdChecker_);
    }
    else {
        inited = _shortMsg.verifyMD(mdChecker_);
    }

    if( !_outMsg.init_MD(keyId) ) {
        inited = false;
    }

    return inited;
}

#ifdef DEBUG
int SafeSock::getMsgSize()
{
	int result;

	while(!_msgReady) {
		if(_timeout > 0) {
			Selector selector;
			selector.set_timeout( _timeout );
			selector.add_fd( _sock, Selector::IO_READ );
				
			selector.execute();
			if ( selector.timed_out() ) {
				return 0;
			} else if ( !selector.has_ready() ) {
					dprintf(D_NETWORK,
					        "select returns %d, recv failed\n",
						  selector.select_retval() );
					return FALSE;
			}
		}
		(void)handle_incoming_packet();
	}

	if(_longMsg)
		return _longMsg->msgLen;
	return _shortMsg.length;
}

void SafeSock::dumpSock()
{
	_condorInMsg *tempMsg;

	dprintf(D_NETWORK, "[In] Long Messages\n");
	for(int i=0; i<SAFE_SOCK_HASH_BUCKET_SIZE; i++) {
		dprintf(D_NETWORK, "\nBucket [%d]\n", i);
		tempMsg = _inMsgs[i];
		while(tempMsg) {
			tempMsg->dumpMsg();
			tempMsg = tempMsg->nextMsg;
		}
	}

	dprintf(D_NETWORK, "\n\n[In] Short Message\n");
	if(_msgReady && _longMsg == NULL)
		_shortMsg.dumpPacket();
	
	dprintf(D_NETWORK, "\n\n[Out] out message\n");
	_outMsg.dumpMsg(_outMsgID);
}
#endif

void
SafeSock::cancel_reverse_connect() {
}

void
SafeSock::setTargetSharedPortID( char const *id)
{
	if( id ) {
		dprintf(D_ALWAYS,
			"WARNING: UDP does not support connecting to a shared port! "
			"(requested address is %s with SharedPortID=%s)\n",
			peer_description(), id);
	}
}

bool
SafeSock::sendTargetSharedPortID()
{
		// do nothing; shared ports are not currently supported by UDP
	return true;
}

bool
SafeSock::msgReady() {
	return _msgReady;
}

/* static */ int
SafeSock::recvQueueDepth(int port) {
	int depth = 0;
#ifdef LINUX
	FILE *f = NULL;

	f = fopen("/proc/net/udp", "r");
	if (!f) {
		dprintf(D_ALWAYS, "Cannot open /proc/net/udp, no UDP statistics will be available\n");
		return 0;
	}
	// /proc/net/udp entries look like
	//   sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode ref pointer drops             
	// 1: 00000000:9C7E 00000000:0000 07 00000000:00000000 00:00000000 00000000 28297        0 570473 2 ffff8803b781fc00 0     

	// skip first line, it is a header
	char skipLine[256];
	if (fgets(skipLine, sizeof(skipLine), f) == NULL) {
		fclose(f);
		return 0;
	};

	int sl = 0;
	int localAddr = 0;
	int localPort = 0;
	int remoteAddr = 0;
	int remotePort = 0;
	int status = 0;
	int tx_queue = 0;
	int queueDepth = 0;
	while (fscanf(f, "%d: %x:%x %x:%x %x %x:%x\n", &sl, &localAddr, &localPort, &remoteAddr, &remotePort, &status, &tx_queue, &queueDepth) > 1) {

		if (localPort == port) {
			depth = queueDepth;
		}
		// skip to beginning of next line
		if (fgets(skipLine, sizeof(skipLine), f) == NULL) {
			dprintf(D_ALWAYS, "Error skipping to end of in /proc/net/udp\n");
			fclose(f);
			return -1;
		}
	}
	fclose(f);
#else
	// Shut the compiler up
	(void)port;
#endif
	return depth;
}

int
SafeSock::do_reverse_connect(char const *,bool,CondorError *)
{
	dprintf(D_ALWAYS,
			"CCBClient: WARNING: UDP not supported by CCB."
			"  Will therefore try to send packet directly to %s.\n",
			peer_description());

	return CEDAR_ENOCCB;
}

int
SafeSock::do_shared_port_local_connect( char const *, bool, char const * )
{
	dprintf(D_ALWAYS,
			"SharedPortClient: WARNING: UDP not supported."
			"  Failing to connect to %s.\n",
			peer_description());

	return 0;
}
