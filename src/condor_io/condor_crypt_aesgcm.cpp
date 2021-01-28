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

#include <algorithm>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define IV_SIZE 12
#define MAC_SIZE 16

unsigned char g_unset_iv[IV_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/*
Condor_Crypt_AESGCM::Condor_Crypt_AESGCM(const KeyInfo& key)
    : Condor_Crypt_Base(CONDOR_AESGCM, key)
{
	resetState(key.getConnCryptoState());
}

Condor_Crypt_AESGCM::~Condor_Crypt_AESGCM()
{
}
*/

/*
void Condor_Crypt_AESGCM::resetState()
{
	dprintf(D_NETWORK, "Condor_Crypt_AESGCM::resetState\n");
	m_conn_crypto_state.reset(new ConnCryptoState());
	while (!memcmp(m_conn_crypto_state->m_iv_enc.iv, g_unset_iv, IV_SIZE)) {
		RAND_pseudo_bytes(m_conn_crypto_state->m_iv_enc.iv, IV_SIZE);
	}
	memset(m_conn_crypto_state->m_iv_dec.iv, '\0', IV_SIZE);
}
*/

// this function is static
void Condor_Crypt_AESGCM::initState(std::shared_ptr<ConnCryptoState> connState)
{
	dprintf(D_NETWORK, "ZKM: ******: Condor_Crypt_AESGCM::initState for %p.\n", connState.get());
	if (connState.get()) {
		std::shared_ptr<ConnCryptoState> m_conn_crypto_state = connState;
		if (!memcmp(m_conn_crypto_state->m_iv_enc.iv, g_unset_iv, IV_SIZE)) {
			while (!memcmp(m_conn_crypto_state->m_iv_enc.iv, g_unset_iv, IV_SIZE)) {
				RAND_pseudo_bytes(m_conn_crypto_state->m_iv_enc.iv, IV_SIZE);
			}
			// Do not increment the first connection so it is connection 0.
		} else if (m_conn_crypto_state->m_ctr_conn != 0xffffffff)
			m_conn_crypto_state->m_ctr_conn ++;
		else
			dprintf(D_NETWORK, "Condor_Crypt_AESGCM::resetState - Max session reuse hit!\n");
		m_conn_crypto_state->m_ctr_enc = 0;
		m_conn_crypto_state->m_ctr_dec = 0;
		dprintf(D_NETWORK, "Condor_Crypt_AESGCM::resetState: Connection count %u.\n", m_conn_crypto_state->m_ctr_conn);
	} else {
		// resetState();
		ASSERT("connState must not be NULL!");
	}	
}

int Condor_Crypt_AESGCM::ciphertext_size_with_cs(int plaintext_size, std::shared_ptr<ConnCryptoState> connState) const
{
    int ct_sz = plaintext_size;
    std::shared_ptr<ConnCryptoState> m_conn_crypto_state = connState;
    // Authentication tag is an additional 16 bytes; IV is 12 bytes
    //
    // ideally, we would only need to send the IV on the first transmission,
    // and it could be computed after that based on the message counter:
    //ct_sz += MAC_SIZE + IV_SIZE * (m_conn_crypto_state->m_ctr_enc ? 0 : 1);

    // however, we are opting to send it every time so the two sides are never
    // out of sync as to whether or not they are expecting it.
    ct_sz += MAC_SIZE + IV_SIZE;
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
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt with %d bytes of input\n", input_len);

    dprintf(D_ALWAYS, "ZKM ENCRYPT: Have *cs* %p, *key* %p, *conn* %p.\n", (void*)cs, (void*)&cs->m_keyInfo, (void*)(cs->m_keyInfo.getConnCryptoState().get()));
    std::shared_ptr<ConnCryptoState> m_conn_crypto_state = cs->m_keyInfo.getConnCryptoState();

        // output length must be aligned to nearest 128-bit block for AES-GCM
    int output_len = input_len;
    if (output_buf_len < output_len) {
        dprintf(D_NETWORK, "Output buffer must be at least %d bytes.\n", output_buf_len);
        return false;
    }
    if (!output) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt - cannot pass a null output buffer.\n");
        return false;
    }

    // determine if we will send an IV or not (this gets looked at often)
    //
    // the normal behavior is to only send it on first pack.  (for debugging
    // you may wish to hard code this to send on every packet, which yo ALSO
    // need to expect on the decrypt side.  see ::decrypt() function)

    // ideally, we would only need to send the IV on the first transmission,
    // and it could be computed after that based on the message counter:
    bool sending_IV = (m_conn_crypto_state->m_ctr_enc == 0);

    // however, we are opting to send it every time so the two sides are never
    // out of sync as to whether or not they are expecting it.
    sending_IV = true;

        // Authentication tag is an additional 16 bytes; IV is 12 bytes
    output_len += MAC_SIZE + (sending_IV ? IV_SIZE : 0);

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

    // normally we would do the math here to change the IV.  however, for
    // debugging only, I am treating the counter as ZERO.  that is, the result
    // is the base.
    //
    // this means the IV does not change from message to message, which in
    // real life, it needs to, but for now it does not.
    //
    // the reason i made this hack is thought i was seeing the _enc and _dec
    // counters be off by one the sending/receiving side (in non-negotiated
    // sessions I believe)
    //
    // making these zero let me continue debugging past that issue.

    int32_t base = ntohl(m_conn_crypto_state->m_iv_enc.ctr.pkt);
    //int32_t result = base + *reinterpret_cast<int32_t*>(&m_conn_crypto_state->m_ctr_enc);
    int32_t result = base;
    int32_t ctr_encoded = htonl(result);
    if (m_conn_crypto_state->m_ctr_enc == 0xffffffff) {
        dprintf(D_NETWORK, "Hit max number of packets per connection.\n");
        return false;
    }


    // we have decided the connection counter is not needed, and possibly also
    // a bad idea according to Lamport's "byzantine general problem". (that is,
    // how would both sides agree a connection was made).

    int32_t base_conn = ntohl(m_conn_crypto_state->m_iv_enc.ctr.conn);
    //int32_t result_conn = base_conn + *reinterpret_cast<int32_t*>(&m_conn_crypto_state->m_ctr_conn);
    int32_t result_conn = base_conn;
    int32_t ctr_encoded_conn = htonl(result_conn);

    if (m_conn_crypto_state->m_ctr_conn == 0xffffffff) {
        dprintf(D_NETWORK, "Hit max number of connections in a session.\n");
        return false;
    }

    unsigned char iv[IV_SIZE];
    memcpy(iv, &ctr_encoded, sizeof(ctr_encoded));
    memcpy(iv + sizeof(ctr_encoded), &ctr_encoded_conn, sizeof(ctr_encoded_conn));
    memcpy(iv + 2*sizeof(ctr_encoded), m_conn_crypto_state->m_iv_enc.iv + 2*sizeof(ctr_encoded), IV_SIZE - 2*sizeof(ctr_encoded));

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : IV base value %d\n", base);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : IV Counter value _enc %u\n", m_conn_crypto_state->m_ctr_enc);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : IV Counter plus base value %d\n", result);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : IV Counter plus base value (encoded) %d\n", ctr_encoded);

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : IV conn base value %d\n", base_conn);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : IV Connection counter value %d\n", m_conn_crypto_state->m_ctr_conn);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : IV Connection Counter plus base value %d\n", result_conn);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : IV conn ctr value %d\n", ctr_encoded_conn);

        // Only need to send IV for the first encrypted packet.
    if (sending_IV) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : First packet - will send IV, copying to beginning of output\n");
        memcpy(output, iv, IV_SIZE);
    }

    char hex[3 * IV_SIZE + 1];
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : Final IV used for outgoing encrypt: %s\n",
        debug_hex_dump(hex, reinterpret_cast<char*>(iv), IV_SIZE));

    if (cs->m_keyInfo.getProtocol() != CONDOR_AESGCM) {
        dprintf(D_NETWORK, "Failed to have correct AES-GCM key type.\n");
        return false;
    }

    const unsigned char *kdp = cs->m_keyInfo.getKeyData();
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : about to init key %0x %0x %0x %0x.\n",
        *(kdp), *(kdp + 15), *(kdp + 16), *(kdp + 31));

    if (1 != EVP_EncryptInit_ex(ctx, NULL, NULL, cs->m_keyInfo.getKeyData(), iv)) {
        dprintf(D_NETWORK, "Failed to initialize key and IV.\n");
        return false;
    }

    // Authenticate additional data from the caller.
    int len;
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : We have %d bytes of AAD data: %s\n",
        aad_len, debug_hex_dump(hex, reinterpret_cast<const char *>(aad), std::min(IV_SIZE, aad_len)));
    if (aad && (1 != EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len))) {
        dprintf(D_NETWORK, "Failed to authenticate caller input data.\n");
        return false;
    }

    if (!sending_IV) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : We have %d bytes of previous MAC: %s\n",
            MAC_SIZE, debug_hex_dump(hex, reinterpret_cast<const char *>(m_conn_crypto_state->m_prev_mac_enc), MAC_SIZE));

        if ( 1 != EVP_EncryptUpdate(ctx, NULL, &len, m_conn_crypto_state->m_prev_mac_enc, MAC_SIZE)) {
            dprintf(D_NETWORK, "Failed to authenticate prior MAC.\n");
            return false;
        }
    } else {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : No previous MAC, so not adding to EncryptUpdate()\n");
    }

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : We have %d bytes of plaintext. (not printing)\n", input_len);
    if (1 != EVP_EncryptUpdate(ctx, output + (sending_IV ? IV_SIZE : 0),
        &len, input, input_len))
    {
        dprintf(D_NETWORK, "Failed to encrypt plaintext buffer.\n");
        return false;
    }
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : First %d bytes written to ciphertext.\n", len);

    int len2;
    if (1 != EVP_EncryptFinal_ex(ctx, output + (sending_IV ? IV_SIZE : 0) + len, &len2)) {
        dprintf(D_NETWORK, "Failed to finalize cipher text.\n");
        return false;
    }
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : Finalized an additional %d bytes written to ciphertext.\n", len2);
    len += len2;

    // we never use the result of len or len2 after this.  how would len2 be non-zero?
    ASSERT(len2 == 0);

    dprintf(D_NETWORK,
        "Condor_Crypt_AESGCM::encrypt DUMP : Plain text: %0x %0x %0x %0x ... %0x %0x %0x %0x\n",
        *(input),
        *(input + 1),
        *(input + 2),
        *(input + 3),
        *(input + input_len - 4),
        *(input + input_len - 3),
        *(input + input_len - 2),
        *(input + input_len - 1));

    dprintf(D_NETWORK,
        "Condor_Crypt_AESGCM::encrypt DUMP : Cipher text: %0x %0x %0x %0x ... %0x %0x %0x %0x\n",
        *(output + (sending_IV ? IV_SIZE : 0)),
        *(output + (sending_IV ? IV_SIZE : 0) + 1),
        *(output + (sending_IV ? IV_SIZE : 0) + 2),
        *(output + (sending_IV ? IV_SIZE : 0) + 3),
        *(output + output_len - MAC_SIZE - 4),
        *(output + output_len - MAC_SIZE - 3),
        *(output + output_len - MAC_SIZE - 2),
        *(output + output_len - MAC_SIZE - 1));

    // extract the tag directly into the output stream to be given to CEDAR
    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, MAC_SIZE, output + output_len - MAC_SIZE)) {
        dprintf(D_NETWORK, "Failed to get tag.\n");
        return false;
    }
    char hex2[3 * MAC_SIZE + 1];
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt DUMP : Outgoing MAC : %s\n",
        debug_hex_dump(hex2, reinterpret_cast<char*>(output + output_len - MAC_SIZE), MAC_SIZE));

    // store the compute mac in our state obj for use next time
    memcpy(m_conn_crypto_state->m_prev_mac_enc, output + output_len - MAC_SIZE, MAC_SIZE);

        // Only change state if everything was successful.
    m_conn_crypto_state->m_ctr_enc++;

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::encrypt.  Successful encryption with cipher text %d bytes.\n", output_len);
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
    EVP_CIPHER_CTX *ctx;

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt with input buffer %d.\n", input_len);

    dprintf(D_ALWAYS, "ZKM DECRYPT: Have *cs* %p, *key* %p, *conn* %p.\n", (void*)cs, (void*)&cs->m_keyInfo, (void*)(cs->m_keyInfo.getConnCryptoState().get()));
    std::shared_ptr<ConnCryptoState> m_conn_crypto_state = cs->m_keyInfo.getConnCryptoState();

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

    if (cs->m_keyInfo.getProtocol() != CONDOR_AESGCM) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to the wrong protocol.\n");
        return false;
    }

    if (m_conn_crypto_state->m_ctr_dec == 0xffffffff) {
        dprintf(D_NETWORK, "Hit max number of packets per connection.\n");
        return false;
    }
    if (m_conn_crypto_state->m_ctr_conn == 0xffffffff) {
        dprintf(D_NETWORK, "Hit max number of connections per session.\n");
        return false;
    }

    // Ideally, we would only need to receive the IV on the first transmission,
    // and it could be computed after that based on the message counter:
    bool receiving_IV = (m_conn_crypto_state->m_ctr_dec == 0);

    // however, we are opting to send it every time so the two sides are never
    // out of sync as to whether or not they are expecting it.
    receiving_IV = true;

    // on the code for the encrypt side, it checks to see if the number of message is non-zero.
    //
    // but on the decypt side, it looks to see if the IV is all zeros.
    //
    // let's force consistency:
    if ( receiving_IV ) {
        memset(m_conn_crypto_state->m_iv_dec.iv, '\0', IV_SIZE);
    }

    // if you want to force the IV to be read every time, it would happen here.
    //
    // you should be able to set receiving_IV to true and have that happen.
    // however, the code is not yet wired to look at that variable, and instead
    // does some conditional math as needed.


    if (receiving_IV) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decyrpt DUMP : First decrypt - initializing IV\n");
        memcpy(m_conn_crypto_state->m_iv_dec.iv, input, IV_SIZE);
    }

    // normally we would do the math here to change the IV.  however, for
    // debugging only, I am treating the counter as ZERO.  that is, the result
    // is the base.
    //
    // this means the IV does not change from message to message, which in
    // real life, it needs to, but for now it does not.
    //
    // the reason i made this hack is thought i was seeing the _enc and _dec
    // counters be off by one the sending/receiving side (in non-negotiated
    // sessions I believe)
    //
    // making these zero let me continue debugging past that issue.
   
    int32_t base = ntohl(m_conn_crypto_state->m_iv_dec.ctr.pkt);
    //int32_t result = base + *reinterpret_cast<int32_t*>(&m_conn_crypto_state->m_ctr_dec);
    int32_t result = base;
    int32_t ctr_encoded = htonl(result);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decyrpt DUMP : IV base value %d\n", base);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decyrpt DUMP : IV Counter value _dec %u\n", m_conn_crypto_state->m_ctr_dec);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decyrpt DUMP : IV Counter plus base value %d\n", result);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decyrpt DUMP : IV Counter plus base value (encoded) %d\n", ctr_encoded);


    // we have decided the connection counter is not needed, and possibly also
    // a bad idea according to Lamport's "byzantine general problem". (that is,
    // how would both sides agree a connection was made).

    int32_t result_conn = ntohl(m_conn_crypto_state->m_iv_dec.ctr.conn);
    //int32_t base_conn = result_conn - *reinterpret_cast<int32_t*>(&m_conn_crypto_state->m_ctr_conn);
    int32_t base_conn = result_conn;
    int32_t ctr_encoded_conn = htonl(result_conn);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decyrpt DUMP : IV conn base value %d\n", base_conn);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decyrpt DUMP : IV Connection counter value %d\n", m_conn_crypto_state->m_ctr_conn);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decyrpt DUMP : IV Connection Counter plus base value %d\n", result_conn);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decyrpt DUMP : IV conn ctr value %d\n", ctr_encoded_conn);

        // Assemble our expected IV.
    unsigned char iv[IV_SIZE];
    memcpy(iv, &ctr_encoded, sizeof(ctr_encoded));
    memcpy(iv + sizeof(ctr_encoded), &ctr_encoded_conn, sizeof(ctr_encoded_conn));
    memcpy(iv + 2*sizeof(ctr_encoded), m_conn_crypto_state->m_iv_dec.iv + 2*sizeof(ctr_encoded), IV_SIZE - 2*sizeof(ctr_encoded));

    const unsigned char *kdp = cs->m_keyInfo.getKeyData();
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt DUMP : about to init key %0x %0x %0x %0x.\n",
        *(kdp), *(kdp + 15), *(kdp + 16), *(kdp + 31));

    char hex[3 * (std::max(IV_SIZE,aad_len)) + 1];
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decyrpt DUMP : IV used for incoming decrypt: %s\n",
        debug_hex_dump(hex,
        reinterpret_cast<const char *>(iv), IV_SIZE));

    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, kdp, iv)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to failed init.\n");
        return false;
    }

    int len;
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt DUMP : We have %d bytes of AAD data: %s\n",
        aad_len, debug_hex_dump(hex, reinterpret_cast<const char *>(aad), aad_len));
    if (aad && !EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed when authenticating user AAD.\n");
        return false;
    }

    if (!receiving_IV) {
        // if we aren't receiving an IV, it means there was a previous msg MAC we want to add
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt DUMP : We have %d bytes of previous MAC: %s\n",
            MAC_SIZE, debug_hex_dump(hex, reinterpret_cast<const char *>(m_conn_crypto_state->m_prev_mac_dec), MAC_SIZE));
