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

class PrometheusMetric: public Metric {
 public:
	std::string prometheusType() const;
};

class PrometheusD: public StatsD {
 public:
	PrometheusD();

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

	std::string m_output_file;
	bool m_include_timestamp{false};
	std::map<std::string,std::string> m_default_labels;
	std::vector<PendingMetric> m_pending;

	std::string buildPrometheusName(const Metric &m) const;
	std::string buildPrometheusHelp(const Metric &m) const;
	std::string buildEffectiveLabels(const Metric &m) const;
	void writeMetricsFile();
	static std::map<std::string,std::string> parseLabels(const std::string &s);
	static std::string serializeLabels(const std::map<std::string,std::string> &labels);
};

#endif
