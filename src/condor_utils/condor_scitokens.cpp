
#include "condor_common.h"

#ifndef WIN32
#include <dlfcn.h>
#endif

#include "condor_debug.h"
#include "condor_config.h"

#include "condor_scitokens.h"

GCC_DIAG_OFF(float-equal)
GCC_DIAG_OFF(cast-qual)
#include "jwt-cpp/jwt.h"
GCC_DIAG_ON(float-equal)
GCC_DIAG_ON(cast-qual)

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
static int (*scitoken_get_claim_string_list_ptr)(const SciToken token, const char *key, char ***value, char **err_msg) = nullptr;
static void (*scitoken_free_string_list_ptr)(char **value) = nullptr;

#define LIBSCITOKENS_SO "libSciTokens.so.0"

bool g_init_tried = false;
bool g_init_success = false;

bool
normalize_token(const std::string &input_token, std::string &output_token)
{
	static const std::string whitespace = " \t\f\n\v\r";
		// Tokens are meant for use in an HTTP header; these are forbidden
		// characters.
	static const std::string nonheader_whitespace = "\r\n";
	auto begin = input_token.find_first_not_of(whitespace);
		// No token here, but not an error.
	if (begin == std::string::npos) {
		output_token = "";
		return true;
	}

	std::string token = input_token.substr(begin);
	auto end = token.find_last_not_of(whitespace);
	token = token.substr(0, end + 1);

		// If non-permitted header characters are present ("\r\n"),
		// then this token is not permitted.
	if (token.find(nonheader_whitespace) != std::string::npos) {
		output_token = "";
		dprintf(D_SECURITY, "Token discovery failure: token contains non-permitted character sequence (\\r\\n)\n");
		return false;
	}

	output_token = token;
	return true;
}

bool
find_token_in_file(const std::string &token_file, std::string &output_token)
{
	dprintf(D_FULLDEBUG,  "Looking for token in file %s\n", token_file.c_str());
	int fd = safe_open_no_create(token_file.c_str(), O_RDONLY);
	if (fd == -1) {
		output_token = "";
		if (errno == ENOENT) {
			return true;
		}
		dprintf(D_SECURITY,  "Token discovery failure: failed to open file %s: %s (errno=%d).\n",
			token_file.c_str(), strerror(errno), errno);
		return false;
	}

	// As a pragmatic matter, we limit tokens to 16KB; this is often the limit
	// in size of HTTP headers for many web servers.
	// This also avoids unbounded memory use in case if the user points us to
	// a large file.
	static const size_t max_size = 16384;

	std::vector<char> input_buffer;
	input_buffer.resize(max_size);

	ssize_t retval = full_read(fd, &input_buffer[0], max_size);

	close(fd);

	if (retval == -1) {
		output_token = "";
		dprintf(D_SECURITY,  "Token discovery failure: failed to read file %s: %s (errno=%d).\n",
			token_file.c_str(), strerror(errno), errno);
		return false;
	}
	if (retval == 16384) {
		// Token may have been truncated!  Erring on the side of caution.
		dprintf(D_SECURITY, "Token discovery failure: token was larger than 16KB limit.\n");
		return false;
	}

	std::string token(&input_buffer[0], retval);

	return normalize_token(token, output_token);
}

} // end anonymous namespace

bool
htcondor::init_scitokens()
{
	if (g_init_tried) {
		return g_init_success;
	}

#ifndef WIN32
#if defined(DLOPEN_SECURITY_LIBS)
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
		dprintf(D_SECURITY, "Failed to open SciTokens library: %s\n", err_msg ? err_msg : "(no error message available)");
		g_init_success = false;
	} else {
		g_init_success = true;
			// Note: these methods are only in a more recent version of the SciTokens library; hence, if they
			// are missing it's considered non-fatal.
		scitoken_get_claim_string_list_ptr = (int (*)(const SciToken token, const char *key, char ***value, char **err_msg))dlsym(dl_hdl, "scitoken_get_claim_string_list");
		scitoken_free_string_list_ptr = (void (*)(char **value))dlsym(dl_hdl, "scitoken_free_string_list");
	}
