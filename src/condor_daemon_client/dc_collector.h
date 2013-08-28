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


#ifndef _CONDOR_DC_COLLECTOR_H
#define _CONDOR_DC_COLLECTOR_H

#include "condor_common.h"
#include "daemon.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "extArray.h"
#include "daemon_list.h"
#include "condor_timeslice.h"


/** Class to manage the sequence nubmers of individual ClassAds
 * published by the application
 **/
class DCCollectorAdSeq {
  public:
	DCCollectorAdSeq( const char *, const char *, const char * );
	DCCollectorAdSeq( const DCCollectorAdSeq & );
	~DCCollectorAdSeq( void );

	bool Match( const char *, const char *, const char * ) const;
	unsigned getSequenceAndIncrement( void );
	unsigned getSequence( void ) const { return sequence; };

	const char *getName( void ) const { return Name; };
	const char *getMyType( void ) const { return MyType; };
	const char *getMachine( void ) const { return Machine; };

  private:
	char		*Name;			// "Name" in the ClassAd
	char		*MyType;		// "MyType" in the ClassAd
	char		*Machine;		// "Machine" in ClassAd
	unsigned	sequence;		// The sequence number for it.
};

/** Class to manage the sequence nubmers of all ClassAds published by
 * the application
 **/
class DCCollectorAdSeqMan {
  public:
	DCCollectorAdSeqMan( void );
	DCCollectorAdSeqMan( const DCCollectorAdSeqMan &copy,
						 bool copy_array = true );
	~DCCollectorAdSeqMan( void );

	unsigned getSequence( const ClassAd *ad );
	int getNumAds( void ) const { return numAds; };

	// Used for the copy constructor
  protected:
	const ExtArray<DCCollectorAdSeq *> & getSeqInfo( void ) const
		{ return adSeqInfo; };

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

		/// Copy constructor (implemented using deepCopy())
	DCCollector( const DCCollector& );
	DCCollector& operator = ( const DCCollector& );

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
	bool sendUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking );

	void reconfig( void );

	const char* updateDestination( void );

	bool isBlacklisted();
	void blacklistMonitorQueryStarted();
	void blacklistMonitorQueryFinished( bool success );

	bool useTCPForUpdates() { return use_tcp; }

  protected:
		// Get the ad sequence manager (for copy constructor)
	const DCCollectorAdSeqMan &getAdSeqMan( void ) const
		{ return *adSeqMan; };


private:

	void init( bool needs_reconfig );

	void deepCopy( const DCCollector& copy );

	ReliSock* update_rsock;

	char* tcp_collector_host;
	char* tcp_collector_addr;
	int tcp_collector_port;
	bool use_tcp;
	bool use_nonblocking_update;
	UpdateType up_type;

	class UpdateData *pending_update_list;
	friend class UpdateData;

	bool sendTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking );
	bool sendUDPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking );

	static bool finishUpdate( DCCollector *self, Sock* sock, ClassAd* ad1, ClassAd* ad2 );

	void parseTCPInfo( void );
	void initDestinationStrings( void );

	bool initiateTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking );

	char* tcp_update_destination;
	char* udp_update_destination;

	UtcTime m_blacklist_monitor_query_started;
	static std::map< std::string, Timeslice > blacklist;

	Timeslice &getBlacklistTimeslice();

	void displayResults( void );

	// Items to manage the sequence numbers
	time_t startTime;
	DCCollectorAdSeqMan *adSeqMan;
};



#endif /* _CONDOR_DC_COLLECTOR_H */
