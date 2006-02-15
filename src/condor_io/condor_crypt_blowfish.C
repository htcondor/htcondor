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

#include "condor_common.h"
#include "condor_crypt_blowfish.h"
//#include <string.h>
//#include <malloc.h>

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


