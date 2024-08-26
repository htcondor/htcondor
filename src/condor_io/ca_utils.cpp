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
#include "condor_debug.h"
#include "condor_config.h"
#include "CondorError.h"
#include "condor_secman.h"
#include "condor_uid.h"
#include "subsystem_info.h"
#include "condor_blkng_full_disk_io.h"
#include "directory.h"
#include "fcloser.h"

#include <sstream>
#include <iomanip>
#include <iostream>
#include <memory>

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/pem.h>

#include "ca_utils.h"

namespace {

std::unique_ptr<ASN1_INTEGER, decltype(&ASN1_INTEGER_free)> certificate_serial()
{
	std::unique_ptr<ASN1_INTEGER, decltype(&ASN1_INTEGER_free)> an_int(ASN1_INTEGER_new(), &ASN1_INTEGER_free);
	std::unique_ptr<BIGNUM, decltype(&BN_free)> bignum(BN_new(), &BN_free);

	bignum && an_int && BN_rand(bignum.get(), 64, 0, 0)
	 && BN_to_ASN1_INTEGER(bignum.get(), an_int.get());

	return an_int;
}

std::unique_ptr<X509, decltype(&X509_free)> read_cert(const std::string &certfile) {
	std::unique_ptr<X509, decltype(&X509_free)> result(nullptr, &X509_free);

	std::unique_ptr<FILE, fcloser> fp(safe_fopen_no_create(certfile.c_str(), "r"));
	if (!fp) {
		dprintf(D_ALWAYS, "Failed to open %s for reading X509 certificate: %s (errno=%d)\n", certfile.c_str(), strerror(errno), errno);
		return result;
	}

	decltype(result) x509(
		PEM_read_X509(fp.get(), nullptr, nullptr, nullptr),
		&X509_free);
	if (!x509) {
		dprintf(D_ALWAYS, "Failed to parse certificate from file %s.\n", certfile.c_str());
		return result;
	}
	return x509;
}

	// Generates an EC key in a keyfile if the file does not exist.
std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> generate_key(const std::string &keyfile) {
	std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> result(nullptr, &EVP_PKEY_free);

	if (0 == access(keyfile.c_str(), R_OK)) {
		std::unique_ptr<FILE, fcloser> fp(safe_fopen_no_create(keyfile.c_str(), "r"));
		if (!fp) {
			dprintf(D_ALWAYS, "X509 generation: failed to open the private key file %s: %s (errno=%d)\n",
			keyfile.c_str(), strerror(errno), errno);
			return result;
		}

		std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(PEM_read_PrivateKey(fp.get(), nullptr, nullptr, nullptr), &EVP_PKEY_free);
		if (!key) {
			dprintf(D_ALWAYS, "X509 generation: failed to read the private key from file %s.\n", keyfile.c_str());
			return result;
		}
		return key;
	}

	dprintf(D_FULLDEBUG|D_SECURITY, "Will generate a new key in %s\n", keyfile.c_str());
	CondorError err;
	auto key = SecMan::GenerateKeyExchange(&err);
	if (!key) {
		dprintf(D_ALWAYS, "Error in generating key: %s\n", err.getFullText().c_str());
		return result;
	}

	std::unique_ptr<FILE, fcloser> fp(
		safe_fcreate_fail_if_exists(keyfile.c_str(), "w", 0600));

	if (!fp) {
		dprintf(D_ALWAYS, "Key generation: failed to open the private key file %s for writing: %s (errno=%d)\n",
			keyfile.c_str(), strerror(errno), errno);
		return result;
	}

	if (1 != PEM_write_PrivateKey(fp.get(), key.get(), nullptr, nullptr, 0, 0, nullptr)) {
		dprintf(D_ALWAYS, "Key generation: failed to write private key to file %s: %s (errno=%d)\n",
			keyfile.c_str(), strerror(errno), errno);
		unlink(keyfile.c_str());
		return result;
	}
	fflush(fp.get());
	dprintf(D_FULLDEBUG|D_SECURITY, "Successfully wrote new private key to file %s\n",
		keyfile.c_str());

	return key;
}


bool add_x509v3_ext(X509 *issuer, X509 *cert, int ext_nid, const std::string &value, bool critical) {
	std::unique_ptr<char, decltype(&free)> value_copy(
		reinterpret_cast<char *>(malloc(value.size() + 1)),
		&free);
	if (!value_copy) {return false;}

	strcpy(value_copy.get(), value.c_str());

	X509V3_CTX ctx;
	X509V3_set_ctx_nodb(&ctx);
	X509V3_set_ctx(&ctx, issuer, cert, NULL, NULL, 0);

	std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)> new_ext(
		X509V3_EXT_conf_nid(NULL, &ctx, ext_nid, value_copy.get()),
		&X509_EXTENSION_free);
	if (!new_ext) {
		dprintf(D_ALWAYS, "Failed to create X509 extension with value %s.\n",
			value_copy.get());
		return false;
	}

