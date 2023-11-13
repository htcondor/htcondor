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
	HTCondor has three layers of address information.  At the bottom layer,
	a condor_sockaddr represents a data structure that can be passed into the
	kernel and identifies a particular actual endpoint.  It identifies a
	protocol and a protocol-specific address.  For instance, the protocol
	IPv4 splits its address field into "addresses" and "ports".

	The middle layer is an individual source route, a specific sequence of
	connections to make and data to send down those connections.  At present,
	the most complicated individual source route we can implement or
	represent is the following: a shared port daemon in front of a collector,
	a CCB callback, and a shared port daemon in front of the callback's
	return address.  Note that this only requires the client (source) to
	know a single condor_sockaddr: that of the first shared port daemon.

	Sinful internally represents source routes with the SourceRoute class;
	this is presently used only to simplify de/serializaing the v1 string;
	see below.

	(When we change from chooseAddrFromAddrs() to chooseRouteFromAddress(), we
	should return a SourceRoute, but will most likely return a degenerate v1
	Sinful which lists only the single SourceRoute.  This will allow us
	to continue to use Sinful de/serialization exclusively until we switch
	over to passing objects, instead.)

	The upper layer of addressing is a list of all a daemon's source routes
	and the network name associated with each route's condor_sockaddr.  This
	class manages these, highest-level addresses.

	This class used to be a string-manipulation class.  It serialized a
	route's condor_sockaddr with its ip-and-port string, and appended a
	URL-style parameter list which encoded the routing parameters.  Some
	of these (e.g., CCBID, PrivAddr) included one or more serialized
	Sinful strings.

	We refer to strings of that style as "original Sinful strings."  They
	should never be generated*, but for backwards compatibility, we must
	correctly parse them.  Strings of that style which do not include any IPv6
	addresses, and which have only the following parameters, are referred
	to as "v0 Sinful strings".  We generate v0 strings only for backwards-
	compability, and only as part of the ClassAd we return when asked for
	our ClassAd representation.  (*: Work in progress.)

	(Valid v0 parameters are, exclusively: "PrivAddr", "PrivNet", "noUDP",
	"sock", "alias", and "CCBID".  All other parameters will be ignored
	whe generated a v0 string, including the "addrs" parameter set by
	addAddrToAddrs(), or any custom attributes from setParam().)

	For internal usage, we support generating "v1 Sinful strings."  This
	support is born deprecated; whensoever possible, you should simply
	pass the corresponding Sinful object, instead.  A "v1 Sinful string"
	is the canonical serialization of the corresponding ClassAd (as
	returned by getClassad()).

	The v1 ClassAd is a list of ClassAds, each an individual source route,
	each of which must have the following attributes:
		* "a", the address literal (a string)
		* "p", the protocol (a string)
		* "port", the port number (an integer)
		* "n", the network name
	and which may have the following attributes:
		* "alias", a hostname alias for GSI security
		* "CCB", the CCB ID
		* "CCBSharedPort", the shared port ID of the broker
		* "noUDP", a flag (absent if unset) that prevents UDP,
		* "sp", the shared port ID,
	Note that the a, p, port tuple is a serialization of a condor_sockaddr,
	and should arguably be implemented that way.
*/

#include <string>
#include <map>
#include <vector>

#include "condor_sockaddr.h"
#include "SourceRoute.h"

class Sinful {
	public:
		// We would like getV1String() to be a drop-in replacement for
		// getSinful() at some point, so it has to have the same API.
		char const * getV1String() const;

		// Will return false if the Sinful is invalid.  The two pointer
		// arguments are used to return the "primary" address.
		bool getSourceRoutes( std::vector< SourceRoute > & v,
			std::string * hostOut = NULL, std::string * portOut = NULL ) const;

		// Deprecated.  You should use this, unless you're the CCB server.
		std::string getCCBAddressString() const;

	private:
		void parseV1String();
		std::string m_v1String;
		void regenerateV1String();

		void parseSinfulString();
		std::string m_sinfulString;
		void regenerateSinfulString();

		void regenerateStrings();

 public:
	Sinful(char const *sinful=NULL);
	bool valid() const { return m_valid; }

	char const *getSinful() const;
	char const *getHost() const;
	char const *getPort() const;

	// returns -1 if port not set; o.w. port number
	int getPortNum() const;

	void setHost(char const *host);
	void setPort(char const *port, bool update_all=false);
	void setPort(int port, bool update_all=false);

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


	const std::vector< condor_sockaddr > & getAddrs() const;
	void addAddrToAddrs( const condor_sockaddr & sa );
	void clearAddrs();
	bool hasAddrs();

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
	bool addressPointsToMe( Sinful const &addr ) const;

 private:
	bool m_valid;

	std::string m_host;
	std::string m_port;
	std::string m_alias;
	std::map<std::string,std::string> m_params;

	std::vector< condor_sockaddr > addrs;
};

#endif
