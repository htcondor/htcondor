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


#ifndef CONDOR_CRYPTO
#define CONDOR_CRYPTO

#include "condor_common.h"
#include "condor_debug.h"

#include "CryptKey.h"
#include <openssl/evp.h>

struct StreamCryptoState {
    // The IV is a 16-byte random number.  The first 4 bytes are modified with
    // a message counter to ensure it is unique.
    union Packed_IV {
        unsigned char iv[16]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        struct {
            uint32_t pkt;
        } ctr;
    };

    uint32_t m_ctr_enc{0}; // Number of outgoing (encrypted) packets
    uint32_t m_ctr_dec{0}; // Number of incoming (decrypted) packets
    union Packed_IV m_iv_enc; // IV for outgoing data
    union Packed_IV m_iv_dec; // IV for incoming data.
};


class Condor_Crypto_State {

public:
    Condor_Crypto_State(Protocol proto, KeyInfo& key);
    ~Condor_Crypto_State();

    void reset();
    const KeyInfo& getkey() { return m_keyInfo; }

    // the KeyInfo holds:
    // protocol
    // key length
    // key data
    // duration
    KeyInfo       m_keyInfo;

	// holds encryption and decryption cipher contexts for methods (3DES and BLOWFISH)
#if OPENSSL_VERSION_NUMBER < 0x30000000L
	const
#endif
	EVP_CIPHER *m_cipherType;

	EVP_CIPHER_CTX *enc_ctx;
	EVP_CIPHER_CTX *dec_ctx;

// this data structure is used for AESGCM:
//
    StreamCryptoState m_stream_crypto_state;

private:
    Condor_Crypto_State() = delete;
    Condor_Crypto_State(Condor_Crypto_State&) = delete;

};


class Condor_Crypt_Base {

 public:
    Condor_Crypt_Base() {}
    virtual ~Condor_Crypt_Base() {};

		// Returns an array of random bytes.
		// Caller should free returned buffer when done.
    static unsigned char * randomKey(int length = 24);
		// Same as randomKey(), but returns string encoded as hex number.
		// The supplied length specifies the number of random bytes,
		// not the number of hex digits (which is twice the number of bytes).
	static char *randomHexKey(int length = 24);
	// Generate a 16-byte key using the MD5 algorithm.
    static unsigned char * oneWayHashKey(const char * initialKey);
	// Generate a key from random data using the HKDF algorithm.
	// Returns a key which must be free'd by the caller.
	// Returns nullptr on error.
    static unsigned char *hkdf(const unsigned char * initialKey, size_t input_key_len, size_t output_key_length);
    //------------------------------------------
    // PURPOSE: Generate a random key
    //          First method use rand function to generate the key
    //          Second method use a hashing algorithm to generate a key
    //             using the input string. initialkey should not be too short!
    // REQUIRE: length of the key, default to 12
    // RETURNS: a buffer (malloc) with length 
    //------------------------------------------

    virtual bool encrypt(Condor_Crypto_State *s,
                         const unsigned char *input,
                         int               input_len, 
                         unsigned char *&  output, 
                         int&              output_len) = 0;

    virtual bool decrypt(Condor_Crypto_State *s,
                         const unsigned char *input,
                         int              input_len, 
                         unsigned char *& output, 
                         int&             output_len) = 0;

    virtual int ciphertext_size_with_cs(int ciphertext, StreamCryptoState *ss) const = 0;

 protected:
    static int encryptedSize(int inputLength, int blockSize = 8);
    //------------------------------------------
    // PURPOSE: return the size of the cipher text given the clear
    //          text size and cipher block size
    // REQUIRE: intputLength -- length of the clear text
    //          blockSize    -- size of the block for encrypting in BYTES
    //                          For instance, DES/blowfish use 64bit(8bytes)
    //                          Default is 8
    // RETURNS: size of the cipher text
    //------------------------------------------

};

#endif
