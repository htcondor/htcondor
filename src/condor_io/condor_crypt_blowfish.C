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


