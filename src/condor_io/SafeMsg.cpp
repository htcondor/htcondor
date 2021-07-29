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
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_md.h"       // Condor_MD_MAC
#include "condor_sockfunc.h"
#include "condor_config.h"

#define USABLE_PACKET_SIZE m_SAFE_MSG_FRAGMENT_SIZE - SAFE_MSG_HEADER_SIZE
const char THIS_IS_TOO_UGLY_FOR_THE_SAKE_OF_BACKWARD[] = "CRAP";
static const int SAFE_MSG_CRYPTO_HEADER_SIZE = 10;

_condorPacket::_condorPacket()
{
    init();
}

_condorPacket::~_condorPacket()
{
    if (incomingHashKeyId_) {
        free(incomingHashKeyId_);
        incomingHashKeyId_ = 0;
    }

    if (outgoingHashKeyId_) {
        free(outgoingHashKeyId_);
        outgoingHashKeyId_ = 0;
    }

    if (incomingEncKeyId_) {
        free(incomingEncKeyId_);
        incomingEncKeyId_ = 0;
    }

    if (outgoingEncKeyId_) {
        free(outgoingEncKeyId_);
        outgoingEncKeyId_ = 0;
    }
	if( md_ ) {
		free( md_ );
	}
}

void _condorPacket :: init()
{
	length     = 0;
	data       = &dataGram[SAFE_MSG_HEADER_SIZE];
	curIndex   = 0;
	next       = NULL;
    verified_  = true;
    incomingHashKeyId_ = 0;
    outgoingHashKeyId_ = 0;
    outgoingMdLen_    = 0;
    incomingEncKeyId_ = 0;
    outgoingEncKeyId_ = 0;
    outgoingEidLen_   = 0;
	md_ = NULL;
	m_SAFE_MSG_FRAGMENT_SIZE = DEFAULT_SAFE_MSG_FRAGMENT_SIZE;
	m_desired_fragment_size = m_SAFE_MSG_FRAGMENT_SIZE;
}

const char * _condorPacket :: isDataHashed()
{
    return incomingHashKeyId_;
}

const char * _condorPacket :: isDataEncrypted()
{
    return incomingEncKeyId_;
}

const unsigned char * _condorPacket :: md()
{
    return md_;
}

bool _condorPacket :: set_encryption_id(const char * keyId)
{
    // This should be for outpacket only!
    ASSERT(empty());
    if (outgoingEncKeyId_) {
        if( curIndex > 0 ) {
            curIndex -= outgoingEidLen_;
            if (curIndex == SAFE_MSG_CRYPTO_HEADER_SIZE) {
                    // empty header
                curIndex -= SAFE_MSG_CRYPTO_HEADER_SIZE;
            }
            ASSERT( curIndex >= 0 );
        }

        free(outgoingEncKeyId_);
        outgoingEncKeyId_ = 0;
        outgoingEidLen_   = 0;
    }
    
    if (keyId) {
        outgoingEncKeyId_ = strdup(keyId);
        outgoingEidLen_   = strlen(outgoingEncKeyId_);
		if( IsDebugVerbose(D_SECURITY) ) {
			dprintf( D_SECURITY, 
					 "set_encryption_id: setting key length %d (%s)\n", 
					 outgoingEidLen_, keyId );  
		}
        if ( curIndex == 0 ) {
            curIndex += SAFE_MSG_CRYPTO_HEADER_SIZE;
        }
        curIndex += outgoingEidLen_;
    }

    length = curIndex;

    return true;
}

bool _condorPacket::init_MD(const char * keyId)
{
    bool inited = true;
    ASSERT(empty());

    if (outgoingHashKeyId_) {
        if( curIndex > 0 ) {
            curIndex -= MAC_SIZE;
            curIndex -= outgoingMdLen_;
            if (curIndex == SAFE_MSG_CRYPTO_HEADER_SIZE) {
                    // empty header
                curIndex -= SAFE_MSG_CRYPTO_HEADER_SIZE;
            }
            ASSERT( curIndex >= 0 );
        }

        free(outgoingHashKeyId_);
        outgoingHashKeyId_ = 0;
        outgoingMdLen_ = 0;
    }

    if (keyId) {
        outgoingHashKeyId_ = strdup(keyId);
        outgoingMdLen_    = strlen(outgoingHashKeyId_);
        if ( curIndex == 0 ) {
            curIndex += SAFE_MSG_CRYPTO_HEADER_SIZE;
        }
        curIndex += MAC_SIZE;
        curIndex += outgoingMdLen_; 
    }

    length = curIndex;

    return inited;
}

/* Demarshall - Get the values of the header:
 * Set the values of data structure to the values of header field
 * of received packet and initialize other values
 *	@param:
 *        msgsize - size of the UDP msg
 *        last - is this the last packet of a message
 *        seq - sequence number of the packet
 *        len - length of the packet
 *        mID - message id
 *        dta - pointer to the data field of dataGram
 *	@return: true, if this packet is the whole message
 *         false, otherwise
 */
