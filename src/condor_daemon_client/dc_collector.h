/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.
 * No use of the CONDOR Software Program Source Code is authorized
 * without the express consent of the CONDOR Team.  For more
 * information contact: CONDOR Team, Attention: Professor Miron Livny,
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

#ifndef _CONDOR_DC_COLLECTOR_H
#define _CONDOR_DC_COLLECTOR_H

#include "condor_common.h"
#include "daemon.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "extArray.h"

/** Class to manage the sequence nubmers of individual ClassAds
 * published by the application
 **/
class DCCollectorAdSeq {
  public:
	DCCollectorAdSeq( const char *, const char *, const char * );
	~DCCollectorAdSeq( void );
	bool Match( const char *, const char *, const char * );
	unsigned GetSequence( void );

  private:
	char		*Name;			// "Name" in the ClassAd
	char		*MyType;		// "MyType" in the ClassAd
	char		*Machine;		// "Machine" in ClassAd
	unsigned sequence;		// The sequence number for it.
};

/** Class to manage the sequence nubmers of all ClassAds published by
 * the application
 **/
class DCCollectorAdSeqMan {
  public:
	DCCollectorAdSeqMan( void );
	~DCCollectorAdSeqMan( void );
	unsigned GetSequence( ClassAd *ad );

  private:
	ExtArray<DCCollectorAdSeq *> adSeqInfo;
	int		numAds;
};

/** This is the Collector-specific class derived from Daemon.  It
	implements some of the collectors's daemonCore command interface.  
	For now, it handles sending updates to the collector, so that we
	can optionally turn on TCP updates at sites which require it.
*/
class DCCollector : public Daemon {
public:
	enum UpdateType { UDP, TCP, CONFIG };

		/** Constructor
			@param name The name (or sinful string) of the collector, 
			NULL if you want local
			@param type What kind up updates to use for it
		*/
	DCCollector( const char* name = NULL, UpdateType type = CONFIG );

		/// Destructor
	~DCCollector();

		/** Send a classad update to this collector.  This will look
			in the config file for TCP_COLLECTOR_HOST, and if defined,
			we'll use a ReliSock (TCP) to connect and send the update.
			If so, we try to keep the ReliSock open and stashed in our
			object, so that we can reuse it for subsequent updates.
			If TCP_COLLECTOR_HOST is not defined, we use a SafeSock
			(UDP) and send it to the regular COLLECTOR_HOST
			@param cmd Integer command you want to send the update with
			@param ad ClassAd you want to use for the update
			file)
		*/
	bool sendUpdate( int cmd, ClassAd* ad1, ClassAd* ad2 = NULL );

	void reconfig( void );

	const char* updateDestination( void );

private:

	void init( void );

		// I can't be copied (yet)
	DCCollector( const DCCollector& );
	DCCollector& operator = ( const DCCollector& );

	ReliSock* update_rsock;

	char* tcp_collector_host;
	char* tcp_collector_addr;
	int tcp_collector_port;
	bool use_tcp;
	UpdateType up_type;

	bool sendTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2 );
	bool sendUDPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2 );

	bool finishUpdate( Sock* sock, ClassAd* ad1, ClassAd* ad2 );

	void parseTCPInfo( void );
	void initDestinationStrings( void );

	bool initiateTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2 );

	char* tcp_update_destination;
	char* udp_update_destination;


	void displayResults( void );

	// Items to manage the sequence numbers
	time_t startTime;
	DCCollectorAdSeqMan adSeqMan;
};



#endif /* _CONDOR_DC_COLLECTOR_H */
