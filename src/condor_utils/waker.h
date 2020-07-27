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

#ifndef _WAKER_BASE_H_
#define _WAKER_BASE_H_

/***************************************************************
 * Forward declarations
 ***************************************************************/

#include "condor_classad.h"
//class ClassAd;

/***************************************************************
 * Base Waker class
 ***************************************************************/

class WakerBase {

public:

    //@}

	/** @name Instantiation.
		*/
	//@{
	
	/// Constructor
    WakerBase () noexcept;
	
    /// Destructor
    virtual ~WakerBase () noexcept;

    //@}

	/** @name Wake-up Mechanism.
		*/
	//@{

    /** Caries out the wake process.
        @return true if it was succesful; otherwise, false.
    */
    virtual bool doWake () const = 0;

    //@}

    /** We use this to create waker objects so we don't need to deal
    with the differences between OSs at the invocation level.
    @return if the OS is supported a valid WakerBase*; otherwise NULL.
	*/
	static WakerBase* createWaker ( ClassAd *ad );

};

#endif // _WAKER_BASE_H_