int _condorPacket::getHeader(int /* msgsize */,
                             bool &last,
                             int &seq,
                             int &len,
                             _condorMsgID &mID,
                             void *&dta)
{
    uint16_t stemp;
    uint32_t ltemp;
	
    if (md_) {
        free(md_);
        md_ = 0;
    }

    if(memcmp(&dataGram[0], SAFE_MSG_MAGIC, 8)) {
        if(len >= 0) {
            length = len;
        }
        dta = data = &dataGram[0];
        checkHeader(len , dta);
        return true;
	}   

	last = (bool)dataGram[8];

	memcpy(&stemp, &dataGram[9], 2);
	seq = ntohs(stemp);

	memcpy(&stemp, &dataGram[11], 2);
	len = length = ntohs(stemp);

	memcpy(&ltemp, &dataGram[13], 4);
	mID.ip_addr = ntohl(ltemp);

	memcpy(&stemp, &dataGram[17], 2);
	mID.pid = ntohs(stemp);

	memcpy(&ltemp, &dataGram[19], 4);
	mID.time = ntohl(ltemp);

	memcpy(&stemp, &dataGram[23], 2);
	mID.msgNo = ntohs(stemp);

    dta = data = &dataGram[25];
    dprintf(D_NETWORK, "Fragmentation Header: last=%d,seq=%d,len=%d,data=[25]\n",
           last, seq, len); 

    checkHeader(len, dta);    

    return false;
}

void _condorPacket :: checkHeader(int & len, void *& dta)
{
    uint16_t stemp;
    short flags = 0, mdKeyIdLen = 0, encKeyIdLen = 0;

    if(memcmp(data, THIS_IS_TOO_UGLY_FOR_THE_SAKE_OF_BACKWARD, 4) == 0) {
        // We found stuff, go with 6.3 header format
        // First six bytes are hash/encryption related
        data += 4;
        memcpy(&stemp, data, 2);
        flags = ntohs(stemp);
        data += 2;
        memcpy(&stemp, data, 2);
        mdKeyIdLen = ntohs(stemp);
        data += 2;
        memcpy(&stemp, data, 2);
        encKeyIdLen = ntohs(stemp);
        data += 2;

        length -= SAFE_MSG_CRYPTO_HEADER_SIZE;
        dprintf(D_NETWORK,
                "Sec Hdr: tag(4), flags(2), mdKeyIdLen(2), encKeyIdLen(2), mdKey(%d), MAC(16), encKey(%d)\n",
                mdKeyIdLen, encKeyIdLen);

        if ((flags & MD_IS_ON) && (mdKeyIdLen > 0)) {
            // Scan for the key
            incomingHashKeyId_ = (char *) malloc(mdKeyIdLen+1);
            memset(incomingHashKeyId_, 0, mdKeyIdLen+1);
            memcpy(incomingHashKeyId_, data, mdKeyIdLen);
            dprintf(D_NETWORK|D_VERBOSE, "UDP: HashKeyID is %s\n", incomingHashKeyId_);
            data += mdKeyIdLen;
            length -= mdKeyIdLen;

            // and 16 bytes of MAC
            md_ = (unsigned char *) malloc(MAC_SIZE);
            memcpy((void*)&md_[0], data, MAC_SIZE);
            data += MAC_SIZE;
            length -= MAC_SIZE;
            verified_ = false;
        }
        else {
            if (flags & MD_IS_ON) {
                dprintf(D_ALWAYS,"Incorrect MD header information\n");
            }
        }

        if ((flags & ENCRYPTION_IS_ON) && (encKeyIdLen > 0)) {
            incomingEncKeyId_ = (char *) malloc(encKeyIdLen+1);
            memset(incomingEncKeyId_, 0, encKeyIdLen + 1);
            memcpy(incomingEncKeyId_, data, encKeyIdLen);
            dprintf(D_NETWORK|D_VERBOSE, "UDP: EncKeyID is %s\n", incomingEncKeyId_);
            data += encKeyIdLen;
            length -= encKeyIdLen;
        }
        else {
            if (flags & ENCRYPTION_IS_ON) {
                dprintf(D_ALWAYS, "Incorrect ENC Header information\n");
            }
        }

        len = length;
        dta = data;
    }
}

bool _condorPacket::verifyMD(Condor_MD_MAC * mdChecker)
{
    if (mdChecker) {
        if (md_ == NULL) {
            verified_ = false;
            return verified_;
        }
        else {
            if ( (curIndex == 0) && !verified_) {
                mdChecker->addMD((unsigned char *)data, length);
            
                if (mdChecker->verifyMD((unsigned char *)&md_[0])) {
                    dprintf(D_SECURITY, "MD verified!\n");
                    verified_ = true;
                }
                else {
                    dprintf(D_SECURITY, "MD verification failed for short message\n");
                    verified_ = false;
                }
            }
            else if (curIndex != 0) {
                verified_ = false;
            }
            else {
                // verified_ is true
            }
        }
    }
    else {
        verified_ = true;
    }
    return verified_;
}

/* Get the next n bytes from packet:
 * Copy 'size' bytes of the packet, starting from 'curIndex', to 'dta'
 *	@param: dta - buffer to which data will be copied
 *        size - the number of bytes to be copied
 *	@return: the actual number of bytes copied, if success
 *         -1, if failed
 *
 * Side effect: packet pointer will be advanced by the return value;
 *
 * Notice: 'dta' must be a pointer to an object of at least 'size' bytes
 */
int _condorPacket::getn(char* dta, const int size)
{
	if(!dta || curIndex + size > length) {
        dprintf(D_NETWORK, "dta is NULL or more data than queued is requested\n");
		return -1;
    }
	memcpy(dta, &data[curIndex], size);
	curIndex += size;
	return size;
}


