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
