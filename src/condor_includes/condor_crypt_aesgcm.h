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

    bool encrypt(const unsigned char * input,
                 int          input_len, 
                 unsigned char *&      output, 
                 int&         output_len);

    bool decrypt(const unsigned char * input,
                 int          input_len, 
                 unsigned char *&      output, 
                 int&         output_len);

 private:
    Condor_Crypt_AESGCM();

    uint64_t m_ctr{0};
};

#endif
