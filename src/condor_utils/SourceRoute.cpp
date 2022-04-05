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

#include "condor_common.h"
#include "condor_debug.h"
#include "stl_string_utils.h"
#include "SourceRoute.h"

condor_sockaddr SourceRoute::getSockAddr() const {
	condor_sockaddr sa;
	int r = sa.from_ip_string( a.c_str() );
	if (!r) {
		dprintf( D_NETWORK, "Warning -- format of source route %s is not valid.\n", a.c_str());
	}
	sa.set_port( port );
	if( sa.get_protocol() != p ) {
		dprintf( D_NETWORK, "Warning -- protocol of source route doesn't match its address in getSockAddr().\n" );
	}
	return sa;
}

std::string SourceRoute::serialize() {
	std::string rv;
	formatstr( rv, "p=\"%s\"; a=\"%s\"; port=%d; n=\"%s\";", condor_protocol_to_str( p ).c_str(), a.c_str(), port, n.c_str() );
	if(! alias.empty()) { rv += " alias=\"" + alias + "\";"; }
	if(! spid.empty()) { rv += " spid=\"" + spid + "\";"; }
	if(! ccbid.empty()) { rv += " ccbid=\"" + ccbid + "\";"; }
	if(! ccbspid.empty()) { rv += " ccbspid=\"" + ccbspid + "\";"; }
	if( noUDP ) { rv += " noUDP=true;"; }
	if( brokerIndex != -1 ) { formatstr_cat( rv, " brokerIndex=%d;", brokerIndex ); }
	formatstr( rv, "[ %s ]", rv.c_str() );
	return rv;
}
