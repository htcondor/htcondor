#include "bandCtl.h"

#define MAX_MNG_SOCK 128
#define	SZ_RELI_HASH 47
#define SZ_BAND_HASH 23
#define EPSILONE 65536

#define FULL_USAGE_LIMIT 2
#define BACKOFF_RATE 0.8
#define ADVANCE_RATE 1.5

#define INIT_BANDWIDTH 1194721186
#define HALFCONN_WINDOW 240 // in sec
#define HALFCONN_PERCENT 80 // in percent


class NetMnger;
class BandInfo;


/* Connection information */
class HalfConn {
	friend NetMnger;
	friend BandInfo;
	public:
		HalfConn(const long initMax,
                 const long sec,
                 const short percent,
                 const int mngSock);
		void sendAlloc();

	private:
		long maxBytes;	// in bytes
		long window;	// in sec
		short maxPercent; // 0 - 100
		long curUse;
		bool backOff;	// in backoff state?
		bool reported;	// has been reported from since the last realloc?
		bool realloc;	// necessary to be reallocated?
		int mngSock;
		HalfConn *next;
};


/* Bandwidth information between two hosts */
class BandInfo {
	friend NetMnger;
	public:
		BandInfo(const int ipA,
                 const int ipB,
                 const long bytes);
		void reallocate();
	private:
		int ipA;		// Pair of IP addrs uniquely identifying this info
		int ipB;
		long capacity;	// Maximum bandwidth
		int noConn;		// Number of connections currently being established
		long allocated; // Sum of bandwidths allocated
		long curUse; 	// Sum of bandwidths reported from hosts as being used
		int reported;	// Number of reports having been collected for this session
		HalfConn *halfConn; // List of active connections 
		long spillOut;
		long room;
		int fullUsage;
		BandInfo *next;
};


/* Information of management socket */
class MngSockInfo {
	friend NetMnger;
	public:
	private:
		BandInfo *bandInfo;   // IP of daemon A being connected to this mnger sock
		HalfConn *halfConn;   // Port number of A 
};


//typedef void Sigfunc(int);

class NetMnger {
	public:
        NetMnger(int port, long initBand, int window, int backOffRate);
		void mainLoop();
		void sendPolling();

	private:
		/* Data members */
        int _port;
        long _initBandWidth;
        int _window;
        int _backOffRate;
		SOCKET _listenSock;
		BandInfo *_bandInfoTbl[SZ_BAND_HASH];
		MngSockInfo _mngSockTbl[MAX_MNG_SOCK];

		/* Methods */

        //Sigfunc * mySignal(int signo, Sigfunc *func);
        //void *(mySignal(int signo, void *()))(int);
		void handleMngPacket(const int fd);
		void handleConnRpt(const int fd,
                           const int myIpAddr,
                           const int myPort,
                           const int peerIpAddr,
                           const int peerPort);
		void handleClose(int fd);
		void handleBandRpt(const int fd,
                           const bool congested,
                           const long curUsage);
};
