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
#include "condor_config.h"
#include "condor_debug.h"
#include "directory_util.h"
#include "condor_regex.h"
#include "condor_attributes.h"
#include "condor_auth_ssl.h"   // AUTH_SSL_SERVER_CERTFILE_STR / KEYFILE_STR, OpenSSL types
#include "condor_base64.h"
#include "safe_fopen.h"
#include "safe_open.h"
#include "prometheusd.h"

#if !defined(WIN32)
#include <unistd.h>
#include <crypt.h>
#endif

#include <map>
#include <unordered_map>
#include <algorithm>
#include <sys/socket.h>
#include <sys/time.h>

#if defined(DLOPEN_SECURITY_LIBS)
#include <dlfcn.h>
#endif

// ---------------------------------------------------------------------------
// Minimal OpenSSL function-pointer layer used only by the Prometheus HTTP
// server.  We follow the same DLOPEN_SECURITY_LIBS pattern used elsewhere in
// the codebase so that the binary degrades gracefully when libssl is absent.
// ---------------------------------------------------------------------------
namespace {

// Function pointer types and storage
static decltype(&SSL_CTX_new)                       g_SSL_CTX_new                       = nullptr;
static decltype(&SSL_CTX_free)                      g_SSL_CTX_free                      = nullptr;
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
static decltype(&SSLv23_server_method)              g_TLS_server_method                 = nullptr;
#else
static decltype(&TLS_server_method)                 g_TLS_server_method                 = nullptr;
#endif
static decltype(&SSL_CTX_use_certificate_chain_file) g_SSL_CTX_use_certificate_chain_file = nullptr;
static decltype(&SSL_CTX_use_PrivateKey_file)       g_SSL_CTX_use_PrivateKey_file       = nullptr;
static decltype(&SSL_CTX_check_private_key)         g_SSL_CTX_check_private_key         = nullptr;
static decltype(&SSL_CTX_set_verify)                g_SSL_CTX_set_verify                = nullptr;
static decltype(&SSL_new)                           g_SSL_new                           = nullptr;
static decltype(&SSL_free)                          g_SSL_free                          = nullptr;
static decltype(&SSL_set_fd)                        g_SSL_set_fd                        = nullptr;
static decltype(&SSL_accept)                        g_SSL_accept                        = nullptr;
static decltype(&SSL_read)                          g_SSL_read                          = nullptr;
static decltype(&SSL_write)                         g_SSL_write                         = nullptr;
static decltype(&SSL_shutdown)                      g_SSL_shutdown                      = nullptr;
static decltype(&SSL_get_error)                     g_SSL_get_error                     = nullptr;
static decltype(&SHA1)                              g_SHA1                              = nullptr;

// Load all function pointers.  Returns true if SSL is available.
static bool prom_ssl_initialize()
{
	static bool tried = false;
	static bool ok    = false;
	if (tried) return ok;
	tried = true;

#if defined(DLOPEN_SECURITY_LIBS)
	void *hdl = dlopen(LIBSSL_SO, RTLD_LAZY | RTLD_NOLOAD);
	if (!hdl) hdl = dlopen(LIBSSL_SO, RTLD_LAZY);
	if (!hdl) {
		dprintf(D_FULLDEBUG, "PrometheusD: libssl not available (%s); HTTPS disabled\n",
		        dlerror());
		return false;
	}

	// libcrypto (SHA1) – loaded transitively; get a handle via NOLOAD
	void *crypto_hdl = dlopen("libcrypto.so", RTLD_LAZY | RTLD_NOLOAD);
	if (!crypto_hdl) crypto_hdl = dlopen("libcrypto.so.3", RTLD_LAZY | RTLD_NOLOAD);
	if (!crypto_hdl) crypto_hdl = dlopen("libcrypto.so.1.1", RTLD_LAZY | RTLD_NOLOAD);
	if (crypto_hdl) {
		g_SHA1 = reinterpret_cast<decltype(g_SHA1)>(dlsym(crypto_hdl, "SHA1"));
	}

#define LOAD(hdl, sym) \
	!(g_##sym = reinterpret_cast<decltype(g_##sym)>(dlsym(hdl, #sym)))

	if (LOAD(hdl, SSL_CTX_new)                        ||
	    LOAD(hdl, SSL_CTX_free)                        ||
	    LOAD(hdl, SSL_CTX_use_certificate_chain_file)  ||
	    LOAD(hdl, SSL_CTX_use_PrivateKey_file)         ||
	    LOAD(hdl, SSL_CTX_check_private_key)           ||
	    LOAD(hdl, SSL_CTX_set_verify)                  ||
	    LOAD(hdl, SSL_new)                             ||
	    LOAD(hdl, SSL_free)                            ||
	    LOAD(hdl, SSL_set_fd)                          ||
	    LOAD(hdl, SSL_accept)                          ||
	    LOAD(hdl, SSL_read)                            ||
	    LOAD(hdl, SSL_write)                           ||
	    LOAD(hdl, SSL_shutdown)                        ||
	    LOAD(hdl, SSL_get_error)) {
		dprintf(D_ERROR, "PrometheusD: failed to load SSL symbol: %s\n", dlerror());
		return false;
	}
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	if (LOAD(hdl, SSLv23_server_method)) {
		dprintf(D_ERROR, "PrometheusD: failed to load SSLv23_server_method: %s\n", dlerror());
		return false;
	}
#else
	if (LOAD(hdl, TLS_server_method)) {
		dprintf(D_ERROR, "PrometheusD: failed to load TLS_server_method: %s\n", dlerror());
		return false;
	}
#endif
#undef LOAD

#else // not DLOPEN – functions are linked directly
	g_SSL_CTX_new                       = SSL_CTX_new;
	g_SSL_CTX_free                      = SSL_CTX_free;
	g_SSL_CTX_use_certificate_chain_file = SSL_CTX_use_certificate_chain_file;
	g_SSL_CTX_use_PrivateKey_file        = SSL_CTX_use_PrivateKey_file;
	g_SSL_CTX_check_private_key          = SSL_CTX_check_private_key;
	g_SSL_CTX_set_verify                 = SSL_CTX_set_verify;
	g_SSL_new                            = SSL_new;
	g_SSL_free                           = SSL_free;
	g_SSL_set_fd                         = SSL_set_fd;
	g_SSL_accept                         = SSL_accept;
	g_SSL_read                           = SSL_read;
	g_SSL_write                          = SSL_write;
	g_SSL_shutdown                       = SSL_shutdown;
	g_SSL_get_error                      = SSL_get_error;
	g_SHA1                               = SHA1;
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	g_TLS_server_method                  = SSLv23_server_method;
#else
	g_TLS_server_method                  = TLS_server_method;
#endif
#endif

	ok = true;
	return true;
}

} // anonymous namespace

std::string
PrometheusMetric::prometheusType() const
{
	if (derivative && aggregate == NO_AGGREGATE) {
		return "counter";
	}
	return "gauge";
}

PrometheusD::PrometheusD()
{
}

PrometheusD::~PrometheusD()
{
	if (m_ssl_ctx && g_SSL_CTX_free) {
		g_SSL_CTX_free(static_cast<SSL_CTX*>(m_ssl_ctx));
		m_ssl_ctx = nullptr;
	}
}

void
PrometheusD::buildSslCtx()
{
	// Free any previous context
	if (m_ssl_ctx) {
		if (g_SSL_CTX_free) {
			g_SSL_CTX_free(static_cast<SSL_CTX*>(m_ssl_ctx));
		}
		m_ssl_ctx = nullptr;
	}

	if (!prom_ssl_initialize()) {
		return;  // libssl not available
	}

	std::string certfile, keyfile;
	if (!param(certfile, AUTH_SSL_SERVER_CERTFILE_STR) ||
	    !param(keyfile,  AUTH_SSL_SERVER_KEYFILE_STR)) {
		dprintf(D_FULLDEBUG,
		        "PrometheusD: AUTH_SSL_SERVER_CERTFILE or AUTH_SSL_SERVER_KEYFILE"
		        " not set; Prometheus HTTPS disabled\n");
		return;
	}

	// Verify the files are readable before bothering to build a context
	{
		auto fd = safe_open_no_create(certfile.c_str(), O_RDONLY);
		if (fd < 0) {
			dprintf(D_ERROR,
			        "PrometheusD: cannot read cert file '%s': %s; HTTPS disabled\n",
			        certfile.c_str(), strerror(errno));
			return;
		}
		close(fd);
		fd = safe_open_no_create(keyfile.c_str(), O_RDONLY);
		if (fd < 0) {
			dprintf(D_ERROR,
			        "PrometheusD: cannot read key file '%s': %s; HTTPS disabled\n",
			        keyfile.c_str(), strerror(errno));
			return;
		}
		close(fd);
	}

	SSL_CTX *ctx = g_SSL_CTX_new(g_TLS_server_method());
	if (!ctx) {
		dprintf(D_ERROR, "PrometheusD: SSL_CTX_new failed; HTTPS disabled\n");
		return;
	}

	// No client certificate required
	g_SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);

