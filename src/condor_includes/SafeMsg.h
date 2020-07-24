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
            const char * HashKeyId,
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

        void set_sec(const char * HashKeyId,  // key id
                const unsigned char * md,  // key id
                const char * EncKeyId);

        const char * isDataHashed();
        const char * isDataEncrypted();

        void resetEnc();
        void resetMD();

		bool consumed() const;
		void dumpMsg() const;

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
		size_t tempBufLen;
        char * incomingHashKeyId_;
        char * incomingEncKeyId_;
        unsigned char * md_;
        bool   verified_;

			// advance current read position by n
			// n must be <= remaining unread bytes in current chunk
		inline void incrementCurData( int n );
};

static const int SAFE_MSG_MAX_PACKET_SIZE = 60000;
static const int SAFE_MSG_HEADER_SIZE = 25;      
static const int DEFAULT_SAFE_MSG_FRAGMENT_SIZE = 1000;

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
		int getHeader(int msgsize,
                bool &last,
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
		bool consumed() const;

		// 
		int putMax(const void* dta,	// input buffer
			     const int size);	// # of bytes requested

		// check if full
		bool full() const;

		// check if empty
		bool empty();

		// fill the header part with given arguments, return length of the header
		void makeHeader(bool last, int seqNo, _condorMsgID msgID, unsigned char * md = 0);

		// request size for outgoing datagram fragments; returns size
		// actually used.
		int set_MTU(const int mtu);

        const char * isDataHashed();
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
		int m_SAFE_MSG_FRAGMENT_SIZE; // max size of udp datagram
		int m_desired_fragment_size;  // desired max datagram size for next packet

        short            outgoingMdLen_;
        short            outgoingEidLen_;
        char *           incomingHashKeyId_;     // Keyid as seen from the incoming packet
        char *           outgoingHashKeyId_;     // Keeyid for outgoing packet
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
		            const condor_sockaddr& who,
                    _condorMsgID msgID,
                    unsigned char * mac = 0);

		void endOfMsg();

		void clearMsg();

        bool init_MD(const char * keyId = 0);
        bool set_encryption_id(const char * keyId);
		unsigned long getAvgMsgSize() const;

		// request size for outgoing datagram fragments; returns size
		// actually used.
		int set_MTU(const int mtu);

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
		int m_mtu;	// requested size for data fragments
};
