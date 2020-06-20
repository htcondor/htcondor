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
#include "condor_debug.h"
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

int Condor_Crypt_AESGCM::ciphertext_size(int plaintext_size) const
{
    int ct_sz;
    if (plaintext_size % 16)
        ct_sz = ((plaintext_size >> 4) + 1) << 4;
    else
        ct_sz = plaintext_size;
    // Authentication tag is an additional 16 bytes; IV is 8 bytes; padding size is 1 byte
    ct_sz += 16 + 8 + 1;
    return ct_sz;
}

bool Condor_Crypt_AESGCM::encrypt(const unsigned char *  input,
                                  int              input_len, 
                                  unsigned char *& output, 
                                  int&             output_len)
{
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt\n");

        // output length must be aligned to nearest 128-bit block for AES-GCM
    if (input_len % 16)
        output_len = ((input_len >> 4) + 1) << 4;
    else
        output_len = input_len;
    if ((output_len - input_len < 0) || (output_len - input_len >= 16)) {
        dprintf(D_NETWORK, "Failed to calculate output size.\n");
        return false;
    }
    char padding_bytes = output_len - input_len;

        // Authentication tag is an additional 16 bytes; IV is 8 bytes; padding size is 1 byte
    output_len += 16 + 8 + 1;

    output = (unsigned char *) malloc(output_len);
    if (!output) {
        dprintf(D_NETWORK, "Failed to allocate encryption buffer.\n");
        return false;
    }

    dprintf(D_NETWORK, "Padding bytes: %d; input length %d\n", padding_bytes, input_len);
    output[0] = padding_bytes;

    EVP_CIPHER_CTX *ctx;
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        dprintf(D_NETWORK, "Failed to allocate new EVP method.\n");
        return false;
    }

    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
        dprintf(D_NETWORK, "Failed to create AES-GCM-256 mode.\n");
        return false;
    }

    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 8, NULL)) {
        dprintf(D_NETWORK, "Failed to set IV length.\n");
        return false;
    }

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
    dprintf(D_NETWORK, "IV value %lu\n", ctr_enc);
    dprintf(D_NETWORK, "IV value not encoded %lu\n", m_ctr);

    memcpy(output + 1, iv, 8);

    if (get_key().getProtocol() != CONDOR_AESGCM) {
        dprintf(D_NETWORK, "Failed to have correct AES-GCM key type.\n");
        return false;
    }
    dprintf(D_NETWORK, "Key length %d\n", get_key().getKeyLength());

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt about to init key %0x %0x %0x %0x.\n", *(get_key().getKeyData()), *(get_key().getKeyData() + 15), *(get_key().getKeyData() + 16), *(get_key().getKeyData() + 31));
    if (1 != EVP_EncryptInit_ex(ctx, NULL, NULL, get_key().getKeyData(), iv)) {
        dprintf(D_NETWORK, "Failed to initialize key and IV.\n");
        return false;
    }

    // Ensure that the length of the decrypted buffer is authenticated.
    int len;
    dprintf(D_NETWORK, "Padding value is %c\n", *output);
    if (1 != EVP_EncryptUpdate(ctx, NULL, &len, output, 1)) {
        dprintf(D_NETWORK, "Failed to authenticate length of padding.\n");
        return false;
    }

    dprintf(D_NETWORK, "First byte of plaintext is %0x\n", *input);
    if (1 != EVP_EncryptUpdate(ctx, output+9, &len, input, input_len)) {
        dprintf(D_NETWORK, "Failed to encrypt plaintext buffer.\n");
        return false;
    }
    dprintf(D_NETWORK, "First %d bytes written to ciphertext.\n", len);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt Cipher text: %0x %0x %0x %0x\n", *(output + 1 + 8), *(output + 1 + 8 + 1), *(output + 1 + 8 + 2), *(output + 1 + 8 + 3));

    int len2;
    if (1 != EVP_EncryptFinal_ex(ctx, output + 9 + len, &len2)) {
        dprintf(D_NETWORK, "Failed to finalize cipher text.\n");
        return false;
    }
    dprintf(D_NETWORK, "Finalized an additional %d bytes written to ciphertext.\n", len2);
    len += len2;
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt Cipher text: %0x %0x %0x %0x\n", *(output + 1 + 8), *(output + 1 + 8 + 1), *(output + 1 + 8 + 2), *(output + 1 + 8 + 3));

    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, output + output_len - 16)) {
        dprintf(D_NETWORK, "Failed to get tag.\n");
        return false;
    }
    dprintf(D_NETWORK, "First two tag values are %0x, %0x starting at %d\n", *(output + output_len - 16), *(output + output_len - 15), output_len - 9);
    memcpy(&ctr_enc, output + 1, 8);
    dprintf(D_NETWORK, "IV value %lu\n", ctr_enc);

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt.  Successful encryption with cipher text %d bytes.\n", output_len);
    return true;
}

