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

#include <openssl/dh.h>
#include <openssl/rand.h>
#include "condor_common.h"

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


