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

#include "condor_crypt_blowfish.h"
#include <string.h>
#include <malloc.h>

Condor_Crypt_Blowfish :: Condor_Crypt_Blowfish(const KeyInfo& key)
#if !defined(SKIP_AUTHENTICATION)
    : Condor_Crypt_Base(CONDOR_BLOWFISH, key)
{
    // initialize 
    resetState();

    // Generate the key
    KeyInfo k(key);
    BF_set_key(&key_, k.getKeyLength(), k.getKeyData());
}
#else
{
}
#endif

Condor_Crypt_Blowfish :: ~Condor_Crypt_Blowfish()
{
}

void Condor_Crypt_Blowfish:: resetState()
#if !defined(SKIP_AUTHENTICATION)
{
     memset(ivec_, 0, 8);
     num_=0;
}
#else
{
}
#endif

bool Condor_Crypt_Blowfish :: encrypt(unsigned char *  input, 
                                      int              input_len, 
                                      unsigned char *& output, 
                                      int&             output_len)
{
#if !defined(SKIP_AUTHENTICATION)
    output_len = input_len;

    output = (unsigned char *) malloc(output_len);

    if (output) {
        // Now, encrypt
        BF_cfb64_encrypt(input, output, output_len, &key_, ivec_, &num_, BF_ENCRYPT);
        return true;
    }
    else {
        return false;
    }
#else
	return true;
#endif
}

bool Condor_Crypt_Blowfish :: decrypt(unsigned char *  input, 
                                      int              input_len, 
                                      unsigned char *& output, 
                                      int&             output_len)
{
#if !defined(SKIP_AUTHENTICATION)
    output_len = input_len;

    output = (unsigned char *) malloc(output_len);

    if (output) {
        // Now, encrypt
        BF_cfb64_encrypt(input, output, output_len, &key_, ivec_, &num_, BF_DECRYPT);
        return true;
    }
    else {
        return false;
    }
#else
	return true;
#endif
}


