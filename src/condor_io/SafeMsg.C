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

_condorPacket::_condorPacket()
{
	length = 0;
	data = &dataGram[SAFE_MSG_HEADER_SIZE];
	curIndex = 0;
	next = NULL;
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
bool _condorPacket::getHeader(bool &last,
                              int &seq,
					int &len,
					_condorMsgID &mID,
					void *&dta)
{
	if(memcmp(&dataGram[0], SAFE_MSG_MAGIC, 8)) {
		if(len >= 0)
			length = len;
		dta = data = &dataGram[0];
		return true;
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
	return false;
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
	length = 0;
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
	int len = size;

	if(len > SAFE_MSG_MAX_PACKET_SIZE - SAFE_MSG_HEADER_SIZE - curIndex)
		len = SAFE_MSG_MAX_PACKET_SIZE - SAFE_MSG_HEADER_SIZE - curIndex;
	memcpy(&data[curIndex], dta, len);
	curIndex += len;
	length = curIndex;

	return len;
}

bool _condorPacket::full()
{
	if(curIndex == SAFE_MSG_MAX_PACKET_SIZE - SAFE_MSG_HEADER_SIZE)
		return true;
	else return false;
}

bool _condorPacket::empty()
{
	return(length == 0);
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
	int stemp;
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
}


_condorOutMsg::~_condorOutMsg() {
	_condorPacket* tempPacket;
   
	while(headPacket) {
		tempPacket = headPacket;
		headPacket = headPacket->next;
		delete tempPacket;
	}
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
		total += sent;
		delete tempPkt;
	}

	// headPacket = lastPacket
	if(seqNo == 0) { // a short message
		msgLen = lastPacket->length;
		sent = sendto(sock, lastPacket->data, lastPacket->length,
		              0, who, sizeof(struct sockaddr));
		if( D_FULLDEBUG & DebugFlags )
			dprintf(D_NETWORK, "SafeMsg: Packet[%d] sent\n", sent);
		if(sent != lastPacket->length) {
			dprintf( D_NETWORK, 
				 "SafeMsg: sending small msg failed. errno: %d\n",
				 errno );
			headPacket->reset();
			return -1;
		}
		total = sent;;
	} else { // the last packet of a long message
		lastPacket->makeHeader(true, seqNo, msgID);
		msgLen += lastPacket->length;
		sent = sendto(sock, lastPacket->dataGram,
		              lastPacket->length + SAFE_MSG_HEADER_SIZE,
		              0, who, sizeof(struct sockaddr));
		if( D_FULLDEBUG & DebugFlags )
			dprintf(D_NETWORK, "SafeMsg: Packet[%d] sent\n", sent);
		if(sent != lastPacket->length + SAFE_MSG_HEADER_SIZE) {
			dprintf( D_NETWORK, 
			         "SafeMsg: sending last packet failed. errno: %d\n",
			         errno );
			headPacket->reset();
			return -1;
		}
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
		dEntry[i].dLen = 0;
		dEntry[i].dGram = NULL;
	}
	nextDir = NULL;
}


_condorDirPage::~_condorDirPage() {
	for(int i=0; i< SAFE_MSG_NO_OF_DIR_ENTRY; i++)
		if(dEntry[i].dGram) {
			free(dEntry[i].dGram);
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

	prevMsg = prev;
	nextMsg = NULL;
}


_condorInMsg::~_condorInMsg() {

	if(tempBuf) free(tempBuf);

	_condorDirPage* tempDir;
	while(headDir) {
		tempDir = headDir;
		headDir = headDir->nextDir;
		delete tempDir;
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
			return true;
		} else {
			lastTime = time(NULL);
			return false;
		}
	} else // duplicated packet, including "curDir->dEntry[index].dGram == NULL"
		return false;
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
