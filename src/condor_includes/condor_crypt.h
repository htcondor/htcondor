/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef CONDOR_CRYPTO
#define CONDOR_CRYPTO

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
    static int    count_;
};

#endif
