/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_md.h"       // Condor_MD_MAC

#define USABLE_PACKET_SIZE SAFE_MSG_MAX_PACKET_SIZE - SAFE_MSG_HEADER_SIZE

_condorPacket::_condorPacket()
{
    init();
}

_condorPacket::_condorPacket(bool outPacket, KeyInfo * key, const char * keyID)
{
    init();
    init_MD(outPacket, key, keyID);
}

_condorPacket::~_condorPacket()
{
    delete mdChecker_;
    if (incomingMD5KeyId_) {
        free(incomingMD5KeyId_);
        incomingMD5KeyId_ = 0;
    }

    if (outgoingMD5KeyId_) {
        free(outgoingMD5KeyId_);
        outgoingMD5KeyId_ = 0;
    }

    if (md_) {
        free(md_);
        md_ = 0;
    }

    if (incomingEncKeyId_) {
        free(incomingEncKeyId_);
        incomingEncKeyId_ = 0;
    }

    if (outgoingEncKeyId_) {
        free(outgoingEncKeyId_);
        outgoingEncKeyId_ = 0;
    }
}

void _condorPacket :: init()
{
	length     = 0;
	data       = &dataGram[SAFE_MSG_HEADER_SIZE];
	curIndex   = 0;
	next       = NULL;
    mdChecker_        = 0;
    incomingMD5KeyId_ = 0;
    outgoingMD5KeyId_ = 0;
    outgoingMdLen_    = 0;
    incomingEncKeyId_ = 0;
    outgoingEncKeyId_ = 0;
    outgoingEidLen_   = 0;
    md_               = 0;
    verified_         = true;
}

void _condorPacket :: resetMD()
{
    if (incomingMD5KeyId_) {
        free(incomingMD5KeyId_);
        incomingMD5KeyId_ = 0;
    }
    if (md_) {
        free(md_);
        md_ = 0;
    }
    if (mdChecker_) {
        delete mdChecker_;
        mdChecker_ = 0;
    }
    verified_ = true;
}

void _condorPacket :: resetEnc()
{
    if (incomingEncKeyId_) {
        free(incomingEncKeyId_);
        incomingEncKeyId_ = 0;
    }
}

int _condorPacket :: headerLen()
{
    int len = 0;
    if (incomingMD5KeyId_) {
        len += strlen(incomingMD5KeyId_);
    }
    if (incomingEncKeyId_) {
        len += strlen(incomingEncKeyId_);
    }
    return len;
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
        free(outgoingEncKeyId_);
        outgoingEncKeyId_ = 0;
    }
    outgoingEidLen_ = 0;
    
    if (keyId) {
        outgoingEncKeyId_ = strdup(keyId);
        outgoingEidLen_   = strlen(outgoingEncKeyId_);
    }

    // This might be a problem if someone calls twice in a row
    curIndex += outgoingEidLen_;
    length    = curIndex;

    return true;
}

bool _condorPacket::init_MD(bool outPacket, KeyInfo * key, const char * keyId)
{
    bool inited = true;
    if (outPacket && !empty()) {
        return false;  // You can not change state in the middle of a packet!
    }

    delete mdChecker_;
    mdChecker_     = 0;

    if (outgoingMD5KeyId_) {
        free(outgoingMD5KeyId_);
        outgoingMD5KeyId_ = 0;
    }
    outgoingMdLen_ = 0;

    if (key) {
        mdChecker_ = new Condor_MD_MAC(key);

        if (keyId) {
            outgoingMD5KeyId_ = strdup(keyId);
            outgoingMdLen_    = strlen(outgoingMD5KeyId_);
        }
        if (mdChecker_ && outPacket) {
            curIndex += MAC_SIZE;
            length = curIndex;
        }
    }
    else {
        // Reset all key ids if necessary
        resetMD();
    }

    if (outPacket) {
        // you can not change in the middle of a packet!
        curIndex += outgoingMdLen_; 
        length    = curIndex;
    }
    else {
        // maybe we should verify the packet if necessary
        if (!verified_ && (md_ != NULL)) {
            inited = verifyMD();
        }
    }

    return inited;
}

