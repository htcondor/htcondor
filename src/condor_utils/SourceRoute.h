/***************************************************************
 *
 * Copyright (C) 2015, Condor Team, Computer Sciences Department,
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


#ifndef _SOURCE_ROUTE_H
#define _SOURCE_ROUTE_H

#include "condor_sockaddr.h"

#define PUBLIC_NETWORK_NAME "Internet"

class SourceRoute {
	public:
		SourceRoute(	condor_sockaddr sa, const std::string & networkName ) :
			p( sa.get_protocol() ), a( sa.to_ip_string() ), port( sa.get_port() ), n( networkName ), noUDP( false ), brokerIndex( -1 ) { }
		SourceRoute(	condor_protocol protocol,
						const std::string & address,
						int portNumber,
						const std::string & networkName ) :
						p( protocol ), a( address ), port( portNumber ), n( networkName ), noUDP( false ), brokerIndex( -1 ) {}
		SourceRoute( 	const SourceRoute & ma, const std::string & networkName ) :
			p( ma.p ), a( ma.a ), port( ma.port ), n( networkName ), noUDP( false ), brokerIndex( -1 ) {}

		condor_protocol getProtocol() const { return p; }
		const std::string & getAddress() const { return a; }
		int getPort() const { return port; }
		const std::string & getNetworkName() const { return n; }
		condor_sockaddr getSockAddr() const;

		// Optional attributes.
		void setSharedPortID( const std::string & spid ) { this->spid = spid; }
		void setCCBID( const std::string & ccbid ) { this->ccbid = ccbid; }
		void setCCBSharedPortID( const std::string & ccbspid ) { this->ccbspid = ccbspid; }
		void setAlias( const std::string & alias ) { this->alias = alias; }
		void setBrokerIndex( int brokerIndex ) { this->brokerIndex = brokerIndex; }

		// Optional attributes are empty if unset.
		const std::string & getSharedPortID() const { return spid; }
		const std::string & getCCBID() const { return ccbid; }
		const std::string & getCCBSharedPortID() const { return ccbspid; }
		const std::string & getAlias() const { return alias; }

		// -1 if unset.
		int getBrokerIndex() const { return brokerIndex; }

		bool getNoUDP() const { return noUDP; }
		void setNoUDP( bool flag ) { noUDP = flag; }

		std::string serialize();

	private:
		// Required.
		condor_protocol p;
		std::string a;
		int port;
		std::string n;

		// Optional.
		std::string spid;
		std::string ccbid;
		std::string ccbspid;
		std::string alias;

		bool noUDP;

		int brokerIndex;
};

#endif