	if (critical && 1 != X509_EXTENSION_set_critical(new_ext.get(), 1)) {
		dprintf(D_ALWAYS, "Failed to mark extension as critical.\n");
		return false;
	}

	if (1 != X509_add_ext(cert, new_ext.get(), -1)) {
		dprintf(D_ALWAYS, "Failed to add new extension to certificate.\n");
		return false;
	}
	return true;
}


std::unique_ptr<X509, decltype(&X509_free)> generate_generic_cert(X509_NAME *x509_name,
	EVP_PKEY *key, int lifetime)
{
	std::unique_ptr<X509, decltype(&X509_free)> result(nullptr, &X509_free);

	std::unique_ptr<X509, decltype(&X509_free)> x509(X509_new(), &X509_free);
	if (!x509) {
		dprintf(D_ALWAYS, "X509 generation: failed to create a new X509 request object\n");
		return result;
	}

	if (1 != X509_set_version(x509.get(), 2)) {
		dprintf(D_ALWAYS, "X509 generation: failed to set version number\n");
		return result;
	}

	if (1 != X509_set_pubkey(x509.get(), key)) {
		dprintf(D_ALWAYS, "X509 generation: failed to set public key in the request\n");
		return result;
	}

	if (1 != X509_set_subject_name(x509.get(), x509_name)) {
		dprintf(D_ALWAYS, "X509 generation: failed to set requested certificate name.\n");
		return result;
	}

	auto serial = certificate_serial();
	if (!serial) {
		dprintf(D_ALWAYS, "X509 generation: failed to create new serial number.\n");
		return result;
	}
	if (1 != X509_set_serialNumber(x509.get(), serial.get())) {
		dprintf(D_ALWAYS, "X509 generation: failed to set serial number.\n");
		return result;
	}

	auto now = time(NULL);
	std::unique_ptr<ASN1_TIME, decltype(&ASN1_TIME_free)> tm(
		ASN1_TIME_adj(nullptr, now, 0, 0),
		&ASN1_TIME_free);
	X509_set_notBefore(x509.get(), tm.get());
	ASN1_TIME_adj(tm.get(), now, lifetime, -1);
	X509_set_notAfter(x509.get(), tm.get());

	if (!add_x509v3_ext(nullptr, x509.get(), NID_subject_key_identifier, "hash", false)) {
		return result;
	}

	return x509;
}


std::unique_ptr<X509_NAME, decltype(&X509_NAME_free)> generate_ca_name()
{
	std::unique_ptr<X509_NAME, decltype(&X509_NAME_free)> result(nullptr, &X509_NAME_free);
	std::string ca_name;

	if (!param(ca_name, "TRUST_DOMAIN")) {return result;}

	decltype(result) x509_name(X509_NAME_new(), &X509_NAME_free);
	if (
		1 != X509_NAME_add_entry_by_txt(x509_name.get(), "O", MBSTRING_ASC,
			(const unsigned char *)"condor", -1, -1, 0) ||
		1 != X509_NAME_add_entry_by_txt(x509_name.get(), "CN", MBSTRING_ASC,
			(const unsigned char *)ca_name.c_str(), -1, -1, 0)
	) {
		dprintf(D_ALWAYS, "Failed to create new CA name.\n");
		return result;
	}

	return x509_name;
}


std::unique_ptr<X509_NAME, decltype(&X509_NAME_free)> generate_cert_name(const std::string &cn)
{
	std::unique_ptr<X509_NAME, decltype(&X509_NAME_free)> result(nullptr, &X509_NAME_free);

	decltype(result) x509_name(X509_NAME_new(), &X509_NAME_free);
	if (1 != X509_NAME_add_entry_by_txt(x509_name.get(), "CN", MBSTRING_ASC,
		(const unsigned char *)cn.c_str(), -1, -1, 0))
	{
		dprintf(D_ALWAYS, "Failed to create new certificate name.\n");
		return result;
	}
	return x509_name;
}


