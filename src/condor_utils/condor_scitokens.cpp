
#include "condor_common.h"

#ifndef WIN32
#include <dlfcn.h>
#endif

#include "condor_debug.h"
#include "condor_config.h"

#include "condor_scitokens.h"

#ifdef HAVE_EXT_SCITOKENS
#include <scitokens/scitokens.h>
#else
typedef void * SciToken;
typedef void * Enforcer;
typedef struct Acl_s {
	const char *authz;
	const char *resource;
} Acl;
#endif

namespace {

static int (*scitoken_deserialize_ptr)(const char *value, SciToken *token, const char * const*allowed_issuers, char **err_msg) = nullptr;
static int (*scitoken_get_claim_string_ptr)(const SciToken token, const char *key, char **value, char **err_msg) = nullptr;
static void (*scitoken_destroy_ptr)(SciToken token) = nullptr;
static Enforcer (*enforcer_create_ptr)(const char *issuer, const char **audience, char **err_msg) = nullptr;
static void (*enforcer_destroy_ptr)(Enforcer) = nullptr;
static int (*enforcer_generate_acls_ptr)(const Enforcer enf, const SciToken scitokens, Acl **acls, char **err_msg) = nullptr;
static void (*enforcer_acl_free_ptr)(Acl *acls) = nullptr;
static int (*scitoken_get_expiration_ptr)(const SciToken token, long long *value, char **err_msg) =
	nullptr;

#define LIBSCITOKENS_SO "libSciTokens.so.0"

bool g_init_success = false; 

bool
init_scitokens(CondorError &err)
{
	if (g_init_success) {
		return true;
	}

#ifndef WIN32
	dlerror();
	void *dl_hdl = nullptr;
	if (
		!(dl_hdl = dlopen(LIBSCITOKENS_SO, RTLD_LAZY)) ||
		!(scitoken_deserialize_ptr = (int (*)(const char *value, SciToken *token, const char * const*allowed_issuers, char **err_msg))dlsym(dl_hdl, "scitoken_deserialize")) ||
		!(scitoken_get_claim_string_ptr = (int (*)(const SciToken token, const char *key, char **value, char **err_msg))dlsym(dl_hdl, "scitoken_get_claim_string")) ||
		!(scitoken_destroy_ptr = (void (*)(SciToken token))dlsym(dl_hdl, "scitoken_destroy")) ||
		!(enforcer_create_ptr = (Enforcer (*)(const char *issuer, const char **audience, char **err_msg))dlsym(dl_hdl, "enforcer_create")) ||
		!(enforcer_destroy_ptr = (void (*)(Enforcer))dlsym(dl_hdl, "enforcer_destroy")) ||
		!(enforcer_generate_acls_ptr = (int (*)(const Enforcer enf, const SciToken scitokens, Acl **acls, char **err_msg))dlsym(dl_hdl, "enforcer_generate_acls")) ||
		!(enforcer_acl_free_ptr = (void (*)(Acl *acls))dlsym(dl_hdl, "enforcer_acl_free")) ||
		!(scitoken_get_expiration_ptr = (int (*)(const SciToken token, long long *value, char **err_msg))dlsym(dl_hdl, "scitoken_get_expiration"))
	) {
		const char *err_msg = dlerror();
		if (err_msg) {
			err.pushf("SCITOKENS", 1, "Failed to open SciTokens library: %s", err_msg);
		} else {
			err.pushf("SCITOKENS", 1, "Failed to initialize SciTokens (no error message available)");
		}
		g_init_success = false;
	}
	return (g_init_success = true);
#else
	err.pushf("SCITOKENS", 1, "SciTokens library not supported on Windwos.");
	return (g_init_success = false);
#endif
}

} // end anonymous namespace

