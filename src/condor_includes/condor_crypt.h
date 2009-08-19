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


#ifndef CONDOR_CRYPTO
#define CONDOR_CRYPTO

#include "condor_common.h"

#include "CryptKey.h"

class Condor_Crypt_Base {

 public:
    Condor_Crypt_Base(Protocol, const KeyInfo& key);
    //------------------------------------------
    // PURPOSE: Cryto base class constructor
    // REQUIRE: None
    // RETURNS: None
    //------------------------------------------

    virtual ~Condor_Crypt_Base();
    //------------------------------------------
    // PURPOSE: Crypto base class destructor
    // REQUIRE: None
    // RETURNS: None
    //------------------------------------------

		// Returns an array of random bytes.
		// Caller should free returned buffer when done.
    static unsigned char * randomKey(int length = 24);
		// Same as randomKey(), but returns string encoded as hex number.
		// The supplied length specifies the number of random bytes,
		// not the number of hex digits (which is twice the number of bytes).
	static char *randomHexKey(int length = 24);
    static unsigned char * oneWayHashKey(const char * initialKey);
    //------------------------------------------
    // PURPOSE: Generate a random key
    //          First method use rand function to generate the key
    //          Second method use MD5 hashing algorithm to generate a key
    //             using the input string. initialkey should not be too short!
    // REQUIRE: length of the key, default to 12
    // RETURNS: a buffer (malloc) with length 
    //------------------------------------------

    Protocol protocol();
    //------------------------------------------
    // PURPOSE: return protocol 
    // REQUIRE: None
    // RETURNS: protocol
    //------------------------------------------

    const KeyInfo& get_key() { return keyInfo_; }

    virtual void resetState() = 0;
    //------------------------------------------
    // PURPOSE: Reset encryption state. This is 
    //          required for UPD encryption
    // REQUIRE: None
    // RETURNS: None
    //------------------------------------------

    virtual bool encrypt(unsigned char *   input, 
                         int               input_len, 
                         unsigned char *&  output, 
                         int&              output_len) = 0;

    virtual bool decrypt(unsigned char *  input, 
                         int              input_len, 
                         unsigned char *& output, 
                         int&             output_len) = 0;

 protected:
    static int encryptedSize(int inputLength, int blockSize = 8);
    //------------------------------------------
    // PURPOSE: return the size of the cipher text given the clear
    //          text size and cipher block size
    // REQUIRE: intputLength -- length of the clear text
    //          blockSize    -- size of the block for encrypting in BYTES
    //                          For instance, DES/blowfish use 64bit(8bytes)
    //                          Default is 8
    // RETURNS: size of the cipher text
    //------------------------------------------

 protected:
    Condor_Crypt_Base();
    //------------------------------------------
    // Pricate constructor
    //------------------------------------------
 private:
    KeyInfo       keyInfo_;
};

#endif
