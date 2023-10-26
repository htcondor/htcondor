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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "hashkey.h"
#include "condor_attributes.h"
#include "internet.h"

#ifndef WIN32
#include <netinet/in.h>
#endif

void
AdNameHashKey::sprint( std::string &s ) const {
	formatstr( s, "< %s >", name.c_str() );
}

bool operator== (const AdNameHashKey &lhs, const AdNameHashKey &rhs)
{
    return (lhs.name == rhs.name);
}

size_t adNameHashFunction (const AdNameHashKey &key)
{
    size_t bkt = 0;

    bkt += hashFunction(key.name);

    return bkt;
}

// Log a missing attribute warning
void
logWarning( const char *ad_type,
			const char *attrname,
			const char *attrold = NULL,
			const char *attrextra = NULL)
{
	if ( attrold && attrextra ) {
		dprintf(D_FULLDEBUG,
				"%sAd Warning: No '%s' attribute; trying '%s' and '%s'\n",
				ad_type, attrname, attrold, attrextra );
	} else if ( attrold ) {
		dprintf(D_FULLDEBUG,
				"%sAd Warning: No '%s' attribute; trying '%s'\n",
				ad_type, attrname, attrold );
	} else {
		dprintf(D_FULLDEBUG,
				"%sAd Warning: No '%s' attribute; giving up\n",
				ad_type, attrname );
	}
}

// Log a missing attribute error
void
logError( const char *ad_type,
		  const char *attrname,
		  const char *attrold = NULL)
{
	if ( attrold ) {
		dprintf (D_ALWAYS,
				 "%sAd Error: Neither '%s' nor '%s' found in ad\n",
				 ad_type, attrname, attrold );
	} else if ( attrname ) {
		dprintf (D_ALWAYS,
				 "%sAd Error: '%s' not found in ad\n",
				 ad_type, attrname );
	} else {
		dprintf (D_ALWAYS,
				 "%sAd Error: invalid ad\n",
				 ad_type );
	}
}

// Look up an attribute in an ad, optionally fall back to an alternate
// and/or log errors
bool
adLookup( const char *ad_type,
		  const ClassAd *ad,
		  const char *attrname,
		  const char *attrold,
		  std::string &value,
		  bool log = true )
{
	bool	rval = true;

    if ( !ad->LookupString( attrname, value ) ) {
		if ( log ) {
			logWarning( ad_type, attrname, attrold );
		}

		if ( !attrold ) {
			value.clear();
			rval = false;
		} else {
			if ( !ad->LookupString( attrold, value ) ) {
				if ( log ) {
					logError( ad_type, attrname, attrold );
				}
				value.clear();
				rval = false;
			}
		}
	}

	return rval;
}

// Look up an IP attribute in an ad, optionally fall back to an alternate
bool
getIpAddr( const char *ad_type,
		   const ClassAd *ad,
		   const char *attrname,
		   const char *attrold,
		   std::string &ip )
{
	std::string tmp;

	// get the IP and port of the startd
	if ( !adLookup( ad_type, ad, attrname, attrold, tmp, true ) ) {
		return false;
	}

	// If no valid string, do our own thing..
	char* host;
	if ( ( tmp.length() == 0 ) || (host = getHostFromAddr(tmp.c_str())) == NULL  ) {
		dprintf (D_ALWAYS, "%sAd: Invalid IP address in classAd\n", ad_type );
		return false;
	}
	ip = host;
	free(host);

	return true;
}

// functions to make the hashkeys ...
// make hashkeys from the obtained ad
bool
makeStartdAdHashKey (AdNameHashKey &hk, const ClassAd *ad )
{

	// get the name of the startd;
	// this gets complicated with ID
	if ( !adLookup( "Start", ad, ATTR_NAME, NULL, hk.name, false ) ) {
		logWarning( "Start", ATTR_NAME, ATTR_MACHINE, ATTR_SLOT_ID );

		// Get the machine name; if it's not there, give up
		if ( !adLookup( "Start", ad, ATTR_MACHINE, NULL, hk.name, false ) ) {
			logError( "Start", ATTR_NAME, ATTR_MACHINE );
			return false;
		}
		// Finally, if there is a slot ID, append it.
		int	slot;
		if (ad->LookupInteger( ATTR_SLOT_ID, slot)) {
			hk.name += ":";
			hk.name += std::to_string( slot );
		}
	}

	hk.ip_addr = "";
	// As of 7.5.0, we look for MyAddress.  Prior to that, we did not,
	// so new startds must continue to send StartdIpAddr to remain
	// compatible with old collectors.
	if ( !getIpAddr( "Start", ad, ATTR_MY_ADDRESS, ATTR_STARTD_IP_ADDR,
					 hk.ip_addr ) ) {
		dprintf (D_FULLDEBUG,
				 "StartAd: No IP address in classAd from %s\n",
				 hk.name.c_str() );
	}

	return true;
}

