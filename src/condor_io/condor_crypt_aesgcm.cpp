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
#include <openssl/rand.h>

#define IV_SIZE 12
#define PADDING_SIZE 1
#define MAC_SIZE 16

unsigned char g_unset_iv[IV_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

Condor_Crypt_AESGCM::Condor_Crypt_AESGCM(const KeyInfo& key)
    : Condor_Crypt_Base(CONDOR_AESGCM, key)
{
	resetState(key.getCryptoState());
}

Condor_Crypt_AESGCM::~Condor_Crypt_AESGCM()
{
}

void Condor_Crypt_AESGCM::resetState()
{
	dprintf(D_NETWORK, "Condor_Crypt_AESGCM::resetState\n");
	m_state.reset(new CryptoState());
	while (!memcmp(m_state->m_iv_enc.iv, g_unset_iv, IV_SIZE)) {
		RAND_pseudo_bytes(m_state->m_iv_enc.iv, IV_SIZE);
	}
	memset(m_state->m_iv_dec.iv, '\0', IV_SIZE);
}

void Condor_Crypt_AESGCM::resetState(std::shared_ptr<CryptoState> state)
{
	dprintf(D_NETWORK, "Condor_Crypt_AESGCM::resetState with key\n");
	if (state.get()) {
		m_state = state;
		while (!memcmp(m_state->m_iv_enc.iv, g_unset_iv, IV_SIZE)) {
			RAND_pseudo_bytes(m_state->m_iv_enc.iv, IV_SIZE);
		}
		if (m_state->m_ctr_conn != 0xffffffff)
			m_state->m_ctr_conn ++;
		else
			dprintf(D_NETWORK, "Condor_Crypt_AESGCM::resetState - Max session reuse hit!\n");
		m_state->m_ctr_enc = 0;
		m_state->m_ctr_dec = 0;
		dprintf(D_NETWORK, "Condor_Crypt_AESGCM::resetState: Connection count %u.\n", m_state->m_ctr_conn);
	} else {
		resetState();
	}	
}

int Condor_Crypt_AESGCM::ciphertext_size(int plaintext_size) const
{
    int ct_sz;
    if (plaintext_size % 16)
        ct_sz = ((plaintext_size >> 4) + 1) << 4;
    else
        ct_sz = plaintext_size;
    // Authentication tag is an additional 16 bytes; IV is 12 bytes; padding size is 1 byte
    ct_sz += MAC_SIZE + IV_SIZE + PADDING_SIZE;
    return ct_sz;
}

bool Condor_Crypt_AESGCM::encrypt(const unsigned char *aad,
                                  int                  aad_len,
                                  const unsigned char *input,
                                  int                  input_len, 
                                  unsigned char *      output, 
                                  int                  output_buf_len)
{
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt\n");

        // output length must be aligned to nearest 128-bit block for AES-GCM
    int output_len;
    if (input_len % 16)
        output_len = ((input_len >> 4) + 1) << 4;
    else
        output_len = input_len;
    if ((output_len - input_len < 0) || (output_len - input_len >= 16)) {
        dprintf(D_NETWORK, "Failed to calculate output size.\n");
        return false;
    }
    char padding_bytes = output_len - input_len;
    if (output_buf_len < output_len) {
        dprintf(D_NETWORK, "Output buffer must be at least %d bytes.\n", output_buf_len);
        return false;
    }
    if (!output) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt - cannot pass a null output buffer.\n");
        return false;
    }

        // Authentication tag is an additional 16 bytes; IV is 12 bytes; padding size is 1 byte
    output_len += MAC_SIZE + IV_SIZE + PADDING_SIZE;

    dprintf(D_NETWORK, "Padding bytes: %d; input length %d\n", padding_bytes, input_len);
    output[0] = padding_bytes;
    memset(output + output_len - MAC_SIZE - padding_bytes, '\0', padding_bytes);

    EVP_CIPHER_CTX *ctx;
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        dprintf(D_NETWORK, "Failed to allocate new EVP method.\n");
        return false;
    }

    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
        dprintf(D_NETWORK, "Failed to create AES-GCM-256 mode.\n");
        return false;
    }

    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL)) {
        dprintf(D_NETWORK, "Failed to set IV length.\n");
        return false;
    }

    int32_t base = ntohl(m_state->m_iv_enc.ctr);
    int32_t result = base + *reinterpret_cast<int32_t*>(&m_state->m_ctr_enc);
    int32_t ctr_enc = htonl(result);
    if (m_state->m_ctr_enc == 0xffffffff) {
        dprintf(D_NETWORK, "Hit max number of packets per connection.\n");
        return false;
    }
    m_state->m_ctr_enc++;

    int32_t base_conn = ntohl(m_state->m_iv_enc.ctr_conn);
    int32_t result_conn = base_conn + *reinterpret_cast<int32_t*>(&m_state->m_ctr_conn);
    int32_t ctr_enc_conn = htonl(result_conn);
    if (m_state->m_ctr_conn == 0xffffffff) {
        dprintf(D_NETWORK, "Hit max number of connections in a session.\n");
        return false;
    }

    unsigned char iv[IV_SIZE];
    memcpy(iv, &ctr_enc, sizeof(ctr_enc));
    memcpy(iv + sizeof(ctr_enc), &ctr_enc_conn, sizeof(ctr_enc_conn));
    memcpy(iv + 2*sizeof(ctr_enc), m_state->m_iv_enc.iv + 2*sizeof(ctr_enc), IV_SIZE - 2*sizeof(ctr_enc));

    dprintf(D_NETWORK, "IV ctr value %d\n", ctr_enc);
    dprintf(D_NETWORK, "Counter plus base value %d\n", result);
    dprintf(D_NETWORK, "Counter value %u\n", m_state->m_ctr_enc-1);

    dprintf(D_NETWORK, "IV conn ctr value %d\n", ctr_enc);
    dprintf(D_NETWORK, "Connection Counter plus base value %d\n", result);
    dprintf(D_NETWORK, "Connection Counter value %u\n", m_state->m_ctr_conn);

    memcpy(output + PADDING_SIZE, iv, IV_SIZE);

    char hex[3 * IV_SIZE + 1];
    dprintf(D_ALWAYS,"IO: Outgoing IV : %s\n",
        debug_hex_dump(hex, reinterpret_cast<char*>(output + PADDING_SIZE), IV_SIZE));

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

    // Authenticate additional data from the caller.
    int len;
    if (aad && (1 != EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len))) {
        dprintf(D_NETWORK, "Failed to authenticate caller input data.\n");
        return false;
    }

    // Ensure that the length of the decrypted buffer is authenticated.
    dprintf(D_NETWORK, "Padding value is %0x\n", *output);
    if (1 != EVP_EncryptUpdate(ctx, NULL, &len, output, PADDING_SIZE)) {
        dprintf(D_NETWORK, "Failed to authenticate length of padding.\n");
        return false;
    }

    dprintf(D_NETWORK, "First byte of plaintext is %0x\n", *input);
    if (1 != EVP_EncryptUpdate(ctx, output + PADDING_SIZE + IV_SIZE,
        &len, input, input_len))
    {
        dprintf(D_NETWORK, "Failed to encrypt plaintext buffer.\n");
        return false;
    }
    dprintf(D_NETWORK, "First %d bytes written to ciphertext.\n", len);

    int len2;
    if (1 != EVP_EncryptFinal_ex(ctx, output + PADDING_SIZE + IV_SIZE + len, &len2)) {
        dprintf(D_NETWORK, "Failed to finalize cipher text.\n");
        return false;
    }
    dprintf(D_NETWORK, "Finalized an additional %d bytes written to ciphertext.\n", len2);
    len += len2;
    dprintf(D_NETWORK,
        "Condor_Crypt_AESGCM::encrypt Cipher text: %0x %0x %0x %0x\n",
        *(output + PADDING_SIZE + IV_SIZE),
        *(output + PADDING_SIZE + IV_SIZE + 1),
        *(output + PADDING_SIZE + IV_SIZE + 2),
        *(output + PADDING_SIZE + IV_SIZE + 3));

    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, MAC_SIZE, output + output_len - MAC_SIZE)) {
        dprintf(D_NETWORK, "Failed to get tag.\n");
        return false;
    }
    dprintf(D_NETWORK, "First two tag values are %0x, %0x starting at %d\n", *(output + output_len - MAC_SIZE), *(output + output_len - MAC_SIZE), output_len - MAC_SIZE);
    memcpy(&ctr_enc, output + PADDING_SIZE, sizeof(ctr_enc));
    dprintf(D_NETWORK, "IV value %d\n", ctr_enc);

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt.  Successful encryption with cipher text %d bytes.\n", output_len);
    return true;
}