	if (g_SSL_CTX_use_certificate_chain_file(ctx, certfile.c_str()) != 1) {
		dprintf(D_ERROR,
		        "PrometheusD: SSL_CTX_use_certificate_chain_file('%s') failed;"
		        " HTTPS disabled\n", certfile.c_str());
		g_SSL_CTX_free(ctx);
		return;
	}
	if (g_SSL_CTX_use_PrivateKey_file(ctx, keyfile.c_str(), SSL_FILETYPE_PEM) != 1) {
		dprintf(D_ERROR,
		        "PrometheusD: SSL_CTX_use_PrivateKey_file('%s') failed;"
		        " HTTPS disabled\n", keyfile.c_str());
		g_SSL_CTX_free(ctx);
		return;
	}
	if (g_SSL_CTX_check_private_key(ctx) != 1) {
		dprintf(D_ERROR,
		        "PrometheusD: SSL_CTX_check_private_key failed (cert/key mismatch);"
		        " HTTPS disabled\n");
		g_SSL_CTX_free(ctx);
		return;
	}

	m_ssl_ctx = ctx;
	dprintf(D_ALWAYS,
	        "PrometheusD: HTTPS enabled (cert=%s)\n", certfile.c_str());
}

void
PrometheusD::initAndReconfig()
{
	// Parse metric definitions and learn target types, but never register a
	// timer of our own (the owning MetricD runs the cycle).
	StatsD::base_initAndReconfig("METRICD", true);

	param(m_output_file,"PROMETHEUS_METRICS_FILE");
	m_include_timestamp = param_boolean("PROMETHEUS_METRICS_INCLUDE_TIMESTAMP", false);

	std::string default_labels_str;
	param(default_labels_str,"PROMETHEUS_DEFAULT_LABELS");
	m_default_labels = parseLabels(default_labels_str);

	m_reset_metrics_filename.clear();
	if (param_boolean("PROMETHEUS_WANT_RESET_METRICS", false)) {
		param(m_reset_metrics_filename,"PROMETHEUS_RESET_METRICS_FILE");
		if (!m_reset_metrics_filename.empty()) {
			if (!IS_ANY_DIR_DELIM_CHAR(m_reset_metrics_filename[0])) {
				std::string fname = m_reset_metrics_filename;
				std::string dirname;
				param(dirname,"SPOOL");
				dircat(dirname.c_str(),fname.c_str(),m_reset_metrics_filename);
			}
			if (!m_reset_metrics_filename.ends_with(".prometheus_metrics")) {
				m_reset_metrics_filename += ".prometheus_metrics";
			}
		}
	}

	// HTTP Basic auth password file (Apache htpasswd format).
	// If empty, the /metrics endpoint is unauthenticated.
	param(m_http_auth_file, "PROMETHEUS_HTTP_AUTH_FILE");

	// (Re)build the TLS context whenever config changes.
	buildSslCtx();

	// Register the HTTP command handler exactly once per process lifetime.
	// DaemonCore only allows a single HTTP handler so we guard with a flag.
	if (!m_http_handler_registered && !m_output_file.empty()) {
		int rc = daemonCore->Register_HTTP_CommandHandler(
			[this](int cmd, Stream *s) { return this->handleHttpCommand(cmd, s); },
			"PrometheusD::handleHttpCommand");
		if (rc >= 0) {
			m_http_handler_registered = true;
			dprintf(D_ALWAYS,
			        "PrometheusD: registered HTTP handler for /metrics endpoint\n");
		} else {
			dprintf(D_ERROR,
			        "PrometheusD: Register_HTTP_CommandHandler failed (rc=%d)\n", rc);
		}
	}
}