bool
makeScheddAdHashKey (AdNameHashKey &hk, const ClassAd *ad )
{

	// get the name of the schedd
	if ( !adLookup( "Schedd", ad, ATTR_NAME, ATTR_MACHINE, hk.name ) ) {
		return false;
	}
	
	// this may be a submittor ad.  if so, we also want to append the
	// schedd name to the hash.  this will fix problems were submittor
	// ads will clobber one another if the more than one schedd runs
	// on the same IP address submitting into the same pool.
	// -Todd Tannenbaum <tannenba@cs.wisc.edu> 2/2005
	std::string tmp;
	if ( adLookup( "Schedd", ad, ATTR_SCHEDD_NAME, NULL, tmp, false ) ) {
		hk.name += tmp;
	}

	// get the IP and port of the schedd 
	// As of 7.5.0, we look for MyAddress.  Prior to that, we did not,
	// so new schedds must continue to send StartdIpAddr to remain
	// compatible with old collectors.
	if ( !getIpAddr( "Schedd", ad, ATTR_MY_ADDRESS, ATTR_SCHEDD_IP_ADDR,
					 hk.ip_addr ) ) {
		return false;
	}

	return true;
}


bool
makeLicenseAdHashKey (AdNameHashKey &hk, const ClassAd *ad )
{

	// get the name of the license
	if ( !adLookup( "License", ad, ATTR_NAME, ATTR_MACHINE, hk.name ) ) {
		return false;
	}
	
	// get the IP and port of the startd 
	if ( !getIpAddr( "License", ad, ATTR_MY_ADDRESS, NULL, hk.ip_addr ) ) {
		return false;
	}

	return true;
}


bool
makeMasterAdHashKey (AdNameHashKey &hk, const ClassAd *ad )
{
	hk.ip_addr = "";
	return adLookup( "Master", ad, ATTR_NAME, ATTR_MACHINE, hk.name );
}


bool
makeCkptSrvrAdHashKey (AdNameHashKey &hk, const ClassAd *ad)
{
	hk.ip_addr = "";
	return adLookup( "CheckpointServer", ad, ATTR_MACHINE, NULL, hk.name );
}

bool
makeCollectorAdHashKey (AdNameHashKey &hk, const ClassAd *ad)
{
	hk.ip_addr = "";
	return adLookup( "Collector", ad, ATTR_NAME, ATTR_MACHINE, hk.name );
}

bool
makeStorageAdHashKey (AdNameHashKey &hk, const ClassAd *ad)
{
	hk.ip_addr = "";
	return adLookup( "Storage", ad, ATTR_NAME, NULL, hk.name );
}

bool
makeAccountingAdHashKey (AdNameHashKey &hk, const ClassAd *ad)
{
	hk.ip_addr = "";
	if ( !adLookup( "Accounting", ad, ATTR_NAME, NULL, hk.name ) ) {
		return false;
	}

	// Get the name of the negotiator this accounting ad is from.
	// Older negotiators didn't set ATTR_NEGOTIATOR_NAME, so this is
	// optional.
	std::string tmp;
	if ( adLookup( "Accounting", ad, ATTR_NEGOTIATOR_NAME, NULL, tmp ) ) {
		hk.name += tmp;
	}
	return true;
}


bool
makeNegotiatorAdHashKey (AdNameHashKey &hk, const ClassAd *ad)
{
	hk.ip_addr = "";
	return adLookup( "Negotiator",  ad, ATTR_NAME, NULL, hk.name );
}


bool
makeHadAdHashKey (AdNameHashKey &hk, const ClassAd *ad)
{
	hk.ip_addr = "";
	return adLookup( "HAD", ad, ATTR_NAME, NULL, hk.name );
}

bool
makeGridAdHashKey (AdNameHashKey &hk, const ClassAd *ad)
{
    std::string tmp;
    
    // get the hash name of the the resource
    if ( !adLookup( "Grid", ad, ATTR_HASH_NAME, NULL, hk.name ) ) {
        return false;
    }
    
    // get the owner associated with the resource
    if ( !adLookup( "Grid", ad, ATTR_OWNER, NULL, tmp ) ) {
        return false;
	}
    hk.name += tmp;

    // get the schedd associated with the resource if we can
    // (this _may_ be undefined when running two instances or
    // Condor on one machine).
    if ( adLookup( "Grid", ad, ATTR_SCHEDD_NAME, NULL, tmp ) ) {
        hk.name += tmp;
    } else {
        if ( !adLookup ( "Grid", ad, ATTR_SCHEDD_IP_ADDR, NULL, 
						 hk.ip_addr ) ) {
            return false;
        }
    }

	if ( adLookup( "Grid", ad, ATTR_GRIDMANAGER_SELECTION_VALUE, NULL, tmp, false ) ) {
		hk.name += tmp;
	}

    return true;
}

// for anything that sends its updates via UPDATE_AD_GENERIC, this
// needs to provide a key that will uniquely identify each entity
// with respect to all entities of that type
// (e.g. this wouldn't work for submitter ads - see code for
// makeScheddAdHashKey above)
bool
makeGenericAdHashKey (AdNameHashKey &hk, const ClassAd *ad )
{
	hk.ip_addr = "";
	return adLookup( "Generic", ad, ATTR_NAME, NULL, hk.name );
}