bool Condor_Crypt_AESGCM::decrypt(const unsigned char *  input,
                                  int                    input_len, 
                                  unsigned char *&       output, 
                                  int&                   output_len)
{
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt with payload %d.\n", input_len);
    output_len = 0;
    EVP_CIPHER_CTX *ctx;

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt with input buffer %d.\n", input_len);

    if (!(ctx = EVP_CIPHER_CTX_new())) {
        dprintf(D_NETWORK, "Failed to initialize EVP object.\n");
        return false;
    }

    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
        dprintf(D_NETWORK, "Failed to initialize AES-GCM-256 mode.\n");
        return false;
    }

    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 8, NULL)) {
        dprintf(D_NETWORK, "Failed to initialize IV length to 8.\n");
        return false;
    }

    if (get_key().getProtocol() != CONDOR_AESGCM) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to the wrong protocol.\n");
        return false;
    }
    dprintf(D_NETWORK, "Key length %d\n", get_key().getKeyLength());

        // TODO: for a ReliSock, we should enforce an ordering
    uint64_t ctr_enc;
    memcpy(&ctr_enc, input + 1, 8);
    dprintf(D_NETWORK, "IV value %lu\n", ctr_enc);
#if __BYTE_ORDER == __BIG_ENDIAN
    m_ctr = ctr_enc;
#else
    m_ctr = ((uint64_t)ntohl(static_cast<uint32_t>(ctr_enc)) << 32) + htonl(ctr_enc >> 32);
#endif
    dprintf(D_NETWORK, "IV value unencoded %lu\n", m_ctr);

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt about to init key %0x %0x %0x %0x.\n", *(get_key().getKeyData()), *(get_key().getKeyData() + 15), *(get_key().getKeyData() + 16), *(get_key().getKeyData() + 31));
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, get_key().getKeyData(), input + 1)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to failed init.\n");
        return false;
    }

    int len;
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt about to decrypt AAD %c.\n", *input);
    if (!EVP_DecryptUpdate(ctx, NULL, &len, input, 1)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to failed AAD update.\n");
        return false;
    }

        // output is at most input_len.
    output = static_cast<unsigned char *>(malloc(input_len));

        // Padding bytes + IV + tag = 1 + 8 + 16
    char padding = input[0];
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt about to decrypt cipher text.  Input length is %d\n", input_len - 1 - 8 - 16 - padding);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt Cipher text: %0x %0x %0x %0x\n", *(input + 1 + 8), *(input + 1 + 8 + 1), *(input + 1 + 8 + 2), *(input + 1 + 8 + 3));
    if (!EVP_DecryptUpdate(ctx, output, &len, input + 1 + 8, input_len - 1 - 8 - 16 - padding)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to failed cipher text update.\n");
        return false;
    }
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt produced output of size %d with value %0x\n", len, *output);

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt about to set tag %0x,%0x.\n", *(input + input_len - 16), *(input + input_len - 15));
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<unsigned char *>(input + input_len - 16))) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to failed set of tag.\n");
        return false;
    }

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt about to finalize output.\n");
    if (!EVP_DecryptFinal_ex(ctx, output + len, &len)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to finalize decryption and check of tag.\n");
       return false;
    }

    dprintf(D_NETWORK, "decrypt; input_len is %d and output_len is %d\n", input_len, input_len - padding - 1 - 8 - 16);
    output_len = input_len - padding - 1 - 8 - 16;

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt.  Successful decryption with plain text %d bytes.\n", output_len);
    return true;
}
