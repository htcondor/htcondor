/***************************************************************
 *
 * Copyright (C) 2021, Condor Team, Computer Sciences Department,
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

/**
 * The memory seal / unseal functions provide a mechanism for sealing (encrypting and
 * base64-encoding) and unsealing (decrypting) some contents in memory.
 *
 * The current encryption uses AES256 encryption in CBC mode.  The encryption key is
 * generated lazily per-process and 16 random bytes are used for the IV.
 *
 * These functions are useful when there is some value in memory that should be encrypted
 * "at-rest" and only decrypted when needed.  For example, if the encrypted value is
 * accidentally written into the logfile, there is little harm as the process's key is
 * needed for it to be usable.
 */

#include <vector>
#include <string>

namespace htcondor {

/**
 * Take some binary 'plaintext' data and convert it to base64-encoded ciphertext.
 */
bool memory_seal(const std::vector<unsigned char> &plaintext, std::string &ciphertext_base64);
bool memory_seal(const unsigned char* plaintext, size_t plaintext_len, std::string &ciphertext_base64);
// This alternate assumes text data that is null-terminated.  DOES NOT SERIALIZE THE NULL.
bool memory_seal(const char* plaintext, std::string &ciphertext_base64);

/**
 * Take the base64-encoded output of the memory_seal function and convert it to plaintext
 * binary output.
 */
bool memory_unseal(const std::string &encoded_data_b64, std::vector<unsigned char> &plaintext);
}
