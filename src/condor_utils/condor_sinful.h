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


#ifndef _CONDOR_SINFUL_H
#define _CONDOR_SINFUL_H

/*

 Sinful is HTCondor's source-routed address class.

 As a result of upgrades to support simultaneous IPv4 and IPv6 operation
 ("mixed mode"), this class supports three serializations:
 	* v0, a "sinful" string;
 	* v1, a "sinful" string which supports IPv6 literals and multiple addresses;
 	* v2, a (nested) ClassAd.

 The format of a v0 serialization (a "sinful" string) is:
 	<host:port?params>
 where params is a url-encoded list of parameters.  Host must be an IPv4
 literal.  The parameters ATTR_SOCK, ATTR_ALIAS, ATTR_CCBID,
 'PrivAddr', 'PrivNet', and 'noUDP' have routing significance.

 The format of a v1 serialization (a "sinful" string) is the same, except
 that host may be [host], a bracketed IPv6 literal.  This version is
 presently intended for use internal to the HTCondor networking code.
 Consequently, Sinful does NOT deserialize v1 strings unless specifically
 requested.

 The format of a v2 serialization is a ClassAd list of ClassAds, each
 specifying the following attributes:
	* "a", the address literal (a string)
	* "p", the protocol (a string)
	* "port", the port number (an integer)
	* "n", the network name (a string)
 and optionally
	* "CCB", the CCB ID (a string)
	* "SP", the shared port ID (a string)
	* "CCBSharedPort", the shared port ID of the broker (a string)

 */


#include <string>
#include <map>
#include <vector>

#include "condor_sockaddr.h"

class Sinful {
 public:
 	//
 	// Version-independent methods.
 	//
	Sinful( char const * sinful = NULL, int version = -1 );
	bool valid() const { return m_valid; }
	const std::string & serialize() const;
	std::string logging() const;
	std::string logging( const std::string & ifInvalid ) const;

	//
	// Methods for v1.
	//

	// You should not call this function; use serialize() instead.
	// (Intended only for use internal to the networking code.)
	std::string getV1() const;

	const std::string & getV1Host() const { return v1_host; }
	void setV1Host( const std::string & host ) { v1_host = host; }

	//
	// Methods for v0.
	//

	// You should not call this function; use serialize() instead.
	// (Intended only for constructing ClassAds.)
	char const * getV0() const;

	bool hasV0HostSockAddr() const;
	condor_sockaddr getV0HostSockAddr() const;
	std::string getV0CCBEmbedding() const;

 	//
 	// All methods below this line are deprecated and should be renamed
 	// (e.g., getV0Host()), so that we can find and replace the old and
 	// broken code that assumes that each daemon has One True Address.
 	//

	// returns the host portion of the sinful string
	char const *getHost() const { if( m_host.empty() ) return NULL; else return m_host.c_str(); }

	// returns the port portion of the sinful string
	char const *getPort() const { if( m_port.empty() ) return NULL; else return m_port.c_str(); }

	// returns -1 if port not set; o.w. port number
	int getPortNum() const;

	void setHost(char const *host);
	void setPort(char const *port);
	void setPort(int port);

	// specific params

	// returns the CCB contact info (may be NULL)
	char const *getCCBContact() const;
	void setCCBContact(char const *contact);
	// private network address (may be NULL)
	char const *getPrivateAddr() const;
	void setPrivateAddr(char const *addr);
	// private network name (may be NULL)
	char const *getPrivateNetworkName() const;
	void setPrivateNetworkName(char const *name);
	// id of SharedPortEndpoint (i.e. basename of named socket)
	char const *getSharedPortID() const;
	void setSharedPortID(char const *contact);
	// hostname alias
	char const *getAlias() const;
	void setAlias(char const *alias);
	// is the noUDP flag set in this address?
	bool noUDP() const;
	void setNoUDP(bool flag);

	// generic param interface

	// returns the value of the named parameter (may be NULL)
	char const *getParam(char const *key) const;
	// if value is NULL, deletes the parameter; o.w. sets it
	void setParam(char const *key,char const *value);
	// remove all params
	void clearParams();
	// returns number of params
	int numParams() const;

	// returns true if addr points to this address
	// FIXME: needs rewrite to handle versions...
	bool addressPointsToMe( Sinful const &addr ) const;

 private:
	int m_version;

	// Update the cached serializations.
	void reserialize();

 	// v0 data members.
	std::string m_host;
	std::string m_port;
	std::string m_alias;
	std::map<std::string,std::string> m_params;
	bool m_valid;

	// v1 data members.
	std::string v1_host;

	// v2 data members.

	// Cached serializations.
	std::string v0_serialized;
	std::string v1_serialized;
	std::string v2_serialized;
};

#endif