/* Get data from packet up to the delimeter:
 *	@param: ptr - will be set to the current packet pointer
 *	        delim - the delimeter
 *	@return: the number of bytes from the current pointer to the location, if success
 *	         -1, if failed
 *
 * Side effect: packet pointer will be advanced by the return value;
 *
 * Notice: 'ptr' will point to the location of the packet directly, so 'dta' must be
 *         a pointer to nothing and must not be deleted by caller
 */
int _condorPacket::getPtr(void *&ptr, const char delim)
{
	if(curIndex >= length) {
		return -1;
	}
	char *msgbuf = &data[curIndex];
	size_t msgbufsize = length - curIndex;
	char *delim_ptr = (char *)memchr(msgbuf, delim, msgbufsize);

	if(delim_ptr == NULL) {
		return -1;
	}
	// read past the delimiter
	delim_ptr++;
	ptr = &data[curIndex];
	curIndex = delim_ptr - data;
	return delim_ptr - msgbuf;
}


/* Peek the next byte
 *	@param: c - character which will be returned
 *	@return: TRUE, if succeed
 *	         FALSE, if fail	          
 *
 * Notice: packet pointer won't be advanced
 */
int _condorPacket::peek(char &c)
{
	if(curIndex == length)
		return FALSE;
	c = data[curIndex];
	return TRUE;
}

/* Initialize data structure */
void _condorPacket::reset()
{
    curIndex = 0;
    length   = 0;

    if (outgoingHashKeyId_) {
        //free(outgoingHashKeyId_);
        //outgoingHashKeyId_ = 0;
        curIndex = MAC_SIZE + outgoingMdLen_;
    }

    if (outgoingEncKeyId_) {
        //free(outgoingEncKeyId_);
        //outgoingEncKeyId_ = 0;
        curIndex += outgoingEidLen_;
    }

    if (curIndex > 0) {
        curIndex += SAFE_MSG_CRYPTO_HEADER_SIZE;
    }

    length = curIndex;

    if (incomingHashKeyId_) {
        free(incomingHashKeyId_);
        incomingHashKeyId_ = 0;
    }

    if (incomingEncKeyId_) {
        free(incomingEncKeyId_);
        incomingEncKeyId_ = 0;
    }

	// Set fragment size to desired size. We don't want
	// to change m_SAFE_MSG_FRAGMENT_SIZE in the middle of
	// a message, so only change it on reset().
	m_SAFE_MSG_FRAGMENT_SIZE = m_desired_fragment_size;
}

/* Check if every data in the packet has been read */
bool _condorPacket::consumed() const
{
	return(curIndex == length);
}

/* Put as many bytes as possible but not more than size:
 * Copy as many bytes as possible, but not more than 'size'
 *	@param: dta - buffer holding data to be copied
 *	        size - the number of bytes desired
 *	@return: the actual number of bytes copied
 *
 * Side effect: curIndex and length will be adjusted appropriately
 */
int _condorPacket::putMax(const void* dta, const int size)
{
    int len, left = USABLE_PACKET_SIZE - curIndex;
    
    len = size > left? left : size;
    
	memcpy(&data[curIndex], dta, len);
	curIndex += len;
	length = curIndex;

	return len;
}

void _condorPacket::addExtendedHeader(unsigned char * mac) 
{
    int where = SAFE_MSG_CRYPTO_HEADER_SIZE;
    if (mac) {
        if (mac && outgoingHashKeyId_) {
            // First, add the key id if possible
            memcpy(&dataGram[SAFE_MSG_HEADER_SIZE+where], 
                   outgoingHashKeyId_, outgoingMdLen_);
            // Next the Mac
            where += outgoingMdLen_;
            memcpy(&dataGram[SAFE_MSG_HEADER_SIZE+where], mac, MAC_SIZE);
            where += MAC_SIZE;
        }
    }

    if (outgoingEncKeyId_) {
        // stick outgoingEncKeyId_
        memcpy(&dataGram[SAFE_MSG_HEADER_SIZE+where], 
               outgoingEncKeyId_, outgoingEidLen_);
    }
}

bool _condorPacket::full() const
{
    return (curIndex == USABLE_PACKET_SIZE);
}

int _condorPacket::set_MTU(int mtu)
{
	if ( mtu <= 0 ) {
		mtu = DEFAULT_SAFE_MSG_FRAGMENT_SIZE;
	}
	if ( mtu < SAFE_MSG_HEADER_SIZE + 1 ) {
		mtu = SAFE_MSG_HEADER_SIZE + 1;
	}
	if ( mtu > SAFE_MSG_MAX_PACKET_SIZE-SAFE_MSG_HEADER_SIZE-1 ) {
		mtu = SAFE_MSG_MAX_PACKET_SIZE-SAFE_MSG_HEADER_SIZE-1;
	}

	if ( mtu != m_desired_fragment_size ) {
		// stash santized mtu as the size next time reset() is called
		m_desired_fragment_size = mtu;

		// if this packet is empty, we can safely set the size immediately
		if ( empty() ) {
			m_SAFE_MSG_FRAGMENT_SIZE = m_desired_fragment_size;
		}
	}

	return m_desired_fragment_size;
}

