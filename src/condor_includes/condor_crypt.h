/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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

    static unsigned char * randomKey(int length = 24);
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