Metric *
PrometheusD::newMetric(Metric const *copy_me)
{
	if (copy_me) {
		return new PrometheusMetric(*static_cast<const PrometheusMetric*>(copy_me));
	}
	return new PrometheusMetric();
}

std::string
PrometheusD::buildPrometheusName(const Metric &m) const
{
	std::string name = m.name;

	// best-effort unit suffix
	std::string units_lc = m.units;
	std::transform(units_lc.begin(), units_lc.end(), units_lc.begin(),
		[](unsigned char c){ return std::tolower(c); });
	const char *unit_suffix = nullptr;
	if (units_lc == "bytes") unit_suffix = "_bytes";
	else if (units_lc == "seconds") unit_suffix = "_seconds";
	else if (units_lc == "milliseconds") unit_suffix = "_milliseconds";
	else if (units_lc == "microseconds") unit_suffix = "_microseconds";
	else if (units_lc == "percent" || units_lc == "%") unit_suffix = "_ratio";
	if (unit_suffix && !name.ends_with(unit_suffix)) {
		name += unit_suffix;
	}

	// counter suffix
	if (m.derivative && m.aggregate == Metric::NO_AGGREGATE) {
		if (!name.ends_with("_total")) {
			name += "_total";
		}
	}

	return name;
}

std::string
PrometheusD::buildPrometheusHelp(const Metric &m) const
{
	std::string help = m.desc;
	if (!m.units.empty()) {
		help += " (";
		help += m.units;
		help += ")";
	}
	return help;
}