bool _condorPacket::empty()
{
    int forward = 0;
    if (outgoingHashKeyId_) {
        forward = MAC_SIZE + outgoingMdLen_;
    }
    if (outgoingEncKeyId_) {
        forward += outgoingEidLen_;
    }
    if (forward > 0) {
        forward += SAFE_MSG_CRYPTO_HEADER_SIZE;    
        // For backward compatibility reasons, we need 
        // to adjust the header size dynamically depends
        // on which version of condor we are talking with
    }

    return(length == forward);
}

/* Marshall - Complete header fields of the packet:
 * Fill header fields of the packet with the values of
 * corresponding data structures
 * param: last - whether this packet is last one or not
 *        seqNo - sequence number of this packet within the message
 *        msgID - message id
 */
void _condorPacket::makeHeader(bool last, int seqNo, 
                               _condorMsgID msgID, 
                               unsigned char * mac)
{
    uint16_t stemp;
    uint32_t ltemp;

	memcpy(dataGram, SAFE_MSG_MAGIC, 8);

	dataGram[8] = (char) last;
	stemp = htons((unsigned short)seqNo);
	memcpy(&dataGram[9], &stemp, 2);

	stemp = htons((unsigned short)length);
	memcpy(&dataGram[11], &stemp, 2);

	ltemp = htonl((unsigned long)msgID.ip_addr);
	memcpy(&dataGram[13], &ltemp, 4);

	stemp = htons((unsigned short)msgID.pid);
	memcpy(&dataGram[17], &stemp, 2);

	ltemp = htonl((unsigned long)msgID.time);
	memcpy(&dataGram[19], &ltemp, 4);

	stemp = htons((unsigned short)msgID.msgNo);
	memcpy(&dataGram[23], &stemp, 2);

    // Above is the end of the regular 6.2 version header
    // Now the new stuff, technically these fields
    // are not in the header, but they are
    short flags = 0;
    if (outgoingHashKeyId_) {
        flags |= MD_IS_ON;
    }
    if (outgoingEncKeyId_) {
        flags |= ENCRYPTION_IS_ON;
    }
    
    if (flags != 0) {
        // First, set our special handshake code
        memcpy(&dataGram[25], THIS_IS_TOO_UGLY_FOR_THE_SAKE_OF_BACKWARD, 4);

        stemp = htons((unsigned short)flags);
        memcpy(&dataGram[29], &stemp, 2);
        // how long is the outgoing MD key?
        stemp = htons((unsigned short) outgoingMdLen_);
        memcpy(&dataGram[31], &stemp, 2);
        // Maybe one for encryption as well
        stemp = htons((unsigned short) outgoingEidLen_);
        memcpy(&dataGram[33], &stemp, 2);

        addExtendedHeader(mac);   // Add encryption id if necessary
    }
}

#ifdef DEBUG
void _condorPacket::dumpPacket()
{
	bool last;
	int seq;
	int len = -1;
	_condorMsgID mID;
	void* dta;

	// if single packet message--backward compatible message
	if(getHeader(6000, last, seq, len, mID, dta)) {
		dprintf(D_NETWORK, "(short) ");
		for(int i=0; i<length; i++) {
			if(i < 200)
				dprintf(D_NETWORK, "%c", data[i]);
			else {
				dprintf(D_NETWORK, "...");
				break;
			}
		}
		dprintf(D_NETWORK, "\n");
	}
	// if part of a long message
	else {
		dprintf(D_NETWORK, "[%d] (", seq);
		if(last)
			dprintf(D_NETWORK, "Y, ");
		else
			dprintf(D_NETWORK, "N, ");
		dprintf(D_NETWORK, "%d)\t", len);
		for(int i=0; i<len; i++) {
			if(i < 200)
				dprintf(D_NETWORK, "%c", dta[i]);
			else {
				dprintf(D_NETWORK, "...");
				break;
			}
		}
		dprintf(D_NETWORK, "\n");
	}
}
#endif

_condorOutMsg::_condorOutMsg()
{
	headPacket = lastPacket = new _condorPacket();
	if(!headPacket) {
		dprintf(D_ALWAYS, "new Packet failed. out of memory\n");
		EXCEPT("new Packet failed. out of memory");
	}
	noMsgSent = 0;
	avgMsgSize = 0;
	m_mtu = DEFAULT_SAFE_MSG_FRAGMENT_SIZE;
}

_condorOutMsg::~_condorOutMsg() {
	_condorPacket* tempPacket;
   
	while(headPacket) {
		tempPacket = headPacket;
		headPacket = headPacket->next;
		delete tempPacket;
	}
}

int _condorOutMsg :: set_MTU(const int mtu)
{
	if ( mtu != DEFAULT_SAFE_MSG_FRAGMENT_SIZE ) {
		dprintf(D_NETWORK,
			"_condorOutMsg MTU changed from default to %d\n",
			mtu);
	}
	m_mtu = mtu;
	return lastPacket->set_MTU(m_mtu);
}

bool _condorOutMsg :: set_encryption_id(const char * keyId)
{
    // Only set the data in the first packet
    if ((headPacket != lastPacket) || (!headPacket->empty())) {
        return false;
    }

    return headPacket->set_encryption_id(keyId);
}

bool _condorOutMsg::init_MD(const char * keyId)
{
    if ((headPacket != lastPacket) || (!headPacket->empty())) {
        return false;
    }

    return headPacket->init_MD(keyId);
}

/* Put n bytes of data
 * This method puts n bytes into the message, while adding packets as needed
 *	@returns: the number of bytes actually stored into the message
 */