// ZKM FIXME TODO why is this EncryptUpdate and not DecryptUpdate?  (It seems to work though ?!?!)
        if (1 != EVP_EncryptUpdate(ctx, NULL, &len, m_conn_crypto_state->m_prev_mac_dec, MAC_SIZE)) {
            dprintf(D_NETWORK, "Failed to authenticate prior MAC.\n");
            return false;
        }
    } else {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt DUMP : No previous MAC, so not adding to EncryptUpdate()\n");
    }


    dprintf(D_NETWORK,
        "Condor_Crypt_AESGCM::decrypt DUMP : about to decrypt cipher text."
        " Input length is %d\n",
        input_len - (receiving_IV ? IV_SIZE : 0) - MAC_SIZE);

    if( (input_len - (receiving_IV ? IV_SIZE : 0) - MAC_SIZE) < 0) {
        dprintf(D_NETWORK, "FAILING.\n");
        return false;
    }

    if (!EVP_DecryptUpdate(ctx, output, &len, input + (receiving_IV ? IV_SIZE : 0), input_len - (receiving_IV ? IV_SIZE : 0) - MAC_SIZE)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to failed cipher text update.\n");
        return false;
    }
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt DUMP : produced output of size %d\n", len);
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt DUMP : Cipher text: "
        "%0x %0x %0x %0x ... %0x %0x %0x %0x\n",
        *(input + (receiving_IV ? IV_SIZE : 0)),
        *(input + (receiving_IV ? IV_SIZE : 0) + 1),
        *(input + (receiving_IV ? IV_SIZE : 0) + 2),
        *(input + (receiving_IV ? IV_SIZE : 0) + 3),
        *(input + input_len - MAC_SIZE - 4),
        *(input + input_len - MAC_SIZE - 3),
        *(input + input_len - MAC_SIZE - 2),
        *(input + input_len - MAC_SIZE - 1));
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt DUMP : Plain text: "
        "%0x %0x %0x %0x ... %0x %0x %0x %0x\n",
        *(output),
        *(output + 1),
        *(output + 2),
        *(output + 3),
        *(output + len - 4),
        *(output + len - 3),
        *(output + len - 2),
        *(output + len - 1));
 
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, MAC_SIZE, const_cast<unsigned char *>(input + input_len - MAC_SIZE))) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to failed set of tag.\n");
        return false;
    }

    char hex2[3 * MAC_SIZE + 1];
    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt DUMP : Incoming MAC : %s\n",
        debug_hex_dump(hex2, reinterpret_cast<const char*>(input + input_len - MAC_SIZE), MAC_SIZE));


    memcpy(m_conn_crypto_state->m_prev_mac_dec, input + input_len - MAC_SIZE, MAC_SIZE);

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt DUMP : about to finalize output (len is %i).\n", len);
    if (!EVP_DecryptFinal_ex(ctx, output + len, &len)) {
        dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt failed due to finalize decryption and check of tag.\n");
       return false;
    }

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt DUMP : input_len is %d and output_len is %d\n", input_len, input_len - (receiving_IV ? IV_SIZE : 0) - MAC_SIZE);
    output_len = input_len - (receiving_IV ? IV_SIZE : 0) - MAC_SIZE;

        // Only touch state after success
    m_conn_crypto_state->m_ctr_dec ++;

    dprintf(D_NETWORK, "Condor_Crypt_AESGCM::decrypt.  Successful decryption with plain text %d bytes.\n", output_len);
    return true;
}
