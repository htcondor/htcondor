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

#ifndef CONDOR_CRYPTO_3DES_H
#define CONDOR_CRYPTO_3DES_H

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


#endif /* CONDOR_CRYPTO_3DES_H */