int _condorOutMsg::putn(const char *dta, const int size) {
	int total = 0, len = 0;

	while(total != size) {
		if(lastPacket->full()) {
			lastPacket->next = new _condorPacket();
			if(!lastPacket->next) {
				dprintf(D_ALWAYS, "Error: OutMsg::putn: out of memory\n");
				return -1;
			}
			lastPacket->next->set_MTU(m_mtu);
			lastPacket = lastPacket->next;
		}
		len = lastPacket->putMax(&dta[total], size - total);
		total += len;
	}
	return total;
}


/*
 *	@returns: the number of bytes sent, if succeeds
 *	          -1, if fails
 */
int _condorOutMsg::sendMsg(const int sock,
                           const condor_sockaddr& who,
                           _condorMsgID msgID,
                           unsigned char * mac)
{
	_condorPacket* tempPkt;
	int seqNo = 0, msgLen = 0, sent;
	int total = 0;
    unsigned char * md = mac;
    //char str[10000];

	if(headPacket->empty()) // empty message
		return 0;
   
	while(headPacket != lastPacket) {
		tempPkt    = headPacket;
		headPacket = headPacket->next;
		tempPkt->makeHeader(false, seqNo++, msgID, md);
		msgLen    += tempPkt->length;

		
		sent = condor_sendto(sock, tempPkt->dataGram,
		              tempPkt->length + SAFE_MSG_HEADER_SIZE,
                      0, who);

		if(sent != tempPkt->length + SAFE_MSG_HEADER_SIZE) {
			dprintf(D_ALWAYS, "sendMsg:sendto failed - errno: %d\n", errno);
			headPacket = tempPkt;
			clearMsg();
			return -1;
		}
        //int i;
        //str[0] = 0;
        //for (i=0; i<tempPkt->length + SAFE_MSG_HEADER_SIZE; i++) {
        //    sprintf(&str[strlen(str)], "%02x,", tempPkt->dataGram[i]);
        //}
        //dprintf(D_NETWORK, "--->packet [%d bytes]: %s\n", sent, str);

		dprintf( D_NETWORK, "SEND [%d] %s ", sent, sock_to_string(sock) );
		dprintf( D_NETWORK|D_NOHEADER, "%s\n",
				 who.to_sinful().c_str());
		total += sent;
		delete tempPkt;
        md = 0;
	}

	// headPacket = lastPacket
    if(seqNo == 0) { // a short message
		msgLen = lastPacket->length;
        lastPacket->makeHeader(true, 0, msgID, md);
			// Short messages are sent without initial "magic" header,
			// because we don't need to specify sequence number,
			// and presumably for backwards compatibility with ancient
			// versions of Condor.  The crypto header may still
			// be there, since that is in the buffer starting at
			// the position pointed to by "data".
		sent = condor_sendto(sock, lastPacket->data, lastPacket->length,
							 0, who);
		if(sent != lastPacket->length) {
			dprintf( D_ALWAYS, 
				 "SafeMsg: sending small msg failed. errno: %d\n",
				 errno );
			headPacket->reset();
			return -1;
		}
        //str[0] = 0;
        //for (i=0; i<lastPacket->length + SAFE_MSG_HEADER_SIZE; i++) {
        //    sprintf(&str[strlen(str)], "%02x,", lastPacket->dataGram[i]);
        //}
        //dprintf(D_NETWORK, "--->packet [%d bytes]: %s\n", sent, str);
		dprintf( D_NETWORK, "SEND [%d] %s ", sent, sock_to_string(sock) );
		dprintf( D_NETWORK|D_NOHEADER, "%s\n", who.to_sinful().c_str());
		total = sent;
    }
    else {
        lastPacket->makeHeader(true, seqNo, msgID, md);
        msgLen += lastPacket->length;
        sent = condor_sendto(sock, lastPacket->dataGram,
                      lastPacket->length + SAFE_MSG_HEADER_SIZE,
                      0, who);
        if(sent != lastPacket->length + SAFE_MSG_HEADER_SIZE) {
            dprintf( D_ALWAYS, "SafeMsg: sending last packet failed. errno: %d\n", errno );
            headPacket->reset();
            return -1;
        }
        //str[0] = 0;
        //for (i=0; i<lastPacket->length + SAFE_MSG_HEADER_SIZE; i++) {
        //    sprintf(&str[strlen(str)], "%02x,", lastPacket->dataGram[i]);
        //}
        //dprintf(D_NETWORK, "--->packet [%d bytes]: %s\n", sent, str);
        dprintf( D_NETWORK, "SEND [%d] %s ", sent, sock_to_string(sock) );
        dprintf( D_NETWORK|D_NOHEADER, "%s\n", who.to_sinful().c_str());
        total += sent;
    }

	headPacket->reset();
	noMsgSent++;
	if(noMsgSent == 1)
		avgMsgSize = msgLen;
	else
		avgMsgSize = ((noMsgSent - 1) * avgMsgSize + msgLen) / noMsgSent;
	return total;
}


unsigned long _condorOutMsg::getAvgMsgSize() const
{
	return avgMsgSize;
}


void _condorOutMsg::clearMsg()
{
	_condorPacket* tempPkt;

	if(headPacket->empty()) // empty message
		return;
   
	while(headPacket != lastPacket) {
		tempPkt = headPacket;
		headPacket = headPacket->next;
		delete tempPkt;
	}

	headPacket->reset();
}

