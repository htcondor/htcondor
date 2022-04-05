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

#ifndef _OFFLINE_PLUGIN_H_
#define _OFFLINE_PLUGIN_H_

/***************************************************************
 * Headers
 ***************************************************************/

#include "classad_collection.h"

/***************************************************************
 * OfflineCollectorPlugin class
 ***************************************************************/

class OfflineCollectorPlugin : public Service {

public:

    OfflineCollectorPlugin () noexcept;
    virtual ~OfflineCollectorPlugin ();

    /* Methods */

    /** Reconfigures the plug-in (also used in construction, to
        initialize the plug-in) .
        */
    void configure ();

    /** Receive a ClassAd sent as part of an UPDATE_ command,
	    command int is provided.
        */
	void update ( int command, ClassAd &ad );

    /** Receive a ClassAd INVALIDATE_ command, command int
	    provided.
	    */
	void invalidate ( int command, const ClassAd &ad );

	/** This ClassAd is about to removed because its lease has
		expired (aka it is stale).
		If it returns false, that means the Collector should go ahead
		and remove the ad as planned; if returns true,
		then the collector should NOT delete the ad.
		*/
	bool expire ( ClassAd &ad );

    /** Start iterations on all class-ads in the repository.
        */
    void rewind ();

    /** Get the next class-ad in the repository and its key.
        */
    bool iterate ( ClassAd *&ad );

    /* Attributes */

    /** Returns true if the plug-in is enabled; otherwise, false.
        */
    bool enabled () const;

private:

    /** Persistent copies of machine class-ads to allow us
        to wake hibernating machines. */
    ClassAdCollection   *_ads;

    /** Storage destination for persistent ads */
    char                *_persistent_store;

	/** Storage for parsed ABSENT_REQUIREMENTS contraint */
	ExprTree *AbsentReq;

	/** overwrite attributes of ad in collection from given ad */
	void mergeClassAd ( ClassAd &ad, char const *key );

	/** Given an ad, create a key for this ad to use with the
		persistent collection and store result in mykey.
		On success, return a const char* pointing to
		mykey.Value()  (this is what the persistent ad collection
		code wants); on failure, return NULL.
		*/
	const char* makeOfflineKey( const ClassAd &ad, std::string & mykey );

	/** Store ad into the persistent log with the given key, first
		removing any ad w/ the same key already stored.
		If key is passed in as NULL, then a key will be created via
		makeOfflineKey().
		Return true on success, false on failure.
		*/
	bool persistentStoreAd(const char *key, ClassAd &ad);

	/** Remove the ad in persistent log with the given key.
		Return true on success, false on failure. 
		*/
	bool persistentRemoveAd(const char *key);

};

#endif // _GREEN_COMPUTING_PLUGIN_H_
