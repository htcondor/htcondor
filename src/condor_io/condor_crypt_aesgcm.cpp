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
#include "condor_crypt_aesgcm.h"

#include <openssl/evp.h>

Condor_Crypt_AESGCM::Condor_Crypt_AESGCM(const KeyInfo& key)
    : Condor_Crypt_Base(CONDOR_AESGCM, key)
{
}

Condor_Crypt_AESGCM::~Condor_Crypt_AESGCM()
{
}

	// IMPORTANT: we never actually reset the state as AES-GCM
	// IVs may not be reused.
void Condor_Crypt_AESGCM::resetState()
{
}

bool Condor_Crypt_AESGCM::encrypt(const unsigned char *  input,
                                  int              input_len, 
                                  unsigned char *& output, 
                                  int&             output_len)
{
        // Encryption size is at most 4 bytes long.
    if (input_len > (1 << 24)) {
        return false;
    }

        // output length must be aligned to nearest 128-bit block for AES-GCM
    if (input_len % 16)
        output_len = ((input_len >> 3) + 1) << 3;
    else
        output_len = input_len;
        // Authentication tag is an additional 16 bytes; IV is 8 bytes; padding size is 1 byte
    output_len += 16 + 8 + 1;

    output = (unsigned char *) malloc(output_len);
    if (!output)
        return false;

    if ((output_len - input_len < 0) || (output_len - input_len >= 16))
        return false;

    char padding_bytes = output_len - input_len;
    output[0] = padding_bytes;

    EVP_CIPHER_CTX *ctx;
    if (!(ctx = EVP_CIPHER_CTX_new()))
        return false;

    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
        return false;

    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 8, NULL))
        return false;

    // TODO: Increase the IV size to also include a timestamp.  If the
    // session key is reused, currently the IV may be reused, resulting in
    // a complete defeat of the encryption.
    uint64_t ctr_enc;
#if __BYTE_ORDER == __BIG_ENDIAN
    ctr_enc = m_ctr; 
#else
    ctr_enc = (((uint64_t)htonl(m_ctr)) << 32) + htonl(m_ctr >> 32);
#endif
    m_ctr++;
    unsigned char iv[8];
    memcpy(iv, &ctr_enc, 8);

    memcpy(output + 1, iv, 8);

    if (get_key().getProtocol() != CONDOR_AESGCM)
        return false;

    if (1 != EVP_EncryptInit_ex(ctx, NULL, NULL, get_key().getKeyData(), iv))
        return false;

    // Ensure that the length of the decrypted buffer is authenticated.
    int len;
    if (1 != EVP_EncryptUpdate(ctx, NULL, &len, output, 1))
        return false;

    if (1 != EVP_EncryptUpdate(ctx, output+9, &len, input, input_len))
        return false;

    int len2;
    if (1 != EVP_EncryptFinal_ex(ctx, output + 9 + len, &len2))
        return false;
    len += len2;

    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, output + 9 + len))
        return false;

    return true;
}

bool Condor_Crypt_AESGCM::decrypt(const unsigned char *  input,
                                  int                    input_len, 
                                  unsigned char *&       output, 
                                  int&                   output_len)
{
    EVP_CIPHER_CTX *ctx;

    if (!(ctx = EVP_CIPHER_CTX_new()))
        return false;

    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
        return false;

    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 8, NULL))
        return false;

    if (get_key().getProtocol() != CONDOR_AESGCM)
        return false;

        // TODO: for a ReliSock, we should enforce an ordering
    uint64_t ctr_enc;
    memcpy(&ctr_enc, input + 1, 8);
#if __BYTE_ORDER == __BIG_ENDIAN
    m_ctr = ctr_enc;
#else
    m_ctr = (((uint64_t)ntohl(static_cast<uint32_t>(ctr_enc)) << 32) + htonl(ctr_enc >> 32);
#endif

    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, get_key().getKeyData(), input + 1))
        return false;

    int len;
    if (!EVP_DecryptUpdate(ctx, NULL, &len, input, 1))
        return false;

        // output is at most input_len.
    output = static_cast<unsigned char *>(malloc(input_len));

        // Padding bytes + IV + tag = 1 + 8 + 16
    if (!EVP_DecryptUpdate(ctx, output, &len, input + 1 + 8, input_len - 1 - 8 - 16))
        return false;

    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<unsigned char *>(input + input_len - 16)))
        return false;

    if (!EVP_DecryptFinal_ex(ctx, output + len, &len))
       return false;

    char padding = input[0];
    output_len = input_len - padding;

    return true;
}