bool Condor_Crypt_AESGCM::decrypt(const unsigned char *  aad,
                                  int                    aad_len,
                                  const unsigned char *  input,
                                  int                    input_len, 
                                  unsigned char *        output, 
                                  int&                   output_len)
{
    EVP_CIPHER_CTX *ctx;

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt with input buffer %d.\n", input_len);

    if (output_len < input_len) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt - output length %d must be at least the size of input %d.\n", output_len, input_len);
        return false;
    }
    if (!output) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt - cannot pass a null output buffer.\n");
        return false;
    }

    if (!(ctx = EVP_CIPHER_CTX_new())) {
        dprintf(D_NETWORK, "Failed to initialize EVP object.\n");
        return false;
    }

    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
        dprintf(D_NETWORK, "Failed to initialize AES-GCM-256 mode.\n");
        return false;
    }

    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL)) {
        dprintf(D_NETWORK, "Failed to initialize IV length to %d.\n", IV_SIZE);
        return false;
    }

    if (get_key().getProtocol() != CONDOR_AESGCM) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to the wrong protocol.\n");
        return false;
    }
    dprintf(D_NETWORK, "Key length %d\n", get_key().getKeyLength());

        // TODO: For a SafeSock, we can't enforce an ordering...
    if (m_state->m_ctr_dec == 0xffffffff) {
        dprintf(D_NETWORK, "Hit max number of packets per connection.\n");
        return false;
    }
    if (m_state->m_ctr_conn == 0xffffffff) {
        dprintf(D_NETWORK, "Hit max number of connections per session.\n");
        return false;
    }
    if (!memcmp(m_state->m_iv_dec.iv, g_unset_iv, IV_SIZE)) {
        dprintf(D_NETWORK, "First decrypt - initializing IV\n");
        memcpy(m_state->m_iv_dec.iv, input + PADDING_SIZE, IV_SIZE);
    }

    int32_t base = ntohl(m_state->m_iv_dec.ctr);
    int32_t ctr_enc;
    memcpy(&ctr_enc, input + PADDING_SIZE, sizeof(ctr_enc));
    int32_t cur_packet = ntohl(ctr_enc) - base;
    dprintf(D_NETWORK, "IV ctr value (encoded) %d\n", ctr_enc);
    dprintf(D_NETWORK, "Counter value %u\n", m_state->m_ctr_dec);
    dprintf(D_NETWORK, "Counter value from IV %d\n", cur_packet);
    if (cur_packet != *reinterpret_cast<int32_t*>(&m_state->m_ctr_dec)) {
        dprintf(D_NETWORK, "Counter does not match expected value.\n");
        return false;
    }

    int32_t base_conn = ntohl(m_state->m_iv_dec.ctr_conn);
    int32_t ctr_enc_conn;
    memcpy(&ctr_enc_conn, input + PADDING_SIZE + sizeof(ctr_enc), sizeof(ctr_enc_conn));
    int32_t cur_packet_conn = ntohl(ctr_enc_conn) - base_conn;
    dprintf(D_NETWORK, "IV conn ctr value (encoded) %d\n", ctr_enc_conn);
    dprintf(D_NETWORK, "Connection Counter value %u\n", m_state->m_ctr_conn);
    dprintf(D_NETWORK, "Connection Counter value from IV %d\n", cur_packet_conn);
    if (cur_packet_conn != *reinterpret_cast<int32_t*>(&m_state->m_ctr_conn)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt: Connection counter does not match expected value.\n");
        return false;
    }

    m_state->m_ctr_dec ++;
    if (!memcmp(input + PADDING_SIZE + sizeof(ctr_enc),
        m_state->m_iv_dec.iv, IV_SIZE - sizeof(ctr_enc)))
    {
        dprintf(D_NETWORK, "Unexpected IV from remote side.\n");
        return false;
    }

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt about to init key %0x %0x %0x %0x.\n",
        *(get_key().getKeyData()), *(get_key().getKeyData() + 15),
        *(get_key().getKeyData() + 16), *(get_key().getKeyData() + 31));

    char hex[3 * IV_SIZE + 1];
    dprintf(D_ALWAYS,"IO: Incoming IV : %s\n",
        debug_hex_dump(hex,
        reinterpret_cast<const char *>(input + PADDING_SIZE), IV_SIZE));

    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, get_key().getKeyData(), input + PADDING_SIZE)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to failed init.\n");
        return false;
    }

    int len;
    if (aad && !EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed when authenticating user AAD.\n");
        return false;
    }

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt about to decrypt AAD %0x.\n", *input);
    if (!EVP_DecryptUpdate(ctx, NULL, &len, input, PADDING_SIZE)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to failed AAD update.\n");
        return false;
    }

        // Padding bytes + IV + tag = 1 + 12 + 16
    char padding = input[0];
    dprintf(D_NETWORK,
        "Condor_Crypt_AESGCM::decrypt about to decrypt cipher text."
        "Input length is %d\n",
        input_len - PADDING_SIZE - IV_SIZE - MAC_SIZE - padding);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt Cipher text: "
        "%0x %0x %0x %0x\n",
        *(input + PADDING_SIZE + IV_SIZE),
        *(input + PADDING_SIZE + IV_SIZE + 1),
        *(input + PADDING_SIZE + IV_SIZE + 2),
        *(input + PADDING_SIZE + IV_SIZE + 3));
    if (!EVP_DecryptUpdate(ctx, output, &len, input + PADDING_SIZE + IV_SIZE, input_len - PADDING_SIZE - IV_SIZE - MAC_SIZE - padding)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to failed cipher text update.\n");
        return false;
    }
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt produced output of size %d with value %0x\n", len, *output);

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt about to set tag %0x,%0x.\n", *(input + input_len - 16), *(input + input_len - 15));
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, MAC_SIZE, const_cast<unsigned char *>(input + input_len - MAC_SIZE))) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to failed set of tag.\n");
        return false;
    }

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt about to finalize output.\n");
    if (!EVP_DecryptFinal_ex(ctx, output + len, &len)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to finalize decryption and check of tag.\n");
       return false;
    }

    dprintf(D_NETWORK, "decrypt; input_len is %d and output_len is %d\n", input_len, input_len - padding - PADDING_SIZE - IV_SIZE - MAC_SIZE);
    output_len = input_len - padding - PADDING_SIZE - IV_SIZE - MAC_SIZE;

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt.  Successful decryption with plain text %d bytes.\n", output_len);
    return true;
}
