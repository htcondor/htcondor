/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

/* Note: this code needs to have error handling overhauled */

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_socket_types.h"
#include "condor_string.h"

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

	// initialize username
	_fqu = NULL;

	for(int i=0; i<SAFE_SOCK_HASH_BUCKET_SIZE; i++)
		_inMsgs[i] = NULL;
	_msgReady = false;
	_longMsg = NULL;
	_tOutBtwPkts = SAFE_SOCK_MAX_BTW_PKT_ARVL;

	// initialize msgID
	if(_outMsgID.msgNo == 0) { // first object of this class
		_outMsgID.ip_addr = (unsigned long)my_ip_addr();
		_outMsgID.pid = (short)getpid();
		_outMsgID.time = (unsigned long)time(NULL);
		_outMsgID.msgNo = (unsigned long)get_random_int();
	}

    _authenticated = false;
    mdChecker_     = NULL;
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
	char *buf = NULL;
	buf = orig.serialize();	// get state from orig sock
	ASSERT(buf);
	serialize(buf);	// put the state into the new sock
	delete [] buf;
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
	if (_fqu) {
		delete []_fqu;
	}

    delete mdChecker_;
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
                    sent = _outMsg.sendMsg(_sock, (struct sockaddr *)&_who, 
				           _outMsgID, md);
		    if (md) {
		    	free(md);
		    }
                    _outMsgID.msgNo++; // It doesn't hurt to increment msgNO even if fails
                    resetCrypto();

                    if ( allow_empty_message_flag ) {
                        allow_empty_message_flag = FALSE;
                        return TRUE;
                    }
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
            _authenticated = false;
			break;

		default:
            resetCrypto();
            _authenticated = false;
			break;
	}
			
	if ( allow_empty_message_flag ) {
		allow_empty_message_flag = FALSE;
		ret_val = TRUE;
	}

	return ret_val;
}


