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

#include "condor_common.h"
#include "condor_crypt_3des.h"
#include "condor_debug.h"


Condor_Crypt_3des :: Condor_Crypt_3des(const KeyInfo& key)
#if !defined(SKIP_AUTHENTICATION)
    : Condor_Crypt_Base(CONDOR_3DES, key)
{
    KeyInfo k(key);
    const unsigned char * keyData = k.getKeyData();

    ASSERT(k.getKeyLength() >= 24);
    
    des_set_key((des_cblock *)  keyData    , keySchedule1_);
    des_set_key((des_cblock *) (keyData+8) , keySchedule2_);
    des_set_key((des_cblock *) (keyData+16), keySchedule3_);

    // initialize ivsec
    resetState();
}
#else
{
}
#endif

Condor_Crypt_3des :: ~Condor_Crypt_3des()
{
}

void Condor_Crypt_3des:: resetState()
#if !defined(SKIP_AUTHENTICATION)
{
     memset(ivec_, 0, 8);
     num_=0;
}
#else
{
}
#endif

bool Condor_Crypt_3des :: encrypt(unsigned char *  input, 
                                  int              input_len, 
                                  unsigned char *& output, 
                                  int&             output_len)
{
#if !defined(SKIP_AUTHENTICATION)
    output_len = input_len;

    output = (unsigned char *) malloc(input_len);

    if (output) {
        des_ede3_cfb64_encrypt(input, output, output_len,
                               keySchedule1_, keySchedule2_, keySchedule3_,
                               (des_cblock *)ivec_, &num_, DES_ENCRYPT);
        return true;   
    }
    else {
        return false;
    }
#else
	return true;
#endif
}

bool Condor_Crypt_3des :: decrypt(unsigned char *  input, 
                                  int              input_len, 
                                  unsigned char *& output, 
                                  int&             output_len)
{
#if !defined(SKIP_AUTHENTICATION)
    output = (unsigned char *) malloc(input_len);

    if (output) {
        output_len = input_len;

        des_ede3_cfb64_encrypt(input, output, output_len,
                               keySchedule1_, keySchedule2_, keySchedule3_,
                               (des_cblock *)ivec_, &num_, DES_DECRYPT);
        
        return true;           // Should be changed
    }
    else {
        return false;
    }
#else
	return true;
#endif
}

Condor_Crypt_3des :: Condor_Crypt_3des()
{
}
