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
#include "condor_crypt_3des.h"
#include "condor_debug.h"

Condor_Crypt_3des :: Condor_Crypt_3des(const KeyInfo& key)
#if !defined(SKIP_AUTHENTICATION)
    : Condor_Crypt_Base(CONDOR_3DES, key)
{
    KeyInfo k(key);
    
		// triple des requires a key of 8 x 3 = 24 bytes
		// so pad the key out to at least 24 bytes if needed
	unsigned char * keyData = k.getPaddedKeyData(24);
	ASSERT(keyData);

    des_set_key((des_cblock *)  keyData    , keySchedule1_);
    des_set_key((des_cblock *) (keyData+8) , keySchedule2_);
    des_set_key((des_cblock *) (keyData+16), keySchedule3_);

    // initialize ivsec
    resetState();

	free(keyData);
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
