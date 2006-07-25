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

#if defined(CONDOR_BLOWFISH_ENCRYPTION) && !defined(CONDOR_CRYPTO_BLOWFISH)
#define CONDOR_CRYPTO_BLOWFISH

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