std::unique_ptr<FILE, fcloser> get_known_hosts()
{
	TemporaryPrivSentry tps;
	auto subsys = get_mySubSystem();
	if (subsys->isDaemon()) {
		set_priv(PRIV_ROOT);
	}

	std::string fname = htcondor::get_known_hosts_filename();
	make_parents_if_needed(fname.c_str(), 0755);

	std::unique_ptr<FILE, fcloser> fp(nullptr);
	fp.reset(safe_fcreate_keep_if_exists(fname.c_str(), "a+", 0644));
	if (!fp) {
		dprintf(D_SECURITY, "Failed to check known hosts file %s: %s (errno=%d)\n",
			fname.c_str(), strerror(errno), errno);
	} else {
		// We want to read from beginning of the file; since we open in append mode,
		// we will write to the end.
		fseek(fp.get(), 0, SEEK_SET);
	}

	return fp;
}


bool check_known_hosts_any_match(const std::string &hostname, bool permitted, std::string method, std::string method_info)
{
	auto fp = get_known_hosts();
	if (!fp) {return false;}

	for (std::string line; readLine(line, fp.get(),false);) {
		trim(line);
		if (line.empty() || line[0] == '#') continue;

		std::vector<std::string> tokens = split(line, " ");
		if (tokens.size() < 3) {
			dprintf(D_SECURITY, "Incorrect format in known host file.\n");
			continue;
		}

		if (method != tokens[1] || method_info != tokens[2]) {continue;}

		std::string host_entry = std::string(permitted ? "" : "!") + hostname;
		if (host_entry != tokens[0]) {continue;}
		return true;
	}
	return false;
}


} // namespace


std::string htcondor::get_known_hosts_filename()
{
	std::string fname;
	if (!param(fname, "SEC_KNOWN_HOSTS")) {
		std::string file_location;
		if (find_user_file(file_location, "known_hosts", false, false)) {
			fname = file_location;
		} else {
			param(fname, "SEC_SYSTEM_KNOWN_HOSTS");
		}
	}
	return fname;
}


// Given a non-newline, base64-encoded certificate, decode to an X509 object.
std::unique_ptr<X509, decltype(&X509_free)> htcondor::load_x509_from_b64(const std::string &info, CondorError &err)
{
        std::unique_ptr<BIO, decltype(&BIO_free)> b64( BIO_new(BIO_f_base64()), &BIO_free);
        BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);
        if (!b64) {
		err.push("X509", 1, "Failed to initialize base64 buffer");
		return std::unique_ptr<X509, decltype(&X509_free)>(nullptr, &X509_free);
	}

        decltype(b64) mem(BIO_new_mem_buf(info.c_str(), info.size()), &BIO_free);
        if (!mem) {
		err.push("X509", 2, "Failed to initialize memory buffer");
		return std::unique_ptr<X509, decltype(&X509_free)>(nullptr, &X509_free);
	}
        BIO_push(b64.get(), mem.get());

	std::unique_ptr<X509, decltype(&X509_free)> result(d2i_X509_bio(b64.get(), nullptr), &X509_free);
        if (!result) {
                err.push("X509", 3, "Failed to parse X.509 object from data");
		auto err_msg = ERR_error_string(ERR_get_error(), nullptr);
		if (err_msg) err.pushf("X509", 3, "OpenSSL error: %s", err_msg);
		return std::unique_ptr<X509, decltype(&X509_free)>(nullptr, &X509_free);
        }

        return result;
}



