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

#include "condor_md.h"

struct _condorDEntry {
	int             dLen;
	char*           dGram;
};

struct _condorMsgID {
	long ip_addr;
	int pid;
	long time;
	int msgNo;
};


/*******************************************************************
 * Classes for SafeSock buffer management 
 *******************************************************************/
static const int SAFE_MSG_NO_OF_DIR_ENTRY = 41;
class _condorDirPage
{
	friend class _condorInMsg;
	private:
		_condorDirPage* prevDir;
		int dirNo;
		_condorDEntry dEntry[SAFE_MSG_NO_OF_DIR_ENTRY];
		_condorDirPage* nextDir;

	public:
		_condorDirPage(_condorDirPage* prev, const int num);
		~_condorDirPage();
};


class _condorInMsg
{
	friend class SafeSock;

	public:
		// Make a new message with a packet
		_condorInMsg(const _condorMsgID mID,// message ID of this
			const bool last,	// this packet is last or not
			const int seq,	// seq. # of the packet
			const int len,	// length of the packet
			const void* data,	// data of the packet
            const char * MD5KeyId, 
            const unsigned char * md, 
            const char * EncKeyId, 
			_condorInMsg* prev);	// pointer to the previous InMsg in the chain

		~_condorInMsg();

		// add a packet
		bool addPacket(const bool last,	// this packet is last or not
                       const int seq,		// seq. # of the packet
                       const int len,		// length of the packet
                       const void* data);

		// get the next n bytes from the message
		int getn(char *dta,	// output buffer
			   const int n);	// the # of bytes required

		int getPtr (void *&ptr, const char delim);

		// get the current data without incrementing the 'current position'
		int peek(char &c);

        const char * isDataMD5ed();
        const char * isDataEncrypted();

        void resetEnc();
        void resetMD();

		bool consumed();
#ifdef DEBUG
		void dumpMsg();
#endif

	// next line should be uncommented after testing
	private:
        bool verifyMD(Condor_MD_MAC * mdChecker);

		_condorMsgID msgID;	// message ID of this message
		long msgLen;		// length of this message
		int lastNo;			// last packet #, 0 if the last not arrived yet
		int received;		// # of packets received
		time_t lastTime;		// time when the last packet received
		int passed;			// # of bytes read so far
		_condorDirPage* headDir;// head of the linked list of directories
		_condorDirPage* curDir;	// current directory
		int curPacket;		/* index of the current packet into
						 * the curDir->dEntry[] */
		int curData;		/* index of the current data
						 * into the curPacket */
		_condorInMsg* prevMsg;	/* pointer to the previous message in hashed
						 * bucket chain */
		_condorInMsg* nextMsg;	/* pointer to the next message in hashed
						 * bucket chain */
		char *tempBuf;		/* temporary buffer to hold data being taken
						 * from possibly multiple packets */
        char * incomingMD5KeyId_;
        char * incomingEncKeyId_;
        unsigned char * md_;
        bool   verified_;
};

static const int SAFE_MSG_MAX_PACKET_SIZE = 60000;
static const int SAFE_MSG_HEADER_SIZE = 25;      
static const int SAFE_MSG_FRAGMENT_SIZE = 1000;

static const char* const SAFE_MSG_MAGIC = "MaGic6.0";

class _condorPacket
{
	friend class _condorOutMsg;
	friend class SafeSock;

	public:
		_condorPacket();
        ~_condorPacket();

		// get the contents of header
                // returns 1 if is short message; 0 if not; -1 if checksum failed
		int getHeader(bool &last,
				   int &seq,
		   		   int &len,
				   _condorMsgID &mID,
				   void *&dta);

        int headerLen();
        // Get the part of the header that is also check summed

		// get the next n bytes from 'data' portion
		int getn(char *dta,	// output buffer
			   const int n);	// # of required bytes

		int getPtr(void *&ptr, const char delim);

		// Peek the next byte
		int peek(char &c);

		// Initialize data structure
		void reset();

		// Check if every data in the packet has been read
		bool consumed();

		// 
		int putMax(const void* dta,	// input buffer
			     const int size);	// # of bytes requested

		// check if full
		bool full();

		// check if empty
		bool empty();

		// fill the header part with given arguments, return length of the header
		void makeHeader(bool last, int seqNo, _condorMsgID msgID, unsigned char * md = 0);
        
        const char * isDataMD5ed();
        const char * isDataEncrypted();   
        const unsigned char * md();
        bool verifyMD(Condor_MD_MAC * mdChecker);
        void addExtendedHeader(unsigned char * mac) ;
        bool init_MD(const char * keyID = 0);
        bool set_encryption_id(const char * keyId);
        void addMD();
        
#ifdef DEBUG
		// dump the contents of the packet
		void dumpPacket();
#endif

	// next line should be uncommented after testing
	private:
        void init();
        void addEID();       // Add encryption key id to the header
        void checkHeader(int & len, void *& dta);
        void resetMD();
        void resetEnc();
        //------------------------------------------
        //  Private data
        //------------------------------------------
		int length;			// length of this packet
		char* data;			// data portion of this packet
						/* this just points the starting index
						   of data of dataGram[] */
		int curIndex;		// current index into this packet
		char dataGram[SAFE_MSG_MAX_PACKET_SIZE];/* marshalled packet
		                                * including header and data */
		_condorPacket* next;	// next packet

        short            outgoingMdLen_;
        short            outgoingEidLen_;
        char *           incomingMD5KeyId_;     // Keyid as seen from the incoming packet
        char *           outgoingMD5KeyId_;     // Keeyid for outgoing packet
        char *           incomingEncKeyId_;     // Keyid as seen from the incoming packet
        char *           outgoingEncKeyId_;     // Keeyid for outgoing packet
        bool             verified_;
        unsigned char *  md_;
};


class _condorOutMsg
{
	friend class SafeSock;

	public:
		// constructor
		_condorOutMsg();
		_condorOutMsg(KeyInfo * key, const char * keyId);

		~_condorOutMsg();

		// add n bytes of data to the end of the message
		int putn(const char *dta,	// input buffer
			   const int n);		// the # of bytes requested

		// send message to the recipient addressed by (sock, who)
		int sendMsg(const int sock,
		            const struct sockaddr* who,
                    _condorMsgID msgID,
                    unsigned char * mac = 0);

		void endOfMsg();

		void clearMsg();

        bool init_MD(const char * keyId = 0);
        bool set_encryption_id(const char * keyId);
		unsigned long getAvgMsgSize();

#ifdef DEBUG
		// for deburgging purpose
		int dumpMsg(const _condorMsgID mID);
#endif

	// next line should be uncommented after testing
	private:
		_condorPacket* headPacket;	// pointer to the first packet
		_condorPacket* lastPacket;	// pointer to the last packet
		unsigned long noMsgSent;
		unsigned long avgMsgSize;
};
