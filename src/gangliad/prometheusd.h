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

#ifndef __PROMETHEUSD_H__
#define __PROMETHEUSD_H__

#include "statsd.h"
#include <map>
#include <memory>

class PrometheusMetric: public Metric {
 public:
	std::string prometheusType() const;
};

class PrometheusD: public StatsD {
 public:
	PrometheusD();
	virtual ~PrometheusD();

	virtual void initAndReconfig();

	virtual Metric *newMetric(Metric const *copy_me = NULL);
	virtual void publishMetric(Metric const &metric);
	virtual void initializeHostList() {}
	virtual void sendHeartbeats() {}
	virtual void postPublishMetrics();
	virtual void extraProjectionRefs(classad::References &refs) const;
	virtual const char *exportFilterName() const { return "prometheus"; }
	virtual const char *backendName() const { return "prometheus"; }

 private:
	struct PendingMetric {
		std::string name;
		std::string labels;
		std::string value;
		std::string help;
		std::string prom_type;
		int64_t timestamp{0};
	};

	// Per-connection state for the non-blocking HTTP read loop.
	struct PromHttpConn {
		std::string request_buf;
		void       *ssl{nullptr};   // SSL* if TLS, nullptr for plain HTTP
	};

	std::string m_output_file;
	bool m_include_timestamp{false};
	std::map<std::string,std::string> m_default_labels;
	std::vector<PendingMetric> m_pending;
	std::string m_reset_metrics_filename;

	// HTTP / HTTPS serving
	bool  m_http_handler_registered{false};
	std::string m_http_auth_file;
	void *m_ssl_ctx{nullptr};       // SSL_CTX* when TLS is configured

	std::string buildPrometheusName(const Metric &m) const;
	std::string buildPrometheusHelp(const Metric &m) const;
	std::string buildEffectiveLabels(const Metric &m) const;
	void writeMetricsFile();
	static std::map<std::string,std::string> parseLabels(const std::string &s);
	static std::string serializeLabels(const std::map<std::string,std::string> &labels);

	// HTTP command handler registered with DaemonCore.
	int handleHttpCommand(int cmd, Stream *s);

	// Socket-ready callback used when the first recv returns EAGAIN.
	int continueHttpRead(Stream *s, std::shared_ptr<PromHttpConn> conn);

	// Parse a complete HTTP request and send the metrics response.
	void processHttpRequest(int fd, std::shared_ptr<PromHttpConn> conn);

	// Write all bytes to fd (plain) or ssl (TLS).  Returns false on error.
	static bool writeFully(int fd, void *ssl, const void *buf, size_t len);

	// Send a minimal HTTP error response.
	static void sendHttpError(int fd, void *ssl, int code, const char *reason);

	// Validate user:password against an Apache-style htpasswd file.
	// Supports {SHA}, $apr1$, bcrypt ($2y$/$2b$), and standard crypt().
	static bool checkHtpasswd(const std::string &path,
	                          const std::string &user,
	                          const std::string &pass);

	// (Re)build m_ssl_ctx from AUTH_SSL_SERVER_CERTFILE / AUTH_SSL_SERVER_KEYFILE.
	void buildSslCtx();
};

#endif