bool htcondor::generate_x509_ca(const std::string &cafile, const std::string &cakeyfile)
{
	if (0 == access(cafile.c_str(), R_OK)) {
		return true;
	}

	auto ca_private_key = generate_key(cakeyfile);
	if (!ca_private_key) {
		return false;
	}

	auto x509_name = generate_ca_name();
	if (!x509_name) {return false;}
	auto cert = generate_generic_cert(x509_name.get(), ca_private_key.get(), 10*365);
	if (!cert) {return false;}

	X509_set_issuer_name(cert.get(), x509_name.get());

	if (!add_x509v3_ext(cert.get(), cert.get(), NID_authority_key_identifier, "keyid:always", false) ||
		!add_x509v3_ext(cert.get(), cert.get(), NID_basic_constraints, "CA:true", true) ||
		!add_x509v3_ext(cert.get(), cert.get(), NID_key_usage, "keyCertSign", true))
	{
		return false;
	}

	if (0 > X509_sign(cert.get(), ca_private_key.get(), EVP_sha256())) {
		dprintf(D_ALWAYS, "CA generation: failed to sign the CA certificate\n");
		return false;
	}

	std::unique_ptr<FILE, fcloser> fp(
		safe_fcreate_fail_if_exists(cafile.c_str(), "w"));
	if (!fp) {
		dprintf(D_ALWAYS, "CA generation: failed to create a new CA file at %s: %s (errno=%d)\n",
			cafile.c_str(), strerror(errno), errno);
		return false;
	}

	if (1 != PEM_write_X509(fp.get(), cert.get())) {
		dprintf(D_ALWAYS, "CA generation: failed to write the CA certificate %s: %s (errno=%d)\n",
			cafile.c_str(), strerror(errno), errno);
		unlink(cafile.c_str());
		return false;
	}

	dprintf(D_FULLDEBUG, "Successfully generated new condor CA.\n");
	return true;
}

	// Generate a new certificate and a new CA
bool htcondor::generate_x509_cert(const std::string &certfile, const std::string &keyfile, const std::string &cafile, const std::string &cakeyfile)
{
	if (0 == access(certfile.c_str(), R_OK)) {
		return true;
	}

	auto ca_private_key = generate_key(cakeyfile);
	if (!ca_private_key) {
		return false;
	}
	auto ca_cert = read_cert(cafile);

	auto private_key = generate_key(keyfile);
	if (!private_key) {
		return false;
	}

	std::string host_alias;
	if (!param(host_alias, "HOST_ALIAS")) {
		dprintf(D_ALWAYS, "Cannot generate new certificate - HOST_ALIAS is not set.");
		return false;
	}

	auto x509_name = generate_cert_name(host_alias);
	if (!x509_name) {return false;}
	auto cert = generate_generic_cert(x509_name.get(), private_key.get(), 2*365);
	if (!cert) {return false;}

        X509_set_issuer_name(cert.get(), X509_get_issuer_name(ca_cert.get()));

	if (!add_x509v3_ext(ca_cert.get(), cert.get(), NID_authority_key_identifier, "keyid:always", false) ||
		!add_x509v3_ext(ca_cert.get(), cert.get(), NID_basic_constraints, "CA:false", true) ||
		!add_x509v3_ext(ca_cert.get(), cert.get(), NID_ext_key_usage, "serverAuth", true))
	{
		return false;
	}

	std::unique_ptr<GENERAL_NAMES, decltype(&GENERAL_NAMES_free)> gens(
		sk_GENERAL_NAME_new_null(),
		&GENERAL_NAMES_free);
	std::unique_ptr<GENERAL_NAME, decltype(&GENERAL_NAME_free)> gen(
		GENERAL_NAME_new(),
		&GENERAL_NAME_free);
	ASN1_IA5STRING *ia5 = ASN1_IA5STRING_new();
	if (!gens || !gen || !ia5) {
		dprintf(D_ALWAYS, "Certificate generation: failed to allocate data.\n");
		return false;
	}
	ASN1_STRING_set(ia5, host_alias.data(), host_alias.length());
	GENERAL_NAME_set0_value(gen.get(), GEN_DNS, ia5);
	sk_GENERAL_NAME_push(gens.get(), gen.get());
	gen.release();

	if (1 != X509_add1_ext_i2d(cert.get(), NID_subject_alt_name, gens.get(), 0, X509V3_ADD_DEFAULT)) {
		dprintf(D_ALWAYS, "Certificate generation: failed to add SAN to certificate.\n");
		return false;
	}

	if (0 > X509_sign(cert.get(), ca_private_key.get(), EVP_sha256())) {
		dprintf(D_ALWAYS, "Certificate generation: failed to sign the certificate\n");
		return false;
	}

	std::unique_ptr<FILE, fcloser> fp(
		safe_fcreate_fail_if_exists(certfile.c_str(), "w"));
	if (!fp) {
		dprintf(D_ALWAYS, "Certificate generation: failed to create a new file at %s: %s (errno=%d)\n",
			certfile.c_str(), strerror(errno), errno);
		return false;
	}


	if (1 != PEM_write_X509(fp.get(), cert.get())) {
		dprintf(D_ALWAYS, "Certificate generation: failed to write the certificate %s: %s (errno=%d)\n",
			certfile.c_str(), strerror(errno), errno);
		unlink(certfile.c_str());
		return false;
	}
	if (1 != PEM_write_X509(fp.get(), ca_cert.get())) {
		dprintf(D_ALWAYS, "Certificate generation: failed to write the CA certificate %s: %s (errno=%d)\n",
			certfile.c_str(), strerror(errno), errno);
		unlink(certfile.c_str());
		return false;
	}

	return true;
}


