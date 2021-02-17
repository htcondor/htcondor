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

#include <openssl/des.h>

bool Condor_Crypt_3des :: encrypt(Condor_Crypto_State *cs,
                                  const unsigned char *  input,
                                  int              input_len, 
                                  unsigned char *& output, 
                                  int&             output_len)
{
    output_len = input_len;

    output = (unsigned char *) malloc(input_len);

    const int des_ks = sizeof(DES_key_schedule);
    if (output) {
        DES_ede3_cfb64_encrypt(input, output, output_len,
                               (DES_key_schedule*)(cs->m_method_key_data),
                               (DES_key_schedule*)(cs->m_method_key_data + des_ks),
                               (DES_key_schedule*)(cs->m_method_key_data + 2*des_ks),
                               (DES_cblock *)cs->m_ivec, &cs->m_num, DES_ENCRYPT);
        return true;   
    }
    else {
        return false;
    }
}

bool Condor_Crypt_3des :: decrypt(Condor_Crypto_State *cs,
                                  const unsigned char *  input,
                                  int              input_len, 
                                  unsigned char *& output, 
                                  int&             output_len)
{
    output = (unsigned char *) malloc(input_len);

    if (output) {
        output_len = input_len;

        const int des_ks = sizeof(DES_key_schedule);
        DES_ede3_cfb64_encrypt(input, output, output_len,
                               (DES_key_schedule*)(cs->m_method_key_data),
                               (DES_key_schedule*)(cs->m_method_key_data + des_ks),
                               (DES_key_schedule*)(cs->m_method_key_data + 2*des_ks),
                               (DES_cblock *)cs->m_ivec, &cs->m_num, DES_DECRYPT);
        
        return true;           // Should be changed
    }
    else {
        return false;
    }
}