void _condorPacket::setVerified(bool verified)
{
    verified_ = false;
}

bool _condorPacket :: verified()
{
    return verified_;
}

bool _condorPacket::verifyMD()
{
    int headerlen = headerLen();

    if (curIndex == 0) {
        if (mdChecker_ && !verified_) {
            mdChecker_->addMD((unsigned char *)(data-headerlen), 
                              length+headerlen);
            
            if (mdChecker_->verifyMD((unsigned char *)&md_[0])) {
                dprintf(D_SECURITY, "MD verified!\n");
                verified_ = true;
            }
            else {
                dprintf(D_SECURITY, "MD verification failed for short message\n");
                verified_ = false;
            }
        }
        else if (mdChecker_ == 0){
            return true;
            // Fake it for now.
        }
        else {
            // nothing here
        }
    }
    else {
        dprintf(D_SECURITY, "Trying to verify MD but data has changed!\n");
        if (md_ != NULL) {
            verified_ = false;
        }
        else {
            verified_ = true;
        }
    }
    return verified_;
}
/* Demarshall - Get the values of the header:
 * Set the values of data structure to the values of header field
 * of received packet and initialize other values
 *	@param: last - is this the last packet of a message
 *        seq - sequence number of the packet
 *        len - length of the packet
 *        mID - message id
 *        dta - pointer to the data field of dataGram
 *	@return: true, if this packet is the whole message
 *         false, otherwise
 */
int _condorPacket::getHeader(bool &last,
                             int &seq,
                             int &len,
                             _condorMsgID &mID,
                             void *&dta)
{
    short flags = 0, mdKeyIdLen, encKeyIdLen;

    if (md_) {
        free(md_);
        md_ = 0;
    }

    if(memcmp(&dataGram[0], SAFE_MSG_MAGIC, 8)) {
        if(len >= 0) {
            length = len - 6;

            dta = data = &dataGram[0];
        
            if (checkHeader(len, dta) != 0) {
                return -1;
            }
            else {
                return 1;
            }
        }
        else {
            return -1;
        }
	}   

	last = (bool)dataGram[8];

	memcpy(&seq, &dataGram[9], 2);
	seq = ntohs(seq);

	memcpy(&length, &dataGram[11], 2);
	len = length = ntohs(length);

	memcpy(&mID.ip_addr, &dataGram[13], 4);
	mID.ip_addr = ntohl(mID.ip_addr);

	memcpy(&mID.pid, &dataGram[17], 2);
	mID.pid = ntohs(mID.pid);

	memcpy(&mID.time, &dataGram[19], 4);
	mID.time = ntohl(mID.time);

	memcpy(&mID.msgNo, &dataGram[23], 2);
	mID.msgNo = ntohs(mID.msgNo);

    dta = data = &dataGram[25];

    return checkHeader(len, dta);    
}