#ifdef DEBUG
int _condorOutMsg::dumpMsg(const _condorMsgID mID)
{
	_condorPacket *tempPkt, *nextPkt;
	int dumped;
	int seqNo = 0;
	int total = 0;

	dprintf(D_NETWORK,
	        "================================================\n");

	if(headPacket->empty()) { // empty message
		dprintf(D_NETWORK, "empty message\n");
		return 0;
	}
   
	// complete the last packet
	tempPkt = headPacket;
	while(tempPkt) {
		nextPkt = tempPkt->next;
		tempPkt->makeHeader((nextPkt == NULL), seqNo++, mID);
		tempPkt->dumpPacket();
		dumped = tempPkt->length + SAFE_MSG_HEADER_SIZE;
		total += dumped;
		tempPkt = nextPkt;
	}

	dprintf(D_NETWORK,
	        "------------------- has %d bytes -----------------------\n",
		  total);

	return total;
}
#endif


_condorDirPage::_condorDirPage(_condorDirPage* prev, const int num)
{
	prevDir = prev;
	dirNo = num;
	for(int i=0; i< SAFE_MSG_NO_OF_DIR_ENTRY; i++) {
		dEntry[i].dLen    = 0;
		dEntry[i].dGram   = NULL;
	}
	nextDir = NULL;
}


_condorDirPage::~_condorDirPage() {
	for(int i=0; i< SAFE_MSG_NO_OF_DIR_ENTRY; i++) {
		if(dEntry[i].dGram) {
			free(dEntry[i].dGram);
		}
    }
}


/* Make a new message with a packet
 * Initialize data structures and make a new message with given packet,
 * whose header and data fields are given as parameters.
 * The message is composed of a header entry with meta data about message,
 * doubly linked list of multiple directory entries, and data pages.
 * This method creates as many directory entries as needed.
 */
_condorInMsg::_condorInMsg(const _condorMsgID mID,// the id of this message
				const bool last,		// the packet is last or not
				const int seq,		// seq. # of the packet
				const int len,		// length of the packet
				const void* data,		// data of the packet
                const char * HashKeyId,  // key id
                const unsigned char * md,  // key id
                const char * EncKeyId,
				_condorInMsg* prev)	// pointer to previous message
								// in the bucket chain
{
	int destDirNo;
	int index;

	// initialize message id
	msgID.ip_addr = mID.ip_addr;
	msgID.pid = mID.pid;
	msgID.time = mID.time;
	msgID.msgNo = mID.msgNo;
   
	// initialize other data structures
	msgLen = len;
	lastNo = (last) ? seq : 0;
	received = 1;
	lastTime = time(NULL);
	passed = 0;
	curData = 0;
	curPacket = 0;

	// make directory entries as needed
	headDir = curDir = new _condorDirPage(NULL, 0);
	if(!curDir) {
		EXCEPT( "::InMsg, new DirPage failed. out of mem" );
	}
	destDirNo = seq / SAFE_MSG_NO_OF_DIR_ENTRY;
	while(curDir->dirNo != destDirNo) {
		curDir->nextDir = new _condorDirPage(curDir, curDir->dirNo + 1);
		if(!curDir->nextDir) {
		    EXCEPT("::InMsg, new DirPage failed. out of mem");
		}
		curDir = curDir->nextDir;
	}

	// make a packet and add to the appropriate directory entry
	index = seq % SAFE_MSG_NO_OF_DIR_ENTRY;
	curDir->dEntry[index].dLen = len;
	curDir->dEntry[index].dGram = (char *)malloc(len);
	if(!curDir->dEntry[index].dGram) {
		EXCEPT( "::InMsg, new char[%d] failed. out of mem", len );
	}
	memcpy(curDir->dEntry[index].dGram, data, len);

	// initialize temporary buffer to NULL
	tempBuf = NULL;
	tempBufLen = 0;

	prevMsg = prev;
	nextMsg = NULL;

    set_sec(HashKeyId, md, EncKeyId);
}

_condorInMsg::~_condorInMsg() {

	if(tempBuf) free(tempBuf);

	_condorDirPage* tempDir;
	while(headDir) {
		tempDir = headDir;
		headDir = headDir->nextDir;
		delete tempDir;
	}
    if (incomingHashKeyId_) {
        free(incomingHashKeyId_);
    }
    if (incomingEncKeyId_) {
        free(incomingEncKeyId_);
    }
    if (md_) {
        free(md_);
    }
}


/* Add a packet
 *
 */
