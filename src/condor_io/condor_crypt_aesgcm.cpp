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

#include <memory>
#include <algorithm>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define IV_SIZE 16
#define MAC_SIZE 16

unsigned char g_unset_iv[IV_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// this function is static
void Condor_Crypt_AESGCM::initState(StreamCryptoState* stream_state)
{
	dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::initState for %p.\n", stream_state);
	if (stream_state) {
		// reset encrypt side
		int r = RAND_bytes(stream_state->m_iv_enc.iv, IV_SIZE);
		ASSERT(r == 1);
		stream_state->m_ctr_enc = 0;

		// reset decrypt side
		memset(stream_state->m_iv_dec.iv, '\0', IV_SIZE);
		stream_state->m_ctr_dec = 0;
	} else {
		// resetState();
		ASSERT("stream_state must not be NULL!");
	}	
}

int Condor_Crypt_AESGCM::ciphertext_size_with_cs(int plaintext_size, StreamCryptoState * stream_state) const
{
    int ct_sz = plaintext_size;
    // Authentication tag is an additional 16 bytes; IV is also 16 bytes
    //
    // we only need to send the IV on the first transmission.
    //
    ct_sz += MAC_SIZE + IV_SIZE * (stream_state->m_ctr_enc ? 0 : 1);

    return ct_sz;
}

bool Condor_Crypt_AESGCM::encrypt(Condor_Crypto_State *cs,
                                  const unsigned char *aad,
                                  int                  aad_len,
                                  const unsigned char *input,
                                  int                  input_len, 
                                  unsigned char *      output, 
                                  int                  output_buf_len)
{
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt **********************\n");
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt with %d bytes of input\n", input_len);
    StreamCryptoState *stream_state = &(cs->m_stream_crypto_state);

    int output_len = input_len;
    if (output_buf_len < output_len) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::encrypt: ERROR: Output buffer must be at least %d bytes.\n", output_buf_len);
        return false;
    }
    if (!output) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::encrypt: ERROR: cannot pass a null output buffer.\n");
        return false;
    }

    // determine if we will send an IV or not (this gets looked at often)
    //
    bool sending_IV = (stream_state->m_ctr_enc == 0);

    // Authentication tag is an additional 16 bytes; IV is 16 bytes
    output_len += MAC_SIZE + (sending_IV ? IV_SIZE : 0);

    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!ctx) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::encrypt: ERROR: Failed to allocate new EVP method.\n");
        return false;
    }

    if (1 != EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), NULL, NULL, NULL)) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::encrypt: ERROR: Failed to create AES-GCM-256 mode.\n");
        return false;
    }

    if (1 != EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL)) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::encrypt: ERROR: Failed to set IV length.\n");
        return false;
    }

    // here we do the math to change the IV.  we take the lowest 4 bytes, treat
    // it as an int, add the message counter, and put it back.  this guarantees
    // the IV changes from packet to packet.  if we max out, we don't want to
    // reuse an IV, so we bail.
    int32_t base = ntohl(stream_state->m_iv_enc.ctr.pkt);
    int32_t result = base + *reinterpret_cast<int32_t*>(&stream_state->m_ctr_enc);
    int32_t ctr_encoded = htonl(result);
    if (stream_state->m_ctr_enc == 0xffffffff) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::encrypt: ERROR: Hit max number of packets per connection.\n");
        return false;
    }

    // construct the IV to be used.
    unsigned char iv[IV_SIZE];
    memcpy(iv, &ctr_encoded, sizeof(ctr_encoded));
    memcpy(iv + sizeof(ctr_encoded), stream_state->m_iv_enc.iv + sizeof(ctr_encoded), IV_SIZE - sizeof(ctr_encoded));

    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt DUMP : IV base value %d\n", base);
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt DUMP : IV Counter value _enc %u\n", stream_state->m_ctr_enc);
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt DUMP : IV Counter plus base value %d\n", result);
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt DUMP : IV Counter plus base value (encoded) %d\n", ctr_encoded);

        // Only need to send IV for the first encrypted packet.
    if (sending_IV) {
        dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt DUMP : First packet - will send IV, copying to beginning of output\n");
        memcpy(output, iv, IV_SIZE);
    }

    // for debugging, hexdbg at different times needs to hold hex
    // representation of IV, MAC, or initial AAD bytes.  currently
    // none are larger than 16 so the 128 is plenty.
    char hexdbg[128];
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt DUMP : Final IV used for outgoing encrypt: %s\n",
        debug_hex_dump(hexdbg, reinterpret_cast<char*>(iv), IV_SIZE));

    if (cs->m_keyInfo.getProtocol() != CONDOR_AESGCM) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::encrypt: ERROR: Failed to have correct AES-GCM key type.\n");
        return false;
    }

    const unsigned char *kdp = cs->m_keyInfo.getKeyData();
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt DUMP : about to init key %0x %0x %0x %0x.\n",
        *(kdp), *(kdp + 15), *(kdp + 16), *(kdp + 31));

    if (1 != EVP_EncryptInit_ex(ctx.get(), NULL, NULL, cs->m_keyInfo.getKeyData(), iv)) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::encrypt: ERROR: Failed to initialize key and IV.\n");
        return false;
    }

    // Authenticate additional data from the caller.
    int len;
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt DUMP : We have %d bytes of AAD data: %s...\n",
        aad_len, debug_hex_dump(hexdbg, reinterpret_cast<const char *>(aad), std::min(16, aad_len)));
    if (aad && (1 != EVP_EncryptUpdate(ctx.get(), NULL, &len, aad, aad_len))) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::encrypt: ERROR: Failed to authenticate caller input data.\n");
        return false;
    }

    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt DUMP : We have %d bytes of plaintext\n", input_len);
    if (1 != EVP_EncryptUpdate(ctx.get(), output + (sending_IV ? IV_SIZE : 0),
        &len, input, input_len))
    {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::encrypt: ERROR: Failed to encrypt plaintext buffer.\n");
        return false;
    }
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt DUMP : First %d bytes written to ciphertext.\n", len);

    int len2;
    if (1 != EVP_EncryptFinal_ex(ctx.get(), output + (sending_IV ? IV_SIZE : 0) + len, &len2)) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::encrypt: ERROR: Failed to finalize cipher text.\n");
        return false;
    }
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt DUMP : Finalized an additional %d bytes written to ciphertext.\n", len2);
    len += len2;

    // we never use the result of len or len2 after this.  how would len2 be non-zero?
    ASSERT(len2 == 0);

	if (IsDebugLevel(D_NETWORK) && (input_len > 3) && (output_len > 3)) {
		dprintf(D_NETWORK | D_VERBOSE,
				"Condor_Crypt_AESGCM::encrypt DUMP : Plain text: %0x %0x %0x %0x ... %0x %0x %0x %0x\n",
				*(input),
				*(input + 1),
				*(input + 2),
				*(input + 3),
				*(input + input_len - 4),
				*(input + input_len - 3),
				*(input + input_len - 2),
				*(input + input_len - 1));

		dprintf(D_NETWORK | D_VERBOSE,
				"Condor_Crypt_AESGCM::encrypt DUMP : Cipher text: %0x %0x %0x %0x ... %0x %0x %0x %0x\n",
				*(output + (sending_IV ? IV_SIZE : 0)),
				*(output + (sending_IV ? IV_SIZE : 0) + 1),
				*(output + (sending_IV ? IV_SIZE : 0) + 2),
				*(output + (sending_IV ? IV_SIZE : 0) + 3),
				*(output + output_len - MAC_SIZE - 4),
				*(output + output_len - MAC_SIZE - 3),
				*(output + output_len - MAC_SIZE - 2),
				*(output + output_len - MAC_SIZE - 1));
	}

    // extract the tag directly into the output stream to be given to CEDAR
    if (1 != EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, MAC_SIZE, output + output_len - MAC_SIZE)) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::encrypt: ERROR: Failed to get tag.\n");
        return false;
    }
    char hex2[3 * MAC_SIZE + 1];
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt DUMP : Outgoing MAC : %s\n",
        debug_hex_dump(hex2, reinterpret_cast<char*>(output + output_len - MAC_SIZE), MAC_SIZE));

    // Only change state if everything was successful.
    stream_state->m_ctr_enc++;

    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::encrypt.  Successful encryption with cipher text %d bytes.\n", output_len);
    return true;
}

