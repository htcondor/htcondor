/***************************************************************
 *
 * Copyright (C) 2020, Condor Team, Computer Sciences Department,
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


#ifndef CONDOR_CRYPTO_AESGCM_H
#define CONDOR_CRYPTO_AESGCM_H

#include "condor_crypt.h"          // base class

class Condor_Crypt_AESGCM : public Condor_Crypt_Base {

 public:
    Condor_Crypt_AESGCM(const KeyInfo& key);

    ~Condor_Crypt_AESGCM();

    void resetState();

    bool encrypt(const unsigned char *,
                 int                  ,
                 unsigned char *&     ,
                 int&                 ) {return false;}

    bool decrypt(const unsigned char * ,
                       int             ,
                       unsigned char *&,
                       int&            ) {return false;}

    bool encrypt(const unsigned char * aad_data,
                 int                   aad_data_len,
                 const unsigned char * input,
                 int                   input_len, 
                 unsigned char *       output, 
                 int                   output_len);

    bool decrypt(const unsigned char * aad_data,
                 int                   aad_data_len,
                 const unsigned char * input,
                 int                   input_len, 
                 unsigned char *       output, 
                 int&                  output_len);

    virtual int ciphertext_size(int plaintext) const;

 private:
    Condor_Crypt_AESGCM();

    union Packed_IV {
       unsigned char iv[12];
       uint32_t ctr;
    };

        // The IV is split in two: a 32-bit counter and a
        // 64-bit random number for a total of 12 bytes.
        // The ctr is added to the last 4 bytes of the IV.
    uint32_t m_ctr_enc{0};
    uint32_t m_ctr_dec{0};
    union Packed_IV m_iv;
        // The first packet this object receives will set the
        // IV here.
    union Packed_IV m_iv_decrypt;
};

#endif
