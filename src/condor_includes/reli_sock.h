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

#ifndef RELI_SOCK_H
#define RELI_SOCK_H

#include "buffers.h"
#include "sock.h"
#include "condor_adtypes.h"
#include "bandCtl.h"

#define BND_CTL_WINS 16
#define BND_CTL_LEVELS 5



/*
**	R E L I A B L E    S O C K
*/

class Authentication;

/** The ReliSock class implements the Sock interface with TCP. */

class ReliSock : public Sock {
	friend class Authentication;

//	PUBLIC INTERFACE TO RELIABLE SOCKS
//
public:

	friend class DaemonCore;

	/*
	**	Methods
	*/

	// Virtual socket services
	//
    ///
	virtual int handle_incoming_packet();

    ///
	virtual int end_of_message();

    /** Connect to a host on a port
        @param s can be a hostname or sinful string
        @param port the port to connect to, ignorred if s contains port
    */
	virtual int connect(char *s, int port=0,  bool do_not_block = false);


    ///
	ReliSock();

	/// Copy ctor
	ReliSock(const ReliSock &);

    ///
	~ReliSock();
    ///
	void init();				/* shared initialization method */

    ///
	int listen();
    ///
	inline int listen(int p) { if (!bind(p)) return FALSE; return listen(); }
    ///

	inline int listen(char *s) { if (!bind(s)) return FALSE; return listen(); }

    ///
	ReliSock *accept();
    ///
	int accept(ReliSock &);
    ///
	int accept(ReliSock *);

    ///
	int put_bytes_nobuffer(char *buf, int length, int send_size=1);
    ///
	int get_bytes_nobuffer(char *buffer, int max_length,
							 int receive_size=1);
    /// returns -1 on failure, else number of bytes transferred
	int get_file(const char *destination, bool flush_buffers=false);
    /// returns -1 on failure, else number of bytes transferred
    int put_file(const char *source);

    ///
	float get_bytes_sent() { return _bytes_sent; }
    ///
	float get_bytes_recvd() { return _bytes_recvd; }
    ///
	void reset_bytes_sent() { _bytes_sent = 0; }
    ///
	void reset_bytes_recvd() { _bytes_recvd = 0; }

#ifndef WIN32
	// interface no longer supported 
	int attach_to_file_desc(int);
#endif

	/*
	**	Stream protocol
	*/

    ///
	virtual stream_type type();

	//	byte operations
	//
    ///
	virtual int put_bytes(const void *, int);
    ///
	virtual int get_bytes(void *, int);
    ///
	virtual int get_ptr(void *&, char);
    ///
	virtual int peek(char &);

    ///
	int authenticate();
    ///
	int isAuthenticated();
    ///
	void unAuthenticate();
    ///
	const char *getOwner();
    ///
	void setOwner( const char * );

    ///
	int isClient() { return is_client; };
    
	/*
	 * for Bandwidth regulation
	 */
    int bandRegulate();

//	PROTECTED INTERFACE TO RELIABLE SOCKS
//
protected:

	/*
	**	Types
	*/

	enum relisock_state { relisock_none, relisock_listen };
	enum bndCtl_state {normal,		// no congestion, wall is defined by _maxBytes
	                   backOff,		// being congested, wall is defined by _maxPercent
				       scouting};	// having been congested and
				                	// now probing network situation

	/*
	**	Methods
	*/
	char * serialize(char *);	// restore state from buffer
	char * serialize() const;	// save state into buffer

	int prepare_for_nobuffering( stream_coding = stream_unknown);

	/*
	**	Data structures
	*/

	class RcvMsg {
	public:
		RcvMsg() : ready(0) {}
		int rcv_packet(SOCKET _sock,
		               SOCKET _mngSock,
		               int _timeout,
					   bool &ready);

		ChainBuf	buf;
		int			ready;
	} rcv_msg;

	class SndMsg {
	public:
		int snd_packet(int _sock,
                       int _mngSock,
					   int end,
					   int _timeout,
                       int threshold,
					   bool &congested,
					   bool &ready);

		Buf			buf;
	} snd_msg;

	relisock_state	_special_state;
	int	ignore_next_encode_eom;
	int	ignore_next_decode_eom;
	float _bytes_sent, _bytes_recvd;

	Authentication * authob;
	int is_client;
	char *hostAddr;

	/*
	 * for Bandwidth regulation
	 */
	// private methods
	int setLimit(const int sec, const int bytes, const float percent);
	int getLimit(int sec[], int bytes[]);
	int getBandwidth(long &shortBand, long &longBand);
	void retreatWall();
	void advanceWall();
	void calculateAllowance();
	void beNice(int size);
	int greetMnger();
	int reportConnect(const int myIP, const short myPort,
                      const int peerIP, const short peerPort);
	void reportClose();
	int reportBandwidth();
	int handleMngPacket();
	int handleAlloc();