std::string
PrometheusD::buildEffectiveLabels(const Metric &m) const
{
	std::map<std::string,std::string> merged = m_default_labels;
	std::map<std::string,std::string> per_metric = parseLabels(m.prometheus_labels);
	for (const auto &kv : per_metric) {
		merged[kv.first] = kv.second;
	}
	return serializeLabels(merged);
}

void
PrometheusD::publishMetric(Metric const &m_in)
{
	if (m_output_file.empty()) {
		return;
	}

	if (!m_in.export_systems.empty() && !contains_anycase(m_in.export_systems, "prometheus")) {
		return;
	}

	std::string prom_name = buildPrometheusName(m_in);

	// Validate Prometheus metric name: [a-zA-Z_:][a-zA-Z0-9_:]*
	Regex re;
	int errcode = 0;
	int erroffset = 0;
	if (!re.compile("^[a-zA-Z_:][a-zA-Z0-9_:]*$",&errcode,&erroffset)) {
		dprintf(D_ERROR, "Prometheus name regex failed to compile\n");
		return;
	}
	if (!re.match(prom_name)) {
		dprintf(D_ERROR, "Invalid Prometheus metric name '%s'; skipping\n", prom_name.c_str());
		return;
	}

	std::string value;
	if (!m_in.getValueString(value) || value.empty()) {
		return;
	}

	PendingMetric pm;
	pm.name = prom_name;
	pm.labels = buildEffectiveLabels(m_in);
	pm.value = value;
	pm.help = buildPrometheusHelp(m_in);
	pm.prom_type = static_cast<const PrometheusMetric &>(m_in).prometheusType();
	pm.timestamp = m_in.timestamp;
	m_pending.push_back(pm);
}

void
PrometheusD::postPublishMetrics()
{
	writeMetricsFile();
	m_pending.clear();
}

void
PrometheusD::extraProjectionRefs(classad::References &refs) const
{
	// When emitting timestamps we need ATTR_LAST_HEARD_FROM. The owning
	// MetricD merges this into its own projection because MetricD, not this
	// backend, is the one that issues the collector query.
	if (!m_output_file.empty() && m_include_timestamp) {
		refs.insert(ATTR_LAST_HEARD_FROM);
	}
}