bool _condorInMsg::addPacket(const bool last,
                             const int seq,
                             const int len, 
                             const void* data)
{
	int destDirNo, index;

	// check if the message is already ready
	if(lastNo != 0 && lastNo+1 == received) {
        dprintf(D_NETWORK, "Duplicated packet. The msg fully defragmented.\n");
		return false;
	}
	// find the correct dir entry
	destDirNo = seq / SAFE_MSG_NO_OF_DIR_ENTRY;
	while(destDirNo != curDir->dirNo) {
		if(destDirNo > curDir->dirNo) {// move forward, while appending new dir entries
			if(curDir->nextDir == NULL) {
				curDir->nextDir = new _condorDirPage(curDir, curDir->dirNo + 1);
				if(!curDir->nextDir) {
					dprintf(D_ALWAYS, "addPacket, out of memory\n");
					return false;
				}
			}
			curDir = curDir->nextDir;
		}
		else // move backward, in this case dir entries have already been made
			curDir = curDir->prevDir;
	} // of while(destDirNo...)

	index = seq % SAFE_MSG_NO_OF_DIR_ENTRY;
	if(curDir->dEntry[index].dLen == 0) { // new packet
		curDir->dEntry[index].dLen = len;
		curDir->dEntry[index].dGram = (char *)malloc(len);
		if(!curDir->dEntry[index].dGram) {
			dprintf(D_ALWAYS, "addPacket, new char[%d] failed. out of mem\n", len);
			return false;
		}
		memcpy(curDir->dEntry[index].dGram, data, len);
		msgLen += len;

		if(last)
			lastNo = seq;
		received++;
		if(lastNo + 1 == received) { // every packet has been received
			curDir = headDir;
			curPacket = 0;
			curData = 0;
            dprintf(D_NETWORK, "long msg ready: %ld bytes\n", msgLen);
			return true;
		} else {
			lastTime = time(NULL);
			return false;
		}
	} else // duplicated packet, including "curDir->dEntry[index].dGram == NULL"
		return false;
}

bool _condorInMsg :: verifyMD(Condor_MD_MAC * mdChecker)
{
    int currentPacket;
    _condorDirPage * start = headDir;

    if (!verified_) {
        if (curDir != start) { 
            return false;
        }

        if (mdChecker && md_) {
            while (start != NULL) {
                currentPacket = 0;
                while(currentPacket != SAFE_MSG_NO_OF_DIR_ENTRY) {
                    mdChecker->addMD((unsigned char *)start->dEntry[currentPacket].dGram, 
                                     start->dEntry[currentPacket].dLen);
                    if(++currentPacket == SAFE_MSG_NO_OF_DIR_ENTRY) {
                        // was the last of the current dir
                        start = start->nextDir;
                    }
                }
            } // of while(

            // Now, verify the packet
            if (mdChecker->verifyMD(md_)) {
                dprintf(D_SECURITY, "MD verified!\n");
                verified_ = true;
            }
            else {
                dprintf(D_SECURITY, "MD verification failed for long messag\n");
                verified_ = false;
            }
        }
        else {
            if (md_) {
                dprintf(D_SECURITY, "WARNING, incorrect MAC object is being used\n");
            }
            else {
                dprintf(D_SECURITY, "WARNING, no MAC data is found!\n");
            }
        }
    }

    return verified_;
}

void _condorInMsg::incrementCurData( int n ) {
	curData += n;
	passed += n;

	if(curData == curDir->dEntry[curPacket].dLen) {
		// current data page consumed
		// delete the consumed data page and then go to the next page
		free(curDir->dEntry[curPacket].dGram);
		curDir->dEntry[curPacket].dGram = NULL;

		if(++curPacket == SAFE_MSG_NO_OF_DIR_ENTRY) {
			// was the last of the current dir
			_condorDirPage* tempDir = headDir;
			headDir = curDir = headDir->nextDir;
			if(headDir) {
				headDir->prevDir = NULL;
			}
			delete tempDir;
			curPacket = 0;
		}
		curData = 0;
	}
}

/* Get the next n bytes from the message:
 * Copy the next 'size' bytes of the message to 'dta'
 * param: dta - buffer to which data will be copied
 *        size - the number of bytes to be copied
 * return: the actual number of bytes copied, if success
 *         -1, if failed
 * Notice: dta must be a pointer to object(>= size)
 */
int _condorInMsg::getn(char* dta, const int size)
{
	int len, total = 0;

	if(!dta || passed + size > msgLen) {
        dprintf(D_NETWORK, "dta is NULL or more data than queued is requested\n");
		return -1;
    }

	while(total != size) {
		len = size - total;
		if(len > curDir->dEntry[curPacket].dLen - curData)
			len = curDir->dEntry[curPacket].dLen - curData;
		memcpy(&dta[total],
		       &(curDir->dEntry[curPacket].dGram[curData]), len);
		total += len;
		incrementCurData(len);
	} // of while(total..)

    if( IsDebugVerbose(D_NETWORK) ) {
        dprintf(D_NETWORK, "%d bytes read from UDP[size=%ld, passed=%d]\n",
                total, msgLen, passed);
    }
	return total;
}

/* Get data from message up to the delimeter:
 * Copy data of the message to 'dta', starting from current position up to meeting 'delim'
 *	@param: buf - buffer to which data will be copied
 *	        delim - the delimeter
 *	@return: the number of bytes copied, if success
 *	         -1, if failed
 *
 * Notice: new character array will be allocated as needed and the space will be
 *         deallocated at the time of the next call of this method, so 'buf' must be
 *         a pointer to nothing and must not be deleted by caller
 */
