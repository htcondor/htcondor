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


#ifndef CONDOR_CRYPTO_BLOWFISH_H
#define CONDOR_CRYPTO_BLOWFISH_H

#ifdef HAVE_EXT_OPENSSL

#include "condor_crypt.h"          // base class
#include <openssl/blowfish.h>

class Condor_Crypt_Blowfish : public Condor_Crypt_Base {

 public:
    Condor_Crypt_Blowfish(const KeyInfo& key);
    //------------------------------------------
    // PURPOSE: Cryto base class constructor
    // REQUIRE: None
    // RETURNS: None
    //------------------------------------------

    ~Condor_Crypt_Blowfish();
    //------------------------------------------
    // PURPOSE: Crypto base class destructor
    // REQUIRE: None
    // RETURNS: None
    //------------------------------------------

    void resetState();


    bool encrypt(unsigned char * input, 
                 int          input_len, 
                 unsigned char *&      output, 
                 int&         output_len);

    bool decrypt(unsigned char * input, 
                 int          input_len, 
                 unsigned char *&      output, 
                 int&         output_len);

 private:
    Condor_Crypt_Blowfish();
    //------------------------------------------
    // Private constructor
    //------------------------------------------

    int             num_;            // For stream encryption
    BF_KEY          key_;
    unsigned char   ivec_[8];

};

#endif

#endif