void
PrometheusD::writeMetricsFile()
{
	if (m_output_file.empty()) return;

	std::string tmp = m_output_file + ".tmp";
	FILE *fp = safe_fcreate_replace_if_exists(tmp.c_str(),"w");
	if (!fp) {
		dprintf(D_ERROR, "Failed to open Prometheus output file %s for writing: %s\n",
		        tmp.c_str(), strerror(errno));
		return;
	}

	// Preserve first-occurrence ordering of metric names while emitting
	// HELP/TYPE once per group followed by each sample.
	std::vector<std::string> order;
	std::unordered_map<std::string,std::vector<size_t>> groups;
	for (size_t i = 0; i < m_pending.size(); ++i) {
		const std::string &n = m_pending[i].name;
		auto it = groups.find(n);
		if (it == groups.end()) {
			order.push_back(n);
		}
		groups[n].push_back(i);
	}

	for (const auto &name : order) {
		const auto &indices = groups[name];
		if (indices.empty()) continue;
		const PendingMetric &first = m_pending[indices.front()];
		fprintf(fp,"# HELP %s %s\n",name.c_str(),first.help.c_str());
		fprintf(fp,"# TYPE %s %s\n",name.c_str(),first.prom_type.c_str());
		for (size_t idx : indices) {
			const PendingMetric &pm = m_pending[idx];
			if (m_include_timestamp && pm.timestamp != 0) {
				fprintf(fp,"%s%s %s %lld\n",
				        pm.name.c_str(), pm.labels.c_str(), pm.value.c_str(),
				        (long long)pm.timestamp * 1000);
			} else {
				fprintf(fp,"%s%s %s\n",
				        pm.name.c_str(), pm.labels.c_str(), pm.value.c_str());
			}
		}
	}

	if (fclose(fp)) {
		dprintf(D_ERROR, "fclose failed on Prometheus output file %s: %s\n",
		        tmp.c_str(), strerror(errno));
		return;
	}

	if (rename(tmp.c_str(),m_output_file.c_str()) != 0) {
		dprintf(D_ERROR, "Failed to rename %s to %s: %s\n",
		        tmp.c_str(), m_output_file.c_str(), strerror(errno));
	}
}

std::map<std::string,std::string>
PrometheusD::parseLabels(const std::string &s)
{
	std::map<std::string,std::string> result;
	const char *p = s.c_str();
	const char *end = p + s.size();
	while (p < end) {
		// skip whitespace and commas
		while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) ++p;
		if (p >= end) break;

		// read key
		const char *kstart = p;
		while (p < end && *p != '=' && *p != ',' && *p != ' ' && *p != '\t') ++p;
		std::string key(kstart, p - kstart);
		if (key.empty()) break;

		// skip whitespace
		while (p < end && (*p == ' ' || *p == '\t')) ++p;
		if (p >= end || *p != '=') {
			// malformed: skip
			while (p < end && *p != ',') ++p;
			continue;
		}
		++p; // skip '='
		while (p < end && (*p == ' ' || *p == '\t')) ++p;

		// read value (optionally double-quoted)
		std::string value;
		if (p < end && *p == '"') {
			++p;
			while (p < end && *p != '"') {
				if (*p == '\\' && (p + 1) < end) {
					++p;
					value += *p;
				} else {
					value += *p;
				}
				++p;
			}
			if (p < end && *p == '"') ++p;
		} else {
			while (p < end && *p != ',') {
				value += *p;
				++p;
			}
			while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
				value.pop_back();
			}
		}
		result[key] = value;
	}
	return result;
}

std::string
PrometheusD::serializeLabels(const std::map<std::string,std::string> &labels)
{
	if (labels.empty()) return "";
	std::string result = "{";
	bool first = true;
	for (const auto &kv : labels) {
		if (!first) result += ",";
		first = false;
		result += kv.first;
		result += "=\"";
		for (char c : kv.second) {
			if (c == '\\' || c == '"') result += '\\';
			result += c;
		}
		result += "\"";
	}
	result += "}";
	return result;
}

// ---------------------------------------------------------------------------
// HTTP / HTTPS serving
// ---------------------------------------------------------------------------

// Helper: write all bytes to a plain fd or through SSL.
bool
PrometheusD::writeFully(int fd, void *ssl, const void *buf, size_t len)
{
	const char *p = static_cast<const char*>(buf);
	while (len > 0) {
		ssize_t n;
		if (ssl) {
			n = g_SSL_write(static_cast<SSL*>(ssl), p, static_cast<int>(len));
			if (n <= 0) return false;
		} else {
			n = write(fd, p, len);
			if (n < 0 && (errno == EINTR)) continue;
			if (n <= 0) return false;
		}
		p   += n;
		len -= n;
	}
	return true;
}

// Helper: send a minimal HTTP error response and log it.
void
PrometheusD::sendHttpError(int fd, void *ssl, int code, const char *reason)
{
	char buf[256];
	snprintf(buf, sizeof(buf),
	         "HTTP/1.0 %d %s\r\nContent-Length: 0\r\n\r\n", code, reason);
	writeFully(fd, ssl, buf, strlen(buf));
}

