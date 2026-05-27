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
#include "prometheusd.h"

#include <map>
#include <unordered_map>
#include <algorithm>

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

void
PrometheusD::initAndReconfig(const char * /*unused*/)
{
	// Parse metric definitions and learn target types, but never register a
	// timer of our own (the owning MetricD runs the cycle).
	StatsD::initAndReconfig("METRICD", true);

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
