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
