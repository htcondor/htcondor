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

enum Protocol {
    CONDOR_NO_PROTOCOL,
    CONDOR_BLOWFISH,
    CONDOR_3DES,
    CONDOR_AESGCM
};

class KeyInfo {
 public:
    KeyInfo();
    //------------------------------------------
    // Default constructor
    //------------------------------------------

    KeyInfo(const unsigned char * keyData,
            int             keyDataLen,
            Protocol        protocol,
            int             duration);

    //------------------------------------------
    // Construct a key object
    //------------------------------------------

    KeyInfo(const KeyInfo& copy);
    //------------------------------------------
    // Copy constructor
    //------------------------------------------

    KeyInfo& operator=(const KeyInfo& copy);

    ~KeyInfo();

    const unsigned char * getKeyData() const;
    //------------------------------------------
    // PURPOSE: Return the key
    // REQUIRE: None
    // RETURNS: unsigned char * 
    //------------------------------------------
    
    int getKeyLength() const;
    //------------------------------------------
    // PURPOSE: Return length of the key
    // REQUIRE: None
    // RETURNS: length
    //------------------------------------------

    Protocol getProtocol() const;
    //------------------------------------------
    // PURPOSE: Return protocol
    // REQUIRE: None
    // RETURNS: protocol
    //------------------------------------------

    int getDuration() const;
    //------------------------------------------
    // PURPOSE: Return duration
    // REQUIRE: None
    // REQUIRE: None
    //------------------------------------------

	/** Returns a padded key.
		@param len Minimum length of padded key in bytes.
		@return A buffer with the padded key that be deallocated via free() 
				by the caller, or NULL on failure.
	*/
	unsigned char * getPaddedKeyData(int len) const;

 private:
    void init(const unsigned char * keyData, int keyDataLen);

    unsigned char * keyData_;
    int             keyDataLen_;
	//int				keyBufferLen_;
    Protocol        protocol_;
    int             duration_;
};

#endif
