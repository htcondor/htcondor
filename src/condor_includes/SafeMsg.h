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

#include "condor_md.h"

struct _condorDEntry {
	int             dLen;
	char*           dGram;
    unsigned char * md_;         // not very efficient at this time
    unsigned char * header_;
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
            int   headerLen, // length of the header
            const char * EncKeyId, 
			_condorInMsg* prev);	// pointer to the previous InMsg in the chain

		~_condorInMsg();

		// add a packet
		bool addPacket(const bool last,	// this packet is last or not
                       const int seq,		// seq. # of the packet
                       const int len,		// length of the packet
                       const void* data,
                       const unsigned char* MD,
                       int   headerLen);	// data of the packet

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

        bool init_MD(KeyInfo * key = 0);
		// Check if every data of the message has been read
		bool consumed();
#ifdef DEBUG
		void dumpMsg();
#endif

	// next line should be uncommented after testing
	private:
        bool verifyMD(int packet);

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
        Condor_MD_MAC  * mdChecker_;    // This is for outgoing data       
};

static const int SAFE_MSG_MAX_PACKET_SIZE = 60000;
static const int SAFE_MSG_HEADER_SIZE = 31;      

static const char* const SAFE_MSG_MAGIC = "MaGic6.0";

class _condorPacket
{
	friend class _condorOutMsg;
	friend class SafeSock;

	public:
		_condorPacket();
		_condorPacket(bool outPacket, KeyInfo * key, const char * keyID = 0);
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

		// fill the header part with given arguments
		void makeHeader(bool last, int seqNo, _condorMsgID msgID);


        bool init_MD(bool outPacket, KeyInfo * key = 0, const char * keyID = 0);
        bool set_encryption_id(const char * keyId);
        void addMD();
        
        const char * isDataMD5ed();
        const char * isDataEncrypted();
        const unsigned char * md();  

        bool verified();
#ifdef DEBUG
		// dump the contents of the packet
		void dumpPacket();
#endif

	// next line should be uncommented after testing
	private:
        void init();
        bool verifyMD();
        void setVerified(bool);
        void addEID();       // Add encryption key id to the header
        int checkHeader(int & len, void *& dta);
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

        Condor_MD_MAC  * mdChecker_;    // This is for outgoing data
        short            outgoingMdLen_;
        short            outgoingEidLen_;
        char *           incomingMD5KeyId_;     // Keyid as seen from the incoming packet
        char *           outgoingMD5KeyId_;     // Keeyid for outgoing packet
        char *           incomingEncKeyId_;     // Keyid as seen from the incoming packet
        char *           outgoingEncKeyId_;     // Keeyid for outgoing packet
        unsigned char *  md_;    
        bool             verified_;             // MD is verified
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
                    _condorMsgID msgID);

		void endOfMsg();

		void clearMsg();

        bool init_MD(KeyInfo * key = 0, const char * keyId = 0);
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
        KeyInfo     * key_;
        char *        MDKeyId_;
        char *        EncKeyId_;
};