#else
	scitoken_deserialize_ptr = scitoken_deserialize;
	scitoken_get_claim_string_ptr = scitoken_get_claim_string;
	scitoken_destroy_ptr = scitoken_destroy;
	enforcer_create_ptr = enforcer_create;
	enforcer_destroy_ptr = enforcer_destroy;
	enforcer_generate_acls_ptr = enforcer_generate_acls;
	enforcer_acl_free_ptr = enforcer_acl_free;
	scitoken_get_expiration_ptr = scitoken_get_expiration;
	scitoken_get_claim_string_list_ptr = scitoken_get_claim_string_list;
	scitoken_free_string_list_ptr = scitoken_free_string_list;
#endif
#else
	dprintf(D_SECURITY, "SciTokens is not supported on Windows.\n");
	g_init_success = false;
#endif
	g_init_tried = true;
	return g_init_success;
}

std::string
htcondor::discover_token()
{
	const char *bearer_token = getenv("BEARER_TOKEN");
	std::string token;
	if (bearer_token && *bearer_token)
	{
		if (!normalize_token(bearer_token, token)) {return "";}
		if (!token.empty()) {return token;}
	}

	const char *bearer_token_file = getenv("BEARER_TOKEN_FILE");
	if (bearer_token_file)
	{
		if (!find_token_in_file(bearer_token_file, token)) {return "";}
		if (!token.empty()) {return token;}
	}

#ifndef WIN32
	uid_t euid = geteuid();
	std::string fname = "/bt_u";
	fname += std::to_string(euid);

	const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (xdg_runtime_dir) {
		std::string xdg_token_file = std::string(xdg_runtime_dir) + fname;
		if (!find_token_in_file(xdg_token_file, token)) {return "";}
		if (!token.empty()) {return token;}
	}

	if (!find_token_in_file("/tmp" + fname, token)) {return "";}
	return token;
#else
		// WLCG profile doesn't define search paths on Windows;
		// skip this for now.
	return "";
#endif
}

bool
htcondor::validate_scitoken(const std::string &scitoken_str, std::string &issuer, std::string &subject,
	long long &expiry, std::vector<std::string> &bounding_set, std::vector<std::string> &groups, std::vector<std::string> &scopes, std::string &jti, int ident, CondorError &err)
{
	if (!htcondor::init_scitokens()) {
		err.pushf("SCITOKENS", 1, "Failed to open SciTokens library.");
		return false;
	}

	if (ident && IsDebugCategory(D_AUDIT)) {
		try {
			auto jwt = jwt::decode(scitoken_str);
			dprintf(D_AUDIT, ident, "Examining SciToken with payload %s.\n", jwt.get_payload().c_str());
		} catch (...) {
			err.pushf("SCITOKENS", 2, "Failed to decode scitoken for audit log.");
			dprintf(D_AUDIT, ident, "Failed to decode a SciToken in order to examine it - rejecting.\n");
			return false;

		}
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

		char *scopes_char = nullptr;
		if (!(*scitoken_get_claim_string_ptr)(token, "scope", &scopes_char, nullptr)) {
			StringList scopes_list(scopes_char);
			scopes_list.rewind();
			free(scopes_char);
			char *scope;
			while ( (scope = scopes_list.next()) ) {
				scopes.emplace_back(scope);
			}
		}

		char *jti_char = nullptr;
		if (!(*scitoken_get_claim_string_ptr)(token, "jti", &jti_char, nullptr)) {
			if (jti_char) {jti = jti_char;}
			free(jti_char);
		}

		char **group_list = nullptr;
		if (scitoken_get_claim_string_list_ptr &&
			!(*scitoken_get_claim_string_list_ptr)(token, "wlcg.groups", &group_list, nullptr) &&
			group_list)
		{
			int idx = 0;
			char *grp;
			while ( (grp = group_list[idx++]) ) {
				groups.emplace_back(grp);
			}
		}
		if (scitoken_free_string_list_ptr && group_list) {
			(*scitoken_free_string_list_ptr)(group_list);
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
