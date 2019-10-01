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

#include <deque>
#include <map>

// This holds a single update ad sequence number
//
class DCCollectorAdSeq {
public:
	DCCollectorAdSeq() : sequence(0), last_advance(0) {}
	long long getSequence() { return sequence; }
	time_t    lastAdvance() { return last_advance; }
	long long advance(time_t now) {
		//if ( ! now) now = time(NULL);
		last_advance = now;
		++sequence;
		return sequence;
	}
protected:
	long long sequence;     // current sequence number
	time_t    last_advance; // last time advance was called (for garbage collection if we ever want to do that)
};

typedef std::map<std::string, DCCollectorAdSeq> DCCollectorAdSeqMap;
class DCCollectorAdSequences {
public:
	DCCollectorAdSeq* getAdSeq(const ClassAd & ad);
private:
	DCCollectorAdSeqMap seqs;
};


/** This is the Collector-specific class derived from Daemon.  It
	implements some of the collectors's daemonCore command interface.  
	For now, it handles sending updates to the collector, so that we
	can optionally turn on TCP updates at sites which require it.
*/
class DCCollector : public Daemon {
public:
	enum UpdateType { UDP, TCP, CONFIG, CONFIG_VIEW };

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
	bool sendUpdate( int cmd, ClassAd* ad1, DCCollectorAdSequences& seq, ClassAd* ad2, bool nonblocking, StartCommandCallbackType=nullptr, void *miscdata=nullptr );

	void reconfig( void );

	const char* updateDestination( void );

	bool isBlacklisted();
	void blacklistMonitorQueryStarted();
	void blacklistMonitorQueryFinished( bool success );

	bool useTCPForUpdates() { return use_tcp; }

	time_t getStartTime() { return startTime; }
	time_t getReconfigTime() { return reconfigTime; }

		/** Request that the collector get an identity token from the specified
		 *  schedd.
		 */
	bool requestScheddToken(const std::string &schedd_name,
		const std::vector<std::string> &authz_bounding_set,
		int lifetime, std::string &token, CondorError &err);

private:

	void init( bool needs_reconfig );

	void deepCopy( const DCCollector& copy );

	ReliSock* update_rsock;

	bool use_tcp;
	bool use_nonblocking_update;
	UpdateType up_type;

	std::deque<class UpdateData*> pending_update_list;
	friend class UpdateData;

	bool sendTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking, StartCommandCallbackType callback_fn, void* miscdata );
	bool sendUDPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking, StartCommandCallbackType callback_fn, void *miscdata );

	static bool finishUpdate( DCCollector *self, Sock* sock, ClassAd* ad1, ClassAd* ad2, StartCommandCallbackType callback_fn, void *miscdata );

	void parseTCPInfo( void );
	void initDestinationStrings( void );

	bool initiateTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking, StartCommandCallbackType callback_fn, void *miscdata );

	char* update_destination;

	struct timeval m_blacklist_monitor_query_started;
	static std::map< std::string, Timeslice > blacklist;

	Timeslice &getBlacklistTimeslice();

	void displayResults( void );

	// Items to manage the sequence numbers
	time_t startTime;
	time_t reconfigTime;
};



#endif /* _CONDOR_DC_COLLECTOR_H */
