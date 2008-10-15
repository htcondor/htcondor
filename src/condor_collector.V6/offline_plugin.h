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

#include "MyString.h"
#include "classad_collection.h"

/***************************************************************
 * OfflineCollectorPlugin class
 ***************************************************************/

class OfflineCollectorPlugin {

public:

    OfflineCollectorPlugin () throw ();
    virtual ~OfflineCollectorPlugin ();

    /* Methods */

    /** Reconfigures the plug-in (also used in construction, to
        initialize the plug-in) .
        */
    void update ();

    /** Receive a ClassAd sent as part of an UPDATE_ command,
	    command int is provided.
        */
	void update ( int command, const ClassAd &ad );

    /** Receive a ClassAd INVALIDATE_ command, command int
	    provided.
	    */
	void invalidate ( int command, const ClassAd &ad );

    /* Attributes */

    /** Returns true if the plug-in is enabled; otherwise, false.
        */
    bool enabled () const;  

private:

    /** Persistent copies of machine class-ads to allow us 
        to wake hibernating machines. */
    ClassAdCollection   *ads_;

    /** Storage destination for persistent ads */
    char                *persistent_store_;

};

#endif // _GREEN_COMPUTING_PLUGIN_H_