	/* member variables */
    int _bandReg; // Tag representing whether in bandwidth control or not
	int _mngSock; // socket discriptor to communicate with netMnger
    bool _greeted; // has greeted to netMnger?

	// The following 6 variables are related to limits given by netMnger or
	// internal limits driven from those external limits
	unsigned long _maxBytes; // max bytes allowed to send during given period
	/* When _maxBytes is given too generously and sending as many bytes as
	 * _maxBytes causes network congestion, ReliSock must back off and should
	 * NOT send more than _maxPercent of what it could. This is the following
	 * variable, _maxPercent, is about.
	 */
	float _maxPercent; // (0.0 - 1.0)
	/* While trying not to send more than _maxBytes (for given period)
	 * or _maxPercent of what the current network situation allows,
	 * ReliSock does not uniformly distribute given limit over the period
	 * and rather allows short bursts. On the other hand, ReliSock does not
	 * allow to congest network by sending too much data for a relitively
	 * short period to make up not sending during the remaining period.
	 * To deploy this policy, ReliSock defines multiple (window, limit) pairs,
	 * meaning that at most 'limit' bytes can be sent during 'window' sec.
	 * These pairs will be maintained as _w[i] and _l[i] respectively, with the
	 * largest pair (_w[0], _l[0]) being the same as the limit and period pair
	 * given by netMnger. As i grows, _w[i] is getting smaller but _l[i] is
	 * getting relatively generous to allow short and reasonable burst.
	 * For easy explanation, from now on let's call the smallest window of
	 * (window, limit) pairs as 'WIN'.
	 */
	unsigned long _w[BND_CTL_LEVELS]; // windows of time in millisec
	unsigned long _l[BND_CTL_LEVELS]; // max bytes

	// The next variable is to represent the capacity of the current network
	// situation in terms of the number of bytes to be sent during the
	// given period. This variable will have different value from _maxBytes
	// and _l[0], when network congestion is being occured by sending _maxBytes.
	unsigned long _capability; // the extrapolated value of the # of bytes
                               // which can be sent during the period

	// 5 variables following are for bookkeeping for long(mid) term regulation
	/* To regulate bandwidth, the actual # of bytes sent must be maintained.
	 * Right? The following _s[i] is all about.
	 *
	 * Each slot will have the # of bytes sent during the corresponding WIN.
	 * Forgot what WIN was, it's the smallest window of the above (window, limit)
	 * pairs.
	 *
	 * IMPORTANT: Bandwidth regulation is enforced by unit of WIN. In other words,
	 * at every WIN sec, ReliSock sets the maximum bytes to be sent during the
	 * next WIN sec by comparing _l[i]'s with the corresponding sum of _s[i]'s.
	 * To be more specific, the limit for the next WIN sec will be set to the
	 * min difference between the sums of the most recent 1, 3, 7, 15 _s[i]'s
	 * from _l[3], _l[2], _l[1], _l[0] respectively. Let's suppose that _l[2]
	 * minus the sum of recent 3 _s[i]'s is the smallest difference. The sum of
	 * the recent 3 _s[i]'s is the actual number of bytes sent during the recent
	 * (_w[2] - WIN) sec and the allowance for the next WIN sec must be set to
	 * _l[2] minus the sum.
	 */
	unsigned long _s[BND_CTL_WINS]; // circular arrary
	int _curIdx; // the most recent _s[i] is defined with respect to this index
	int _resolved; // number of consecutive situations having seemingly
	               // been resolved from congestion
	int _congested; // number of consecutive congestion
	bndCtl_state _bndCtl_state;

	// 4 variables following are for short term bookkeeping
	struct timeval _T; // start time of the current WIN
	unsigned long _allowance; // max bytes to be sent for WIN sec since _T,
	unsigned long _deltaT; // time in millisec elasped since _T
	unsigned long _deltaS; // the bytes sent during the last _deltaT
    int _numSends; // the number of send system calls
	int _shortReached; // the number of times since _T that 'write' or 'send'
	                   // sent less than requested

	// for bandwidth report
	unsigned long _setTime; // time of whichever recent of setLimit call or
	                        // bandwidth reporting
	unsigned long _totalSent; // total bytes sent between _setTime and _T
	unsigned long _totalRcvd; // total bytes received since _setTime
	bool _beenBackedOff; // has been backed off since _setTime?


	/* For testing purpose */
	bool monitor;
	long timeTaken;
	int sending;
	long maxDelta;
};

#endif