bool
htcondor::validate_scitoken(const std::string &scitoken_str, std::string &issuer, std::string &subject,
	long long &expiry, std::vector<std::string> &bounding_set, CondorError &err)
{
	if (!init_scitokens(err)) {
		return false;
	}

	SciToken token = nullptr;
	char *err_msg = nullptr;
	char *iss = nullptr;
	char *sub = nullptr;
	Enforcer enf = nullptr;
	Acl *acls = nullptr;
	std::vector<std::string> audiences;
	std::vector<const char *> audience_ptr;
	std::string audience_string;
	if (param(audience_string, "SCITOKENS_SERVER_AUDIENCE")) {
		StringList audience_list(audience_string.c_str());
		audience_list.rewind();
		char *aud;
		while ( (aud = audience_list.next()) ) {
			audiences.emplace_back(aud);
			audience_ptr.push_back(audiences.back().c_str());
		}
		audience_ptr.push_back(nullptr);
	}
	long long expiry_value;
	if ((*scitoken_deserialize_ptr)(scitoken_str.c_str(), &token, nullptr, &err_msg)) {
		err.pushf("SCITOKENS", 2, "Failed to deserialize scitoken: %s",
			err_msg ? err_msg : "(unknown failure)");
		free(err_msg);
		return false;
	} else if ((*scitoken_get_expiration_ptr)(token, &expiry_value, &err_msg)) {
		err.pushf("SCITOKENS", 2, "Unable to retrieve token expiration: %s",
			err_msg ? err_msg : "(unknown failure)");
		free(err_msg);
		(*scitoken_destroy_ptr)(token);
		token = nullptr;
	} else if ((*scitoken_get_claim_string_ptr)(token, "iss", &iss, &err_msg)) {
		err.pushf("SCITOKENS", 2, "Unable to retrieve token issuer: %s",
			err_msg ? err_msg : "(unknown failure)");
		free(err_msg);
		(*scitoken_destroy_ptr)(token);
		token = nullptr;
		return false;
	} else if ((*scitoken_get_claim_string_ptr)(token, "sub", &sub, &err_msg) || !sub) {
		err.pushf("SCITOKENS", 2, "Unable to retrieve token subject: %s",
			err_msg ? err_msg : "(unknown failure)");
		free(err_msg);
		(*scitoken_destroy_ptr)(token);
		token = nullptr;
		free(iss);
		return false;
	} else if (!(enf = (*enforcer_create_ptr)(iss, &audience_ptr[0], &err_msg))) {
		err.pushf("SCITOKENS", 2, "Failed to create SciTokens enforcer: %s",
			err_msg ? err_msg : "(unknown failure)");
		free(err_msg);
		(*scitoken_destroy_ptr)(token);
		free(iss);
		free(sub);
		return false;
	} else if ((*enforcer_generate_acls_ptr)(enf, token, &acls, &err_msg)) {
		err.pushf("SCITOKENS", 2, "Failed to verify token and generate ACLs: %s",
			err_msg ? err_msg : "(unknown failure)");
		free(err_msg);
		(*scitoken_destroy_ptr)(token);
		free(iss);
		free(sub);
		(*enforcer_destroy_ptr)(enf);
		return false;
	} else {
			// Success block - everything checks out!
		std::vector<std::string> authz;
		if (acls) {
			int idx = 0;
			Acl acl = acls[idx++];
			while ( (acl.authz && acl.resource) ) {
				if (!strcmp(acl.authz, "condor")) {
					const char * condor_authz = acl.resource;
					while (*condor_authz != '\0' && *condor_authz == '/') {condor_authz++;}
					if (strlen(condor_authz)) {authz.emplace_back(condor_authz);}
				}
				acl = acls[idx++];
			}

			(*enforcer_acl_free_ptr)(acls);
		}

		issuer = iss;
		subject = sub;
		bounding_set = std::move(authz);
		expiry = expiry_value;
		dprintf(D_SECURITY, "SciToken is mapped to issuer '%s'\n", issuer.c_str());
		(*scitoken_destroy_ptr)(token);
		free(iss);
		free(sub);
		(*enforcer_destroy_ptr)(enf);
		return true;
	}
	return false;
}