int _condorInMsg::getPtr(void *&buf, char delim)
{
	_condorDirPage *tempDir;
	int tempPkt, tempData;
	size_t n = 1;
	int size;

	tempDir = curDir;
	tempPkt = curPacket;
	tempData = curData;

	bool copy_needed = false;
	while(1) {
		char *msgbuf = &tempDir->dEntry[tempPkt].dGram[tempData];
		size_t msgbufsize = tempDir->dEntry[tempPkt].dLen - tempData;
		char *delim_ptr = (char *)memchr(msgbuf,delim,msgbufsize);

		if( delim_ptr ) {
			n += delim_ptr - msgbuf;
			if( n == msgbufsize ) {
					// Need to copy the data in this case, because when
					// buffer is consumed by incrementCurData(), it will
					// be freed.
				copy_needed = true;
			}
			if( !copy_needed ) {
					// Special (common) case: the whole string is in one piece
					// so just return a pointer to it and skip the drudgery
					// of copying the bytes into another buffer.
				incrementCurData(n);
				buf = msgbuf;
				return n;
			}
			break; // found delim
		}
		copy_needed = true; // string spans multiple buffers

		n += msgbufsize;
		tempPkt++;
		tempData = 0;
		if(tempPkt >= SAFE_MSG_NO_OF_DIR_ENTRY) {
			if(!tempDir->nextDir) {
				return -1;
			}
			tempDir = tempDir->nextDir;
			tempPkt = 0;
		} else if(!tempDir->dEntry[tempPkt].dGram) { // was the last packet
			if( IsDebugVerbose(D_NETWORK) )
				dprintf(D_NETWORK,
				        "SafeMsg::getPtr: get to end & '%c' not found\n", delim);
			return -1;
		}
	}

	if( IsDebugVerbose(D_NETWORK) )
		dprintf(D_NETWORK, "SafeMsg::_longMsg::getPtr: found delim = %c & length = %lu\n",
			  delim, (unsigned long)n);
	if( n > tempBufLen ) {
		if(tempBuf) {
			free(tempBuf);
		}
		tempBuf = (char *)malloc(n);
		if(!tempBuf) {
			dprintf(D_ALWAYS, "getPtr, fail at malloc(%lu)\n", (unsigned long)n);
			tempBufLen = 0;
			return -1;
		}
		tempBufLen = n;
	}
	size = getn(tempBuf, n);
	buf = tempBuf;
	//cout << "\t\tInMsg::getPtr: " << (char *)buf << endl;
	return size;
}

/* Peek the next byte */
int _condorInMsg::peek(char &c)
{
	if (curDir->dEntry[curPacket].dGram) {
		c = curDir->dEntry[curPacket].dGram[curData];
		return TRUE;
	} else {
		return FALSE;
	}
}

/* Check if every data in all the bytes of the message have been read */
bool _condorInMsg::consumed() const
{
	return(msgLen != 0 && msgLen == passed);
}

void
_condorInMsg::set_sec(const char * HashKeyId,  // key id
                const unsigned char * md,  // key id
                const char * EncKeyId)
{
    if(md) {
        md_ = (unsigned char *) malloc(MAC_SIZE);
        memcpy(md_, md, MAC_SIZE);
        verified_ = false;
    }
    else {
        md_ = 0;
        verified_ = true;
    }

    if (HashKeyId) {
        incomingHashKeyId_ = strdup(HashKeyId);
    }
    else {
        incomingHashKeyId_ = 0;
    }

    if (EncKeyId) {
        incomingEncKeyId_ = strdup(EncKeyId);
    }
    else {
        incomingEncKeyId_ = 0;
    }
}

const char * _condorInMsg :: isDataHashed()
{
    return incomingHashKeyId_;
}

const char * _condorInMsg :: isDataEncrypted()
{
    return incomingEncKeyId_;
}

void _condorInMsg :: resetEnc()
{
    if (incomingEncKeyId_) {
        free(incomingEncKeyId_);
        incomingEncKeyId_ = 0;
    }
}

void _condorInMsg :: resetMD()
{
    if (incomingHashKeyId_) {
        free(incomingHashKeyId_);
        incomingHashKeyId_ = 0;
    }
}

void _condorInMsg::dumpMsg() const
{
    char str[10000];
    struct in_addr in;

    in.s_addr = msgID.ip_addr;
    sprintf(str, "ID: %s, %d, %lu, %d\n",
            inet_ntoa(in), msgID.pid, msgID.time, msgID.msgNo);
    sprintf(&str[strlen(str)], "len:%lu, lastNo:%d, rcved:%d, lastTime:%lu\n",
            msgLen, lastNo, received, (long)lastTime);
    dprintf(D_NETWORK, "========================\n%s\n===================\n", str);

    /*
	_condorDirPage *tempDir;
	int i, j, total = 0;

	dprintf(D_NETWORK,
	        "\theadDir: %d\tcurDir: %d\tcurPkt: %d\tcurData: %d\n",
	        headDir, curDir, curPacket, curData);
	dprintf(D_NETWORK,
	        "\t------------------------------------\
		     -----------------------------------\n");
	tempDir = headDir;
	while(tempDir) {
		dprintf(D_NETWORK, "\tdir[%d]  pre: %d  next: %d\n",
		        tempDir->dirNo, tempDir->prevDir, tempDir->nextDir);
		for(i=0; i < SAFE_MSG_NO_OF_DIR_ENTRY; i++) {
			dprintf(D_NETWORK, "\t\t[%d] ", i);
			if(tempDir->dEntry[i].dGram) {
				for(j=0; j < tempDir->dEntry[i].dLen; j++) {
					dprintf(D_NETWORK, "%c", tempDir->dEntry[i].dGram[j]);
					total++;
				}
				dprintf(D_NETWORK, "\n");
			} else
				dprintf(D_NETWORK, "\tNULL\n");
		}
		tempDir = tempDir->nextDir;
	}
	dprintf(D_NETWORK,
	        "\t----------------------- has %d bytes\
		     ------------------------------\n", total);;
    */
}