bool Condor_Crypt_AESGCM::decrypt(Condor_Crypto_State *cs,
                                  const unsigned char *aad,
                                  int                    aad_len,
                                  const unsigned char *  input,
                                  int                    input_len, 
                                  unsigned char *        output, 
                                  int&                   output_len)
{
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);

    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decrypt **********************\n");
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decrypt with input buffer %d.\n", input_len);
    StreamCryptoState *stream_state = &(cs->m_stream_crypto_state);

    if (output_len < input_len) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: output length %d must be at least the size of input %d.\n", output_len, input_len);
        return false;
    }
    if (!output) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: cannot pass a null output buffer.\n");
        return false;
    }

    if (!ctx) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: Failed to initialize EVP object.\n");
        return false;
    }

    if (!EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), NULL, NULL, NULL)) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: Failed to initialize AES-GCM-256 mode.\n");
        return false;
    }

    if (!EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL)) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: Failed to initialize IV length to %d.\n", IV_SIZE);
        return false;
    }

    if (cs->m_keyInfo.getProtocol() != CONDOR_AESGCM) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: failed due to the wrong protocol.\n");
        return false;
    }

    if (stream_state->m_ctr_dec == 0xffffffff) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: Hit max number of packets per connection.\n");
        return false;
    }

    // we ould only need to receive the IV on the first transmission (this gets looked at a lot)
    bool receiving_IV = (stream_state->m_ctr_dec == 0);

    if (receiving_IV) {
        dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decyrpt DUMP : First decrypt - initializing IV\n");
        memcpy(stream_state->m_iv_dec.iv, input, IV_SIZE);
    }

    // here we do the math to change the IV.  we take the lowest 4 bytes, treat
    // it as an int, add the message counter, and put it back.  this guarantees
    // the IV changes from packet to packet.
    int32_t base = ntohl(stream_state->m_iv_dec.ctr.pkt);
    int32_t result = base + *reinterpret_cast<int32_t*>(&stream_state->m_ctr_dec);
    int32_t ctr_encoded = htonl(result);
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decyrpt DUMP : IV base value %d\n", base);
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decyrpt DUMP : IV Counter value _dec %u\n", stream_state->m_ctr_dec);
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decyrpt DUMP : IV Counter plus base value %d\n", result);
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decyrpt DUMP : IV Counter plus base value (encoded) %d\n", ctr_encoded);

    // Assemble our expected IV.
    unsigned char iv[IV_SIZE];
    memcpy(iv, &ctr_encoded, sizeof(ctr_encoded));
    memcpy(iv + sizeof(ctr_encoded), stream_state->m_iv_dec.iv + sizeof(ctr_encoded), IV_SIZE - sizeof(ctr_encoded));

    const unsigned char *kdp = cs->m_keyInfo.getKeyData();
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decrypt DUMP : about to init key %0x %0x %0x %0x.\n",
        *(kdp), *(kdp + 15), *(kdp + 16), *(kdp + 31));

    // for debugging, hexdbg at different times needs to hold hex
    // representation of IV, MAC, or initial AAD bytes.  currently
    // none are larger than 16 so the 128 is plenty.
    char hexdbg[128];
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decyrpt DUMP : IV used for incoming decrypt: %s\n",
        debug_hex_dump(hexdbg,
        reinterpret_cast<const char *>(iv), IV_SIZE));

    if (!EVP_DecryptInit_ex(ctx.get(), NULL, NULL, kdp, iv)) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: failed due to failed init.\n");
        return false;
    }

    int len;
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decrypt DUMP : We have %d bytes of AAD data: %s...\n",
        aad_len, debug_hex_dump(hexdbg, reinterpret_cast<const char *>(aad), std::min(16, aad_len)));
    if (aad && !EVP_DecryptUpdate(ctx.get(), NULL, &len, aad, aad_len)) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: failed when authenticating user AAD.\n");
        return false;
    }

    dprintf(D_NETWORK | D_VERBOSE,
        "Condor_Crypt_AESGCM::decrypt DUMP : about to decrypt cipher text."
        " Input length is %d\n",
        input_len - (receiving_IV ? IV_SIZE : 0) - MAC_SIZE);

    if( (input_len - (receiving_IV ? IV_SIZE : 0) - MAC_SIZE) < 0) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: input was too small.\n");
        return false;
    }

    if (!EVP_DecryptUpdate(ctx.get(), output, &len, input + (receiving_IV ? IV_SIZE : 0), input_len - (receiving_IV ? IV_SIZE : 0) - MAC_SIZE)) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: failed due to failed cipher text update.\n");
        return false;
    }
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decrypt DUMP : produced output of size %d\n", len);

	if (IsDebugLevel(D_NETWORK) && (input_len > 3) && (len > 3)) {
		dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decrypt DUMP : Cipher text: "
				"%0x %0x %0x %0x ... %0x %0x %0x %0x\n",
				*(input + (receiving_IV ? IV_SIZE : 0)),
				*(input + (receiving_IV ? IV_SIZE : 0) + 1),
				*(input + (receiving_IV ? IV_SIZE : 0) + 2),
				*(input + (receiving_IV ? IV_SIZE : 0) + 3),
				*(input + input_len - MAC_SIZE - 4),
				*(input + input_len - MAC_SIZE - 3),
				*(input + input_len - MAC_SIZE - 2),
				*(input + input_len - MAC_SIZE - 1));
		dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decrypt DUMP : Plain text: "
				"%0x %0x %0x %0x ... %0x %0x %0x %0x\n",
				*(output),
				*(output + 1),
				*(output + 2),
				*(output + 3),
				*(output + len - 4),
				*(output + len - 3),
				*(output + len - 2),
				*(output + len - 1));
	}

    if (!EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, MAC_SIZE, const_cast<unsigned char *>(input + input_len - MAC_SIZE))) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: failed due to failed set of tag.\n");
        return false;
    }

    char hex2[3 * MAC_SIZE + 1];
    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decrypt DUMP : Incoming MAC : %s\n",
        debug_hex_dump(hex2, reinterpret_cast<const char*>(input + input_len - MAC_SIZE), MAC_SIZE));

    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decrypt DUMP : about to finalize output (len is %i).\n", len);
    if (!EVP_DecryptFinal_ex(ctx.get(), output + len, &len)) {
        dprintf(D_ALWAYS, "Condor_Crypt_AESGCM::decrypt: ERROR: failed due to finalize decryption and check of tag.\n");
       return false;
    }

    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decrypt DUMP : input_len is %d and output_len is %d\n", input_len, input_len - (receiving_IV ? IV_SIZE : 0) - MAC_SIZE);
    output_len = input_len - (receiving_IV ? IV_SIZE : 0) - MAC_SIZE;

        // Only touch state after success
    stream_state->m_ctr_dec ++;

    dprintf(D_NETWORK | D_VERBOSE, "Condor_Crypt_AESGCM::decrypt.  Successful decryption with plain text %d bytes.\n", output_len);
    return true;
}
