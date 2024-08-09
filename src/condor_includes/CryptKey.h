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


#ifndef CONDOR_CRYPT_KEY
#define CONDOR_CRYPT_KEY

#include "stl_string_utils.h"

enum Protocol {
    CONDOR_NO_PROTOCOL,
    CONDOR_BLOWFISH,
    CONDOR_3DES,
    CONDOR_AESGCM
};

class KeyInfo {
 public:
    KeyInfo() = delete;

    KeyInfo(const unsigned char * keyData,
            size_t          keyDataLen,
            Protocol        protocol,
            int             duration);

    const unsigned char * getKeyData() const;
    
    size_t getKeyLength() const;

    Protocol getProtocol() const;

    int getDuration() const;

	/** Returns a padded key.
		@param len Minimum length of padded key in bytes.
		@return A buffer with the padded key that be deallocated via free() 
				by the caller, or NULL on failure.
	*/
	unsigned char * getPaddedKeyData(size_t len) const;

 private:

	secure_vector   keyData_;
    Protocol        protocol_;
    int             duration_;
};

#endif