int SafeSock::connect(
	char	*host,
	int		port, 
	bool
	)
{
	struct hostent	*hostp = NULL;
	unsigned long	inaddr = 0;

	if (!host || port < 0) return FALSE;

	/* we bind here so that a sock may be	*/
	/* assigned to the stream if needed		*/
	if (_state == sock_virgin || _state == sock_assigned) bind();

	if (_state != sock_bound) {
		dprintf(D_ALWAYS,
		        "SafeSock::connect bind() failed: _state = %d\n",
			  _state); 
		return FALSE;
	}
	
	memset(&_who, 0, sizeof(sockaddr_in));
	_who.sin_family = AF_INET;
	_who.sin_port = htons((u_short)port);

	/* might be in <x.x.x.x:x> notation, i.e. sinfull string */
	if (host[0] == '<') {
		string_to_sin(host, &_who);
	}
	/* try to get a decimal notation first 			*/
	else if( (inaddr=inet_addr(host)) != (unsigned long)(-1L) ) {
		memcpy((char *)&_who.sin_addr, &inaddr, sizeof(inaddr));
	} else {
		/* if dotted notation fails, try host database	*/
		hostp = gethostbyname(host);
		if( hostp == (struct hostent *)0 ) {
			return FALSE;
		} else {
			memcpy(&_who.sin_addr, hostp->h_addr, sizeof(hostp->h_addr));
		}
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
	int bytesPut, l_out;
    unsigned char * dta = 0;

    //char str[10000];
    //str[0] = 0;
    //for(int idx=0; idx<sz; idx++) { sprintf(&str[strlen(str)], "%02x,", ((char *)data)[idx]); }
    //dprintf(D_NETWORK, "---> cleartext: %s\n", str);

    // Check to see if we need to encrypt
    // This works only because putn will actually put all 
    if (get_encryption()) {
        if (!wrap((unsigned char *)data, sz, dta , l_out)) { 
            dprintf(D_SECURITY, "Encryption failed\n");
            return -1;  // encryption failed!
        }
    }
    else {
        dta = (unsigned char *) malloc(sz);
        memcpy(dta, data, sz);
    }
    
    // Now, add to the MAC
    if (mdChecker_) {
        mdChecker_->addMD(dta, sz);
    }

    //str[0] = 0;
    //for(int idx=0; idx<sz; idx++) { sprintf(&str[strlen(str)], "%02x,", dta[idx]); }
    //dprintf(D_NETWORK, "---> ciphertext: %s\n", str);

    bytesPut = _outMsg.putn((char *)dta, sz);
    
    free(dta);
    
	return bytesPut;
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
	while(!_msgReady) {
		if(_timeout > 0) {
			struct timeval timer;
			fd_set readfds;
			int nfds=0, nfound;
			timer.tv_sec = _timeout;
			timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
			nfds = _sock + 1;
#endif
			FD_ZERO(&readfds);
			FD_SET(_sock, &readfds);
				
			nfound = select( nfds, &readfds, 0, 0, &timer );
			switch(nfound) {
				case 0:
					return 0;
					break;
				case 1:
					break;
				default:
					dprintf(D_NETWORK, "select returns %d, recv failed\n", nfound );
					return 0;
					break;
			}
		}
		(void)handle_incoming_packet();
	}

	char *tempBuf = (char *)malloc(size);
    if (!tempBuf) { EXCEPT("malloc failed"); }
	int readSize, length;
    unsigned char * dec;

	if(_longMsg) {
        // long message 
        readSize = _longMsg->getn(tempBuf, size);
    }
	else { 
        // short message
        readSize = _shortMsg.getn(tempBuf, size);
    }

    //char str[10000];
    //str[0] = 0;
    //for(int idx=0; idx<readSize; idx++) { sprintf(&str[strlen(str)], "%02x,", tempBuf[idx]); }
    //dprintf(D_NETWORK, "<--- ciphertext: %s\n", str);

	if(readSize == size) {
            if (get_encryption()) {
                unwrap((unsigned char *) tempBuf, readSize, dec, length);
                memcpy(dta, dec, readSize);
                free(dec);
            }
            else {
                memcpy(dta, tempBuf, readSize);
            }

            //str[0] = 0;
            //for(int idx=0; idx<size; idx++) { sprintf(&str[strlen(str)], "%02x,", ((char *)dta)[idx]); }
            //dprintf(D_NETWORK, "<--- cleartext: %s\n", str);

            free(tempBuf);
            return readSize;
	} else {
		free(tempBuf);
        dprintf(D_NETWORK,
                "SafeSock::get_bytes - failed because bytes read is different from bytes requested\n");
		return -1;
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
			struct timeval timer;
			fd_set readfds;
			int nfds=0, nfound;
			timer.tv_sec = _timeout;
			timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
			nfds = _sock + 1;
#endif
			FD_ZERO(&readfds);
			FD_SET(_sock, &readfds);
				
			nfound = select( nfds, &readfds, 0, 0, &timer );
			switch(nfound) {
				case 0:
					return 0;
					break;
				case 1:
					break;
				default:
					dprintf(D_NETWORK,
					        "select returns %d, recv failed\n",
						  nfound );
					return 0;
					break;
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
			struct timeval timer;
			fd_set readfds;
			int nfds=0, nfound;
			timer.tv_sec = _timeout;
			timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
			nfds = _sock + 1;
#endif
			FD_ZERO(&readfds);
			FD_SET(_sock, &readfds);
				
			nfound = select( nfds, &readfds, 0, 0, &timer );
			switch(nfound) {
				case 0:
					return 0;
					break;
				case 1:
					break;
				default:
					dprintf(D_NETWORK,
					        "select returns %d, recv failed\n",
						  nfound );
					return 0;
					break;
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
	SOCKET_SENDRECV_LENGTH_TYPE fromlen = sizeof(struct sockaddr_in);
	bool last;
	int seqNo, length;
	_condorMsgID mID;
	void* data;
	int index;
	int received;
	_condorInMsg *tempMsg, *delMsg, *prev = NULL;
	time_t curTime;

	received = recvfrom(_sock, _shortMsg.dataGram, SAFE_MSG_MAX_PACKET_SIZE,
	                    0, (struct sockaddr *)&_who, &fromlen );
	if(received < 0) {
		dprintf(D_NETWORK, "recvfrom failed: errno = %d\n", errno);
		return FALSE;
	}
    char str[50];
    sprintf(str, sock_to_string(_sock));
    dprintf( D_NETWORK, "RECV %d bytes at %s from %s\n",
            received, str, sin_to_string(&_who));
    //char temp_str[10000];
    //temp_str[0] = 0;
    //for (int i=0; i<received; i++) { sprintf(&temp_str[strlen(temp_str)], "%02x,", _shortMsg.dataGram[i]); }
    //dprintf(D_NETWORK, "<---packet [%d bytes]: %s\n", received, temp_str);

	length = received;
    _shortMsg.reset(); // To be sure
	
    if (_shortMsg.getHeader(received, last, seqNo, length, mID, data)) {
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
            tempMsg->set_sec(_shortMsg.isDataMD5ed(),
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
                                             _shortMsg.isDataMD5ed(), 
                                             _shortMsg.md(), 
                                             _shortMsg.isDataEncrypted(), prev);
            if(!prev->nextMsg) {    
                EXCEPT("Error:handle_incomming_packet: Out of Memory");
            }
            //dprintf(D_NETWORK, "new msg created\n");
            //prev->nextMsg->dumpMsg();
        } else { // first message in the bucket
            _inMsgs[index] = new _condorInMsg(mID, last, seqNo, length, data, 
                                              _shortMsg.isDataMD5ed(), 
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


char * SafeSock::serialize() const
{
	// here we want to save our state into a buffer

	// first, get the state from our parent class
	char * parent_state = Sock::serialize();
	// now concatenate our state
	char outbuf[50];
    int len = 0;

    memset(outbuf, 0, 50);
    if (_fqu) {
        len = strlen(_fqu);
    }

	sprintf(outbuf,"*%d*%s*%d*",_special_state,sin_to_string(&_who), len);
	strcat(parent_state,outbuf);

    if (_fqu) {
        strcat(parent_state, _fqu);
        strcat(parent_state, "*");
    }

	return( parent_state );
}

char * SafeSock::serialize(char *buf)
{
	char sinful_string[28];
	char usernamebuf[128];
	char *ptmp, *ptr = NULL;
    int len;
    
    if (_fqu) {
        delete[] _fqu;
        _fqu = NULL;
    }   
	ASSERT(buf);
    memset(sinful_string, 0, 28);
    memset(usernamebuf, 0, 128);
	// here we want to restore our state from the incoming buffer

	// first, let our parent class restore its state
	ptmp = Sock::serialize(buf);
	ASSERT( ptmp );
	sscanf(ptmp,"%d*",(int*)&_special_state);
    // skip through this
    ptmp = strchr(ptmp, '*');
    ptmp++;

    // Now, see if we are 6.3 or 6.2
    if ( (ptr = strchr(ptmp, '*')) != NULL) {
        // We are 6.3
        memcpy(sinful_string, ptmp, ptr - ptmp);
        ptmp = ++ptr;
        sscanf(ptmp,"%d*", &len);
        ptmp = strchr(ptmp, '*');
        ptmp++;
        if ( len > 0 ) {
            memcpy(usernamebuf, ptmp, len);
        }
    }
    else {
        sscanf(ptmp,"%s",sinful_string);
    }

	string_to_sin(sinful_string, &_who);

	if( usernamebuf[0] ) {
		_fqu = strnewp(usernamebuf);
	} else {
		_fqu = NULL;
	}

	return NULL;
}

const char * SafeSock :: isIncomingDataMD5ed()
{
    char c;
    if (!peek(c)) {
        return 0;
    }
    else {
        if(_longMsg) {
            // long message 
            return _longMsg->isDataMD5ed();
        }
        else { // short message
            return _shortMsg.isDataMD5ed();
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

bool SafeSock :: init_MD(CONDOR_MD_MODE mode, KeyInfo * key, const char * keyId)
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
    
    inited = _outMsg.init_MD(keyId);

    return inited;
}

#ifdef DEBUG
int SafeSock::getMsgSize()
{
	int result;

	while(!_msgReady) {
		if(_timeout > 0) {
			struct timeval timer;
			fd_set readfds;
			int nfds=0, nfound;
			timer.tv_sec = _timeout;
			timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
			nfds = _sock + 1;
#endif
			FD_ZERO(&readfds);
			FD_SET(_sock, &readfds);
				
			nfound = select( nfds, &readfds, 0, 0, &timer );
			switch(nfound) {
				case 0:
					return 0;
					break;
				case 1:
					break;
				default:
					dprintf(D_NETWORK,
					        "select returns %d, recv failed\n",
						  nfound );
					return FALSE;
					break;
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

const char* SafeSock::getFullyQualifiedUser() {
	return _fqu;
}

void SafeSock::setFullyQualifiedUser(char * u) {
	if (_fqu) {
		delete []_fqu;
		_fqu = NULL;
	}
	if (u) {
		_fqu = strnewp(u);
	}
}

int SafeSock :: isAuthenticated()
{
    return _authenticated;
}

void SafeSock :: setAuthenticated(bool authenticated)
{
    _authenticated = authenticated;
}