// ---------------------------------------------------------------------------
// checkHtpasswd – validate user:password against an Apache-style htpasswd file.
//
// Supported hash formats (in order of preference / prevalence):
//   {SHA}base64  – SHA-1 (base64-encoded digest)
//   $apr1$...    – Apache's MD5-crypt variant (via system crypt())
//   $2y$/2b$...  – bcrypt (via system crypt())
//   $5$/$6$...   – SHA-256/SHA-512 crypt (via system crypt())
//   anything else  – assumed DES crypt (via system crypt())
// ---------------------------------------------------------------------------
bool
PrometheusD::checkHtpasswd(const std::string &path,
                            const std::string &user,
                            const std::string &pass)
{
	FILE *fp = safe_fopen_no_create(path.c_str(), "r");
	if (!fp) {
		dprintf(D_ERROR,
		        "PrometheusD: cannot open htpasswd file '%s': %s\n",
		        path.c_str(), strerror(errno));
		return false;
	}

	bool found = false;
	char line[1024];
	while (fgets(line, sizeof(line), fp)) {
		// Strip trailing newline/CR
		size_t linelen = strlen(line);
		while (linelen > 0 &&
		       (line[linelen-1] == '\n' || line[linelen-1] == '\r')) {
			line[--linelen] = '\0';
		}
		// Skip blank lines and comments
		if (linelen == 0 || line[0] == '#') continue;

		// Split on first ':'
		char *colon = strchr(line, ':');
		if (!colon) continue;
		*colon = '\0';
		const char *file_user = line;
		const char *file_hash = colon + 1;

		if (user != file_user) continue;

		// --- {SHA} format: SHA1 of password, base64-encoded ---
		if (strncmp(file_hash, "{SHA}", 5) == 0) {
			bool sha1_ok = false;
			if (g_SHA1) {
				unsigned char digest[20];
				g_SHA1(reinterpret_cast<const unsigned char*>(pass.c_str()),
				       pass.size(), digest);
				char *b64 = condor_base64_encode(digest, sizeof(digest), false);
				if (b64) {
					sha1_ok = (strcmp(b64, file_hash + 5) == 0);
					free(b64);
				}
			} else {
				dprintf(D_ERROR,
				        "PrometheusD: SHA1 function unavailable;"
				        " cannot validate {SHA} htpasswd entry\n");
			}
			found = sha1_ok;
			break;
		}

		// --- All other formats: delegate to system crypt() ---
		// This covers $apr1$ (Apache MD5), $2y$/$2b$ (bcrypt),
		// $5$ (SHA-256 crypt), $6$ (SHA-512 crypt), and DES.
#if !defined(WIN32)
		errno = 0;
		const char *hashed = crypt(pass.c_str(), file_hash);
		if (hashed) {
			found = (strcmp(hashed, file_hash) == 0);
		} else {
			dprintf(D_ERROR,
			        "PrometheusD: crypt() failed for htpasswd entry"
			        " (unsupported format?): %s\n",
			        strerror(errno));
		}
#endif
		break;
	}
	fclose(fp);
	return found;
}

