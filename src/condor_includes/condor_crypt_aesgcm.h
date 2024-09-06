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
    static void initState(StreamCryptoState *ss);

    bool encrypt(Condor_Crypto_State *,
                 const unsigned char *,
                 int                  ,
                 unsigned char *&     ,
                 int&                 ) {EXCEPT("AESGCM: You have called the wrong ::encrypt().\n"); return false;}

    bool decrypt(Condor_Crypto_State *,
                       const unsigned char *,
                       int             ,
                       unsigned char *&,
                       int&            ) {EXCEPT("AESGCM: You have called the wrong ::decrypt().\n"); return false;}

    bool encrypt(Condor_Crypto_State * cs,
                 const unsigned char * aad_data,
                 int                   aad_data_len,
                 const unsigned char * input,
                 int                   input_len, 
                 unsigned char *       output, 
                 int                   output_len);

    bool decrypt(Condor_Crypto_State * cs,
                 const unsigned char * aad_data,
                 int                   aad_data_len,
                 const unsigned char * input,
                 int                   input_len, 
                 unsigned char *       output, 
                 int&                  output_len);

    virtual int ciphertext_size_with_cs(int plaintext_size, StreamCryptoState *ss) const;

};

#endif
