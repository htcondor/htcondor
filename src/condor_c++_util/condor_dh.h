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

#ifndef CONDOR_DH
#define CONDOR_DH

#include "condor_common.h"
#include <openssl/dh.h>
#include <openssl/rand.h>

//----------------------------------------------------------------------
//  Diffie-Hellman key exchange, based on API provided by OpenSSL
//  privately known variables: x and y -- the secret, one for each
//                             party
//  publicly known variables:  g -- the generator, p -- the prime, 
//                             g^x -- the public key
//----------------------------------------------------------------------

class Condor_Diffie_Hellman {

 public:
    Condor_Diffie_Hellman();
    ~Condor_Diffie_Hellman();
    
    char * getPublicKeyChar();
    //------------------------------------------
    // PURPOSE: Return public key in HEX encoded format
    // REQUIRE: None
    // RETURNS: HEX string or NULL
    //------------------------------------------

    BIGNUM * getPrime();
    BIGNUM * getGenerator();
    // These two methods return the prime and the generator
    
    char * getPrimeChar();
    char * getGeneratorChar();
    // These two methods return the prime and the generator
    // in HEX encoded format if they exist. Otherwise, NULL is returned.

    int  compute_shared_secret(const char * pk);
    const unsigned char * getSecret() const;
    int getSecretSize() const;

 private:
    int initialize();

    DH * dh_;
    unsigned char * secret_;
    int keySize_;
};


#endif