// ---------------------------------------------------------------------------
// processHttpRequest – parse a complete HTTP request (headers through
// \r\n\r\n) and send the /metrics response.
// ---------------------------------------------------------------------------
void
PrometheusD::processHttpRequest(int fd, std::shared_ptr<PromHttpConn> conn)
{
	void       *ssl     = conn->ssl;
	const std::string &request = conn->request_buf;

	// Validate the request line
	size_t line_end = request.find("\r\n");
	if (line_end == std::string::npos) {
		sendHttpError(fd, ssl, 400, "Bad Request");
		return;
	}
	std::string req_line = request.substr(0, line_end);

	// Only serve GET /metrics (with or without a query string / HTTP version)
	bool valid_path = (req_line.rfind("GET /metrics ", 0) == 0 ||
	                   req_line == "GET /metrics");
	if (!valid_path) {
		sendHttpError(fd, ssl, 404, "Not Found");
		return;
	}

	// Check HTTP Basic auth if a password file is configured
	if (!m_http_auth_file.empty()) {
		bool authed = false;
		size_t pos = 0;
		// Walk headers looking for Authorization
		while (true) {
			size_t eol = request.find("\r\n", pos);
			if (eol == std::string::npos || eol == pos) break;
			std::string hdr = request.substr(pos, eol - pos);
			pos = eol + 2;
			static const char prefix[] = "Authorization: Basic ";
			if (strncasecmp(hdr.c_str(), prefix, sizeof(prefix)-1) == 0) {
				std::string b64 = hdr.substr(sizeof(prefix)-1);
				unsigned char *decoded = nullptr;
				int decoded_len = 0;
				condor_base64_decode(b64.c_str(), &decoded, &decoded_len, false);
				if (decoded && decoded_len > 0) {
					// decoded is "user:password"
					char *sep = static_cast<char*>(
					    memchr(decoded, ':', decoded_len));
					if (sep) {
						std::string u(reinterpret_cast<char*>(decoded),
						              sep - reinterpret_cast<char*>(decoded));
						std::string p(sep + 1,
						              reinterpret_cast<char*>(decoded) +
						              decoded_len);
						authed = checkHtpasswd(m_http_auth_file, u, p);
					}
					free(decoded);
				}
				break;
			}
		}
		if (!authed) {
			const char resp[] =
			    "HTTP/1.0 401 Unauthorized\r\n"
			    "WWW-Authenticate: Basic realm=\"metrics\"\r\n"
			    "Content-Length: 0\r\n\r\n";
			writeFully(fd, ssl, resp, sizeof(resp) - 1);
			return;
		}
	}

	// Read the metrics file
	if (m_output_file.empty()) {
		sendHttpError(fd, ssl, 503, "Service Unavailable");
		return;
	}
	FILE *fp = safe_fopen_no_create(m_output_file.c_str(), "r");
	if (!fp) {
		sendHttpError(fd, ssl, 503, "Service Unavailable");
		return;
	}
	std::string body;
	char ibuf[4096];
	size_t n;
	while ((n = fread(ibuf, 1, sizeof(ibuf), fp)) > 0) {
		body.append(ibuf, n);
	}
	fclose(fp);

	// Send 200 OK
	char hdr[256];
	snprintf(hdr, sizeof(hdr),
	         "HTTP/1.0 200 OK\r\n"
	         "Content-Type: text/plain; version=0.0.4\r\n"
	         "Content-Length: %zu\r\n"
	         "\r\n",
	         body.size());
	writeFully(fd, ssl, hdr, strlen(hdr));
	writeFully(fd, ssl, body.data(), body.size());
}

// ---------------------------------------------------------------------------
// continueHttpRead – socket-ready callback.  Accumulate more request data
// until the header block is complete, then process.  Returns KEEP_STREAM
// to stay registered when more data is still needed, FALSE otherwise
// (which causes DaemonCore to cancel the registration and delete the stream).
// ---------------------------------------------------------------------------
int
PrometheusD::continueHttpRead(Stream *s, std::shared_ptr<PromHttpConn> conn)
{
	Sock *sock = static_cast<Sock*>(s);
	int   fd   = sock->get_file_desc();

	char buf[4096];
	ssize_t n = recv(fd, buf, sizeof(buf) - 1, MSG_DONTWAIT);
	if (n > 0) {
		conn->request_buf.append(buf, n);
	} else if (n == 0) {
		// Peer closed connection before sending a complete request
		return FALSE;
	} else if (errno != EAGAIN && errno != EWOULDBLOCK) {
		dprintf(D_FULLDEBUG,
		        "PrometheusD: read error from %s: %s\n",
		        sock->peer_description(), strerror(errno));
		return FALSE;
	}

	if (conn->request_buf.size() > 65536) {
		sendHttpError(fd, nullptr, 413, "Request Too Large");
		return FALSE;
	}

	if (conn->request_buf.find("\r\n\r\n") == std::string::npos) {
		// Still waiting for the end of headers – stay registered
		return KEEP_STREAM;
	}

	processHttpRequest(fd, conn);
	return FALSE;
}