int _condorPacket :: checkHeader(int & len, void *& dta)
{
    short flags = 0, mdKeyIdLen = 0, encKeyIdLen = 0;

    // First six bytes are MD5/encryption related
    memcpy(&flags, data, 2);
    flags = ntohs(flags);
    data += 2;
    memcpy(&mdKeyIdLen, data, 2);
    mdKeyIdLen = ntohs(mdKeyIdLen);
    data += 2;
    memcpy(&encKeyIdLen, data, 2);
    encKeyIdLen = ntohs(encKeyIdLen);
    data += 2;

    if (flags & MD_IS_ON) {
        // first 16 bytes are MD
        md_ = (unsigned char *) malloc(MAC_SIZE);
        memcpy((void*)&md_[0], data, MAC_SIZE);
        data += MAC_SIZE;
        if (mdKeyIdLen) {
            incomingMD5KeyId_ = (char *) malloc(mdKeyIdLen+1);
            memset(incomingMD5KeyId_, 0, mdKeyIdLen+1);
            memcpy(incomingMD5KeyId_, data, mdKeyIdLen);
            data += mdKeyIdLen;
            length -= mdKeyIdLen;
        }
        
        length = length - MAC_SIZE;
    }

    if (flags & ENCRYPTION_IS_ON) {
        incomingEncKeyId_ = (char *) malloc(encKeyIdLen+1);
        memset(incomingEncKeyId_, 0, encKeyIdLen + 1);
        memcpy(incomingEncKeyId_, data, encKeyIdLen);
        data += encKeyIdLen;
        length -= encKeyIdLen;
    }

    // Verify MD if necessary
    if (!verifyMD()) {
        dprintf(D_SECURITY, "Incorrect Message Digest!\n");
        return -1;
    }

    len = length;
    dta = data;

	return 0;      // not a short message, checksum was probably okay
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
	if(!dta || curIndex + size > length)
		return -1;
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
	int temp;
	int size =  1;

	temp = curIndex;
	while(temp < length && data[temp] != delim) {
		temp++;
		size++;
	}

	if(temp == length) // not found
		return -1;
	// found
	ptr = &data[curIndex];
	curIndex += size;
	return size;
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

    if (mdChecker_ && outgoingMD5KeyId_) {
        curIndex = MAC_SIZE + outgoingMdLen_;
        length   = MAC_SIZE;
    }

    if (outgoingEncKeyId_) {
        curIndex += outgoingEidLen_;
        length    = curIndex;
    }
    
    resetMD();
    resetEnc();
}

/* Check if every data in the packet has been read */
bool _condorPacket::consumed()
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

const char * _condorPacket :: isDataMD5ed()
{
    return incomingMD5KeyId_;
}

const char * _condorPacket :: isDataEncrypted()
{
    return incomingEncKeyId_;
}

void _condorPacket::addMD() 
{
    if (mdChecker_) {
        // First, add the key id if possible
        if (outgoingMD5KeyId_) {
            // stick outgoingMD5KeyId_
            memcpy(&dataGram[SAFE_MSG_HEADER_SIZE+MAC_SIZE], outgoingMD5KeyId_, outgoingMdLen_);
        }

        mdChecker_->addMD((unsigned char *)&dataGram[SAFE_MSG_HEADER_SIZE+MAC_SIZE], 
                          length - MAC_SIZE);
        unsigned char * md = mdChecker_->computeMD();
        memcpy(&dataGram[SAFE_MSG_HEADER_SIZE], md, MAC_SIZE);
        free(md);
    }
}

void _condorPacket::addEID() 
{
    if (outgoingEncKeyId_) {
        // stick outgoingEncKeyId_
		int where = mdChecker_? MAC_SIZE : 0;
        memcpy(&dataGram[SAFE_MSG_HEADER_SIZE+where+outgoingMdLen_], 
               outgoingEncKeyId_, outgoingEidLen_);
    }
}

bool _condorPacket::full()
{
    return (curIndex == USABLE_PACKET_SIZE);
}

