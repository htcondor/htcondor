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

#ifndef CONDOR_CRYPTO_3DES_H
#define CONDOR_CRYPTO_3DES_H

#if defined(CONDOR_3DES_ENCRYPTION)

#include "condor_common.h"
#include "condor_crypt.h"          // base class
#include <openssl/des.h>

class Condor_Crypt_3des : public Condor_Crypt_Base {

 public:
    Condor_Crypt_3des(const KeyInfo& key);
    //------------------------------------------
    // PURPOSE: Cryto base class constructor
    // REQUIRE: keyLength = 8 * 3 = 24
    // RETURNS: None
    //------------------------------------------

    ~Condor_Crypt_3des();
    //------------------------------------------
    // PURPOSE: Crypto base class destructor
    // REQUIRE: None
    // RETURNS: None
    //------------------------------------------

    void resetState();

    bool encrypt(unsigned char *  input, 
                 int              input_len, 
                 unsigned char *& output, 
                 int&             output_len);

    bool decrypt(unsigned char *  input, 
                 int              input_len, 
                 unsigned char *& output, 
                 int&             output_len);

 private:
    Condor_Crypt_3des();
    //------------------------------------------
    // Private constructor
    //------------------------------------------
    des_key_schedule  keySchedule1_, keySchedule2_, keySchedule3_;
    unsigned char     ivec_[8];
    int               num_;
};


#endif /* defined(CONDOR_3DES_ENCRYPTION) */

#endif /* CONDOR_CRYPTO_3DES_H */
