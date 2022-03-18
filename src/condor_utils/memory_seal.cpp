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

#include "condor_common.h"

#include "memory_seal.h"

#include "condor_crypt.h"
#include "condor_base64.h"

#include <openssl/evp.h>

#include <memory>

#define IV_LEN 16

namespace {

std::vector<unsigned char> g_encrypt_buffer;
std::vector<unsigned char> g_process_key;

size_t calc_ciphertext_len(size_t plaintext_len) {
	return (plaintext_len + 16) & ~15;
}

const unsigned char *process_key() {
	if (g_process_key.size()) return g_process_key.data();


	std::unique_ptr<unsigned char, decltype(&free)> random_bytes(
		Condor_Crypt_Base::randomKey(32),
		&free);
	if (!random_bytes) {return nullptr;}

	// I don't believe the hkdf() call here is necessary, as the input
	// key material should already be uniformly random and the size we
	// need. The call doesn't do any harm, but may slow things down.
	//   - Jaime Frey
	std::unique_ptr<unsigned char, decltype(&free)> new_key(
		Condor_Crypt_Base::hkdf(random_bytes.get(), 32, 32),
		&free);
	if (!new_key) {return nullptr;}

	g_process_key.resize(32);
	memcpy(g_process_key.data(), new_key.get(), 32);

	return g_process_key.data();
}

bool encrypt(const unsigned char *plaintext, size_t plaintext_len)
{
	// In CBC mode, ciphertext is the same length as the plaintext.
	g_encrypt_buffer.resize(calc_ciphertext_len(plaintext_len) + IV_LEN);

	std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);

	if (!ctx) {
		return false;
	}

	if (1 != EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_cbc(), NULL, process_key(), g_encrypt_buffer.data())) {
		return false;
	}

	int len;
	if (1 != EVP_EncryptUpdate(ctx.get(), g_encrypt_buffer.data() + IV_LEN, &len, plaintext, plaintext_len)) {
		return false;
	}
	size_t ciphertext_len = len;

	if (1 != EVP_EncryptFinal_ex(ctx.get(), g_encrypt_buffer.data() + IV_LEN + len, &len)) {
		return false;
	}
	ciphertext_len += len;

	return ciphertext_len == calc_ciphertext_len(plaintext_len);
}

bool decrypt(unsigned char *encoded_data, size_t encoded_len, unsigned char *plaintext, size_t &plaintext_len)
{
	std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);

	if (!ctx) {
		return false;
	}

	if (1 != EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_cbc(), NULL, process_key(), encoded_data)) {
		return false;
	}

	int len;
	plaintext_len = 0;
	if (1 != EVP_DecryptUpdate(ctx.get(), plaintext, &len, encoded_data + IV_LEN, encoded_len - IV_LEN)) {
		return false;
	}
	plaintext_len = len;

	if (1 != EVP_DecryptFinal_ex(ctx.get(), plaintext + len, &len)) {
		return false;
	}
	plaintext_len += len;

	return true;
}

}


bool htcondor::memory_seal(const char *input_data, std::string &output_data) {
	return memory_seal(reinterpret_cast<const unsigned char *>(input_data), strlen(input_data), output_data);
}

bool htcondor::memory_seal(const std::vector<unsigned char> &input_data, std::string &output_data) {
	return memory_seal(input_data.data(), input_data.size(), output_data);
}

bool htcondor::memory_seal(const unsigned char *input_data, size_t input_data_len, std::string &output_data)
{
	output_data.reserve((((input_data_len + IV_LEN + 2) / 3) << 2));
	g_encrypt_buffer.resize(IV_LEN + calc_ciphertext_len(input_data_len));

	std::unique_ptr<unsigned char, decltype(&free)> iv(
		Condor_Crypt_Base::randomKey(IV_LEN),
		&free);
	if (!iv) {
		return false;
	}

	memcpy(g_encrypt_buffer.data(), iv.get(), IV_LEN);
	
	if (!encrypt(input_data, input_data_len)) {
		return false;
	}

	std::unique_ptr<char, decltype(&free)> encoded_data(
		condor_base64_encode(g_encrypt_buffer.data(),
			calc_ciphertext_len(input_data_len) + IV_LEN, false),
		&free);
	if (!encoded_data) {
		return false;
	}

	output_data = encoded_data.get();

	return true;
}

bool htcondor::memory_unseal(const std::string &encoded_data_b64, std::vector<unsigned char> &plaintext)
{
	unsigned char *encoded_data = nullptr;
	int encoded_len;
	condor_base64_decode(encoded_data_b64.c_str(), &encoded_data, &encoded_len, false);
	if (!encoded_data) {
		return false;
	}
	std::unique_ptr<unsigned char, decltype(&free)> encoded_data_ptr(encoded_data, &free);

	if (encoded_len < IV_LEN) {
		return false;
	}
	plaintext.resize(encoded_len - IV_LEN);
	size_t plaintext_len;
	if (!decrypt(encoded_data, encoded_len, plaintext.data(), plaintext_len)) {
		return false;
	}
	plaintext.resize(plaintext_len);

	return true;
}