// ---------------------------------------------------------------------------
// handleHttpCommand – the DaemonCore HTTP command handler entry point.
//
// For TLS connections: performs a blocking SSL_accept (bounded by a short
// SO_RCVTIMEO / SO_SNDTIMEO) then reads the full HTTP request synchronously.
// The SSL handshake is a fast machine-to-machine operation so a brief block
// is acceptable here.
//
// For plain HTTP connections: attempts a non-blocking recv.  If insufficient
// data has arrived yet the socket is registered with DaemonCore and the
// handler returns KEEP_STREAM so the event loop is not blocked.
// ---------------------------------------------------------------------------
int
PrometheusD::handleHttpCommand(int /*cmd*/, Stream *s)
{
	Sock *sock = static_cast<Sock*>(s);
	int   fd   = sock->get_file_desc();

	// Peek at the first 3 bytes to decide plain HTTP vs TLS.
	// DaemonCore already confirmed these bytes are available (it peeked them
	// before routing here), so MSG_PEEK|MSG_DONTWAIT should succeed immediately.
	unsigned char peek[3] = {0, 0, 0};
	recv(fd, peek, sizeof(peek), MSG_PEEK | MSG_DONTWAIT);
	// TLS ClientHello: record type 0x16, legacy version 0x03 0x00–0x04
	bool is_tls = (peek[0] == 0x16 && peek[1] == 0x03 && peek[2] <= 0x04);

	if (is_tls) {
		// ---- TLS path -------------------------------------------------------
		if (!m_ssl_ctx) {
			dprintf(D_ERROR,
			        "PrometheusD: received TLS connection from %s but"
			        " no SSL context configured; closing\n",
			        sock->peer_description());
			return FALSE;
		}

		// Bound the handshake and subsequent I/O with a 10-second timeout.
		struct timeval tv = {10, 0};
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
		           &tv, static_cast<socklen_t>(sizeof(tv)));
		setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
		           &tv, static_cast<socklen_t>(sizeof(tv)));

		SSL *ssl = g_SSL_new(static_cast<SSL_CTX*>(m_ssl_ctx));
		if (!ssl) {
			dprintf(D_ERROR, "PrometheusD: SSL_new failed\n");
			return FALSE;
		}
		g_SSL_set_fd(ssl, fd);

		if (g_SSL_accept(ssl) != 1) {
			int err = g_SSL_get_error(ssl, -1);
			dprintf(D_FULLDEBUG,
			        "PrometheusD: SSL_accept failed (error %d) from %s\n",
			        err, sock->peer_description());
			g_SSL_free(ssl);
			return FALSE;
		}

		// Read HTTP request through SSL with the timeout already set above.
		auto conn = std::make_shared<PromHttpConn>();
		conn->ssl = ssl;

		char buf[4096];
		while (conn->request_buf.find("\r\n\r\n") == std::string::npos) {
			int n = g_SSL_read(ssl, buf, sizeof(buf) - 1);
			if (n <= 0) {
				int err = g_SSL_get_error(ssl, n);
				dprintf(D_FULLDEBUG,
				        "PrometheusD: SSL_read error %d from %s\n",
				        err, sock->peer_description());
				g_SSL_shutdown(ssl);
				g_SSL_free(ssl);
				return FALSE;
			}
			conn->request_buf.append(buf, n);
			if (conn->request_buf.size() > 65536) {
				sendHttpError(fd, ssl, 413, "Request Too Large");
				g_SSL_shutdown(ssl);
				g_SSL_free(ssl);
				return FALSE;
			}
		}

		processHttpRequest(fd, conn);

		g_SSL_shutdown(ssl);
		g_SSL_free(ssl);
		return FALSE;   // done; DaemonCore will delete the stream
	}

	// ---- Plain HTTP path ----------------------------------------------------
	auto conn = std::make_shared<PromHttpConn>();

	char buf[4096];
	ssize_t n = recv(fd, buf, sizeof(buf) - 1, MSG_DONTWAIT);
	if (n > 0) {
		conn->request_buf.append(buf, n);
	} else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		dprintf(D_FULLDEBUG,
		        "PrometheusD: initial recv error from %s: %s\n",
		        sock->peer_description(), strerror(errno));
		return FALSE;
	}

	if (conn->request_buf.find("\r\n\r\n") != std::string::npos) {
		// Already have the full header block – process immediately.
		processHttpRequest(fd, conn);
		return FALSE;
	}

	// Need more data.  Register the socket and yield to the event loop.
	int rc = daemonCore->Register_Socket(
		s,
		"PrometheusD HTTP /metrics",
		[this, conn](Stream *s2) -> int {
			return this->continueHttpRead(s2, conn);
		},
		"PrometheusD::continueHttpRead");

	if (rc < 0) {
		dprintf(D_ERROR,
		        "PrometheusD: Register_Socket failed (rc=%d); closing connection\n", rc);
		return FALSE;
	}

	return KEEP_STREAM;
}