bool htcondor::get_known_hosts_first_match(const std::string &hostname, bool &permitted, std::string &method, std::string &method_info)
{
	auto fp = get_known_hosts();
	if (!fp) {return false;}

	for (std::string line; readLine(line, fp.get(),false);) {
		trim(line);
		if (line.empty() || line[0] == '#') continue;

		std::vector<std::string> tokens = split(line, " ");
		if (tokens.size() < 3) {
			dprintf(D_SECURITY, "Incorrect format in known host file.\n");
			continue;
		}

		if (tokens[0].size() && tokens[0][0] == '!' && tokens[0].substr(1) == hostname) {
			permitted = false;
			method = tokens[1];
			method_info = tokens[2];
			return true;
		} else if (tokens[0] == hostname) {
			permitted = true;
			method = tokens[1];
			method_info = tokens[2];
			return true;
		}
	}
	return false;
}


bool htcondor::add_known_hosts(const std::string &hostname, bool permitted, const std::string &method, const std::string &method_info)
{
	if (check_known_hosts_any_match(hostname, permitted, method, method_info)) {
		return true;
	}

	auto fp = get_known_hosts();
	if (!fp) {return false;}
	auto fd = fileno(fp.get());
	if (fd == -1) {return false;}

	std::stringstream ss;
	ss << (permitted ? "" : "!") << hostname << " " << method << " " << method_info << std::endl;
	std::string line = ss.str();

	if (static_cast<ssize_t>(line.size()) != full_write(fd, line.c_str(), line.size())) {
		dprintf(D_SECURITY, "Failed to record details for hostname %s into known hosts file:"
			" %s (errno=%d)\n", hostname.c_str(), strerror(errno),
			errno);
		return false;
	}
	return true;
}


// Given a reference to an X509 object, produce a human-readable fingerprint
bool htcondor::generate_fingerprint(const X509 *cert, std::string &fingerprint, CondorError &err)
{
	unsigned char md[EVP_MAX_MD_SIZE];
	auto digest = EVP_get_digestbyname("sha256");
	if (!digest) {
		err.push("FINGERPRINT", 1, "sha256 digest is not available");
		return false;
	}
	unsigned int len;
	if (1 != X509_digest(cert, digest, md, &len)) {
		err.push("FINGERPRINT", 2, "Failed to create a digest of the provided X.509 certificate");
		auto err_msg = ERR_error_string(ERR_get_error(), nullptr);
		if (err_msg) err.pushf("FINGERPRINT", 3, "OpenSSL error message: %s\n", err_msg);
		return false;
	}

	std::stringstream ss;
	ss << std::hex << std::setw(2) << std::setfill('0');
	bool first = true;
	for (unsigned idx = 0; idx < len; idx++) {
		if (!first) ss << ":";
		ss << std::setw(2) << static_cast<int>(md[idx]);
		first = false;
	}
	fingerprint = ss.str();
	return true;
}


bool htcondor::ask_cert_confirmation(const std::string &host_alias, const std::string &fingerprint, const std::string &dn, bool is_ca_cert)
{
	fprintf(stderr, "The remote host %s presented an untrusted %scertificate with the following fingerprint:\n",
		host_alias.c_str(),
		is_ca_cert ? "CA " : "");
	fprintf(stderr, "SHA-256: %s\n", fingerprint.c_str());
	fprintf(stderr, "Subject: %s\n", dn.c_str());
	fprintf(stderr, "Would you like to trust this server for current and future communications?\n");

	std::string response;
	do {
		fprintf(stderr, "Please type 'yes' or 'no':\n");
		std::getline(std::cin, response);
	} while (response != "yes" && response != "no");

	return response == "yes";
}