bool _condorPacket::empty()
{
    int forward = 0;
    if (mdChecker_ && outgoingMD5KeyId_) {
        forward = MAC_SIZE + outgoingMdLen_;
    }
    if (outgoingEncKeyId_) {
        forward += outgoingEidLen_;
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
void _condorPacket::makeHeader(bool last, int seqNo, _condorMsgID msgID)
{
	unsigned short stemp;
	long ltemp;

	memcpy(dataGram, SAFE_MSG_MAGIC, 8);

	dataGram[8] = (char) last;
	stemp = htons((u_short)seqNo);
	memcpy(&dataGram[9], &stemp, 2);

	stemp = htons((u_short)length);
	memcpy(&dataGram[11], &stemp, 2);

	ltemp = htonl((u_long)msgID.ip_addr);
	memcpy(&dataGram[13], &ltemp, 4);

	stemp = htons((u_short)msgID.pid);
	memcpy(&dataGram[17], &stemp, 2);

	ltemp = htonl((u_long)msgID.time);
	memcpy(&dataGram[19], &ltemp, 4);

	stemp = htons((u_short)msgID.msgNo);
	memcpy(&dataGram[23], &stemp, 2);

    short flags = 0;
    if (mdChecker_ && outgoingMD5KeyId_) {
        flags |= MD_IS_ON;
    }
    if (outgoingEncKeyId_) {
        flags |= ENCRYPTION_IS_ON;
    }
    stemp = htons((u_short)flags);
    memcpy(&dataGram[25], &stemp, 2);
    // how long is the outgoing MD key?
    stemp = htons((u_short) outgoingMdLen_);
    memcpy(&dataGram[27], &stemp, 2);
    // Maybe one for encryption as well
    stemp = htons((u_short) outgoingEidLen_);
    memcpy(&dataGram[29], &stemp, 2);
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
	if(getHeader(last, seq, len, mID, dta)) {
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
    key_ = 0;
    MDKeyId_ = 0;
    EncKeyId_ = 0;
}

_condorOutMsg::_condorOutMsg(KeyInfo * key, const char * keyId)
{
	headPacket = lastPacket = new _condorPacket(true, key, keyId);
	if(!headPacket) {
		dprintf(D_ALWAYS, "new Packet failed. out of memory\n");
		EXCEPT("new Packet failed. out of memory");
	}
	noMsgSent = 0;
	avgMsgSize = 0;
    EncKeyId_ = 0;
    key_ = 0;
    MDKeyId_ = 0;
    init_MD(key, keyId);
}


_condorOutMsg::~_condorOutMsg() {
	_condorPacket* tempPacket;
   
	while(headPacket) {
		tempPacket = headPacket;
		headPacket = headPacket->next;
		delete tempPacket;
	}

    if (MDKeyId_) {
        free(MDKeyId_);
        MDKeyId_ = 0;
    }

    if (EncKeyId_) {
        free(EncKeyId_);
        EncKeyId_ = 0;
    }

    delete key_;
}

bool _condorOutMsg :: set_encryption_id(const char * keyId)
{
    if ((headPacket == lastPacket) && (!headPacket->empty())) {
        return false;
    }

    if (EncKeyId_) {
        free(EncKeyId_);
        EncKeyId_ = 0; 
    }

    if (keyId) {
        EncKeyId_ = strdup(keyId);
    }

    return headPacket->set_encryption_id(EncKeyId_);
}

bool _condorOutMsg::init_MD(KeyInfo * key, const char * keyId)
{
    bool inited = true;
    if ((headPacket == lastPacket) && (!headPacket->empty())) {
        inited = false;
    }
    else {
        // There is one more possibilites: what if the md key is set consectively?
        delete key_;
        key_ = 0; // just to be safe

        if (MDKeyId_) {
            free(MDKeyId_);
            MDKeyId_ = 0;
        }

        if (key) {
            if (keyId) {
                MDKeyId_ = strdup(keyId);
            }
            key_ = new KeyInfo(*key);
        }
        inited = headPacket->init_MD(true, key_, keyId);
    }
    return inited;
}
/* Put n bytes of data
 * This method puts n bytes into the message, while adding packets as needed
 *	@returns: the number of bytes actually stored into the message
 */
int _condorOutMsg::putn(const char *dta, const int size) {
	int total = 0, len = 0;

	while(total != size) {
		if(lastPacket->full()) {
			lastPacket->next = new _condorPacket(true, key_, MDKeyId_);
            lastPacket->next->set_encryption_id(EncKeyId_);
			if(!lastPacket->next) {
				dprintf(D_ALWAYS, "Error: OutMsg::putn: out of memory\n");
				return -1;
			}
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
                           const struct sockaddr* who,
                           _condorMsgID msgID)
{
	_condorPacket* tempPkt;
	int seqNo = 0, msgLen = 0, sent;
	int total = 0;

	if(headPacket->empty()) // empty message
		return 0;
   
	while(headPacket != lastPacket) {
		tempPkt = headPacket;
		headPacket = headPacket->next;
		tempPkt->makeHeader(false, seqNo++, msgID);
        tempPkt->addEID();
        tempPkt->addMD();
		msgLen += tempPkt->length;
		sent = sendto(sock, tempPkt->dataGram,
		              tempPkt->length + SAFE_MSG_HEADER_SIZE,
                      0, who, sizeof(struct sockaddr));
		if( D_FULLDEBUG & DebugFlags )
			dprintf(D_NETWORK, "SafeMsg: Packet[%d] sent\n", sent);
		if(sent != tempPkt->length + SAFE_MSG_HEADER_SIZE) {
			dprintf(D_NETWORK, "sendMsg:sendto failed - errno: %d\n", errno);
			headPacket = tempPkt;
			clearMsg();
			return -1;
		}
		dprintf( D_NETWORK, "SEND %s ", sock_to_string(sock) );
		dprintf( D_NETWORK|D_NOHEADER, "%s\n",
				 sin_to_string((sockaddr_in *)who) );
		total += sent;
		delete tempPkt;
	}

	// headPacket = lastPacket
	if(seqNo == 0) { // a short message
		msgLen = lastPacket->length + 6; // for 6 bytes MD/Encryption
        lastPacket->addEID();
        lastPacket->makeHeader(true, 0, msgID);
        lastPacket->addMD();
		sent = sendto(sock, lastPacket->data - 6, msgLen, 0, who, sizeof(struct sockaddr));
		if( D_FULLDEBUG & DebugFlags ) {
			dprintf(D_NETWORK, "SafeMsg: Packet[%d] sent\n", sent);
        }
		if(sent != msgLen) {
			dprintf( D_NETWORK, "SafeMsg: sending small msg failed. errno: %d\n",  errno );
			headPacket->reset();
			return -1;
		}
		dprintf( D_NETWORK, "SEND %s ", sock_to_string(sock) );
		dprintf( D_NETWORK|D_NOHEADER, "%s\n", sin_to_string((sockaddr_in *)who) );
		total = sent;;
	} else { // the last packet of a long message
		lastPacket->makeHeader(true, seqNo, msgID);
		msgLen += lastPacket->length;
        lastPacket->addEID();
        lastPacket->addMD();
		sent = sendto(sock, lastPacket->dataGram,
		              lastPacket->length + SAFE_MSG_HEADER_SIZE,
		              0, who, sizeof(struct sockaddr));
		if( D_FULLDEBUG & DebugFlags ) {
			dprintf(D_NETWORK, "SafeMsg: Packet[%d] sent\n", sent);
        }
		if(sent != lastPacket->length + SAFE_MSG_HEADER_SIZE) {
			dprintf( D_NETWORK, "SafeMsg: sending last packet failed. errno: %d\n", errno );
			headPacket->reset();
			return -1;
		}
		dprintf( D_NETWORK, "SEND %s ", sock_to_string(sock) );
		dprintf( D_NETWORK|D_NOHEADER, "%s\n", sin_to_string((sockaddr_in *)who) );
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


unsigned long _condorOutMsg::getAvgMsgSize()
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
        dEntry[i].md_     = 0;
        dEntry[i].header_ = 0;
	}
	nextDir = NULL;
}


_condorDirPage::~_condorDirPage() {
	for(int i=0; i< SAFE_MSG_NO_OF_DIR_ENTRY; i++) {
		if(dEntry[i].dGram) {
			free(dEntry[i].dGram);
		}
        if (dEntry[i].md_) {
            free(dEntry[i].md_);
        }
        if (dEntry[i].header_) {
            free(dEntry[i].header_);
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
                const char * MD5Keyid,  // MD5 key id
                const unsigned char * md,  // MD5 key id
                int headerLen,  // length of the header
                const char * EncKeyId,
				_condorInMsg* prev)	// pointer to previous message
								// in the bucket chain
{
	int destDirNo;
	int index;

    mdChecker_ = 0;
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

    if(md) {
        curDir->dEntry[index].md_ = (unsigned char *) malloc(MAC_SIZE);
        memcpy(curDir->dEntry[index].md_, md, MAC_SIZE);
        if (headerLen > 0) { // just to be safe
            curDir->dEntry[index].header_ = (unsigned char *) malloc(headerLen+1);
            memset(curDir->dEntry[index].header_, 0, headerLen+1);
            memcpy(curDir->dEntry[index].header_, 
                   ((char*)data - headerLen), headerLen);
            // This is really hacky!  Hao
        }
    }
    else {
        curDir->dEntry[index].md_     = 0;
        curDir->dEntry[index].header_ = 0;
    }
	// initialize temporary buffer to NULL
	tempBuf = NULL;

	prevMsg = prev;
	nextMsg = NULL;

    if (MD5Keyid) {
        incomingMD5KeyId_ = strdup(MD5Keyid);
    }
    else {
        incomingMD5KeyId_ = 0;
    }

    if (EncKeyId) {
        incomingEncKeyId_ = strdup(EncKeyId);
    }
    else {
        incomingEncKeyId_ = 0;
    }
}


_condorInMsg::~_condorInMsg() {

	if(tempBuf) free(tempBuf);

	_condorDirPage* tempDir;
	while(headDir) {
		tempDir = headDir;
		headDir = headDir->nextDir;
		delete tempDir;
	}
    if (incomingMD5KeyId_) {
        free(incomingMD5KeyId_);
    }
    if (incomingEncKeyId_) {
        free(incomingEncKeyId_);
    }
    delete mdChecker_;
}


/* Add a packet
 *
 */
bool _condorInMsg::addPacket(const bool last,
                             const int seq,
                             const int len, 
                             const void* data,
                             const unsigned char * md,
                             int   headerLen)
{
	int destDirNo, index;

	// check if the message is already ready
	if(lastNo != 0 && lastNo+1 == received) {
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
        if(md) {
            curDir->dEntry[index].md_ = (unsigned char *) malloc(MAC_SIZE);
            memcpy(curDir->dEntry[index].md_, md, MAC_SIZE);
            if (headerLen > 0) { // just to be safe
                curDir->dEntry[index].header_ = (unsigned char *) malloc(headerLen+1);
                memset(curDir->dEntry[index].header_, 0, headerLen+1);
                memcpy(curDir->dEntry[index].header_, 
                       ((char*)data - headerLen), headerLen);
                // This is really hacky!  Hao
            }
        }
        else {
            curDir->dEntry[index].md_     = 0;
            curDir->dEntry[index].header_ = 0;
        }

		if(last)
			lastNo = seq;
		received++;
		if(lastNo + 1 == received) { // every packet has been received
			curDir = headDir;
			curPacket = 0;
			curData = 0;
			return true;
		} else {
			lastTime = time(NULL);
			return false;
		}
	} else // duplicated packet, including "curDir->dEntry[index].dGram == NULL"
		return false;
}

bool _condorInMsg :: init_MD(KeyInfo * key)
{
    resetMD();
    if (key) {
        mdChecker_ = new Condor_MD_MAC(key);
    }
    return true;
}

bool _condorInMsg :: verifyMD(int packet)
{
    bool verified = true;
    if (curDir->dEntry[packet].md_ && mdChecker_) {
        mdChecker_->addMD(curDir->dEntry[packet].header_, 
                          strlen((const char *)curDir->dEntry[packet].header_));
        // We can be a little bit more efficient by storing the 
        // length of the header during initialization
        mdChecker_->addMD((unsigned char *)curDir->dEntry[packet].dGram, 
                          curDir->dEntry[packet].dLen);
        // Now, verify the packet
        if (mdChecker_->verifyMD(curDir->dEntry[packet].md_)) {
            dprintf(D_SECURITY, "MD verified!\n");
            verified = true;
        }
        else {
            dprintf(D_SECURITY, "MD verification failed for long messag\n");
            verified = false;
        }
        if (curDir->dEntry[packet].md_) {
            free(curDir->dEntry[packet].md_);
            curDir->dEntry[packet].md_ = 0;
        }
        if (curDir->dEntry[packet].header_) {
            free(curDir->dEntry[packet].header_);
            curDir->dEntry[packet].header_ = 0;
        }
    }
    else if (mdChecker_ == NULL) {
        return true;
    }
    else {
    }

    return verified;
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
	_condorDirPage* tempDir;

	if(!dta || passed + size > msgLen)
		return -1;

	while(total != size) {
        if (!verifyMD(curPacket)) {
            // should we also delete the data?
            dprintf(D_ALWAYS, "Message digest verification failed!\n");
            return -1;
        }
		len = size - total;
		if(len > curDir->dEntry[curPacket].dLen - curData)
			len = curDir->dEntry[curPacket].dLen - curData;
		memcpy(&dta[total],
		       &(curDir->dEntry[curPacket].dGram[curData]), len);
		total += len;
		curData += len;

		if(curData == curDir->dEntry[curPacket].dLen) {
			// current data page consumed
			// delete the consumed data page and then go to the next page
			//delete[] curDir->dEntry[curPacket].dGram;
			free(curDir->dEntry[curPacket].dGram);
			curDir->dEntry[curPacket].dGram = NULL;

			if(++curPacket == SAFE_MSG_NO_OF_DIR_ENTRY) {
				// was the last of the current dir
				tempDir = headDir;
				headDir = curDir = headDir->nextDir;
				if(headDir)
					headDir->prevDir = NULL;
				delete tempDir;;
				curPacket = 0;
			}
			curData = 0;
		}
	} // of while(total..)

	passed += total;
	if( D_FULLDEBUG & DebugFlags )
		dprintf(D_NETWORK,
		        "%d bytes read & %d bytes passed\n",
			  total, passed);
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
	int n = 1;
	int size;

	if(tempBuf) {
		free(tempBuf);
		tempBuf = NULL;
	}

	tempDir = curDir;
	tempPkt = curPacket;
	tempData = curData;
	while(tempDir->dEntry[tempPkt].dGram[tempData] != delim) {
        if (!verifyMD(tempPkt)) {
            return -1;
        }

		if(++tempData == tempDir->dEntry[tempPkt].dLen) {
			tempPkt++;
			tempData = 0;
			if(tempPkt == SAFE_MSG_NO_OF_DIR_ENTRY) {
				if(!tempDir->nextDir) {
					return -1;
				}
				tempDir = tempDir->nextDir;
				tempPkt = 0;
			} else if(!tempDir->dEntry[tempPkt].dGram) { // was the last packet
				if( D_FULLDEBUG & DebugFlags )
					dprintf(D_NETWORK,
					        "\nSafeMsg::getPtr: get to end & '%c'\
						  not found\n", delim);
				return -1;
			}
		}
		n++;
	} // end of while(tempDir->dEntry[...
	if( D_FULLDEBUG & DebugFlags )
		dprintf(D_NETWORK, "SafeMsg::_longMsg::getPtr:\
		                    found delim = %c & length = %d\n",
			  delim, n);
	tempBuf = (char *)malloc(n);
	if(!tempBuf) {
		dprintf(D_ALWAYS, "getPtr, fail at malloc(%d)\n", n);
		return -1;
	}
	size = getn(tempBuf, n);
	buf = tempBuf;
	//cout << "\t\tInMsg::getPtr: " << (char *)buf << endl;
	return size;
}

/* Peek the next byte */
int _condorInMsg::peek(char &c)
{
	c = curDir->dEntry[curPacket].dGram[curData];
	return TRUE;
}

/* Check if every data in all the bytes of the message have been read */
bool _condorInMsg::consumed()
{
	return(msgLen != 0 && msgLen == passed);
}

const char * _condorInMsg :: isDataMD5ed()
{
    return incomingMD5KeyId_;
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
    if (incomingMD5KeyId_) {
        free(incomingMD5KeyId_);
        incomingMD5KeyId_ = 0;
    }
    if (mdChecker_) {
        delete mdChecker_;
        mdChecker_ = 0;
    }
}

#ifdef DEBUG
void _condorInMsg::dumpMsg()
{
	_condorDirPage *tempDir;
	int i, j, total = 0;

	dprintf(D_NETWORK, "\t=======< ");
	dprintf(D_NETWORK, "Msg[%d, %d, %d, %d]",
	        msgID.ip_addr, msgID.pid, msgID.time, msgID.msgNo);
	dprintf(D_NETWORK, " >=======\n");
	dprintf(D_NETWORK,
	        "\tmsgLen: %d\tlastNo: %d\treceived: %d\tlastTime: %d\n",
	        msgLen, lastNo, received, lastTime);
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
}
#endif
